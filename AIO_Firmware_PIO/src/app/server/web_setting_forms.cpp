// Form-render handlers for the web_setting page (PR-3.1 split).
// Each *_setting() function builds the HTML for one app's config form
// and writes it via Send_HTML. Reads the current persisted values via
// app_controller->send_to(APP_MESSAGE_GET_PARAM, ...) and inlines them
// into the SETTING macro for that app.
//
// Originally lived alongside the i18n table and page chrome inside a
// 1257-line web_setting.cpp. Split out so the form-rendering layer
// can be reviewed without the i18n / chrome / file-ops noise.

#include "network.h"
#include "common.h"
#include "server.h"
#include "web_setting.h"
#include "web_setting_internal.h"
#include "app/app_conf.h"

// Glass UI form-render macros. Each *_SETTING macro builds one
// <form><div class="card"> wrapping a sequence of <div class="field">
// rows, ending with the row-actions Save bar. The styles for these
// classes live in GLASS_CSS in web_setting.cpp; SETING_CSS from the
// pre-Glass cyberpunk theme is gone with this PR.
//
// %s placeholders preserve the existing per-field substitution order
// so the *_setting() functions below don't need to re-shape their
// snprintf calls — only the surrounding HTML changed.

// Compact field helpers. Each emits one .field row with the right
// number of %s placeholders (text=1, password=1, radio2=2, select3=3).
// Tip column kept as empty <span></span> placeholder; per-field tooltips
// can be wired later by inlining "<span class=\"tip\" data-tip=\"...\">?</span>".
#define G_FIELD_TEXT(lbl, name) \
  "<div class=\"field\"><label>" lbl "</label>" \
  "<input type=\"text\" name=\"" name "\" value=\"%s\"><span></span></div>"

#define G_FIELD_PWD(lbl, name) \
  "<div class=\"field\"><label>" lbl "</label>" \
  "<div class=\"secret-wrap\"><input type=\"password\" name=\"" name "\" value=\"%s\" class=\"mono\">" \
  "<button type=\"button\" class=\"eye-btn\">\xF0\x9F\x91\x81</button></div><span></span></div>"

#define G_FIELD_RADIO2(lbl, name, v0, l0, v1, l1) \
  "<div class=\"field\"><label>" lbl "</label><div>" \
  "<input class=\"radio\" type=\"radio\" value=\"" v0 "\" name=\"" name "\" %s>" l0 \
  "<input class=\"radio\" type=\"radio\" value=\"" v1 "\" name=\"" name "\" %s>" l1 \
  "</div><span></span></div>"

#define G_FORM_OPEN(action, title) \
  "<form method=\"GET\" action=\"" action "\"><div class=\"card\">" \
  "<div class=\"card-head\"><div><div class=\"card-title\">" title "</div></div></div>" \
  "<div class=\"card-body\">"

#define G_FORM_CLOSE \
  "</div><div class=\"row-actions\">" \
  "<button type=\"submit\" class=\"btn primary\">\xE2\x9C\x93 Save</button>" \
  "</div></div></form>"

#define SYS_SETTING G_FORM_OPEN("saveSysConf", "System") \
  G_FIELD_TEXT("WiFi SSID_0 (2.4G)", "ssid_0") \
  G_FIELD_PWD ("WiFi Password_0", "password_0") \
  G_FIELD_TEXT("Power Mode (0=eco, 1=perf)", "power_mode") \
  G_FIELD_TEXT("Backlight (1-100)", "backLight") \
  G_FIELD_TEXT("Rotation (0-5)", "rotation") \
  G_FIELD_TEXT("MPU Order (0-15)", "mpu_order") \
  G_FIELD_RADIO2("MPU6050 Auto-cal", "auto_calibration_mpu", "0", "Off", "1", "On") \
  G_FIELD_TEXT("Auto-start App (e.g. Weather)", "auto_start_app") \
  G_FORM_CLOSE

#define RGB_SETTING G_FORM_OPEN("saveRgbConf", "RGB Lighting") \
  G_FIELD_TEXT("Min Brightness (0-1000)", "min_brightness") \
  G_FIELD_TEXT("Max Brightness (0-1000)", "max_brightness") \
  G_FIELD_TEXT("Cycle Time (10-1000 ms)", "time") \
  G_FORM_CLOSE

#define WEATHER_SETTING G_FORM_OPEN("saveWeatherConf", "Weather (AccuWeather)") \
  G_FIELD_PWD ("AccuWeather API Key", "api_key") \
  G_FIELD_TEXT("City Name", "city_name") \
  G_FIELD_TEXT("Weather Refresh (ms)", "weatherUpdataInterval") \
  G_FIELD_TEXT("Time Refresh (ms)", "timeUpdataInterval") \
  G_FIELD_RADIO2("Display Language", "language", "0", "\xE7\xAE\x80\xE4\xBD\x93", "1", "\xE7\xB9\x81\xE9\xAB\x94") \
  G_FORM_CLOSE

#define WEATHER_OLD_SETTING G_FORM_OPEN("saveWeatherOldConf", "Weather (legacy)") \
  G_FIELD_TEXT("City Name (pinyin)", "cityname") \
  G_FIELD_TEXT("City Language (zh-Hans)", "language") \
  G_FIELD_PWD ("Weather Key", "weather_key") \
  G_FIELD_TEXT("Weather Refresh (ms)", "weatherUpdataInterval") \
  G_FIELD_TEXT("Time Refresh (ms)", "timeUpdataInterval") \
  G_FORM_CLOSE

#define BILIBILI_SETTING G_FORM_OPEN("saveBiliConf", "Bilibili Fans") \
  G_FIELD_TEXT("Bili UID", "bili_uid") \
  G_FIELD_TEXT("Update Interval (ms)", "updataInterval") \
  G_FORM_CLOSE

// Stock has a 3-option select rather than radio; spell it out inline
// rather than adding a single-use G_FIELD_SELECT3 macro.
#define STOCK_SETTING G_FORM_OPEN("saveStockConf", "Stock") \
  G_FIELD_TEXT("Stock Symbol (AAPL, TSLA, 601126)", "stock_symbol") \
  "<div class=\"field\"><label>Market</label>" \
  "<select name=\"market_type\">" \
  "<option value=\"US\" %s>US</option>" \
  "<option value=\"CN\" %s>CN</option>" \
  "<option value=\"HK\" %s>HK</option>" \
  "</select><span></span></div>" \
  G_FIELD_TEXT("Update Interval (ms)", "updataInterval") \
  G_FORM_CLOSE

#define PICTURE_SETTING G_FORM_OPEN("savePictureConf", "Picture") \
  G_FIELD_TEXT("Auto-switch Interval (ms)", "switchInterval") \
  G_FORM_CLOSE

#define MEDIA_SETTING G_FORM_OPEN("saveMediaConf", "Media Player") \
  G_FIELD_TEXT("Auto-switch (0=off, 1=on)", "switchFlag") \
  G_FIELD_TEXT("Power Mode (0=eco, 1=perf)", "powerFlag") \
  G_FORM_CLOSE

#define SCREEN_SETTING G_FORM_OPEN("saveScreenConf", "Screen Share") \
  G_FIELD_TEXT("Power Mode (0=eco, 1=perf)", "powerFlag") \
  G_FORM_CLOSE

#define HEARTBEAT_SETTING G_FORM_OPEN("saveHeartbeatConf", "Heartbeat (MQTT)") \
  G_FIELD_TEXT("Role (0=heart, 1=beat)", "role") \
  G_FIELD_TEXT("QQ Number", "qq_num") \
  G_FIELD_TEXT("MQTT Server", "mqtt_server") \
  G_FIELD_TEXT("MQTT Port", "mqtt_port") \
  G_FIELD_TEXT("MQTT Username (optional)", "mqtt_user") \
  G_FIELD_PWD ("MQTT Password (optional)", "mqtt_password") \
  G_FORM_CLOSE

#define ANNIVERSARY_SETTING G_FORM_OPEN("saveAnniversaryConf", "Anniversary") \
  G_FIELD_TEXT("Event 0", "event_name0") \
  G_FIELD_TEXT("Date 0", "target_date0") \
  G_FIELD_TEXT("Event 1", "event_name1") \
  G_FIELD_TEXT("Date 1", "target_date1") \
  G_FORM_CLOSE

#define REMOTR_SENSOR_SETTING G_FORM_OPEN("savePCResourceConf", "PC Resource") \
  G_FIELD_TEXT("PC IP Address", "pc_ipaddr") \
  G_FIELD_TEXT("Sensor Update Interval (ms)", "sensorUpdataInterval") \
  G_FORM_CLOSE

void sys_setting()
{
    char buf[2048];
    char ssid_0[32];
    char password_0[32];
    char power_mode[32];
    char backLight[32];
    char rotation[32];
    char mpu_order[32];
    char min_brightness[32];
    char max_brightness[32];
    char time[32];
    char auto_calibration_mpu[32];
    char auto_start_app[32];
    // 读取数据
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_READ_CFG,
                            NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"ssid_0", ssid_0);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"password_0", password_0);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"power_mode", power_mode);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"backLight", backLight);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"rotation", rotation);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"mpu_order", mpu_order);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"min_brightness", min_brightness);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"max_brightness", max_brightness);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"time", time);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"auto_calibration_mpu", auto_calibration_mpu);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"auto_start_app", auto_start_app);
    SysUtilConfig cfg = app_controller->sys_cfg;
    // 主要为了处理启停MPU自动校准的单选框
    if (0 == cfg.auto_calibration_mpu)
    {
        snprintf(buf, sizeof(buf), SYS_SETTING,
                ssid_0, password_0,
                power_mode, backLight, rotation,
                mpu_order, "checked=\"checked\"", "",
                auto_start_app);
    }
    else
    {
        snprintf(buf, sizeof(buf), SYS_SETTING,
                ssid_0, password_0,
                power_mode, backLight, rotation,
                mpu_order, "", "checked=\"checked\"",
                auto_start_app);
    }

    // WiFi scan card prepended above the system form. Static skeleton; the
    // GLASS_JS rescanBtn handler in init_page_footer fetches /api/wifi-scan
    // (the endpoint added in Glass UI PR-A) and re-renders #wifiScanList
    // with one .scan-row per network. Clicking a row fills the ssid_0 input
    // below + focuses the password field.
    webpage = F("<div class=\"card\"><div class=\"card-head\"><div><div class=\"card-title\">");
    webpage += getText("wifi_networks");
    webpage += F("</div><div class=\"card-sub\">");
    webpage += getText("scan_subtitle");
    webpage += F("</div></div><button type=\"button\" class=\"btn ghost\" id=\"rescanBtn\" style=\"margin-left:auto\">");
    webpage += getText("rescan");
    webpage += F("</button></div><div class=\"card-body\"><div class=\"scan-list\" id=\"wifiScanList\"><div class=\"scan-empty\">");
    webpage += getText("scan_hint");
    webpage += F("</div></div></div></div>");
    webpage += buf;
    Send_HTML(webpage);
}

void rgb_setting()
{
    char buf[2048];
    char min_brightness[32];
    char max_brightness[32];
    char time[32];
    // 读取数据
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_READ_CFG,
                            NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"min_brightness", min_brightness);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"max_brightness", max_brightness);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM,
                            (void *)"time", time);
    snprintf(buf, sizeof(buf), RGB_SETTING,
            min_brightness, max_brightness, time);
    webpage = buf;
    Send_HTML(webpage);
}

void weather_setting()
{
    char buf[2048];
    char api_key[128];
    char city_name[64];
    char weatherUpdataInterval[32];
    char timeUpdataInterval[32];
    char language[32];
    // 读取数据
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_READ_CFG,
                            NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_GET_PARAM,
                            (void *)"api_key", api_key);
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_GET_PARAM,
                            (void *)"city_name", city_name);
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_GET_PARAM,
                            (void *)"weatherUpdataInterval", weatherUpdataInterval);
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_GET_PARAM,
                            (void *)"timeUpdataInterval", timeUpdataInterval);
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_GET_PARAM,
                            (void *)"language", language);
    
    int lang = atoi(language);
    const char *lang0_checked = (lang == 0) ? "checked" : "";
    const char *lang1_checked = (lang == 1) ? "checked" : "";
    
    snprintf(buf, sizeof(buf), WEATHER_SETTING,
            api_key,
            city_name,
            weatherUpdataInterval,
            timeUpdataInterval,
            lang0_checked,
            lang1_checked);
    webpage = buf;
    Send_HTML(webpage);
}

void weather_old_setting()
{
    char buf[2048];
    char cityname[32];
    char language[32];
    char weather_key[32];
    char weatherUpdataInterval[32];
    char timeUpdataInterval[32];
    // 读取数据
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_READ_CFG,
                            NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_GET_PARAM,
                            (void *)"cityname", cityname);
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_GET_PARAM,
                            (void *)"language", language);
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_GET_PARAM,
                            (void *)"weather_key", weather_key);
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_GET_PARAM,
                            (void *)"weatherUpdataInterval", weatherUpdataInterval);
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_GET_PARAM,
                            (void *)"timeUpdataInterval", timeUpdataInterval);
    snprintf(buf, sizeof(buf), WEATHER_OLD_SETTING,
            cityname,
            language,
            weather_key,
            weatherUpdataInterval,
            timeUpdataInterval);
    webpage = buf;
    Send_HTML(webpage);
}

void bili_setting()
{
    char buf[2048];
    char bili_uid[32];
    char updataInterval[32];
    // 读取数据
    app_controller->send_to(SERVER_APP_NAME, "Bili", APP_MESSAGE_READ_CFG,
                            NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Bili", APP_MESSAGE_GET_PARAM,
                            (void *)"bili_uid", bili_uid);
    app_controller->send_to(SERVER_APP_NAME, "Bili", APP_MESSAGE_GET_PARAM,
                            (void *)"updataInterval", updataInterval);
    snprintf(buf, sizeof(buf), BILIBILI_SETTING, bili_uid, updataInterval);
    webpage = buf;
    Send_HTML(webpage);
}

void stock_setting()
{
    char buf[2048];
    char stock_symbol[32];
    char market_type[32];
    char updataInterval[32];
    // Read configuration data
    app_controller->send_to(SERVER_APP_NAME, "Stock", APP_MESSAGE_READ_CFG,
                            NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Stock", APP_MESSAGE_GET_PARAM,
                            (void *)"stock_symbol", stock_symbol);
    app_controller->send_to(SERVER_APP_NAME, "Stock", APP_MESSAGE_GET_PARAM,
                            (void *)"market_type", market_type);
    app_controller->send_to(SERVER_APP_NAME, "Stock", APP_MESSAGE_GET_PARAM,
                            (void *)"updataInterval", updataInterval);
    
    // Prepare selected options for dropdown
    const char* us_selected = (strcmp(market_type, "US") == 0) ? "selected" : "";
    const char* cn_selected = (strcmp(market_type, "CN") == 0) ? "selected" : "";
    const char* hk_selected = (strcmp(market_type, "HK") == 0) ? "selected" : "";
    
    snprintf(buf, sizeof(buf), STOCK_SETTING, stock_symbol, us_selected, cn_selected, hk_selected, updataInterval);
    webpage = buf;
    Send_HTML(webpage);
}

void picture_setting()
{
    char buf[2048];
    char switchInterval[32];
    // 读取数据
    app_controller->send_to(SERVER_APP_NAME, "Picture", APP_MESSAGE_READ_CFG,
                            NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Picture", APP_MESSAGE_GET_PARAM,
                            (void *)"switchInterval", switchInterval);
    snprintf(buf, sizeof(buf), PICTURE_SETTING, switchInterval);
    webpage = buf;
    Send_HTML(webpage);
}

void media_setting()
{
    char buf[2048];
    char switchFlag[32];
    char powerFlag[32];
    // 读取数据
    app_controller->send_to(SERVER_APP_NAME, "Media", APP_MESSAGE_READ_CFG,
                            NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Media", APP_MESSAGE_GET_PARAM,
                            (void *)"switchFlag", switchFlag);
    app_controller->send_to(SERVER_APP_NAME, "Media", APP_MESSAGE_GET_PARAM,
                            (void *)"powerFlag", powerFlag);
    snprintf(buf, sizeof(buf), MEDIA_SETTING, switchFlag, powerFlag);
    webpage = buf;
    Send_HTML(webpage);
}

void screen_setting()
{
    char buf[2048];
    char powerFlag[32];
    // 读取数据
    app_controller->send_to(SERVER_APP_NAME, "Screen share", APP_MESSAGE_READ_CFG,
                            NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Screen share", APP_MESSAGE_GET_PARAM,
                            (void *)"powerFlag", powerFlag);
    snprintf(buf, sizeof(buf), SCREEN_SETTING, powerFlag);
    webpage = buf;
    Send_HTML(webpage);
}

void heartbeat_setting()
{
    char buf[2048];
    char role[32];
    char qq_num[32];
    char subtopic[32];
    char mqtt_server[32];
    char mqtt_port[32];
    char mqtt_user[32];
    char mqtt_password[32];
    // 读取数据
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_READ_CFG,
                            NULL, NULL);

    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM,
                            (void *)"role", role);
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM,
                            (void *)"qq_num", qq_num);
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM,
                            (void *)"subtopic", subtopic);
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM,
                            (void *)"mqtt_server", mqtt_server);
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM,
                            (void *)"mqtt_port", mqtt_port);
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM,
                            (void *)"mqtt_user", mqtt_user);
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM,
                            (void *)"mqtt_password", mqtt_password);

    snprintf(buf, sizeof(buf), HEARTBEAT_SETTING, role, qq_num, mqtt_server,
            mqtt_port, mqtt_user, mqtt_password);
    webpage = buf;
    Send_HTML(webpage);
}

void anniversary_setting()
{
    char buf[2048];
    char event_name0[32];
    char target_date0[32];
    char event_name1[32];
    char target_date1[32];
    // 读取数据
    app_controller->send_to(SERVER_APP_NAME, "Anniversary", APP_MESSAGE_READ_CFG,
                            NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Anniversary", APP_MESSAGE_GET_PARAM,
                            (void *)"event_name0", event_name0);
    app_controller->send_to(SERVER_APP_NAME, "Anniversary", APP_MESSAGE_GET_PARAM,
                            (void *)"target_date0", target_date0);
    app_controller->send_to(SERVER_APP_NAME, "Anniversary", APP_MESSAGE_GET_PARAM,
                            (void *)"event_name1", event_name1);
    app_controller->send_to(SERVER_APP_NAME, "Anniversary", APP_MESSAGE_GET_PARAM,
                            (void *)"target_date1", target_date1);
    snprintf(buf, sizeof(buf), ANNIVERSARY_SETTING, event_name0, target_date0, event_name1, target_date1);
    webpage = buf;
    Send_HTML(webpage);
}

void pc_resource_setting()
{
    char buf[2048];
    char pc_ipaddr[32];
    char sensorUpdataInterval[32];
    // 读取数据
    app_controller->send_to(SERVER_APP_NAME, "PC Resource", APP_MESSAGE_READ_CFG,
                            NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "PC Resource", APP_MESSAGE_GET_PARAM,
                            (void *)"pc_ipaddr", pc_ipaddr);
    app_controller->send_to(SERVER_APP_NAME, "PC Resource", APP_MESSAGE_GET_PARAM,
                            (void *)"sensorUpdataInterval", sensorUpdataInterval);
    snprintf(buf, sizeof(buf), REMOTR_SENSOR_SETTING, pc_ipaddr, sensorUpdataInterval);
    webpage = buf;
    Send_HTML(webpage);
}
