#ifndef AIO_STUB_WIFI_H
#define AIO_STUB_WIFI_H
#include "Arduino.h"

#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
#define WL_IDLE_STATUS 0
#define WL_NO_SSID_AVAIL 1

#define WIFI_AP    1
#define WIFI_STA   2
#define WIFI_AP_STA 3
#define WIFI_OFF   0

class IPAddress {
public:
    uint8_t a[4] = {0, 0, 0, 0};
    IPAddress() {}
    IPAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) { a[0]=b0; a[1]=b1; a[2]=b2; a[3]=b3; }
    String toString() const { char b[32]; snprintf(b,32,"%u.%u.%u.%u",a[0],a[1],a[2],a[3]); return String(b); }
    operator uint32_t() const { return (a[0]<<24)|(a[1]<<16)|(a[2]<<8)|a[3]; }
    bool fromString(const char *s) {
        if (!s) return false;
        unsigned int v0=0,v1=0,v2=0,v3=0;
        if (sscanf(s, "%u.%u.%u.%u", &v0, &v1, &v2, &v3) != 4) return false;
        a[0]=(uint8_t)v0; a[1]=(uint8_t)v1; a[2]=(uint8_t)v2; a[3]=(uint8_t)v3;
        return true;
    }
    bool fromString(const String &s) { return fromString(s.c_str()); }
    uint8_t &operator[](int i) { return a[i & 3]; }
    uint8_t operator[](int i) const { return a[i & 3]; }
};

class WiFiClass {
public:
    int status() { return WL_DISCONNECTED; }
    int begin(const char *, const char * = nullptr) { return WL_DISCONNECTED; }
    void disconnect(bool = false) {}
    bool softAP(const char *, const char * = nullptr) { return true; }
    bool softAPdisconnect(bool = false) { return true; }
    void mode(int) {}
    IPAddress localIP() { return IPAddress(192,168,4,1); }
    IPAddress softAPIP() { return IPAddress(192,168,4,1); }
    String macAddress() { return String("AA:BB:CC:DD:EE:FF"); }
    String SSID() { return String(""); }
    int RSSI() { return -50; }
    int scanNetworks(bool = false, bool = false) { return 0; }
    String SSID(int) { return String(""); }
    int RSSI(int) { return -100; }
    void setSleep(bool) {}
    void persistent(bool) {}
    void setAutoReconnect(bool) {}
    bool isConnected() { return false; }
};

extern WiFiClass WiFi;

#endif
