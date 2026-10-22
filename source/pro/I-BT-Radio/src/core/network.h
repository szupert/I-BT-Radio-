#ifndef network_h
#define network_h
#include <WiFi.h>

enum n_Status_e { CONNECTED, SOFT_AP, FAILED, SDREADY };

class MyNetwork {
  public:
    n_Status_e status;
    struct tm timeinfo;
    bool lostPlaying = false, beginReconnect = false;
  public:
    MyNetwork() {};
    void begin();
    void service();
    void startSetup();
    void enterOfflinePlayer(unsigned target=1);
    void enterOnlinePlayer();
    bool offlineReady() const;
    bool offlinePlayer() const;
    bool applySetupCredentials(const char* ssid, const char* pass);
    bool connectScannedNetwork(const char* ssid, const char* pass);
    bool setupFailed() const;
    bool setupInProgress() const;
    const char* setupSsid() const;
    bool connectingInfo(char* ssid, size_t capacity, uint8_t* attempt=nullptr) const;
    bool selectedSetupSsid(char* ssid, size_t capacity) const;
    bool requestScan();
    void cancelScan();
    uint8_t scanStatus() const; // 0=idle, 1=searching, 2=ready, 3=failed
    unsigned scanCount() const;
    bool scanEntry(unsigned index, char* ssid, size_t capacity, int* rssi=nullptr) const;
    int selectSetupNetwork(unsigned index); // 0=invalid, 1=connecting, 2=local password needed
    void requestTimeSync(bool withTelnetOutput=false, uint8_t clientId=0);
    void requestWeatherSync();
    void setWifiParams();
    void diagnosticSnapshot(uint8_t clientId=0) const;
    bool wifiBegin(bool silent=false);
  private:
    void raiseSoftAP();
    static void WiFiLostConnection(WiFiEvent_t event, WiFiEventInfo_t info);
    static void WiFiReconnected(WiFiEvent_t event, WiFiEventInfo_t info);
};

extern MyNetwork network;

extern __attribute__((weak)) void network_on_connect();

#endif
