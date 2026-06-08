#include "web_api.h"
#include "server.h"
#include "common.h"
#include "network.h"
#include "web_setting.h"   // extern AppController *app_controller — feeds /api/settings
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

// JSON-escape a String value for embedding inside double quotes.
// Quotes + backslashes get prefixed; control chars <0x20 are dropped
// (these never appear in our config strings — SSID / app name etc.).
String json_escape(const String &s)
{
    String out;
    out.reserve(s.length() + 4);
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s[i];
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if ((unsigned char)c >= 0x20)  out += c;
    }
    return out;
}

}  // namespace

void api_stats(void)
{
    // ESP32's internal temperature sensor returns a raw Fahrenheit-ish
    // byte; the firmware's existing displays use a similar conversion.
    //
    // WiFi state: the cube can be in STA mode (connected to a router),
    // AP mode (hosting its own hotspot that the user joined), or both.
    // The previous wifi_up = (WiFi.status() == WL_CONNECTED) only saw
    // the STA case — when the user reached the web UI via the cube's
    // own AP, the API reported wifi.connected=false and the IP pill
    // stayed at "--" even though the page was clearly being served.
    //
    // Fix: treat AP-up OR STA-up as "wifi up". Prefer the STA IP
    // (router-routable) if available, fall back to softAPIP otherwise.
    bool sta_up = (WiFi.status() == WL_CONNECTED);
    IPAddress ap_ip = WiFi.softAPIP();
    bool ap_up = (ap_ip[0] | ap_ip[1] | ap_ip[2] | ap_ip[3]) != 0;
    bool wifi_up = sta_up || ap_up;

    char ip_buf[32] = {0};
    if (sta_up) {
        IPAddress ip = WiFi.localIP();
        snprintf(ip_buf, sizeof(ip_buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    } else if (ap_up) {
        snprintf(ip_buf, sizeof(ip_buf), "%u.%u.%u.%u", ap_ip[0], ap_ip[1], ap_ip[2], ap_ip[3]);
    }

    int rssi = sta_up ? WiFi.RSSI() : 0;
    String ssid = sta_up ? WiFi.SSID() : (ap_up ? String("AP mode") : String());
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

void api_settings(void)
{
    // Read side of the B15 fix — emit the live config structs as JSON so
    // Studio's Settings tab can populate fields. Mirrors the source of truth
    // the existing /save<Cat>Conf form handlers write to (via
    // APP_MESSAGE_SET_PARAM → deal_config → write_config → SPIFFS .cfg).
    //
    // No password redaction — same level of trust as the existing
    // /sys_setting HTML page, which renders password fields in plaintext.
    // Both flows assume LAN-only access.
    const SysUtilConfig &sys = app_controller->sys_cfg;
    const RgbConfig     &rgb = app_controller->rgb_cfg;
    const SysMpuConfig  &mpu = app_controller->mpu_cfg;

    String body;
    body.reserve(1280);
    body  = F("{\"sys\":{");
    body += F("\"ssid_0\":\"");      body += json_escape(sys.ssid_0);        body += F("\",");
    body += F("\"password_0\":\"");  body += json_escape(sys.password_0);    body += F("\",");
    body += F("\"ssid_1\":\"");      body += json_escape(sys.ssid_1);        body += F("\",");
    body += F("\"password_1\":\"");  body += json_escape(sys.password_1);    body += F("\",");
    body += F("\"ssid_2\":\"");      body += json_escape(sys.ssid_2);        body += F("\",");
    body += F("\"password_2\":\"");  body += json_escape(sys.password_2);    body += F("\",");
    body += F("\"auto_start_app\":\""); body += json_escape(sys.auto_start_app); body += F("\",");
    body += F("\"power_mode\":");        body += (int)sys.power_mode;           body += F(",");
    body += F("\"backLight\":");         body += (int)sys.backLight;            body += F(",");
    body += F("\"rotation\":");          body += (int)sys.rotation;             body += F(",");
    body += F("\"auto_calibration_mpu\":"); body += (int)sys.auto_calibration_mpu; body += F(",");
    body += F("\"mpu_order\":");         body += (int)sys.mpu_order;
    body += F("},\"rgb\":{");
    body += F("\"mode\":");          body += (int)rgb.mode;            body += F(",");
    body += F("\"min_value_0\":");   body += (int)rgb.min_value_0;     body += F(",");
    body += F("\"min_value_1\":");   body += (int)rgb.min_value_1;     body += F(",");
    body += F("\"min_value_2\":");   body += (int)rgb.min_value_2;     body += F(",");
    body += F("\"max_value_0\":");   body += (int)rgb.max_value_0;     body += F(",");
    body += F("\"max_value_1\":");   body += (int)rgb.max_value_1;     body += F(",");
    body += F("\"max_value_2\":");   body += (int)rgb.max_value_2;     body += F(",");
    body += F("\"step_0\":");        body += (int)rgb.step_0;          body += F(",");
    body += F("\"step_1\":");        body += (int)rgb.step_1;          body += F(",");
    body += F("\"step_2\":");        body += (int)rgb.step_2;          body += F(",");
    body += F("\"min_brightness\":"); body += (int)rgb.min_brightness; body += F(",");
    body += F("\"max_brightness\":"); body += (int)rgb.max_brightness; body += F(",");
    body += F("\"brightness_step\":"); body += (int)rgb.brightness_step; body += F(",");
    body += F("\"time\":");          body += rgb.time;
    body += F("},\"mpu\":{");
    body += F("\"x_gyro_offset\":"); body += mpu.x_gyro_offset;   body += F(",");
    body += F("\"y_gyro_offset\":"); body += mpu.y_gyro_offset;   body += F(",");
    body += F("\"z_gyro_offset\":"); body += mpu.z_gyro_offset;   body += F(",");
    body += F("\"x_accel_offset\":"); body += mpu.x_accel_offset; body += F(",");
    body += F("\"y_accel_offset\":"); body += mpu.y_accel_offset; body += F(",");
    body += F("\"z_accel_offset\":"); body += mpu.z_accel_offset;
    body += F("}}");
    send_json(body);
}
