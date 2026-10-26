#include "../dual/lite_memory.h"
#include "options.h"
#if SDC_CS!=255
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "vfs_api.h"
#include "sd_diskio.h"
//#define USE_SD
#include "config.h"
#include "sdmanager.h"
#include "display.h"
#include "player.h"
#include "../dual/media.h"
#include "../dual/alarm_sd_policy.h"
#include "sd_path.h"
#include "ff.h"
#include <esp_heap_caps.h>
#include <memory>
#include <new>

#if defined(SD_SPIPINS) || SD_HSPI
SPIClass  SDSPI(DUAL_SD_HOST);
#define SDREALSPI SDSPI
#else
  #define SDREALSPI SPI
#endif

#ifndef SDSPISPEED
  #define SDSPISPEED 20000000
#endif

SDManager sdman(FSImplPtr(new VFSImpl()));

bool SDManager::start(){
  if(ready)return true;
  Serial.printf("[SD-MOUNT] START cs=%u hz=%lu\n",unsigned(SDC_CS),(unsigned long)SDSPISPEED);
  #if FF_FS_TINY
  constexpr unsigned sharedWindow=1;
  #else
  constexpr unsigned sharedWindow=0;
  #endif
  Serial.printf("[SD-FS] LFN_enabled=%u UTF8=%u exFAT=%u NTFS=0 shared_window=%u max_open=5\n",unsigned(FF_USE_LFN!=0),unsigned(FF_LFN_UNICODE==2),unsigned(FF_FS_EXFAT),sharedWindow);
  ready = begin(SDC_CS, SDREALSPI, SDSPISPEED);
  vTaskDelay(10);
  if(!ready) ready = begin(SDC_CS, SDREALSPI, SDSPISPEED);
  vTaskDelay(20);
  if(!ready) ready = begin(SDC_CS, SDREALSPI, SDSPISPEED);
  vTaskDelay(50);
  if(!ready) ready = begin(SDC_CS, SDREALSPI, SDSPISPEED);
  Serial.printf("[SD-MOUNT] result=%s\n",ready?"PASS":"FAIL");
  return ready;
}

void SDManager::stop(){
  if(scanIoLock)xSemaphoreTake(scanIoLock,portMAX_DELAY);
  catalogReady.store(false);
  indexedTracks.store(0);
  if(activeAlarmScan){activeAlarmScan->playlist.close();activeAlarmScan->status=2;}
  end();
  ready = false;
#if IBT_NO_PSRAM
  releaseIdlePath();
#endif
  if(scanIoLock)xSemaphoreGive(scanIoLock);
}
#include "diskio_impl.h"
bool SDManager::cardPresent() {

  if(!ready) return false;
  if(sectorSize()<1) {
    return false;
  }
  uint8_t buff[sectorSize()] = { 0 };
  bool bread = readRAW(buff, 1);
  if(sectorSize()>0 && !bread) return false;
  return bread;
}

bool SDManager::_checkNoMedia(const char* path){
  String full;
  if(!path||!full.concat(path)||(!full.endsWith("/")&&!full.concat("/"))||!full.concat(".nomedia")||full.length()>=ibtsd::PathCapacity){
    indexCancel.store(true);Serial.println("[SD-INDEX] FAIL nomedia path memory");return true;
  }
  return exists(full.c_str());
}

bool SDManager::indexExpired() const {
  return indexCancel.load() || uint32_t(millis()-indexStarted)>=45000;
}

bool SDManager::_endsWith (const char* base, const char* str) {
  int slen = strlen(str) - 1;
  const char *p = base + strlen(base) - 1;
  while(p > base && isspace(*p)) p--;
  p -= slen;
  if (p < base) return false;
  return (strncmp(p, str, slen) == 0);
}

namespace { char* selectedSdPath=nullptr; size_t selectedSdCapacity=0; }
bool SDManager::selectPath(const char* path,size_t length){
  if(!path||!length||length>=ibtsd::PathCapacity){if(selectedSdPath)selectedSdPath[0]=0;return false;}
#if IBT_NO_PSRAM
  const size_t wanted=length+1;
#else
  const size_t wanted=ibtsd::PathCapacity;
#endif
  if(selectedSdCapacity<wanted){
    auto* next=(char*)heap_caps_realloc(selectedSdPath,wanted,litememory::payloadCaps);
    if(!next){Serial.println("[SD-AUDIO] FAIL path memory");return false;}
    selectedSdPath=next;selectedSdCapacity=wanted;
  }
  if(!selectedSdPath){Serial.println("[SD-AUDIO] FAIL path memory");return false;}
  memcpy(selectedSdPath,path,length);selectedSdPath[length]=0;return true;
}
const char* SDManager::selectedPath() const {return selectedSdPath?selectedSdPath:"";}
bool SDManager::releaseIdlePath(){
  if(!player.onAudioTask()||player.isRunning())return false;
  const unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
  const unsigned before=heap_caps_get_free_size(caps),largestBefore=heap_caps_get_largest_free_block(caps);
  const unsigned released=unsigned(selectedSdCapacity);
  heap_caps_free(selectedSdPath);selectedSdPath=nullptr;selectedSdCapacity=0;
  Serial.printf("[BT-MEMORY] owner=SD_PATH released=%u before=%u/%u after=%u/%u reload=PLAY_FROM_INDEX\n",released,before,largestBefore,unsigned(heap_caps_get_free_size(caps)),unsigned(heap_caps_get_largest_free_block(caps)));
  return true;
}
void SDManager::listSD(File &plSDfile,File &plSDindex,const char* dirname,uint8_t levels){
  // FatFs enumeration avoids libc dirent's 255-byte UTF-8 truncation.
  // Same allocator as FatFs: PSRAM preferred, checked internal fallback.
  auto release=[](void* p){ff_memfree(p);};
  std::unique_ptr<FILINFO,decltype(release)> info((FILINFO*)ff_memalloc(sizeof(FILINFO)),release);
  std::unique_ptr<FF_DIR,decltype(release)> root((FF_DIR*)ff_memalloc(sizeof(FF_DIR)),release);
  if(!info||!root){indexCancel.store(true);Serial.println("[SD-INDEX] FAIL directory memory");return;}
  String drive=String(_pdrv);
  if(!drive.length()||!drive.concat(":")||!drive.concat(dirname)){
    indexCancel.store(true);Serial.println("[SD-INDEX] FAIL drive path memory");return;
  }
  FRESULT result=f_opendir(root.get(),drive.c_str());
  if(result!=FR_OK){indexCancel.store(true);Serial.printf("[SD-INDEX] FAIL opendir=%u\n",unsigned(result));return;}
  while(!indexExpired()){
    vTaskDelay(2);
    result=f_readdir(root.get(),info.get());
    if(result!=FR_OK){indexCancel.store(true);Serial.printf("[SD-INDEX] FAIL readdir=%u\n",unsigned(result));break;}
    if(!info->fname[0])break;
    if(!strcmp(info->fname,".")||!strcmp(info->fname,".."))continue;
    ++scanStats.entries;
    if(scanStats.logged++<24)Serial.printf("[SD-ENTRY] dir=%s name=%s kind=%s bytes=%llu\n",dirname,info->fname,(info->fattrib&AM_DIR)?"DIR":"FILE",(unsigned long long)info->fsize);
    String fileName;
    if(!fileName.concat(dirname)||(!fileName.endsWith("/")&&!fileName.concat("/"))||!fileName.concat(info->fname)){
      indexCancel.store(true);Serial.println("[SD-INDEX] FAIL file path memory");break;
    }
    if(fileName.length()+1>=ibtsd::PathCapacity||strpbrk(info->fname,"\t\r\n")){
      ++scanStats.invalid;Serial.println("[SD-INDEX] SKIP unrepresentable path");continue;
    }
    const char* filePath=fileName.c_str();
    if(info->fattrib&AM_DIR){
      if(!levels){++scanStats.depth;Serial.printf("[SD-INDEX] SKIP depth path=%s\n",filePath);}
      else if(_checkNoMedia(filePath)){++scanStats.nomedia;Serial.printf("[SD-INDEX] SKIP nomedia path=%s\n",filePath);}
      else listSD(plSDfile,plSDindex,filePath,levels-1);
    }
    else if((++scanStats.files,dual::supportedAudioName(info->fname))){
      // Existing audio counters/seek APIs are 32-bit; fail closed above that range.
      if(info->fsize>UINT32_MAX){Serial.println("[SD-INDEX] SKIP file over 4GB");continue;}
      uint32_t pos=plSDfile.position();
      size_t wanted=strlen(info->fname)+strlen(filePath)+4;
      if(plSDfile.printf("%s\t%s\t0\n",info->fname,filePath)!=wanted||plSDindex.write((uint8_t*)&pos,4)!=4){
        indexCancel.store(true);Serial.println("[SD-INDEX] FAIL index write");break;
      }
      if(display.mode()==SDCHANGE)display.putRequest(SDFILEINDEX,_sdFCount+1);
      indexedTracks.store(++_sdFCount);
      if(_sdFCount%64==0)Serial.printf("[SD-AUDIO] indexed=%lu\n",(unsigned long)_sdFCount);
      if(_sdFCount>=65535){indexCancel.store(true);break;}
    }else ++scanStats.unsupported;
  }
  if(f_closedir(root.get())!=FR_OK)indexCancel.store(true);
}

bool SDManager::indexSDPlaylist() {
  Serial.printf("[SD-INDEX] START playlist=%s index=%s\n",PLAYLIST_SD_PATH,INDEX_SD_PATH);
  indexError.store(sdroute::Error::ReadIndex);
  catalogReady.store(false);
  // An incremental reader must finish its current small read before index
  // replacement starts. It sees catalogReady=false on its next step.
  if(scanIoLock){
    xSemaphoreTake(scanIoLock,portMAX_DELAY);
    if(activeAlarmScan){activeAlarmScan->playlist.close();activeAlarmScan->status=2;}
    xSemaphoreGive(scanIoLock);
  }
  _sdFCount = 0;
  scanStats={};
  indexedTracks.store(0);
  indexStarted=millis();
  // A blank FAT card does not contain yoRadio's private index directory.
  if(!exists("/data") && !mkdir("/data")) {
    Serial.println("[SD-AUDIO] FAIL create /data");
    return false;
  }
  if(exists(PLAYLIST_SD_PATH)) remove(PLAYLIST_SD_PATH);
  if(exists(INDEX_SD_PATH)) remove(INDEX_SD_PATH);
  File playlist = open(PLAYLIST_SD_PATH, "w", true);
  if (!playlist) {
    Serial.println("[SD-AUDIO] FAIL open playlist");
    return false;
  }
  File index = open(INDEX_SD_PATH, "w", true);
  if(!index){playlist.close();remove(PLAYLIST_SD_PATH);Serial.println("[SD-AUDIO] FAIL open index");return false;}
  listSD(playlist, index, "/", SD_MAX_LEVELS);
  Serial.printf("[SD-CATALOG] entries=%lu files=%lu accepted=%lu unsupported=%lu nomedia=%lu depth=%lu invalid=%lu cancelled=%u\n",(unsigned long)scanStats.entries,(unsigned long)scanStats.files,(unsigned long)_sdFCount,(unsigned long)scanStats.unsupported,(unsigned long)scanStats.nomedia,(unsigned long)scanStats.depth,(unsigned long)scanStats.invalid,indexCancel.load()?1:0);
  index.flush();
  playlist.flush();
  const bool valid=!indexExpired()&&_sdFCount>0&&index.size()==_sdFCount*4&&playlist.size()>0;
  const auto failure=sdroute::catalogFailure(!indexExpired(),_sdFCount,index.size()==0&&playlist.size()==0);
  index.close();
  playlist.flush();
  playlist.close();
  if(!valid){indexError.store(failure);remove(INDEX_SD_PATH);Serial.printf("[SD-AUDIO] FAIL tracks=%lu cancelled=%u elapsed_ms=%lu\n",(unsigned long)_sdFCount,indexCancel.load()?1:0,(unsigned long)(millis()-indexStarted));return false;}
  indexError.store(sdroute::Error::None);
  catalogReady.store(true);
  Serial.printf("[SD-AUDIO] READY tracks=%lu codecs=MP3,AAC-SBR,M4A-AAC-ALAC,WAV-PCM-ADPCM-G711,FLAC,OGG-VORBIS-OPUS,AMR,AWB,TS,MPEG\n",(unsigned long)_sdFCount);
  Serial.println();
  delay(50);
  return true;
}
void SDManager::freeAlarmFolderScan(){
  if(!activeAlarmScan)return;
  if(xSemaphoreTake(scanIoLock,0)!=pdTRUE)return; // UI retries cleanup next loop
  auto* scan=activeAlarmScan;activeAlarmScan=nullptr;
  scan->playlist.close();heap_caps_free(scan->tracks);
  scan->~AlarmFolderScan();heap_caps_free(scan);
  xSemaphoreGive(scanIoLock);
}
bool SDManager::beginAlarmFolderScan(){
  freeAlarmFolderScan();
  if(activeAlarmScan)return false;
  if(!scanIoLock)scanIoLock=xSemaphoreCreateMutex();
  if(!scanIoLock)return false;
  if(xSemaphoreTake(scanIoLock,0)!=pdTRUE)return false;
  const uint32_t total=indexedTracks.load();
  const bool available=ready&&catalogReady.load()&&total&&total<=65535;
  xSemaphoreGive(scanIoLock);
  if(!available)return false;
  void* storage=heap_caps_malloc(sizeof(AlarmFolderScan),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!storage)return false;
  auto* candidate=new(storage) AlarmFolderScan{};
  candidate->tracks=(uint16_t*)heap_caps_malloc(total*sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!candidate->tracks){candidate->~AlarmFolderScan();heap_caps_free(candidate);return false;}
  if(xSemaphoreTake(scanIoLock,0)!=pdTRUE){heap_caps_free(candidate->tracks);candidate->~AlarmFolderScan();heap_caps_free(candidate);return false;}
  if(ready&&catalogReady.load()&&indexedTracks.load()==total)candidate->playlist=open(PLAYLIST_SD_PATH,"r");
  if(!candidate->playlist){xSemaphoreGive(scanIoLock);heap_caps_free(candidate->tracks);candidate->~AlarmFolderScan();heap_caps_free(candidate);return false;}
  candidate->total=total;candidate->started=millis();activeAlarmScan=candidate;
  xSemaphoreGive(scanIoLock);
  Serial.printf("[ALARM-SD-SCAN] start total=%lu bytes_per_step=256\n",(unsigned long)total);
  return true;
}
void SDManager::stepAlarmFolderScan(AlarmFolderScan* scan){
  if(!scan||scan->status)return;
  if(uint32_t(millis()-scan->started)>=45000){scan->status=2;return;}
  // Never wait for the SD lock on the UI task. Keeping the file open avoids
  // repeatedly seeking from the beginning of a large FAT catalog. stop() and
  // reindexing close this handle under the same lock before changing the card.
  if(xSemaphoreTake(scanIoLock,0)!=pdTRUE)return;
  struct Unlock {SemaphoreHandle_t lock;~Unlock(){xSemaphoreGive(lock);}} unlock{scanIoLock};
  uint8_t chunk[256];size_t n=0;bool eof=false,valid=false;
  if(ready&&catalogReady.load()&&indexedTracks.load()==scan->total){
    if(scan->playlist){
      n=scan->playlist.read(chunk,sizeof(chunk));
      eof=scan->playlist.position()==scan->playlist.size();valid=n>0||eof;
      if(eof)scan->playlist.close();
    }
  }
  if(!valid){scan->status=2;return;}
  scan->offset+=n;
  for(size_t i=0;i<n;++i){
    if(chunk[i]!='\n'){
      if(scan->used>=sizeof(scan->line)-1){scan->status=2;return;}
      scan->line[scan->used++]=char(chunk[i]);continue;
    }
    scan->line[scan->used]=0;
    const char *name=nullptr,*path=nullptr;size_t nameLen=0,pathLen=0;
    if(scan->row>=scan->total||!ibtsd::record(scan->line,name,nameLen,path,pathLen)){
      scan->status=2;return;
    }
    if(alarmsd::inFolder(path,pathLen))scan->tracks[scan->count++]=uint16_t(scan->row+1);
    ++scan->row;scan->used=0;
  }
  if(eof){
    scan->status=scan->used==0&&scan->row==scan->total&&scan->count?1:2;
    Serial.printf("[ALARM-SD-SCAN] result=%s rows=%lu tracks=%u elapsed_ms=%lu\n",
      scan->status==1?"READY":"FAIL",(unsigned long)scan->row,unsigned(scan->count),
      (unsigned long)(millis()-scan->started));
  }
}
#endif

