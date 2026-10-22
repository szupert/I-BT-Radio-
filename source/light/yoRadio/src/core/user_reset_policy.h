#pragma once
// Deliberately allowlisted: never erase the NVS partition, Wi-Fi files,
// Classic BT bonds/last peer, SD media, firmware, or hardware calibration.
namespace ibtreset {
template<class Storage> bool apply(Storage& s) {
  static const char* const spaces[]={"yo-dual-v1","ibt_weather","ibt-mono-v1"};
  static const char* const files[]={"/data/playlist.csv","/data/playlist.prev",
    "/data/playlist.new","/data/playlist.restore","/data/index.dat",
    "/data/index.new","/data/index.prev","/data/plsd.csv","/data/indexsd.dat",
    "/data/tmpfile.txt"};
  for(const char* name:spaces)if(!s.clearNamespace(name))return false;
  if(!s.restoreConnectionOptions())return false;
  for(const char* path:files)if(!s.removeFile(path))return false;
  return s.resetSettings();
}
}
