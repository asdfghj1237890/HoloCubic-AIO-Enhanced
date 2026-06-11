#ifndef AIO_NETWORK_H
#define AIO_NETWORK_H

// 时区偏移(小时) 8*60*60
#define TIMEZERO_OFFSIZE (28800000)

#define CONN_SUCC 0
#define CONN_ERROR 1
#define CONN_TIMEOUT 2
#define CONN_ERR_TIMEOUT 15 // WiFi connection timeout (seconds)

// wifi是否连接标志
#define AP_DISABLE 0
#define AP_ENABLE 1

// Set your server's logical name here e.g. if 'myserver' then address is http://myserver.local/
#define SERVER_NAME "fileserver"

#ifdef ESP8266
#include <ESP8266WiFi.h>      // Built-in
#include <ESP8266WiFiMulti.h> // Built-in
#include <ESP8266WebServer.h> // Built-in
#include <ESP8266mDNS.h>
#else
#include <WiFi.h>      // Built-in
#include <WiFiMulti.h> // 当我们需要使用ESP8266开发板存储多个WiFi网络连接信息时，可以使用ESP8266WiFiMulti库来实现。
#include <WebServer.h> // https://github.com/Pedroalbuquerque/ESP32WebServer download and place in your Libraries folder
#include <ESPmDNS.h>
#include <HTTPClient.h>
#ifndef ESP8266
#include "freertos/timers.h"
#endif
#endif

// #ifdef __cplusplus
// extern "C"
// {
// #endif

// #include "WiFiGeneric.h"

// #ifdef __cplusplus
// } /* extern "C" */
// #endif

extern IPAddress local_ip; // Set your server's fixed IP address here
extern IPAddress gateway;  // Set your network Gateway usually your Router base address
extern IPAddress subnet;   // Set your network sub-network mask here
extern IPAddress dns;      // Set your network DNS usually your Router base address

extern const char *AP_SSID; //热点名称

#ifndef ESP8266
void restCallback(TimerHandle_t xTimer);
#endif

class AioNetwork
{
private:
    unsigned long m_preDisWifiConnInfoMillis; // Timestamp of last connection status display
    unsigned long m_wifiConnStartMillis;       // Timestamp when WiFi connection started
    bool m_isConnecting;                       // Flag to track if currently attempting connection

public:
    AioNetwork();
    void search_wifi(void);
    boolean start_conn_wifi(const char *ssid, const char *password);
    boolean end_conn_wifi(void);
    boolean close_wifi(void);
    boolean open_ap(const char *ap_ssid = AP_SSID, const char *ap_password = NULL);
    unsigned long get_conn_duration(void);    // Get connection attempt duration in milliseconds
};

#endif
