// Included by player.cpp: startup-only extension of its existing audio owner.
static std::atomic<uint8_t> bootSoundMode{0};
static uint8_t bootSoundTheme=3;
static uint16_t bootSoundAutoplay=0;
static uint32_t bootSoundArmedAt=0;
struct BootSoundSession {
 File file;bootsound::Wav wav{};int16_t pcm[512]{};
 uint32_t total=57600,sent=0,drainAt=0;uint16_t count=0,offset=0;bool sd=false;
};
static BootSoundSession* bootSoundSession=nullptr;
bool Player::armBootSound(uint8_t mode,uint8_t theme,uint16_t autoplayStation){
 if(mode<1||mode>2||audioTaskStarted.load())return false;
 bootSoundTheme=theme;bootSoundAutoplay=autoplayStation;bootSoundArmedAt=millis();
 bootSoundMode.store(mode,std::memory_order_release);return true;
}
__attribute__((noinline)) bool Player::bootSoundTick(){
 const uint8_t mode=bootSoundMode.load(std::memory_order_acquire);if(!mode)return false;
 if(!onAudioTask())return false;
 auto finish=[&](const char* result,bool resume){
  if(bootSoundSession){const bool prepared=bootSoundSession->total!=0;
   bootSoundSession->file.close();bootSoundSession->~BootSoundSession();heap_caps_free(bootSoundSession);bootSoundSession=nullptr;
   if(prepared){setOutputPins(false);Audio::stopSong();}}
  bootSoundMode.store(0,std::memory_order_release);
  bool queued=false;if(resume&&bootSoundAutoplay&&queueDepth()==0)queued=sendCommand({PR_PLAY,bootSoundAutoplay});
  Serial.printf("[BOOT-SOUND] result=%s autoplay=%u queued=%u\n",result,unsigned(bootSoundAutoplay),unsigned(queued));bootSoundAutoplay=0;
 };
 // Any normal queued command wins. Never consume/drop it or restart after STOP.
 if(queueDepth()!=0){finish("CANCEL_COMMAND",false);return false;}
 if(!bootprogress::state().setupDone.load(std::memory_order_acquire))return true;
 const bool route=surfacepolicy::allows(surfacepolicy::current(),surfacepolicy::Route::WifiDac);
 if(!bootsound::eligible(mode,network.status==CONNECTED&&WiFi.status()==WL_CONNECTED,true,network.offlinePlayer(),!route,btaudio::wanted()||!btaudio::idle())||isRunning()||sdWorkerState.load()!=0){finish("SKIP_GATE",true);return false;}
 if(uint32_t(millis()-bootSoundArmedAt)>4500){finish("TIMEOUT",true);return false;}
 if(!bootSoundSession){
  if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<65536||
     heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<32768||
     uxTaskGetStackHighWaterMark(nullptr)<3072||
     heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<sizeof(BootSoundSession)+262144){finish("SKIP_MEMORY",true);return false;}
  void* memory=heap_caps_malloc(sizeof(BootSoundSession),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!memory){finish("SKIP_ALLOC",true);return false;}
  if(heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<262144){heap_caps_free(memory);finish("SKIP_PSRAM_RESERVE",true);return false;}
  bootSoundSession=new(memory)BootSoundSession();
  sensors::audioRequested();weather::audioRequested();
  auto& s=*bootSoundSession;
  if(mode==2&&sdman.ready){
   s.file=sdman.open("/boot.wav",FILE_READ);
   s.sd=s.file&&bootsound::wav([&](uint32_t offset,uint8_t* bytes,size_t count){return s.file.seek(offset)&&s.file.read(bytes,count)==count;},s.file?s.file.size():0,s.wav);
   if(s.sd){s.total=s.wav.bytes/(s.wav.channels*2);s.file.seek(s.wav.offset);}else s.file.close();
  }
  const uint32_t rate=s.sd?s.wav.rate:48000;
  if(!Audio::prepareServicePcm(rate)){s.total=0;finish("SKIP_DAC",true);return false;}
  setOutputPins(true);
  Serial.printf("[BOOT-SOUND] start=1 source=%s rate=%lu frames=%lu fallback=%u wifi=1 ready=1\n",s.sd?"SD:/boot.wav":"BUILTIN",(unsigned long)rate,(unsigned long)s.total,unsigned(mode==2&&!s.sd));
 }
 auto& s=*bootSoundSession;const uint32_t rate=s.sd?s.wav.rate:48000;
 if(s.sent==s.total){if(!s.drainAt)s.drainAt=millis()?millis():1;
  if(uint32_t(millis()-s.drainAt)>=150){finish("DONE",true);return false;}return true;}
 if(!s.count){
  s.offset=0;s.count=uint16_t(std::min<uint32_t>(256,s.total-s.sent));
  if(s.sd){
   const unsigned bytes=s.count*s.wav.channels*2;
   if(s.file.read(reinterpret_cast<uint8_t*>(s.pcm),bytes)!=bytes){finish("SD_SHORT_READ",true);return false;}
   for(int i=int(s.count)-1;i>=0;--i){const uint32_t frame=s.sent+unsigned(i),edge=std::min<uint32_t>(frame,s.total-1-frame),ramp=rate/200;
    const int32_t envelope=edge<ramp?int32_t(edge):int32_t(ramp);
    const int16_t left=int16_t(int32_t(s.pcm[i*s.wav.channels])*envelope/int32_t(ramp*8));
    const int16_t right=s.wav.channels==1?left:int16_t(int32_t(s.pcm[i*2+1])*envelope/int32_t(ramp*8));
    s.pcm[i*2]=left;s.pcm[i*2+1]=right;}
  }else for(unsigned i=0;i<s.count;++i)s.pcm[i*2]=s.pcm[i*2+1]=bootsound::sample(s.sent+i,bootSoundTheme);
 }
 const auto result=Audio::writeServicePcm(s.pcm+s.offset*2,s.count,rate);
 if(result.fault||result.frames>s.count){finish("DAC_WRITE",true);return false;}
 s.sent+=result.frames;s.offset+=result.frames;s.count-=result.frames;return true;
}
