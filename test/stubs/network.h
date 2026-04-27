// Guard matches firmware src/network.h. See test/stubs/common.h header.
#ifndef NETWORK_H
#define NETWORK_H
#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#define TIMEZERO_OFFSIZE (28800000)
#define CONN_SUCC 0
#define CONN_ERROR 1
#define CONN_TIMEOUT 2
#define CONN_ERR_TIMEOUT 15
#define AP_DISABLE 0
#define AP_ENABLE 1
#define SERVER_NAME "fileserver"

extern IPAddress local_ip;
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress dns;
extern const char *AP_SSID;

inline void restCallback(TimerHandle_t) {}

class Network {
public:
    Network() {}
    void search_wifi() {}
    boolean start_conn_wifi(const char *, const char *) { return false; }
    boolean end_conn_wifi() { return CONN_TIMEOUT; }
    boolean close_wifi() { return true; }
    boolean open_ap(const char * = AP_SSID, const char * = nullptr) { return false; }
    unsigned long get_conn_duration() { return 0; }
};

#endif
