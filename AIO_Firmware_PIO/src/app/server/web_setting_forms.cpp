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

#define SETING_CSS ".input{display:block;margin:18px 0;background:rgba(10,14,39,0.8);padding:15px;border:2px solid #00ff41;box-shadow:0 0 15px rgba(0,255,65,0.3),inset 0 0 10px rgba(0,255,65,0.05);transition:all 0.3s;position:relative;z-index:2;}" \
                   ".input:hover{box-shadow:0 0 25px rgba(0,255,65,0.6),inset 0 0 15px rgba(0,255,65,0.1);border-color:#00ff41;}" \
                   ".input span{width:320px;float:left;height:42px;line-height:42px;color:#00ff41;font-weight:700;font-size:15px;text-transform:uppercase;letter-spacing:1px;text-shadow:0 0 8px rgba(0,255,65,0.6);font-family:'Courier New',monospace;}" \
                   ".input input[type='text']{height:40px;width:240px;border:2px solid #00d9ff;background:rgba(0,217,255,0.05);color:#00d9ff;padding:0 12px;font-size:14px;font-family:'Courier New',monospace;transition:all 0.3s;}" \
                   ".input input[type='text']:focus{outline:none;border-color:#00ff41;background:rgba(0,255,65,0.1);box-shadow:0 0 20px rgba(0,255,65,0.6);color:#00ff41;}" \
                   ".input .radio{height:18px;width:18px;margin:0 10px;cursor:pointer;accent-color:#00ff41;filter:drop-shadow(0 0 5px #00ff41);}" \
                   ".btn{min-width:160px;height:45px;background:rgba(0,255,65,0.1);border:2px solid #00ff41;color:#00ff41;font-size:16px;font-weight:700;cursor:pointer;margin-top:25px;box-shadow:0 0 20px rgba(0,255,65,0.6);transition:all 0.3s;text-transform:uppercase;letter-spacing:2px;font-family:'Courier New',monospace;}" \
                   ".btn:hover{background:rgba(0,255,65,0.2);box-shadow:0 0 35px rgba(0,255,65,0.9);transform:scale(1.05);}" \
                   "form{background:rgba(10,14,39,0.95);padding:30px;border:3px solid #00ff41;box-shadow:0 0 30px rgba(0,255,65,0.5),inset 0 0 30px rgba(0,255,65,0.05);margin:25px auto;max-width:850px;position:relative;z-index:2;}"

#define SYS_SETTING "<form method=\"GET\" action=\"saveSysConf\">"                                                                                                                                                                                                      \
                    "<label class=\"input\"><span>WiFi SSID_0(2.4G)</span><input type=\"text\"name=\"ssid_0\"value=\"%s\"></label>"                                                                                                                                     \
                    "<label class=\"input\"><span>WiFi Passwd_0</span><input type=\"text\"name=\"password_0\"value=\"%s\"></label>"                                                                                                                                     \
                    "<label class=\"input\"><span>功耗控制（0低发热 1性能优先）</span><input type=\"text\"name=\"power_mode\"value=\"%s\"></label>"                                                                                                        \
                    "<label class=\"input\"><span>屏幕亮度 (值为1~100)</span><input type=\"text\"name=\"backLight\"value=\"%s\"></label>"                                                                                                                         \
                    "<label class=\"input\"><span>屏幕方向 (0~5可选)</span><input type=\"text\"name=\"rotation\"value=\"%s\"></label>"                                                                                                                            \
                    "<label class=\"input\"><span>操作方向（0~15可选）</span><input type=\"text\"name=\"mpu_order\"value=\"%s\"></label>"                                                                                                                       \
                    "<label class=\"input\"><span>MPU6050自动校准</span><input class=\"radio\" type=\"radio\" value=\"0\" name=\"auto_calibration_mpu\" %s>关闭<input class=\"radio\" type=\"radio\" value=\"1\" name=\"auto_calibration_mpu\" %s>开启</label>" \
                    "<label class=\"input\"><span>开机自启的APP名字（如 Weather ）</span><input type=\"text\"name=\"auto_start_app\"value=\"%s\"></label>"                                                                                                    \
                    "</label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"保存\"></form>"

#define RGB_SETTING "<form method=\"GET\" action=\"saveRgbConf\">"                                                                                          \
                    "<label class=\"input\"><span>RGB最低亮度（0~1000可选）</span><input type=\"text\"name=\"min_brightness\"value=\"%s\"></label>" \
                    "<label class=\"input\"><span>RGB最高亮度（0~1000可选）</span><input type=\"text\"name=\"max_brightness\"value=\"%s\"></label>" \
                    "<label class=\"input\"><span>RGB渐变时间（10~1000可选）</span><input type=\"text\"name=\"time\"value=\"%s\"></label>"          \
                    "</label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"保存\"></form>"

#define WEATHER_SETTING "<form method=\"GET\" action=\"saveWeatherConf\">"                                                                                            \
                        "<label class=\"input\"><span>AccuWeather API Key</span><input type=\"text\"name=\"api_key\"value=\"%s\"></label>"                          \
                        "<label class=\"input\"><span>城市名稱 (City Name)</span><input type=\"text\"name=\"city_name\"value=\"%s\"></label>"                       \
                        "<label class=\"input\"><span>天氣更新週期（毫秒）</span><input type=\"text\"name=\"weatherUpdataInterval\"value=\"%s\"></label>"   \
                        "<label class=\"input\"><span>日期更新週期（毫秒）</span><input type=\"text\"name=\"timeUpdataInterval\"value=\"%s\"></label>"      \
                        "<label class=\"input\"><span>界面語言</span><input class=\"radio\" type=\"radio\" value=\"0\" name=\"language\" %s>简体中文<input class=\"radio\" type=\"radio\" value=\"1\" name=\"language\" %s>繁體中文</label>" \
                        "</label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"保存\"></form>"

#define WEATHER_OLD_SETTING "<form method=\"GET\" action=\"saveWeatherOldConf\">"                                                                                       \
                            "<label class=\"input\"><span>知心天气 城市名（拼音）</span><input type=\"text\"name=\"cityname\"value=\"%s\"></label>"          \
                            "<label class=\"input\"><span>City Language(zh-Hans)</span><input type=\"text\"name=\"language\"value=\"%s\"></label>"                      \
                            "<label class=\"input\"><span>Weather Key</span><input type=\"text\"name=\"weather_key\"value=\"%s\"></label>"                              \
                            "<label class=\"input\"><span>天气更新周期（毫秒）</span><input type=\"text\"name=\"weatherUpdataInterval\"value=\"%s\"></label>" \
                            "<label class=\"input\"><span>日期更新周期（毫秒）</span><input type=\"text\"name=\"timeUpdataInterval\"value=\"%s\"></label>"    \
                            "</label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"保存\"></form>"

#define BILIBILI_SETTING "<form method=\"GET\" action=\"saveBiliConf\">"                                                                                      \
                         "<label class=\"input\"><span>Bili UID</span><input type=\"text\"name=\"bili_uid\"value=\"%s\"></label>"                             \
                         "<label class=\"input\"><span>数据更新周期（毫秒）</span><input type=\"text\"name=\"updataInterval\"value=\"%s\"></label>" \
                         "</label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"保存\"></form>"

#define STOCK_SETTING "<form method=\"GET\" action=\"saveStockConf\">"                                                                                                                                                          \
                      "<label class=\"input\"><span>Stock Symbol (e.g., AAPL, TSLA, 601126)</span><input type=\"text\"name=\"stock_symbol\"value=\"%s\"></label>"                                                                             \
                      "<label class=\"input\"><span>Market Type</span><select name=\"market_type\" style=\"height:40px;width:240px;border:2px solid #00d9ff;background:rgba(0,217,255,0.05);color:#00d9ff;padding:0 12px;font-size:14px;\">" \
                      "<option value=\"US\" %s>US (United States)</option>"                                                                                                                                                                    \
                      "<option value=\"CN\" %s>CN (China)</option>"                                                                                                                                                                            \
                      "<option value=\"HK\" %s>HK (Hong Kong)</option>"                                                                                                                                                                        \
                      "</select></label>"                                                                                                                                                                                                       \
                      "<label class=\"input\"><span>Update Interval (milliseconds)</span><input type=\"text\"name=\"updataInterval\"value=\"%s\"></label>"                                                                                     \
                      "</label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"Save\"></form>"

#define PICTURE_SETTING "<form method=\"GET\" action=\"savePictureConf\">"                                                                                         \
                        "<label class=\"input\"><span>自动切换时间间隔（毫秒）</span><input type=\"text\"name=\"switchInterval\"value=\"%s\"></label>" \
                        "</label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"保存\"></form>"

#define MEDIA_SETTING "<form method=\"GET\" action=\"saveMediaConf\">"                                                                                             \
                      "<label class=\"input\"><span>自动切换（0不切换 1自动切换）</span><input type=\"text\"name=\"switchFlag\"value=\"%s\"></label>" \
                      "<label class=\"input\"><span>功耗控制（0低发热 1性能优先）</span><input type=\"text\"name=\"powerFlag\"value=\"%s\"></label>"  \
                      "</label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"保存\"></form>"

#define SCREEN_SETTING "<form method=\"GET\" action=\"saveScreenConf\">"                                                                                           \
                       "<label class=\"input\"><span>功耗控制（0低发热 1性能优先）</span><input type=\"text\"name=\"powerFlag\"value=\"%s\"></label>" \
                       "</label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"保存\"></form>"

#define HEARTBEAT_SETTING "<form method=\"GET\" action=\"saveHeartbeatConf\">"                                                                            \
                          "<label class=\"input\"><span>Role(0:heart,1:beat)</span><input type=\"text\"name=\"role\"value=\"%s\"></label>"                \
                          "<label class=\"input\"><span>QQ num(填写QQ号)</span><input type=\"text\"name=\"qq_num\"value=\"%s\"></label>"                    \
                        "<label class=\"input\"><span>MQTT Server</span><input type=\"text\"name=\"mqtt_server\"value=\"%s\"></label>"                    \
                        "<label class=\"input\"><span>MQTT 端口号</span><input type=\"text\"name=\"mqtt_port\"value=\"%s\"></label>"                   \
                        "<label class=\"input\"><span>MQTT 服务用户名(可不填)</span><input type=\"text\"name=\"mqtt_user\"value=\"%s\"></label>"  \
                        "<label class=\"input\"><span>MQTT 服务密码(可不填)</span><input type=\"text\"name=\"mqtt_password\"value=\"%s\"></label>" \
                        "</label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"保存\"></form>"

#define ANNIVERSARY_SETTING "<form method=\"GET\" action=\"saveAnniversaryConf\">"                                                      \
                            "<label class=\"input\"><span>事件0</span><input type=\"text\"name=\"event_name0\"value=\"%s\"></label>"  \
                            "<label class=\"input\"><span>日期0</span><input type=\"text\"name=\"target_date0\"value=\"%s\"></label>" \
                            "<label class=\"input\"><span>事件1</span><input type=\"text\"name=\"event_name1\"value=\"%s\"></label>"  \
                            "<label class=\"input\"><span>日期1</span><input type=\"text\"name=\"target_date1\"value=\"%s\"></label>" \
                            "</label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"保存\"></form>"

#define REMOTR_SENSOR_SETTING "<form method=\"GET\" action=\"savePCResourceConf\">"                                                                                       \
                              "<label class=\"input\"><span>PC地址</span><input type=\"text\"name=\"pc_ipaddr\"value=\"%s\"></label>"                                   \
                              "<label class=\"input\"><span>传感器数据更新间隔(ms)</span><input type=\"text\"name=\"sensorUpdataInterval\"value=\"%s\"></label>" \
                              "</label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"保存\"></form>"

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
    webpage = buf;
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
