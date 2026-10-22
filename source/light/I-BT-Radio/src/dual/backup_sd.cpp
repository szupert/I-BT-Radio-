#include "../core/options.h"
#include "backup_sd.h"
#include "backup_seal.h"
#include "../core/config.h"
#include "../core/player.h"
#include "../core/netserver.h"
#include "core.h"
#if FEATURE_SD && SDC_CS!=255
#include "../core/sdmanager.h"
#include <mbedtls/sha256.h>
#if defined(IBT_BT_PROBE)
#include "bt_mode_manager.h"
#endif
#endif

namespace backupsd {
namespace {
Result previous=Result::None;
Operation action=Operation::SavePlaylist;

#if FEATURE_SD && SDC_CS!=255
constexpr const char* directory="/IBTBACK";
constexpr size_t playlistMax=256*1024;
constexpr size_t wifiMax=5*(WIFI_SSID_CAP+WIFI_PASS_CAP+2);

static void paths(Operation op,unsigned slot,char data[32],char seal[32]){
    const char letter=(op==Operation::SavePlaylist||op==Operation::RestorePlaylist)?'P':'W';
    snprintf(data,32,"%s/%c%02u.DAT",directory,letter,slot);
    snprintf(seal,32,"%s/%c%02u.SHA",directory,letter,slot);
}
static bool ready(){
    if(!sdman.ready && !sdman.start())return false;
    return sdman.cardPresent();
}
static bool audioStopped(){return !player.isRunning() && player.status()==STOPPED;}
static bool hashFile(File& file,size_t maxBytes,uint32_t& bytes,uint8_t hash[32]){
    if(!file || !file.size() || file.size()>maxBytes)return false;
    mbedtls_sha256_context ctx;mbedtls_sha256_init(&ctx);
    bool good=mbedtls_sha256_starts(&ctx,0)==0;
    uint8_t buffer[1024];size_t total=0;
    while(good && file.available()){
        if(!audioStopped()){good=false;break;}
        size_t count=file.read(buffer,sizeof(buffer));
        if(!count || total+count>maxBytes || mbedtls_sha256_update(&ctx,buffer,count)!=0){good=false;break;}
        total+=count;
        if((total&0x3fff)==0)vTaskDelay(1);
    }
    good=good && total==file.size() && total<=UINT32_MAX && mbedtls_sha256_finish(&ctx,hash)==0;
    mbedtls_sha256_free(&ctx);
    if(good)bytes=uint32_t(total);
    return good;
}
static bool copyFile(File& source,File& target,size_t limit,uint32_t& bytes,uint8_t hash[32]){
    if(!source || !target || !source.size() || source.size()>limit)return false;
    mbedtls_sha256_context ctx;mbedtls_sha256_init(&ctx);
    bool good=mbedtls_sha256_starts(&ctx,0)==0;
    uint8_t buffer[1024];size_t total=0;
    while(good && source.available()){
        if(!audioStopped()){good=false;break;}
        size_t count=source.read(buffer,sizeof(buffer));
        if(!count || total+count>limit || target.write(buffer,count)!=count ||
           mbedtls_sha256_update(&ctx,buffer,count)!=0){good=false;break;}
        total+=count;
        if((total&0x3fff)==0)vTaskDelay(1);
    }
    target.flush();
    good=good && total==source.size() && target.size()==total && total<=UINT32_MAX &&
         mbedtls_sha256_finish(&ctx,hash)==0;
    mbedtls_sha256_free(&ctx);
    if(good)bytes=uint32_t(total);
    return good;
}
static bool verify(fs::FS& fs,const char* path,size_t limit,uint32_t bytes,const uint8_t hash[32]){
    File file=fs.open(path,"r");uint32_t actual=0;uint8_t digest[32];
    bool good=hashFile(file,limit,actual,digest)&&actual==bytes&&memcmp(hash,digest,32)==0;
    file.close();return good;
}
static Result save(Operation op){
    const bool playlist=op==Operation::SavePlaylist;
    const char* sourcePath=playlist?PLAYLIST_PATH:SSIDS_PATH;
    const size_t limit=playlist?playlistMax:wifiMax;
    File source=SPIFFS.open(sourcePath,"r");
    if(!source || !source.size() || source.size()>limit){source.close();return Result::NoSource;}
    if(!sdman.exists(directory) && !sdman.mkdir(directory)){source.close();return Result::WriteFailed;}
    char dataPath[32],sealPath[32];unsigned slot=0;
    for(;slot<100;slot++){
        paths(op,slot,dataPath,sealPath);
        if(!sdman.exists(dataPath) && !sdman.exists(sealPath))break;
    }
    if(slot==100){source.close();return Result::Full;}
    File target=sdman.open(dataPath,"w");uint32_t bytes=0;uint8_t hash[32];
    bool good=copyFile(source,target,limit,bytes,hash);
    target.close();source.close();
    if(good)good=verify(sdman,dataPath,limit,bytes,hash);
    uint8_t seal[ibtbackup::kSealSize];
    if(good)good=ibtbackup::encodeSeal(seal,sizeof(seal),bytes,hash);
    if(good){
        File marker=sdman.open(sealPath,"w");
        good=marker && marker.write(seal,sizeof(seal))==sizeof(seal);
        marker.close();
        if(good){
            File check=sdman.open(sealPath,"r");uint8_t readback[sizeof(seal)];
            good=check && check.size()==sizeof(seal) && check.read(readback,sizeof(readback))==sizeof(readback) && memcmp(readback,seal,sizeof(seal))==0;
            check.close();
        }
    }
    if(!good){sdman.remove(sealPath);sdman.remove(dataPath);return Result::WriteFailed;}
    return Result::Pass;
}
static Result restore(Operation op){
    const bool playlist=op==Operation::RestorePlaylist;
    const size_t limit=playlist?playlistMax:wifiMax;
    char dataPath[32],sealPath[32];int slot=99;
    for(;slot>=0;slot--){paths(op,unsigned(slot),dataPath,sealPath);if(sdman.exists(sealPath))break;}
    if(slot<0)return Result::NoBackup;
    File marker=sdman.open(sealPath,"r");uint8_t seal[ibtbackup::kSealSize];
    bool good=marker && marker.size()==sizeof(seal) && marker.read(seal,sizeof(seal))==sizeof(seal);
    marker.close();
    uint32_t bytes=0;uint8_t hash[32];
    if(!good || !ibtbackup::decodeSeal(seal,sizeof(seal),limit,bytes,hash) ||
       !verify(sdman,dataPath,limit,bytes,hash))return Result::Invalid;
    // The WebUI import uses its own staging paths. A local SD restore must not
    // erase a partially received HTTP upload even if both are attempted.
    const char* stage=playlist?PLAYLIST_RESTORE_PATH:WIFI_RESTORE_PATH;
    SPIFFS.remove(stage);
    File source=sdman.open(dataPath,"r"),target=SPIFFS.open(stage,"w");
    uint32_t copied=0;uint8_t digest[32];
    good=copyFile(source,target,limit,copied,digest);
    source.close();target.close();
    good=good && copied==bytes && memcmp(hash,digest,32)==0 && verify(SPIFFS,stage,limit,bytes,hash);
    if(!good){SPIFFS.remove(stage);return Result::RestoreFailed;}
    if(!audioStopped()){SPIFFS.remove(stage);return Result::Busy;}
    if(playlist){
        good=config.installPlaylist(stage);
        if(good){
            dual::invalidateStationSearchCache();dual::invalidateFavorites();
            dual::preloadStationSearch();netserver.requestOnChange(PLAYLIST,0);
        }
    }else good=config.saveWifi(stage); // validates, replaces atomically, then reboots
    if(!good){SPIFFS.remove(stage);return Result::RestoreFailed;}
    return Result::Pass;
}
#endif
}
Result run(Operation op){
    action=op;
#if FEATURE_SD && SDC_CS!=255
    if(player.isRunning() || player.status()!=STOPPED || config.getMode()!=PM_WEB){previous=Result::Busy;return previous;}
#if defined(IBT_BT_PROBE)
    if(btmode::state()!=btmode::State::Off && btmode::state()!=btmode::State::Fault &&
       btmode::state()!=btmode::State::ScanComplete){previous=Result::Busy;return previous;}
#endif
    if(!ready()){previous=Result::NoCard;return previous;}
    previous=(op==Operation::SavePlaylist||op==Operation::SaveWifi)?save(op):restore(op);
#else
    previous=Result::NoCard;
#endif
    return previous;
}
Result lastResult(){return previous;}
Operation lastOperation(){return action;}
}
