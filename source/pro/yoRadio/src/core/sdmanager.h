#ifndef sdmanager_h
#define sdmanager_h
#include <atomic>
#include <freertos/semphr.h>
#include "sd_path.h"
struct AlarmFolderScan {
    File playlist;
    uint16_t* tracks=nullptr;
    uint16_t count=0,cursor=0;
    uint16_t pendingNext=UINT16_MAX; // retain the chosen EOF successor across queue-full retries
    uint32_t total=0,row=0,offset=0,started=0;
    size_t used=0;
    std::atomic<uint8_t> status{0}; // 0 reading, 1 ready, 2 failed
    char line[2*ibtsd::PathCapacity+1]{};
};

class SDManager : public SDFS {
  public:
    bool ready;
    std::atomic<bool> catalogReady{false};
    std::atomic<uint32_t> indexedTracks{0};
  public:
    SDManager(FSImplPtr impl) : SDFS(impl) {}
    bool start();
    bool selectPath(const char* path,size_t length);
    const char* selectedPath() const;
    void stop();
    bool cardPresent();
    void listSD(File &plSDfile, File &plSDindex, const char * dirname, uint8_t levels);
    bool indexSDPlaylist();
    bool beginAlarmFolderScan();
    void stepAlarmFolderScan(AlarmFolderScan* scan); // at most 256 input bytes
    void freeAlarmFolderScan();
    AlarmFolderScan* alarmScan() const {return activeAlarmScan;}
    void cancelIndex(){indexCancel.store(true);}
    void clearIndexCancel(){indexCancel.store(false);}
    bool indexWasCancelled() const {return indexCancel.load();}
  private:
    uint32_t _sdFCount;
    std::atomic<bool> indexCancel{false};
    uint32_t indexStarted=0;
    SemaphoreHandle_t scanIoLock=nullptr;
    AlarmFolderScan* activeAlarmScan=nullptr;
    bool indexExpired() const;
  private:
    bool _checkNoMedia(const char* path);
    bool _endsWith (const char* base, const char* str);
};

extern SDManager sdman;
#if defined(SD_SPIPINS) || SD_HSPI
extern SPIClass  SDSPI;
#endif
#endif
