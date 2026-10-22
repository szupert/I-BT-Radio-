// Host-controlled diagnostic session. No NVS rate writes; loss of heartbeat returns to DAC.
static bool pathActive=false,pathRestoring=false,pathOffQueued=false;
static uint32_t pathHeartbeat=0,pathRestoreAt=0,pathRetryAt=0,pathSequence=0;
static unsigned pathOriginalRate=32000;
static bool pathLocalAudio(){
#ifdef USE_SD
  return config.getMode()==PM_SDCARD && sdman.ready && sdman.catalogReady.load() && player.localFileSource() && player.getCodec()>=1 && player.getCodec()<=15;
#else
  return false;
#endif
}
static void pathState(){
  dual::surfaceDiagnostics();
  ::ibtlimit::cadenceDiagnostics();
  Serial.printf("[NET-POLL] requested_ms=%lu owner_applied_ms=%lu polls=%lu skips=%lu floor_bytes=65536 scope=DIAGNOSTIC_TLS_MP3_BT default_ms=0\n",(unsigned long)::netpoll::requested().load(),(unsigned long)::netpoll::applied().load(),(unsigned long)::netpoll::polls().load(),(unsigned long)::netpoll::skips().load());
  const auto appliedYield=::owneryield::applied().load();
  Serial.printf("[OWNER-YIELD] requested_ms=%lu effective_ms=%lu owner_applied_ms=%lu scope=WEB_MP3_BT32_BUFFERED_AUTO auto_mode=1 fallback_ms=2 floor_bytes=65536\n",(unsigned long)::owneryield::requested().load(),(unsigned long)appliedYield,(unsigned long)appliedYield);
  Serial.printf("[BT-RATE] web_fixed32=%u selected_hz=%u negotiated_hz=%u\n",unsigned(config.getMode()==PM_WEB),btaudio::outputRate(),btaudio::negotiatedOutputRate());
  Serial.printf("[PATH-STATE] active=%u restoring=%u profile=%u web=%u playing=%u station=%u volume=%u route=%u idle=%u rate=%u saved=%u scanning=%u connected=%u transmitting=%u input_buffer=%u source_rate=%u vu_ms=%u player=%u sbc_busy=%u decoded=%u play_seconds=%lu generation=%lu bt_failed=%u bt_pending=%u\n",
    unsigned(pathActive),unsigned(pathRestoring),unsigned(pathprobe::enabled.load()),unsigned(config.getMode()==PM_WEB),unsigned(player.isRunning()),unsigned(config.lastStation()),unsigned(config.store.volume),unsigned(btaudio::mode()),unsigned(btaudio::idle()),btaudio::outputRate(),btaudio::savedDeviceCount(),unsigned(btaudio::scanning()),unsigned(btaudio::connected()),unsigned(btaudio::transmitting()),unsigned(player.inBufferFilled()),unsigned(player.getSampleRate()),unsigned(vuprobe::refreshMs.load(std::memory_order_relaxed)),unsigned(dual::snapshot().settings.playerEnabled),unsigned(sbcdiag::active()),unsigned(player.hasDecodedAudio()),(unsigned long)player.getAudioCurrentTime(),(unsigned long)player.playbackGeneration(),unsigned(btaudio::linkFailed()),unsigned(btaudio::linkRequestPending()));
  Serial.printf("[PATH-SOURCE] local=%u codec=%u generation=%lu\n",unsigned(player.localFileSource()),unsigned(player.getCodec()),(unsigned long)player.playbackGeneration());
}
static void pathEnd(){
  directrx::setSupplyMeasurement(false);
  btaudio::setPcmStaging(false);
  ::owneryield::reset();::netpoll::reset();::ibtlimit::resetCadence();
  mp3offload::set(false);sbcdiag::cancel();artab::stop();front::artworkTestFast(false);
  pathprobe::enabled.store(false);stagecpu::set(false);
  if(!pathActive&&!pathRestoring){Serial.println("[PATH-RESTORE] result=NOT_ACTIVE");return;}
  pathActive=false;pathRestoring=true;pathRestoreAt=millis();pathRetryAt=pathRestoreAt;
  pathOffQueued=player.sendCommand({PR_BT,-1});Serial.println("[PATH-RESTORE] result=QUEUED");
}
static void pathTick(){
  const uint32_t now=millis();
  if(pathActive&&uint32_t(now-pathHeartbeat)>30000){Serial.println("[PATH-LEASE] expired=1");pathEnd();}
  if(!pathRestoring)return;
  if(btaudio::idle()&&!btaudio::wanted()){
    const bool ok=btaudio::setDiagnosticRate(pathOriginalRate);pathRestoring=false;mp3offload::diagnosticMode(false);
    Serial.printf("[PATH-RESTORE] result=%s rate=%u route=DAC\n",ok?"PASS":"FAIL",btaudio::outputRate());return;
  }
  if(uint32_t(now-pathRestoreAt)>45000){pathRestoring=false;Serial.println("[PATH-RESTORE] result=FAIL timeout=1");return;}
  if(!pathOffQueued&&uint32_t(now-pathRetryAt)>2000){pathOffQueued=player.sendCommand({PR_BT,-1});pathRetryAt=now;}
}
static void pathDevices(){
  btaudio::Device d{};
  for(unsigned i=0;i<btaudio::savedDeviceCount();++i)if(btaudio::savedDevice(i,d))Serial.printf("[PATH-DEVICE] kind=saved index=%u name=%s\n",i,d.name);
  for(unsigned i=0;i<btaudio::deviceCount();++i)if(btaudio::device(i,d))Serial.printf("[PATH-DEVICE] kind=scan index=%u name=%s\n",i,d.name);
  if(btaudio::connectedDevice(d))Serial.printf("[PATH-PEER] name=%s\n",d.name);
  Serial.println("[PATH-DEVICES-END]");
}
static bool pathHandle(const char* s){
  if(strncmp(s,"r10 path ",9))return false;
  const char* cmd=s+9;
  if(!strcmp(cmd,"begin")){
    const bool ok=!pathActive&&!pathRestoring&&!engaged&&!saved&&(config.getMode()==PM_WEB||pathLocalAudio())&&player.isRunning()&&!btaudio::inputSelected();
    if(ok){directrx::setSupplyMeasurement(false);btaudio::setPcmStaging(false);mp3offload::diagnosticMode(true);pathOriginalRate=btaudio::outputRate();pathActive=true;pathHeartbeat=millis();pathprobe::enabled.store(false);}
    Serial.printf("[PATH-BEGIN] result=%s active=%u restoring=%u engaged=%u saved=%u web=%u local=%u playing=%u bt_input=%u\n",ok?"PASS":"DENIED",unsigned(pathActive),unsigned(pathRestoring),unsigned(engaged),unsigned(saved),unsigned(config.getMode()==PM_WEB),unsigned(pathLocalAudio()),unsigned(player.isRunning()),unsigned(btaudio::inputSelected()));pathState();return true;
  }
  if(!strcmp(cmd,"end")){pathEnd();return true;}
  if(pathActive)pathHeartbeat=millis();
  if(!strcmp(cmd,"status")){pathState();return true;}
  if(!strcmp(cmd,"huffmem")){MP3Decoder_MemoryDiagnostics();return true;}
  if(!strcmp(cmd,"devices")){pathDevices();return true;}
  if(!pathActive){Serial.println("[PATH-ACK] result=DENIED inactive=1");return true;}
  bool ok=true;
  if(!strcmp(cmd,"sbcstop"))sbcdiag::cancel();
  else if(!strcmp(cmd,"sbc0"))ok=sbcdiag::startPhase(0);
  else if(!strcmp(cmd,"sbc1"))ok=sbcdiag::startPhase(1);
  else if(!strcmp(cmd,"sbc2"))ok=sbcdiag::startPhase(2);
  else if(!strcmp(cmd,"artprepare"))ok=artab::prepare();
  else if(!strcmp(cmd,"arton"))ok=artab::set(true);
  else if(!strcmp(cmd,"artoff"))ok=artab::set(false);
  else if(!strcmp(cmd,"cpuon"))ok=stagecpu::set(true);
  else if(!strcmp(cmd,"cpuoff"))ok=stagecpu::set(false);
  else if(!strcmp(cmd,"supplyon"))ok=directrx::setSupplyMeasurement(true);
  else if(!strcmp(cmd,"supplyoff"))ok=directrx::setSupplyMeasurement(false);
  else if(!strcmp(cmd,"artslow"))front::artworkTestFast(false);
  else if(!strcmp(cmd,"artfast"))front::artworkTestFast(true);
  else if(!strncmp(cmd,"artrows=",8)) {char* end=nullptr;const long n=strtol(cmd+8,&end,10);ok=end!=cmd+8&&*end=='\0'&&artab::setRows(unsigned(n));}
  else if(!strcmp(cmd,"limit0"))ok=::ibtlimit::setCadence(0);
  else if(!strcmp(cmd,"limit100"))ok=::ibtlimit::setCadence(100);
  else if(!strcmp(cmd,"limitauto")){::ibtlimit::resetCadence();}
  else if(!strcmp(cmd,"poll0"))ok=::netpoll::set(0);
  else if(!strcmp(cmd,"poll50"))ok=::netpoll::set(50);
  else if(!strcmp(cmd,"poll100"))ok=::netpoll::set(100);
  else if(!strcmp(cmd,"yield2"))ok=::owneryield::set(2);
  else if(!strcmp(cmd,"yield50"))ok=::owneryield::set(50);
  else if(!strcmp(cmd,"yieldauto"))ok=::owneryield::set(::owneryield::Auto);
  else if(!strcmp(cmd,"pcmstageon"))ok=btaudio::setPcmStaging(true);
  else if(!strcmp(cmd,"pcmstageoff"))ok=btaudio::setPcmStaging(false);
  else if(!strcmp(cmd,"mp3prio3"))ok=mp3offload::setWorkerPriority(3);
  else if(!strcmp(cmd,"mp3prio4"))ok=mp3offload::setWorkerPriority(4);
  else if(!strcmp(cmd,"imdcton"))ok=mp3offload::setImdct(true);
  else if(!strcmp(cmd,"imdctoff"))ok=mp3offload::setImdct(false);
  else if(!strcmp(cmd,"dqprepare"))ok=mp3offload::prepare();
  else if(!strcmp(cmd,"dqon"))ok=mp3offload::set(true);
  else if(!strcmp(cmd,"dqoff"))mp3offload::set(false);
  else if(!strcmp(cmd,"dq1"))ok=mp3offload::setFraction(1);
  else if(!strcmp(cmd,"dq2"))ok=mp3offload::setFraction(2);
  else if(!strcmp(cmd,"dq4"))ok=mp3offload::setFraction(4);
  else if(!strcmp(cmd,"dq8"))ok=mp3offload::setFraction(8);
  else if(!strcmp(cmd,"on"))pathprobe::enabled.store(true);
  else if(!strcmp(cmd,"off"))pathprobe::enabled.store(false);
  else if(!strcmp(cmd,"dac"))ok=player.sendCommand({PR_BT,-1});
  else if(!strcmp(cmd,"sourceweb"))ok=!btaudio::inputSelected()&&player.sendCommand({PR_SOURCE,PM_WEB});
  else if(!strcmp(cmd,"sourcesd"))ok=!btaudio::inputSelected()&&player.sendCommand({PR_SOURCE,PM_SDCARD});
  else if(!strncmp(cmd,"sdtrack=",8)){
    ok=false;
#ifdef USE_SD
    char* end=nullptr;const long track=strtol(cmd+8,&end,10);
    ok=end!=cmd+8&&*end=='\0'&&track>0&&track<=config.playlistLength()&&
       config.getMode()==PM_SDCARD&&sdman.ready&&sdman.catalogReady.load()&&
       !btaudio::wanted()&&btaudio::idle();
    // Diagnostic selection must not overwrite the user's saved SD track.
    if(ok)ok=player.sendCommand({PR_PLAY,-int(track)});
#endif
  }
  else if(!strcmp(cmd,"restart"))ok=pathLocalAudio()&&player.isRunning()&&!btaudio::inputSelected()&&(!btaudio::outputSelected()||btaudio::transmitting())&&player.sendCommand({PR_RESTART_LOCAL,0});
  else if(!strcmp(cmd,"rate32"))ok=btaudio::setDiagnosticRate(32000);
  else if(!strcmp(cmd,"rate44"))ok=btaudio::setDiagnosticRate(44100);
  else if(!strcmp(cmd,"rate48"))ok=btaudio::setDiagnosticRate(48000);
  else if(!strcmp(cmd,"scan"))ok=config.getMode()==PM_SDCARD&&player.sendCommand({PR_BT,0});
  else if(!strncmp(cmd,"saved=",6)){
    const int index=atoi(cmd+6);ok=config.getMode()==PM_SDCARD&&index>=0&&unsigned(index)<btaudio::savedDeviceCount();
    if(ok)ok=player.sendCommand({PR_BT,0})&&btaudio::connectSavedDevice(unsigned(index));
  }
  else if(!strncmp(cmd,"select=",7)){
    const int index=atoi(cmd+7);ok=index>=0&&btaudio::selectDevice(unsigned(index));
  }
  else if(!strcmp(cmd,"sample")){
    Serial.printf("[PATH-BEGIN-SAMPLE] seq=%lu\n",(unsigned long)++pathSequence);
    pathState();pathDevices();artab::diagnostics();mp3offload::diagnostics();loadprobe::sample();pathprobe::diagnostics();stagecpu::diagnostics();btaudio::diagnostics();telemetry("path");
    Serial.printf("[PATH-END-SAMPLE] seq=%lu\n",(unsigned long)pathSequence);return true;
  }
  else ok=false;
  Serial.printf("[PATH-ACK] command=%s result=%s\n",cmd,ok?"PASS":"DENIED");return true;
}
