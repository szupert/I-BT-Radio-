#include "../core/options.h"
#include "boot_custom.h"
#include "boot_image_format.h"
#if IBT_NO_PSRAM
#include <SPIFFS.h>
#include <Preferences.h>
#include <Arduino.h>
#include <atomic>
#include <new>
#include <algorithm>
#include <initializer_list>
#include <cstring>
#include "../core/replace_file.h"
namespace bootcustom {
static constexpr const char* Target="/boot.img",*Staged="/boot.new",*Previous="/boot.prev";
static File image;
static std::atomic<bool> painting{false},present{false};
static std::atomic<unsigned> hold{0};
static std::atomic<uint32_t> installedCrc{0};
struct Upload {File file;size_t written=0;uint32_t crc=0xffffffffu;uint8_t header[bootimage::Header]{};bool headerValid=false,failed=false;};
static std::atomic<Upload*> upload{nullptr};
static bool validate(File& file,uint32_t& crc){
 uint8_t header[bootimage::Header];if(!file||file.size()!=bootimage::Total||!file.seek(0)||file.read(header,sizeof(header))!=sizeof(header)||!bootimage::valid(header,file.size()))return false;
 uint8_t chunk[640];uint32_t remaining=bootimage::Payload,value=0xffffffffu;unsigned turns=0;
 while(remaining){const unsigned n=std::min<uint32_t>(sizeof(chunk),remaining);if(file.read(chunk,n)!=n)return false;value=bootimage::crc(value,chunk,n);remaining-=n;if((++turns&15u)==0)vTaskDelay(1);}
 crc=value^0xffffffffu;return crc==bootimage::u32(header+16);
}
bool busy(){return upload.load(std::memory_order_acquire)!=nullptr;}
void close(){if(image)image.close();painting=false;}
bool open(){
 if(image)return true;painting=true;
 if(busy()){painting=false;return false;}
 uint32_t crc=0;for(const char* path:{Target,Previous}){File file=SPIFFS.open(path,"r");if(validate(file,crc)){image=file;installedCrc=crc;present=true;painting=true;return true;}file.close();}
 present=false;installedCrc=0;painting=false;return false;
}
void prepare(){
 if(!SPIFFS.begin(false))return; // Never format user data or mount the SD card.
 Preferences p;if(p.begin("ibt_boot",true)){const auto value=p.getUInt("seconds",0);hold=value<=bootimage::MaxSeconds?value:0;p.end();}
 ibtfile::recover(SPIFFS,Target,Previous);open();
}
bool row(unsigned y,uint16_t* pixels){
 if(!image||!pixels||y>=bootimage::Height)return false;
 if(!image.seek(bootimage::Header+y*bootimage::Width*2)||image.read((uint8_t*)pixels,bootimage::Width*2)!=bootimage::Width*2){close();present=false;return false;}
 return true; // ESP32 and the on-disk RGB565 words are little endian.
}
unsigned seconds(){return hold.load();}
bool available(){return present.load();}
uint32_t checksum(){return installedCrc.load();}
bool saveSeconds(unsigned value){
 if(value>bootimage::MaxSeconds||busy()||painting.load())return false;
 Preferences p;if(!p.begin("ibt_boot",false))return false;
 const bool ok=p.putUInt("seconds",value)==4&&p.getUInt("seconds",~0u)==value;p.end();if(ok)hold=value;return ok;
}
void uploadAbort(){auto* state=upload.load();if(!state)return;state->file.close();SPIFFS.remove(Staged);delete state;upload.store(nullptr,std::memory_order_release);}
bool uploadBegin(size_t total){
 if(total!=bootimage::Total||busy()||painting.load()||SPIFFS.totalBytes()<SPIFFS.usedBytes()+bootimage::Total+4096)return false;
 auto* state=new(std::nothrow)Upload;if(!state)return false;
 Upload* expected=nullptr;if(!upload.compare_exchange_strong(expected,state)){delete state;return false;}
 if(painting.load()){delete state;upload.store(nullptr,std::memory_order_release);return false;}
 SPIFFS.remove(Staged);state->file=SPIFFS.open(Staged,"w");if(!state->file){uploadAbort();return false;}return true;
}
bool uploadWrite(size_t index,const uint8_t* data,size_t length){
 auto* state=upload.load();if(!state||state->failed||index!=state->written||!data||length>bootimage::Total-state->written)return false;
 size_t offset=0;if(index<bootimage::Header){const auto n=std::min<size_t>(length,bootimage::Header-index);memcpy(state->header+index,data,n);offset=n;
  if(index+n==bootimage::Header){state->headerValid=bootimage::valid(state->header,bootimage::Total);if(!state->headerValid){state->failed=true;return false;}}}
 if(offset<length)state->crc=bootimage::crc(state->crc,data+offset,length-offset);
 if(state->file.write(data,length)!=length){state->failed=true;return false;}state->written+=length;return true;
}
bool uploadFinish(uint32_t& crc){
 auto* state=upload.load();if(!state||state->failed||!state->headerValid||state->written!=bootimage::Total||(state->crc^0xffffffffu)!=bootimage::u32(state->header+16))return false;
 state->file.flush();state->file.close();File verify=SPIFFS.open(Staged,"r");bool ok=validate(verify,crc);verify.close();
 if(ok)ok=ibtfile::replaceKeepPrevious(SPIFFS,Staged,Target,Previous);
 if(ok){installedCrc=crc;present=true;}
 return ok;
}
}
#else
namespace bootcustom {void prepare(){}bool open(){return false;}void close(){}bool row(unsigned,uint16_t*){return false;}unsigned seconds(){return 0;}bool saveSeconds(unsigned){return false;}bool available(){return false;}uint32_t checksum(){return 0;}bool busy(){return false;}bool uploadBegin(size_t){return false;}bool uploadWrite(size_t,const uint8_t*,size_t){return false;}bool uploadFinish(uint32_t&){return false;}void uploadAbort(){}}
#endif
