// Form-render handlers for the web_setting page (PR-3.1 split).
// Each *_setting() function builds the HTML for one app's config form
// and writes it via Send_HTML. Reads the current persisted values via
// app_controller->send_to(APP_MESSAGE_GET_PARAM, ...) and emits each
// field through the small helpers below.
//
// PR-Glass-i18n rewrite: previous version embedded English literals in
// preprocessor-concatenated SETTING macros (one per app). All field
// labels + form titles + the Save button were hardcoded. To make every
// label flow through getText(), the SETTING macros were dropped and
// each *_setting() now builds its HTML at runtime via the helpers
// below. Per-field tooltips (added in Glass FU #3) are kept as English
// literals — they're hint text, not primary labels, and translating
// them is a separate (larger) task.

#include "aio_network.h"
#include "common.h"
#include "server.h"
#include "web_setting.h"
#include "web_setting_internal.h"
#include "app/app_conf.h"

namespace {

// Helpers — emit one HTML chunk per field into the running webpage
// String. Identical look to the previous SETTING macros; just runtime-
// composed with getText() injection.

void emit_form_open(String &html, const char *action, const char *title_key)
{
    html += F("<form method=\"GET\" action=\"/");
    html += action;
    html += F("\"><div class=\"card\"><div class=\"card-head\"><div><div class=\"card-title\">");
    html += getText(title_key);
    html += F("</div></div></div><div class=\"card-body\">");
}

void emit_form_close(String &html)
{
    html += F("</div><div class=\"row-actions\">"
              "<button type=\"submit\" class=\"btn primary\">\xE2\x9C\x93 ");
    html += getText("save");
    html += F("</button></div></div></form>");
}

// Optional `tip` (English) renders the ? hover hint; nullptr emits an
// empty span so the .field grid stays aligned.
void emit_tip(String &html, const char *tip)
{
    if (tip) {
        html += F("<span class=\"tip\" data-tip=\"");
        html += tip;
        html += F("\">?</span>");
    } else {
        html += F("<span></span>");
    }
}

void emit_text_field(String &html, const char *lbl_key, const char *name,
                     const char *value, const char *tip = nullptr)
{
    html += F("<div class=\"field\"><label>");
    html += getText(lbl_key);
    html += F("</label><input type=\"text\" name=\"");
    html += name;
    html += F("\" value=\"");
    html += value;
    html += F("\">");
    emit_tip(html, tip);
    html += F("</div>");
}

void emit_pwd_field(String &html, const char *lbl_key, const char *name,
                    const char *value, const char *tip = nullptr)
{
    html += F("<div class=\"field\"><label>");
    html += getText(lbl_key);
    html += F("</label><div class=\"secret-wrap\"><input type=\"password\" name=\"");
    html += name;
    html += F("\" value=\"");
    html += value;
    html += F("\" class=\"mono\"><button type=\"button\" class=\"eye-btn\">\xF0\x9F\x91\x81</button></div>");
    emit_tip(html, tip);
    html += F("</div>");
}

// Two-option radio. v0/v1 are the underlying string values written to
// flash; l0/l1 are the displayed labels (already-translated; either
// getText output or fixed strings like "简体" that don't translate).
// `current_val` is the int-parsed current value used to compute the
// `checked` attribute for whichever radio matches.
void emit_radio2_field(String &html, const char *lbl_key, const char *name,
                       const char *v0, const char *l0,
                       const char *v1, const char *l1,
                       int current_val, const char *tip = nullptr)
{
    html += F("<div class=\"field\"><label>");
    html += getText(lbl_key);
    html += F("</label><div>");
    html += F("<input class=\"radio\" type=\"radio\" value=\"");
    html += v0;
    html += F("\" name=\"");
    html += name;
    html += F("\"");
    if (atoi(v0) == current_val) html += F(" checked");
    html += F(">");
    html += l0;
    html += F("<input class=\"radio\" type=\"radio\" value=\"");
    html += v1;
    html += F("\" name=\"");
    html += name;
    html += F("\"");
    if (atoi(v1) == current_val) html += F(" checked");
    html += F(">");
    html += l1;
    html += F("</div>");
    emit_tip(html, tip);
    html += F("</div>");
}

}  // namespace

void sys_setting()
{
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
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_READ_CFG, NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"ssid_0", ssid_0);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"password_0", password_0);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"power_mode", power_mode);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"backLight", backLight);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"rotation", rotation);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"mpu_order", mpu_order);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"min_brightness", min_brightness);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"max_brightness", max_brightness);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"time", time);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"auto_calibration_mpu", auto_calibration_mpu);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"auto_start_app", auto_start_app);

    SysUtilConfig cfg = app_controller->sys_cfg;

    // String reserve(2400) would help fragmentation but the host-test
    // String stub doesn't implement it — skip and let the default
    // doubling-growth handle it. Form is ~2300 bytes worst-case.
    String form;

    // WiFi-scan card prepended above the form (PR Glass FU #1 — populated
    // by the rescanBtn handler in GLASS_JS).
    form = F("<div class=\"card\"><div class=\"card-head\"><div><div class=\"card-title\">");
    form += getText("wifi_networks");
    form += F("</div><div class=\"card-sub\">");
    form += getText("scan_subtitle");
    form += F("</div></div><button type=\"button\" class=\"btn ghost\" id=\"rescanBtn\" style=\"margin-left:auto\">");
    form += getText("rescan");
    form += F("</button></div><div class=\"card-body\"><div class=\"scan-list\" id=\"wifiScanList\"><div class=\"scan-empty\">");
    form += getText("scan_hint");
    form += F("</div></div></div></div>");

    emit_form_open(form, "saveSysConf", "form_sys_title");
    emit_text_field(form, "fld_ssid", "ssid_0", ssid_0);
    emit_pwd_field (form, "fld_password", "password_0", password_0);
    emit_text_field(form, "fld_power_mode", "power_mode", power_mode,
                    "0 throttles CPU for cooler running; 1 maxes clock for snappier UI.");
    emit_text_field(form, "fld_backlight", "backLight", backLight);
    emit_text_field(form, "fld_rotation", "rotation", rotation,
                    "Screen orientation. 0=portrait, 1=landscape, 2/3=flipped, 4/5=mirrored.");
    emit_text_field(form, "fld_mpu_order", "mpu_order", mpu_order,
                    "IMU axis remap. If tilt input feels wrong, walk 0-15 until it matches.");
    emit_radio2_field(form, "fld_mpu_autocal", "auto_calibration_mpu",
                      "0", getText("off"), "1", getText("on"),
                      cfg.auto_calibration_mpu,
                      "Run accelerometer auto-zero on boot. Keep the device flat for ~2s.");
    emit_text_field(form, "fld_auto_start", "auto_start_app", auto_start_app,
                    "App name to launch on boot. Leave blank for the menu.");
    emit_form_close(form);

    webpage = form;
    Send_HTML(webpage);
}

void rgb_setting()
{
    char min_brightness[32];
    char max_brightness[32];
    char time[32];
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_READ_CFG, NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"min_brightness", min_brightness);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"max_brightness", max_brightness);
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_GET_PARAM, (void *)"time", time);

    String form;
    emit_form_open(form, "saveRgbConf", "form_rgb_title");
    emit_text_field(form, "fld_min_brightness", "min_brightness", min_brightness);
    emit_text_field(form, "fld_max_brightness", "max_brightness", max_brightness);
    emit_text_field(form, "fld_cycle_time", "time", time);
    emit_form_close(form);

    webpage = form;
    Send_HTML(webpage);
}

void weather_setting()
{
    char api_key[128];
    char city_name[64];
    char weatherUpdataInterval[32];
    char timeUpdataInterval[32];
    char language[32];
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_READ_CFG, NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_GET_PARAM, (void *)"api_key", api_key);
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_GET_PARAM, (void *)"city_name", city_name);
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_GET_PARAM, (void *)"weatherUpdataInterval", weatherUpdataInterval);
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_GET_PARAM, (void *)"timeUpdataInterval", timeUpdataInterval);
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_GET_PARAM, (void *)"language", language);

    String form;
    emit_form_open(form, "saveWeatherConf", "form_weather_title");
    emit_pwd_field (form, "fld_api_key", "api_key", api_key,
                    "Get a free key at developer.accuweather.com (50 calls/day).");
    emit_text_field(form, "fld_city", "city_name", city_name,
                    "English city name. AccuWeather resolves it to a location key on first fetch.");
    emit_text_field(form, "fld_weather_refresh", "weatherUpdataInterval", weatherUpdataInterval,
                    "Don't go below 60000 — AccuWeather rate-limits the free tier.");
    emit_text_field(form, "fld_time_refresh", "timeUpdataInterval", timeUpdataInterval,
                    "NTP poll cadence. 60000 is plenty.");
    // Language radio: labels stay as the literal Chinese characters
    // (these are the languages themselves, not UI strings to translate).
    emit_radio2_field(form, "fld_display_lang", "language",
                      "0", "\xE7\xAE\x80\xE4\xBD\x93", "1", "\xE7\xB9\x81\xE9\xAB\x94",
                      atoi(language));
    emit_form_close(form);

    webpage = form;
    Send_HTML(webpage);
}

void weather_old_setting()
{
    char cityname[32];
    char language[32];
    char weather_key[32];
    char weatherUpdataInterval[32];
    char timeUpdataInterval[32];
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_READ_CFG, NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_GET_PARAM, (void *)"cityname", cityname);
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_GET_PARAM, (void *)"language", language);
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_GET_PARAM, (void *)"weather_key", weather_key);
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_GET_PARAM, (void *)"weatherUpdataInterval", weatherUpdataInterval);
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_GET_PARAM, (void *)"timeUpdataInterval", timeUpdataInterval);

    String form;
    emit_form_open(form, "saveWeatherOldConf", "form_weather_old_title");
    emit_text_field(form, "fld_city_pinyin", "cityname", cityname);
    emit_text_field(form, "fld_city_lang", "language", language);
    emit_pwd_field (form, "fld_weather_key", "weather_key", weather_key);
    emit_text_field(form, "fld_weather_refresh", "weatherUpdataInterval", weatherUpdataInterval);
    emit_text_field(form, "fld_time_refresh", "timeUpdataInterval", timeUpdataInterval);
    emit_form_close(form);

    webpage = form;
    Send_HTML(webpage);
}

void bili_setting()
{
    char bili_uid[32];
    char updataInterval[32];
    app_controller->send_to(SERVER_APP_NAME, "Bili", APP_MESSAGE_READ_CFG, NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Bili", APP_MESSAGE_GET_PARAM, (void *)"bili_uid", bili_uid);
    app_controller->send_to(SERVER_APP_NAME, "Bili", APP_MESSAGE_GET_PARAM, (void *)"updataInterval", updataInterval);

    String form;
    emit_form_open(form, "saveBiliConf", "form_bili_title");
    emit_text_field(form, "fld_bili_uid", "bili_uid", bili_uid,
                    "Numeric user ID from your space URL: space.bilibili.com/<UID>.");
    emit_text_field(form, "fld_update_interval", "updataInterval", updataInterval,
                    "Polling cadence. 60000ms keeps you well under any rate limit.");
    emit_form_close(form);

    webpage = form;
    Send_HTML(webpage);
}

void stock_setting()
{
    char stock_symbol[32];
    char market_type[32];
    char updataInterval[32];
    app_controller->send_to(SERVER_APP_NAME, "Stock", APP_MESSAGE_READ_CFG, NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Stock", APP_MESSAGE_GET_PARAM, (void *)"stock_symbol", stock_symbol);
    app_controller->send_to(SERVER_APP_NAME, "Stock", APP_MESSAGE_GET_PARAM, (void *)"market_type", market_type);
    app_controller->send_to(SERVER_APP_NAME, "Stock", APP_MESSAGE_GET_PARAM, (void *)"updataInterval", updataInterval);

    const char *us_selected = (strcmp(market_type, "US") == 0) ? "selected" : "";
    const char *cn_selected = (strcmp(market_type, "CN") == 0) ? "selected" : "";
    const char *hk_selected = (strcmp(market_type, "HK") == 0) ? "selected" : "";

    String form;
    emit_form_open(form, "saveStockConf", "form_stock_title");
    emit_text_field(form, "fld_stock_symbol", "stock_symbol", stock_symbol,
                    "US/HK use letter tickers; CN uses 6-digit codes (e.g. 601126).");

    // Market dropdown — inline since it's a single-use 3-option select.
    form += F("<div class=\"field\"><label>");
    form += getText("fld_market");
    form += F("</label><select name=\"market_type\">"
              "<option value=\"US\" ");
    form += us_selected;
    form += F(">US</option><option value=\"CN\" ");
    form += cn_selected;
    form += F(">CN</option><option value=\"HK\" ");
    form += hk_selected;
    form += F(">HK</option></select>"
              "<span class=\"tip\" data-tip=\"Picks the upstream feed: US=Yahoo, CN=Sina, HK=Sina-HK.\">?</span></div>");

    emit_text_field(form, "fld_update_interval", "updataInterval", updataInterval,
                    "Polling cadence. Markets quote every few seconds; 30000ms is plenty.");
    emit_form_close(form);

    webpage = form;
    Send_HTML(webpage);
}

void picture_setting()
{
    char switchInterval[32];
    app_controller->send_to(SERVER_APP_NAME, "Picture", APP_MESSAGE_READ_CFG, NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Picture", APP_MESSAGE_GET_PARAM, (void *)"switchInterval", switchInterval);

    String form;
    emit_form_open(form, "savePictureConf", "form_picture_title");
    emit_text_field(form, "fld_switch_interval", "switchInterval", switchInterval,
                    "How long each image stays on screen before rotating.");
    emit_form_close(form);

    webpage = form;
    Send_HTML(webpage);
}

void media_setting()
{
    char switchFlag[32];
    char powerFlag[32];
    app_controller->send_to(SERVER_APP_NAME, "Media", APP_MESSAGE_READ_CFG, NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Media", APP_MESSAGE_GET_PARAM, (void *)"switchFlag", switchFlag);
    app_controller->send_to(SERVER_APP_NAME, "Media", APP_MESSAGE_GET_PARAM, (void *)"powerFlag", powerFlag);

    String form;
    emit_form_open(form, "saveMediaConf", "form_media_title");
    emit_text_field(form, "fld_switch_flag", "switchFlag", switchFlag,
                    "Auto-rotate through SD videos vs. play one until input.");
    emit_text_field(form, "fld_power_flag", "powerFlag", powerFlag,
                    "1 raises CPU clock for smoother playback at higher power draw.");
    emit_form_close(form);

    webpage = form;
    Send_HTML(webpage);
}

void screen_setting()
{
    char powerFlag[32];
    app_controller->send_to(SERVER_APP_NAME, "Screen share", APP_MESSAGE_READ_CFG, NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Screen share", APP_MESSAGE_GET_PARAM, (void *)"powerFlag", powerFlag);

    String form;
    emit_form_open(form, "saveScreenConf", "form_screen_title");
    emit_text_field(form, "fld_power_flag", "powerFlag", powerFlag,
                    "1 maxes CPU for higher streamed-frame throughput.");
    emit_form_close(form);

    webpage = form;
    Send_HTML(webpage);
}

void heartbeat_setting()
{
    char role[32];
    char qq_num[32];
    char subtopic[32];
    char mqtt_server[32];
    char mqtt_port[32];
    char mqtt_user[32];
    char mqtt_password[32];
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_READ_CFG, NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM, (void *)"role", role);
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM, (void *)"qq_num", qq_num);
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM, (void *)"subtopic", subtopic);
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM, (void *)"mqtt_server", mqtt_server);
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM, (void *)"mqtt_port", mqtt_port);
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM, (void *)"mqtt_user", mqtt_user);
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_GET_PARAM, (void *)"mqtt_password", mqtt_password);

    String form;
    emit_form_open(form, "saveHeartbeatConf", "form_heartbeat_title");
    emit_text_field(form, "fld_role", "role", role,
                    "0 sends touches; 1 receives. Pair must mirror — one of each.");
    emit_text_field(form, "fld_qq", "qq_num", qq_num,
                    "Used as MQTT topic suffix. Two devices with the same QQ pair up.");
    emit_text_field(form, "fld_mqtt_server", "mqtt_server", mqtt_server,
                    "Broker hostname/IP. Use a public broker (test.mosquitto.org) or your own.");
    emit_text_field(form, "fld_mqtt_port", "mqtt_port", mqtt_port,
                    "1883 plain, 8883 TLS. Public brokers usually use 1883.");
    emit_text_field(form, "fld_mqtt_user", "mqtt_user", mqtt_user);
    emit_pwd_field (form, "fld_mqtt_pwd", "mqtt_password", mqtt_password);
    emit_form_close(form);

    webpage = form;
    Send_HTML(webpage);
}

void anniversary_setting()
{
    char event_name0[32];
    char target_date0[32];
    char event_name1[32];
    char target_date1[32];
    app_controller->send_to(SERVER_APP_NAME, "Anniversary", APP_MESSAGE_READ_CFG, NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "Anniversary", APP_MESSAGE_GET_PARAM, (void *)"event_name0", event_name0);
    app_controller->send_to(SERVER_APP_NAME, "Anniversary", APP_MESSAGE_GET_PARAM, (void *)"target_date0", target_date0);
    app_controller->send_to(SERVER_APP_NAME, "Anniversary", APP_MESSAGE_GET_PARAM, (void *)"event_name1", event_name1);
    app_controller->send_to(SERVER_APP_NAME, "Anniversary", APP_MESSAGE_GET_PARAM, (void *)"target_date1", target_date1);

    String form;
    emit_form_open(form, "saveAnniversaryConf", "form_anniversary_title");
    emit_text_field(form, "fld_event0", "event_name0", event_name0);
    emit_text_field(form, "fld_date0", "target_date0", target_date0,
                    "Format: YYYY-MM-DD. Past dates count up; future dates count down.");
    emit_text_field(form, "fld_event1", "event_name1", event_name1);
    emit_text_field(form, "fld_date1", "target_date1", target_date1,
                    "Format: YYYY-MM-DD. Past dates count up; future dates count down.");
    emit_form_close(form);

    webpage = form;
    Send_HTML(webpage);
}

void pc_resource_setting()
{
    char pc_ipaddr[32];
    char sensorUpdataInterval[32];
    app_controller->send_to(SERVER_APP_NAME, "PC Resource", APP_MESSAGE_READ_CFG, NULL, NULL);
    app_controller->send_to(SERVER_APP_NAME, "PC Resource", APP_MESSAGE_GET_PARAM, (void *)"pc_ipaddr", pc_ipaddr);
    app_controller->send_to(SERVER_APP_NAME, "PC Resource", APP_MESSAGE_GET_PARAM, (void *)"sensorUpdataInterval", sensorUpdataInterval);

    String form;
    emit_form_open(form, "savePCResourceConf", "form_pc_resource_title");
    emit_text_field(form, "fld_pc_ip", "pc_ipaddr", pc_ipaddr,
                    "IP of the PC running the AIDA64/HWiNFO bridge that feeds CPU/GPU stats.");
    emit_text_field(form, "fld_sensor_interval", "sensorUpdataInterval", sensorUpdataInterval,
                    "How often to poll the bridge. 1000-2000ms is the sweet spot.");
    emit_form_close(form);

    webpage = form;
    Send_HTML(webpage);
}
