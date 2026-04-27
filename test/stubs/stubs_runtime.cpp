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
int AppController::send_to(const char *, const char *to,
                           APP_MESSAGE_TYPE type, void *message, void *ext_info) {
    if (!to) return 0;
    for (unsigned int i = 0; i < app_num; ++i) {
        if (appList[i] && appList[i]->app_name && !strcmp(appList[i]->app_name, to)) {
            if (appList[i]->message_handle) {
                appList[i]->message_handle("", to, type, message, ext_info);
            }
            return 0;
        }
    }
    return 0;
}
