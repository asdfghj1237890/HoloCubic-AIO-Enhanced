#include "web_api.h"
#include "server.h"
#include "common.h"
#include "network.h"
#include <WiFi.h>

// ESP32-specific helpers: not part of common.h to keep its include surface narrow.
#include <esp_system.h>

extern "C" uint8_t temprature_sens_read();  // undocumented ESP32 internal temp sensor (Fahrenheit raw)

namespace {

// Map WiFi encryption enum to a short label the Glass UI's scan-row
// renders verbatim. Pinned to the wifi_auth_mode_t names in this
// project's espressif32 ~3.5.0 SDK; WPA3 was added in newer Arduino-
// ESP32 cores so it isn't enumerated here yet — falls through to "?".
const char *encryption_label(int enc)
{
    switch (enc) {
        case WIFI_AUTH_OPEN:           return "open";
        case WIFI_AUTH_WEP:            return "WEP";
        case WIFI_AUTH_WPA_PSK:        return "WPA";
        case WIFI_AUTH_WPA2_PSK:       return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:   return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE:return "WPA2-EAP";
        default:                       return "?";
    }
}

void send_json(const String &body)
{
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", body);
}

}  // namespace

void api_stats(void)
{
    // ESP32's internal temperature sensor returns a raw Fahrenheit-ish
    // byte; the firmware's existing displays use a similar conversion.
    // Skipped on disconnected WiFi (RSSI / IP / SSID would be "(null)").
    bool wifi_up = (WiFi.status() == WL_CONNECTED);

    char ip_buf[32] = {0};
    if (wifi_up) {
        IPAddress ip = WiFi.localIP();
        snprintf(ip_buf, sizeof(ip_buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    }

    int rssi = wifi_up ? WiFi.RSSI() : 0;
    String ssid = wifi_up ? WiFi.SSID() : String();
    String mac = WiFi.macAddress();

    uint32_t free_heap  = ESP.getFreeHeap();
    uint32_t total_heap = ESP.getHeapSize();
    uint32_t flash_used = ESP.getSketchSize();
    uint32_t flash_total = ESP.getFlashChipSize();
    uint32_t uptime_ms  = millis();

    // Internal Fahrenheit sensor -> Celsius. Reading is noisy on Rev1
    // ESP32 silicon; treat as informational only.
    float temp_c = (temprature_sens_read() - 32) / 1.8f;

    char buf[640];
    snprintf(buf, sizeof(buf),
        "{"
            "\"uptime_ms\":%lu,"
            "\"free_heap\":%lu,"
            "\"total_heap\":%lu,"
            "\"flash_used\":%lu,"
            "\"flash_total\":%lu,"
            "\"temp_c\":%.1f,"
            "\"wifi\":{"
                "\"connected\":%s,"
                "\"ssid\":\"%s\","
                "\"rssi\":%d,"
                "\"ip\":\"%s\""
            "},"
            "\"mac\":\"%s\","
            "\"version\":\"%s\","
            "\"chip\":\"ESP32\""
        "}",
        (unsigned long)uptime_ms,
        (unsigned long)free_heap,
        (unsigned long)total_heap,
        (unsigned long)flash_used,
        (unsigned long)flash_total,
        temp_c,
        wifi_up ? "true" : "false",
        ssid.c_str(),
        rssi,
        ip_buf,
        mac.c_str(),
        AIO_VERSION);

    send_json(String(buf));
}

void api_wifi_scan(void)
{
    // Scan blocks for ~2-3 seconds; that's the browser's request budget.
    // The Glass UI shows a spinner while waiting and only fires this on
    // the explicit Rescan button so it's not a per-page-load cost.
    int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false);
    String current_ssid = WiFi.SSID();

    String body = "{\"networks\":[";
    for (int i = 0; i < n; ++i) {
        if (i > 0) body += ',';
        String ssid = WiFi.SSID(i);
        // Escape backslashes + double quotes in the SSID — most are
        // ASCII but the JSON string has to stay well-formed if a router
        // operator put a quote in their network name.
        String esc;
        for (size_t k = 0; k < ssid.length(); ++k) {
            char c = ssid[k];
            if (c == '"' || c == '\\') esc += '\\';
            esc += c;
        }
        char row[160];
        snprintf(row, sizeof(row),
            "{\"ssid\":\"%s\",\"rssi\":%d,\"sec\":\"%s\",\"current\":%s}",
            esc.c_str(),
            WiFi.RSSI(i),
            encryption_label(WiFi.encryptionType(i)),
            (current_ssid == ssid) ? "true" : "false");
        body += row;
    }
    body += "]}";
    WiFi.scanDelete();
    send_json(body);
}
