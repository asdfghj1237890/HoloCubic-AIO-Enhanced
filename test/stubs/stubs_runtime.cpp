// Runtime side of the stubs: instances of singletons + simple host-backed
// implementations of FlashFS, AppController, doDelayMillisTime, analyseParam,
// and the various globals declared in firmware headers.

#include "Arduino.h"
#include "WiFi.h"
#include "Wire.h"
#include "SPI.h"
#include "FastLED.h"
#include "FS.h"
#include "SPIFFS.h"
#include "SD.h"
#include "common.h"
#include "sys/app_controller.h"
#include "sys/interface.h"
#include "driver/imu.h"
#include "driver/flash_fs.h"
#include "driver/sd_card.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "ESPmDNS.h"
#include "TJpg_Decoder.h"
#include "HTTPClient.h"

// ---------- Hardware singleton stubs ----------
HardwareSerial Serial;
WiFiClass WiFi;
MDNSResponder MDNS;
TJpg_Decoder TJpgDec;
TwoWire Wire;
SPIClass SPI;
SPIFFSClass SPIFFS;
SDClass SD;
FastLEDClass FastLED;
EspClass ESP;

// app_controller is referenced by heartbeat (and web_setting). Kept as a
// raw pointer that main.cpp wires to the harness's controller after
// AppController construction.
AppController *app_controller = nullptr;

// LHLXW sub-apps reference these globals defined in firmware
// HoloCubic_AIO.cpp (the main sketch we don't link in on host).
bool isCheckAction = false;
ImuAction *act_info = nullptr;

// LHLXW.cpp calls emoji_process; we excluded the emoji sub-app because it
// pulls in MjpegPlayDecoder from the dropped media_player tree. Provide a
// no-op so the hub menu can still link.
struct _lv_obj_t;
void emoji_process(_lv_obj_t *) {}

// ---------- LVGL / display globals ----------
TFT_eSPI g_stub_tft;
TFT_eSPI *tft = &g_stub_tft;

SemaphoreHandle_t lvgl_mutex = (SemaphoreHandle_t)1;

// ---------- Firmware globals declared in common.h ----------
IMU mpu;
SdCard tf;
Pixel rgb;
Network g_network;
FlashFS g_flashCfg;
Display screen;
Ambient ambLight;

// ---------- IMU / encoder input state (used by lv_port_indev) ----------
int32_t encoder_diff = 0;
lv_indev_state_t encoder_state = LV_INDEV_STATE_RELEASED;
const char *active_type_info[] = {
    "TURN_RIGHT", "RETURN", "TURN_LEFT", "UP", "DOWN", "GO_FORWORD", "SHAKE", "UNKNOWN"
};

// ---------- Network globals ----------
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);
const char *AP_SSID = "HoloCubic-AIO-Test";

// ---------- SD card name list (used by picture/file_manager) ----------
int photo_file_num = 0;
char file_name_list[DIR_FILE_NUM][DIR_FILE_NAME_MAX_LEN];

// ---------- Display stub impl ----------
void Display::init(uint8_t, uint8_t) {}
void Display::routine() { lv_timer_handler(); }
void Display::setBackLight(float) {}

// ---------- Simple persistence: route FlashFS to test/fixtures/flash/ ----------
static const char *FLASH_FIXTURE_DIR = "test/fixtures/flash";

static String flash_path(const char *path) {
    String p(FLASH_FIXTURE_DIR);
    if (path && path[0] != '/') p += "/";
    p += (path ? path : "");
    return p;
}

uint16_t FlashFS::readFile(const char *path, uint8_t *info) {
    String full = flash_path(path);
    FILE *f = fopen(full.c_str(), "rb");
    if (!f) return 0;
    size_t n = fread(info, 1, 1024, f);
    fclose(f);
    return (uint16_t)n;
}

void FlashFS::writeFile(const char *path, const char *message) {
    String full = flash_path(path);
    // ensure directory exists
    #ifdef _WIN32
    _mkdir(FLASH_FIXTURE_DIR);
    #else
    mkdir(FLASH_FIXTURE_DIR, 0755);
    #endif
    FILE *f = fopen(full.c_str(), "wb");
    if (!f) return;
    fwrite(message, 1, strlen(message), f);
    fclose(f);
}

void FlashFS::appendFile(const char *path, const char *message) {
    String full = flash_path(path);
    FILE *f = fopen(full.c_str(), "ab");
    if (!f) return;
    fwrite(message, 1, strlen(message), f);
    fclose(f);
}

void FlashFS::deleteFile(const char *path) {
    String full = flash_path(path);
    remove(full.c_str());
}

// ---------- HTTPClient stub: route GET() to test/fixtures/http/ ----------
//
// Maps URL host + path to a fixture file mirroring the URL structure:
//   https://api.bilibili.com/x/relation/stat?vmid=...
//     -> ../test/fixtures/http/api.bilibili.com/x/relation/stat.json
//
// (`../` because the harness binary runs from lv_simulater_platformio/.)
// Query strings are stripped — different params for the same endpoint
// reuse the same fixture; sufficient for the apps' parse-path coverage.
//
// Apps without a fixture get the same -1 the old always-offline stub
// returned, so existing scenarios stay green until a fixture lands.
static const char *HTTP_FIXTURE_DIR = "../test/fixtures/http";

static bool resolve_http_fixture(const String &url, String *out_path) {
    // Skip "http://" or "https://".
    int p = url.indexOf("://");
    if (p < 0) return false;
    int host_start = p + 3;
    int host_end = url.indexOf('/', host_start);
    if (host_end < 0) host_end = url.length();
    int path_end = url.indexOf('?', host_end);
    if (path_end < 0) path_end = url.length();

    String host = url.substring(host_start, host_end);
    String path = url.substring(host_end, path_end);  // starts with '/' or empty
    if (path.length() == 0) path = "/index";

    String full(HTTP_FIXTURE_DIR);
    full += "/";
    full += host;
    full += path;
    full += ".json";
    *out_path = full;
    return true;
}

int HTTPClient::GET() {
    String fixture;
    if (!resolve_http_fixture(m_url, &fixture)) return -1;
    FILE *f = fopen(fixture.c_str(), "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return -1; }
    std::string buf(sz, '\0');
    if (sz > 0) fread(&buf[0], 1, sz, f);
    fclose(f);
    m_payload = String(buf);
    return HTTP_CODE_OK;
}

// Reproduces the firmware's analyseParam: split a buffer on '\n', null-terminating
// each piece in place. argv[i] gets a pointer to the i-th line.
bool analyseParam(char *info, int argc, char **argv) {
    int cnt;
    for (cnt = 0; cnt < argc; ++cnt) {
        argv[cnt] = info;
        while (*info != '\n' && *info != '\0') ++info;
        if (*info == '\0') { ++cnt; break; }
        *info = 0;
        ++info;
    }
    for (; cnt < argc; ++cnt) argv[cnt] = (char *)"";
    return true;
}

// ---------- doDelayMillisTime (firmware utility) ----------
boolean doDelayMillisTime(unsigned long interval,
                          unsigned long *previousMillis,
                          boolean state) {
    unsigned long currentMillis = millis();
    if (currentMillis - *previousMillis >= interval) {
        *previousMillis = currentMillis;
        return true;
    }
    return state;
}

// ---------- AppController stub impl ----------
AppController::AppController(const char *n) {
    strncpy(name, n, APP_CONTROLLER_NAME_LEN - 1);
    name[APP_CONTROLLER_NAME_LEN - 1] = 0;
    sys_cfg.power_mode = 1;
    sys_cfg.backLight = 80;
    sys_cfg.rotation = 0;
    sys_cfg.auto_calibration_mpu = 0;
    sys_cfg.mpu_order = 0;
}
AppController::~AppController() {}
void AppController::init() {}
void AppController::Display() {}
int AppController::app_auto_start() { return 0; }
int AppController::app_install(APP_OBJ *app, APP_TYPE type) {
    if (!app || app_num >= APP_MAX_NUM) return 1;
    appList[app_num] = app;
    appTypeList[app_num] = type;
    ++app_num;
    return 0;
}
int AppController::app_uninstall(const APP_OBJ *) { return 0; }
int AppController::remove_backgroud_task() { return 0; }
int AppController::main_process(ImuAction *act_info) {
    if (app_exit_flag == 0) {
        // Not in any app: select first app and enter it on GO_FORWORD.
        if (act_info->active == GO_FORWORD && app_num > 0) {
            app_exit_flag = 1;
            cur_app_index = 0;
            if (appList[cur_app_index]->app_init) {
                appList[cur_app_index]->app_init(this);
            }
        }
    } else {
        if (appList[cur_app_index]->main_process) {
            appList[cur_app_index]->main_process(this, act_info);
        }
    }
    act_info->active = UNKNOWN;
    act_info->isValid = 0;
    return 0;
}
void AppController::app_exit() {
    if (app_exit_flag && cur_app_index < (int)app_num) {
        if (appList[cur_app_index]->exit_callback) {
            appList[cur_app_index]->exit_callback(nullptr);
        }
    }
    app_exit_flag = 0;
}
int AppController::send_to(const char *from, const char *to,
                           APP_MESSAGE_TYPE type, void *message, void *ext_info) {
    if (!to) return 0;
    // Direct app-to-app message: dispatch to the named target.
    for (unsigned int i = 0; i < app_num; ++i) {
        if (appList[i] && appList[i]->app_name && !strcmp(appList[i]->app_name, to)) {
            if (appList[i]->message_handle) {
                appList[i]->message_handle(from ? from : "", to, type, message, ext_info);
            }
            return 0;
        }
    }
    // Controller-bound WiFi events. The real firmware queues these
    // and req_event_deal eventually fires the wifi callback back at
    // the *from* app once the connection succeeds. The harness fakes
    // "wifi connected" by invoking the callback synchronously — that's
    // enough to drive apps that gate HTTP fetches on WIFI_CONN
    // (bilibili/weather/stockmarket etc).
    if (!strcmp(to, "AppCtrl") && from &&
        (type == APP_MESSAGE_WIFI_CONN || type == APP_MESSAGE_WIFI_AP)) {
        for (unsigned int i = 0; i < app_num; ++i) {
            if (appList[i] && appList[i]->app_name && !strcmp(appList[i]->app_name, from)) {
                if (appList[i]->message_handle) {
                    appList[i]->message_handle("AppCtrl", from, type, message, ext_info);
                }
                return 0;
            }
        }
    }
    return 0;
}
