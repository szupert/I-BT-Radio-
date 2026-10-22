#ifndef AUDIOHANDLERS_H
#define AUDIOHANDLERS_H
#include "../dual/deferred_audio.h"

//=============================================//
//              Audio handlers                 //
//=============================================//

void audio_info(const char *info) {
  if(info&&(strstr(info,"Account already in use")||strstr(info,"HTTP/1.0 401")||strstr(info,"HTTP/1.1 401"))){player.setError(info);return;}
  if(audiod::defer(audiod::Kind::Info,info))return;
  if(player.lockOutput) return;
  if(config.store.audioinfo) telnet.printf("##AUDIO.INFO#: %s\n", info);
  #ifdef USE_NEXTION
    nextion.audioinfo(info);
  #endif
  if (strstr(info, "format is aac")  != NULL) { config.setBitrateFormat(BF_AAC); display.putRequest(DBITRATE); }
  if (strstr(info, "format is flac") != NULL) { config.setBitrateFormat(BF_FLAC); display.putRequest(DBITRATE); }
  if (strstr(info, "format is mp3")  != NULL) { config.setBitrateFormat(BF_MP3); display.putRequest(DBITRATE); }
  if (strstr(info, "format is wav")  != NULL) { config.setBitrateFormat(BF_WAV); display.putRequest(DBITRATE); }
  if (strstr(info, "skip metadata") != NULL) config.setTitle(config.station.name);
  if (strstr(info, "Account already in use") != NULL || strstr(info, "HTTP/1.0 401") != NULL) {
    player.setError(info);
    
  }
  char* ici; char b[20]={0};
  if ((ici = strstr(info, "BitRate: ")) != NULL) {
    strlcpy(b, ici + 9, sizeof(b));
    audio_bitrate(b);
  }
}

void audio_bitrate(const char *info)
{
  if(audiod::defer(audiod::Kind::Bitrate,info))return;
  if(config.store.audioinfo) telnet.printf("%s %s\n", "##AUDIO.BITRATE#:", info);
  config.station.bitrate = atoi(info) / 1000;
  display.putRequest(DBITRATE);
  #ifdef USE_NEXTION
    nextion.bitrate(config.station.bitrate);
  #endif
  netserver.requestOnChange(BITRATE, 0);
}

bool printable(const char *info) {
  if(L10N_LANGUAGE!=RU) return true;
  bool p = true;
  for (int c = 0; c < strlen(info); c++)
  {
    if ((uint8_t)info[c] > 0x7e || (uint8_t)info[c] < 0x20) p = false;
  }
  if (!p) p = (uint8_t)info[0] >= 0xC2 && (uint8_t)info[1] >= 0x80 && (uint8_t)info[1] <= 0xBF;
  return p;
}

void audio_showstation(const char *info) {
  if(audiod::defer(audiod::Kind::Station,info))return;
  bool p = printable(info) && (strlen(info) > 0);(void)p;
  if(player.remoteStationName){
    config.setStation(p?info:config.station.name);
    display.putRequest(NEWSTATION);
    netserver.requestOnChange(STATION, 0);
  }
}

void audio_showstreamtitle(const char *info) {
  if(info&&(strstr(info,"Account already in use")||strstr(info,"HTTP/1.0 401")||strstr(info,"HTTP/1.1 401"))){player.setError(info);return;}
  if(audiod::defer(audiod::Kind::Title,info))return;
  if (info && (strstr(info, "Account already in use") != NULL || strstr(info, "HTTP/1.0 401") != NULL || strstr(info, "HTTP/1.1 401") != NULL)) player.setError(info);
  bool p = info && printable(info) && (strlen(info) > 0);
  if(config.getMode()==PM_WEB) dual::metadataTitle(p?info:"");
  #ifdef DEBUG_TITLES
    config.setTitle(DEBUG_TITLES);
  #else
    config.setTitle(p?info:"");
  #endif
}

void audio_error(const char *info) {
  player.setError(info);
}

void audio_id3artist(const char *info){
  if(audiod::defer(audiod::Kind::Artist,info))return;
  dual::metadataArtist(info);
  if(printable(info)) config.setStation(info);
  display.putRequest(NEWSTATION);
  netserver.requestOnChange(STATION, 0);
}

void audio_id3album(const char *info){
  if(audiod::defer(audiod::Kind::Album,info))return;
  dual::metadataAlbum(info);
  if(player.lockOutput) return;
  if(printable(info)){
    if(strlen(config.station.title)==0){
      config.setTitle(info);
    }else{
      char tmp[BUFLEN+3];
      snprintf(tmp, BUFLEN+3, "%s - %s", config.station.title, info);
      config.setTitle(tmp);
    }
  }
}

void audio_id3title(const char *info){
  if(audiod::defer(audiod::Kind::Id3Title,info))return;
  dual::metadataTitle(info);
  if(printable(info)) config.setTitle(info);
}

void audio_id3image(File&, const size_t pos, const size_t size){
  dual::imageHint(pos,size); // No reads, decoding, allocations, or display calls here.
}

void audio_beginSDread(){
  config.setTitle("");
}

void audio_id3data(const char *info){
  if(audiod::defer(audiod::Kind::Id3Data,info))return;  //id3 metadata
    if(player.lockOutput) return;
    telnet.printf("##AUDIO.ID3#: %s\n", info);
}

void audio_eof_mp3(const char *info){  //end of file
    btaudio::markSourceBoundary();
    config.sdResumePos = 0;
    #if I2S_DOUT!=255 || I2S_INTERNAL
    if(dual::alarmSdEof(!player.hasDecodedAudio()))return;
    #else
    if(dual::alarmSdEof())return;
    #endif
    player.next(true);
}

void audio_eof_stream(const char *info){
  player.sendCommand({PR_STOP, 0});
  if(!player.resumeAfterUrl) return;
  if (config.getMode()==PM_WEB){
    player.sendCommand({PR_PLAY, config.lastStation()});
  }else{
    player.setResumeFilePos( config.sdResumePos==0?0:config.sdResumePos-player.sd_min);
    player.sendCommand({PR_PLAY, config.lastStation()});
  }
}

void audio_progress(uint32_t startpos, uint32_t endpos){
  player.sd_min = startpos;
  player.sd_max = endpos;
  netserver.requestOnChange(SDLEN, 0);
}

#endif
