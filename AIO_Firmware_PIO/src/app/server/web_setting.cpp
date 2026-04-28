
// 参考代码 https://github.com/G6EJD/ESP32-8266-File-Upload

#include "network.h"
#include "common.h"
#include "server.h"
#include "web_setting.h"
#include "app/app_conf.h"
#include "FS.h"
#include "HardwareSerial.h"
#include <esp32-hal.h>

boolean sd_present = true;
String webpage = "";
String webpage_header = "";
String webpage_footer = "";

// Language support
enum Language { LANG_EN_US = 0, LANG_ZH_CN = 1, LANG_ZH_TW = 2 };
static Language current_lang = LANG_EN_US;

// Forward declaration
const char* getText(const char* key);

// Helper function to get current language parameter
String getLangParam() {
    if (server.hasArg("lang")) {
        return "?lang=" + server.arg("lang");
    }
    return "";
}

void Send_HTML(const String &content)
{
    // Regenerate header and footer with current language settings
    init_page_header();
    init_page_footer();
    
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.send(200, "text/html", "");
    server.sendContent(webpage_header);

    server.sendContent(content);
    server.sendContent(webpage_footer);

    server.sendContent("");
    server.client().stop(); // Stop is needed because no content length was sent
}

String file_size(int bytes)
{
    String fsize = "";
    if (bytes < 1024)
        fsize = String(bytes) + " B";
    else if (bytes < (1024 * 1024))
        fsize = String(bytes / 1024.0, 3) + " KB";
    else if (bytes < (1024 * 1024 * 1024))
        fsize = String(bytes / 1024.0 / 1024.0, 3) + " MB";
    else
        fsize = String(bytes / 1024.0 / 1024.0 / 1024.0, 3) + " GB";
    return fsize;
}

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

void init_page_header()
{
    // Check and update language from URL parameter
    if (server.hasArg("lang")) {
        String lang = server.arg("lang");
        if (lang == "en") current_lang = LANG_EN_US;
        else if (lang == "cn") current_lang = LANG_ZH_CN;
        else if (lang == "tw") current_lang = LANG_ZH_TW;
    }
    
    webpage_header = F("<!DOCTYPE html><html>");
    webpage_header += F("<head>");
    webpage_header += F("<title>HoloCubic WebServer</title>"); // NOTE: 1em = 16px
    webpage_header += F("<meta http-equiv='Content-Type' name='viewport' content='user-scalable=yes,initial-scale=1.0,width=device-width; text/html; charset=utf-8' />");
    webpage_header += F("<style>");
    webpage_header += F(SETING_CSS);
    webpage_header += F("@keyframes glitch{0%,100%{text-shadow:2px 2px #00ff41,-2px -2px #ff00de;}50%{text-shadow:-2px -2px #00ff41,2px 2px #ff00de;}}");
    webpage_header += F("@keyframes neon-pulse{0%,100%{box-shadow:0 0 5px #00ff41,0 0 10px #00ff41,0 0 15px #00ff41,0 0 20px #00ff41;}50%{box-shadow:0 0 10px #00ff41,0 0 20px #00ff41,0 0 30px #00ff41,0 0 40px #00ff41;}}");
    webpage_header += F("body{max-width:75%;margin:0 auto;font-family:'Courier New',monospace;font-size:105%;text-align:center;color:#00ff41;background:#0a0e27;min-height:100vh;padding:20px 0;position:relative;}");
    webpage_header += F("body::before{content:'';position:fixed;top:0;left:0;width:100%;height:100%;background:repeating-linear-gradient(0deg,rgba(0,255,65,0.03) 0px,transparent 1px,transparent 2px,rgba(0,255,65,0.03) 3px);pointer-events:none;z-index:1;}");
    webpage_header += F("ul{list-style-type:none;margin:1.5em 0;padding:0;border-radius:0;overflow:hidden;background:rgba(10,14,39,0.9);border:2px solid #00ff41;box-shadow:0 0 20px rgba(0,255,65,0.5),inset 0 0 20px rgba(0,255,65,0.1);font-size:1em;position:relative;z-index:2;}");
    webpage_header += F("li{float:left;border-right:1px solid rgba(0,255,65,0.3);}li:last-child{border-right:none;}");
    webpage_header += F("li a{display:block;padding:14px 18px;text-decoration:none;color:#00ff41;font-size:14px;font-weight:700;text-transform:uppercase;letter-spacing:1px;transition:all 0.3s ease;text-shadow:0 0 10px rgba(0,255,65,0.8);}");
    webpage_header += F("li a:hover{background:rgba(0,255,65,0.2);color:#00ff41;text-shadow:0 0 20px #00ff41,0 0 30px #00ff41;transform:scale(1.05);}");
    webpage_header += F("li a.settings{color:#00d9ff;text-shadow:0 0 10px rgba(0,217,255,0.8);}");
    webpage_header += F("li a.settings:hover{background:rgba(0,217,255,0.2);color:#00d9ff;text-shadow:0 0 20px #00d9ff,0 0 30px #00d9ff;}");
    webpage_header += F("section{font-size:0.88em;position:relative;z-index:2;}");
    webpage_header += F("h1{color:#00ff41;border-radius:0;font-size:2em;padding:25px;background:rgba(10,14,39,0.95);border:3px solid #00ff41;box-shadow:0 0 30px rgba(0,255,65,0.6),inset 0 0 30px rgba(0,255,65,0.1);margin:0 0 25px 0;font-weight:700;letter-spacing:3px;text-transform:uppercase;animation:glitch 3s infinite;position:relative;z-index:2;}");
    webpage_header += F("h2{color:#ff00de;font-size:1.4em;margin:25px 0;text-shadow:0 0 10px #ff00de;text-transform:uppercase;letter-spacing:2px;}");
    webpage_header += F("h3{font-size:1.1em;color:#00d9ff;text-shadow:0 0 8px #00d9ff;text-transform:uppercase;}");
    webpage_header += F("table{font-family:'Courier New',monospace;font-size:0.9em;border-collapse:collapse;width:90%;margin:20px auto;background:rgba(10,14,39,0.9);border:2px solid #00ff41;border-radius:0;overflow:hidden;box-shadow:0 0 20px rgba(0,255,65,0.4);position:relative;z-index:2;}");
    webpage_header += F("th,td{border:1px solid rgba(0,255,65,0.3);text-align:left;padding:12px 15px;color:#00ff41;}");
    webpage_header += F("th{background:rgba(0,255,65,0.15);color:#00ff41;font-weight:700;text-transform:uppercase;letter-spacing:1px;text-shadow:0 0 10px #00ff41;}");
    webpage_header += F("tr:nth-child(even){background-color:rgba(0,255,65,0.05);}");
    webpage_header += F("tr:hover{background-color:rgba(0,255,65,0.15);box-shadow:inset 0 0 10px rgba(0,255,65,0.3);}");
    webpage_header += F(".rcorners_n{border:2px solid #ff00de;background:rgba(255,0,222,0.1);padding:10px 18px;width:20%;color:#ff00de;font-size:85%;box-shadow:0 0 15px rgba(255,0,222,0.6);transition:all 0.3s;text-transform:uppercase;}");
    webpage_header += F(".rcorners_m{border:2px solid #ff00de;background:rgba(255,0,222,0.1);padding:10px 18px;width:50%;color:#ff00de;font-size:85%;box-shadow:0 0 15px rgba(255,0,222,0.6);transition:all 0.3s;text-transform:uppercase;}");
    webpage_header += F(".rcorners_w{border:2px solid #ff00de;background:rgba(255,0,222,0.1);padding:10px 18px;width:70%;color:#ff00de;font-size:85%;box-shadow:0 0 15px rgba(255,0,222,0.6);transition:all 0.3s;text-transform:uppercase;}");
    webpage_header += F(".column{float:left;width:50%;height:45%;}");
    webpage_header += F(".row:after{content:'';display:table;clear:both;}");
    webpage_header += F("*{box-sizing:border-box;}");
    webpage_header += F("footer{background:rgba(10,14,39,0.95);color:#00ff41;text-align:center;padding:18px;border:2px solid #00ff41;border-radius:0;font-size:14px;margin-top:30px;box-shadow:0 0 25px rgba(0,255,65,0.5);font-weight:700;letter-spacing:2px;text-transform:uppercase;position:relative;z-index:2;}");
    webpage_header += F("button:not(.buttonlang):not(.buttonlang-active):not(.buttons):not(.buttonsm):not(.buttonm):not(.buttonw){border:2px solid #00ff41;background:rgba(0,255,65,0.1);padding:14px 28px;width:auto;min-width:160px;color:#00ff41;font-size:16px;cursor:pointer;box-shadow:0 0 20px rgba(0,255,65,0.6);transition:all 0.3s;font-weight:700;text-transform:uppercase;letter-spacing:2px;font-family:'Courier New',monospace;}");
    webpage_header += F("button:not(.buttonlang):not(.buttonlang-active):not(.buttons):not(.buttonsm):not(.buttonm):not(.buttonw):hover{background:rgba(0,255,65,0.2);box-shadow:0 0 35px rgba(0,255,65,0.9);transform:scale(1.05);animation:neon-pulse 1.5s infinite;}");
    webpage_header += F(".buttons{border:2px solid #00d9ff;background:rgba(0,217,255,0.1);padding:10px 20px;width:auto;min-width:120px;color:#00d9ff;font-size:14px;cursor:pointer;box-shadow:0 0 15px rgba(0,217,255,0.5);transition:all 0.3s;margin:5px;font-weight:700;text-transform:uppercase;font-family:'Courier New',monospace;}");
    webpage_header += F(".buttons:hover{background:rgba(0,217,255,0.2);box-shadow:0 0 25px rgba(0,217,255,0.8);transform:scale(1.05);}");
    webpage_header += F(".buttonsm{border:2px solid #ff00de;background:rgba(255,0,222,0.1);padding:10px 20px;width:auto;min-width:90px;color:#ff00de;font-size:13px;cursor:pointer;box-shadow:0 0 12px rgba(255,0,222,0.5);transition:all 0.3s;margin:5px;font-weight:700;text-transform:uppercase;font-family:'Courier New',monospace;}");
    webpage_header += F(".buttonsm:hover{background:rgba(255,0,222,0.2);box-shadow:0 0 25px rgba(255,0,222,0.8);transform:scale(1.05);}");
    webpage_header += F(".buttonlang{border:2px solid #ff00de;background:rgba(255,0,222,0.1);padding:6px 0;width:55px;min-width:55px;color:#ff00de;font-size:13px;cursor:pointer;box-shadow:0 0 12px rgba(255,0,222,0.5);transition:all 0.3s;margin:3px;font-weight:700;text-transform:uppercase;font-family:'Courier New',monospace;}");
    webpage_header += F(".buttonlang:hover{background:rgba(255,0,222,0.25);box-shadow:0 0 30px rgba(255,0,222,0.95);transform:scale(1.05);}");
    webpage_header += F(".buttonlang-active{border:2px solid #ff00de;background:rgba(255,0,222,0.25);padding:6px 0;width:55px;min-width:55px;color:#ff00de;font-size:13px;cursor:pointer;box-shadow:0 0 25px rgba(255,0,222,0.9);transition:all 0.3s;margin:3px;font-weight:700;text-transform:uppercase;font-family:'Courier New',monospace;}");
    webpage_header += F(".buttonlang-active:hover{background:rgba(255,0,222,0.35);box-shadow:0 0 40px rgba(255,0,222,1);transform:scale(1.05);}");
    webpage_header += F(".buttonm{border:2px solid #00ff41;background:rgba(0,255,65,0.1);padding:10px 18px;width:auto;min-width:100px;color:#00ff41;font-size:13px;cursor:pointer;box-shadow:0 0 15px rgba(0,255,65,0.5);transition:all 0.3s;margin:5px;font-weight:700;text-transform:uppercase;font-family:'Courier New',monospace;}");
    webpage_header += F(".buttonw{border:2px solid #00ff41;background:rgba(0,255,65,0.1);padding:10px 20px;width:auto;min-width:200px;color:#00ff41;font-size:13px;cursor:pointer;box-shadow:0 0 15px rgba(0,255,65,0.5);transition:all 0.3s;margin:5px;font-weight:700;text-transform:uppercase;font-family:'Courier New',monospace;}");
    webpage_header += F("a{color:#00d9ff;text-decoration:none;transition:all 0.3s;text-shadow:0 0 8px rgba(0,217,255,0.6);}");
    webpage_header += F("a:hover{color:#00ff41;text-shadow:0 0 15px #00ff41;}");
    webpage_header += F("a:hover .buttonlang,a:hover .buttonlang-active{color:#ff00de;text-shadow:none;}");
    webpage_header += F("p{font-size:90%;color:#00ff41;text-shadow:0 0 5px rgba(0,255,65,0.5);}");

    webpage_header += F("</style></head><body>");

    String langParam = getLangParam();
    
    webpage_header += F("<h1>🎮 HOLOCUBIC_AIO ");
    webpage_header += F(AIO_VERSION "</h1>");
    webpage_header += F("<ul>");
    
    webpage_header += F("<li><a href='/");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("home");
    webpage_header += F("</a></li>");

    webpage_header += F("<li><a href='/download");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("download");
    webpage_header += F("</a></li>");

    webpage_header += F("<li><a href='/upload");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("upload");
    webpage_header += F("</a></li>");

    webpage_header += F("<li><a href='/delete");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("delete");
    webpage_header += F("</a></li>");

    webpage_header += F("<li><a href='/sys_setting");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("system");
    webpage_header += F("</a></li>");

    webpage_header += F("<li><a href='/rgb_setting");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("rgb");
    webpage_header += F("</a></li>");

#if APP_WEATHER_USE
    webpage_header += F("<li><a class='settings' href='/weather_setting");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("weather");
    webpage_header += F("</a></li>");
#endif
#if APP_WEATHER_OLD_USE
    webpage_header += F("<li><a class='settings' href='/weather_old_setting");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("weather_old");
    webpage_header += F("</a></li>");
#endif
#if APP_BILIBILI_FANS_USE
    webpage_header += F("<li><a class='settings' href='/bili_setting");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("bilibili");
    webpage_header += F("</a></li>");
#endif
#if APP_PICTURE_USE
    webpage_header += F("<li><a class='settings' href='/picture_setting");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("picture");
    webpage_header += F("</a></li>");
#endif
#if APP_MEDIA_PLAYER_USE
    webpage_header += F("<li><a class='settings' href='/media_setting");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("media");
    webpage_header += F("</a></li>");
#endif
#if APP_SCREEN_SHARE_USE
    webpage_header += F("<li><a class='settings' href='/screen_setting");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("screen");
    webpage_header += F("</a></li>");
#endif
#if APP_HEARTBEAT_USE
    webpage_header += F("<li><a class='settings' href='/heartbeat_setting");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("heartbeat");
    webpage_header += F("</a></li>");
#endif
#if APP_ANNIVERSARY_USE
    webpage_header += F("<li><a class='settings' href='/anniversary_setting");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("anniversary");
    webpage_header += F("</a></li>");
#endif
#if APP_STOCK_MARKET_USE
    webpage_header += F("<li><a class='settings' href='/stock_setting");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("stock");
    webpage_header += F("</a></li>");
#endif
#if APP_PC_RESOURCE_USE
    webpage_header += F("<li><a class='settings' href='/pc_resource_setting");
    webpage_header += langParam;
    webpage_header += F("'>");
    webpage_header += getText("pc_monitor");
    webpage_header += F("</a></li>");
#endif
    webpage_header += F("</ul>");
}

void init_page_footer()
{
    webpage_footer = F("<footer>&copy; ClimbSnail 2021 + asdfghj1237890 2025</footer>");
    webpage_footer += F("</body></html>");
}

// Language texts
const char* getText(const char* key) {
    // Home page
    if (strcmp(key, "quick_links") == 0) {
        if (current_lang == LANG_ZH_CN) return "快速链接";
        if (current_lang == LANG_ZH_TW) return "快速連結";
        return "Quick Links";
    }
    if (strcmp(key, "tutorial") == 0) {
        if (current_lang == LANG_ZH_CN) return "教程";
        if (current_lang == LANG_ZH_TW) return "教學";
        return "Tutorial";
    }
    if (strcmp(key, "original") == 0) {
        if (current_lang == LANG_ZH_CN) return "原始仓库";
        if (current_lang == LANG_ZH_TW) return "原始倉庫";
        return "Original";
    }
    if (strcmp(key, "custom") == 0) {
        if (current_lang == LANG_ZH_CN) return "自定义版本";
        if (current_lang == LANG_ZH_TW) return "自訂版本";
        return "Custom";
    }
    
    // Navigation menu
    if (strcmp(key, "home") == 0) {
        if (current_lang == LANG_ZH_CN) return "首页";
        if (current_lang == LANG_ZH_TW) return "首頁";
        return "Home";
    }
    if (strcmp(key, "download") == 0) {
        if (current_lang == LANG_ZH_CN) return "下载";
        if (current_lang == LANG_ZH_TW) return "下載";
        return "Download";
    }
    if (strcmp(key, "upload") == 0) {
        if (current_lang == LANG_ZH_CN) return "上传";
        if (current_lang == LANG_ZH_TW) return "上傳";
        return "Upload";
    }
    if (strcmp(key, "delete") == 0) {
        if (current_lang == LANG_ZH_CN) return "删除";
        if (current_lang == LANG_ZH_TW) return "刪除";
        return "Delete";
    }
    if (strcmp(key, "system") == 0) {
        if (current_lang == LANG_ZH_CN) return "系统设置";
        if (current_lang == LANG_ZH_TW) return "系統設定";
        return "System";
    }
    if (strcmp(key, "rgb") == 0) {
        if (current_lang == LANG_ZH_CN) return "RGB设置";
        if (current_lang == LANG_ZH_TW) return "RGB設定";
        return "RGB";
    }
    if (strcmp(key, "weather") == 0) {
        if (current_lang == LANG_ZH_CN) return "天气";
        if (current_lang == LANG_ZH_TW) return "天氣";
        return "Weather";
    }
    if (strcmp(key, "weather_old") == 0) {
        if (current_lang == LANG_ZH_CN) return "旧版天气";
        if (current_lang == LANG_ZH_TW) return "舊版天氣";
        return "Weather Old";
    }
    if (strcmp(key, "bilibili") == 0) {
        if (current_lang == LANG_ZH_CN) return "B站";
        if (current_lang == LANG_ZH_TW) return "B站";
        return "Bilibili";
    }
    if (strcmp(key, "picture") == 0) {
        if (current_lang == LANG_ZH_CN) return "相册";
        if (current_lang == LANG_ZH_TW) return "相簿";
        return "Picture";
    }
    if (strcmp(key, "media") == 0) {
        if (current_lang == LANG_ZH_CN) return "媒体播放器";
        if (current_lang == LANG_ZH_TW) return "媒體播放器";
        return "Media";
    }
    if (strcmp(key, "screen") == 0) {
        if (current_lang == LANG_ZH_CN) return "屏幕分享";
        if (current_lang == LANG_ZH_TW) return "螢幕分享";
        return "Screen Share";
    }
    if (strcmp(key, "heartbeat") == 0) {
        if (current_lang == LANG_ZH_CN) return "心跳";
        if (current_lang == LANG_ZH_TW) return "心跳";
        return "Heartbeat";
    }
    if (strcmp(key, "anniversary") == 0) {
        if (current_lang == LANG_ZH_CN) return "纪念日";
        if (current_lang == LANG_ZH_TW) return "紀念日";
        return "Anniversary";
    }
    if (strcmp(key, "stock") == 0) {
        if (current_lang == LANG_ZH_CN) return "股票行情";
        if (current_lang == LANG_ZH_TW) return "股票行情";
        return "Stock";
    }
    if (strcmp(key, "pc_monitor") == 0) {
        if (current_lang == LANG_ZH_CN) return "PC资源监控";
        if (current_lang == LANG_ZH_TW) return "PC資源監控";
        return "PC Monitor";
    }
    
    // Common words
    if (strcmp(key, "save") == 0) {
        if (current_lang == LANG_ZH_CN) return "保存";
        if (current_lang == LANG_ZH_TW) return "儲存";
        return "Save";
    }
    if (strcmp(key, "back") == 0) {
        if (current_lang == LANG_ZH_CN) return "返回";
        if (current_lang == LANG_ZH_TW) return "返回";
        return "Back";
    }
    
    return "";
}

void setLanguage() {
    if (server.hasArg("lang")) {
        String lang = server.arg("lang");
        if (lang == "en") current_lang = LANG_EN_US;
        else if (lang == "cn") current_lang = LANG_ZH_CN;
        else if (lang == "tw") current_lang = LANG_ZH_TW;
    }
    server.sendHeader("Location", "/");
    server.send(302);
}

// All supporting functions from here...
void HomePage()
{
    if (server.hasArg("lang")) {
        String lang = server.arg("lang");
        if (lang == "en") current_lang = LANG_EN_US;
        else if (lang == "cn") current_lang = LANG_ZH_CN;
        else if (lang == "tw") current_lang = LANG_ZH_TW;
    }
    
    webpage = F("<div style='text-align:right;margin:20px;'>");
    
    webpage += F("<a href='/?lang=en'><button class='");
    webpage += (current_lang == LANG_EN_US) ? F("buttonlang-active") : F("buttonlang");
    webpage += F("'>EN</button></a>");
    
    webpage += F("<a href='/?lang=cn'><button class='");
    webpage += (current_lang == LANG_ZH_CN) ? F("buttonlang-active") : F("buttonlang");
    webpage += F("'>简</button></a>");
    
    webpage += F("<a href='/?lang=tw'><button class='");
    webpage += (current_lang == LANG_ZH_TW) ? F("buttonlang-active") : F("buttonlang");
    webpage += F("'>繁</button></a>");
    
    webpage += F("</div>");
    
    webpage += F("<table style='width:70%;margin:50px auto;'>");
    webpage += F("<tr><th colspan='2' style='text-align:center;font-size:18px;padding:15px;'>");
    webpage += getText("quick_links");
    webpage += F("</th></tr>");
    
    webpage += F("<tr><td style='width:30%;text-align:left;padding-left:30px;'>GitHub</td><td style='text-align:left;padding-left:30px;'>");
    webpage += F("<a href='https://github.com/ClimbSnail/HoloCubic_AIO' target='_blank'><button class='buttonsm'>");
    webpage += getText("original");
    webpage += F("</button></a>");
    webpage += F("<a href='https://github.com/asdfghj1237890/HoloCubic_AIO' target='_blank'><button class='buttonsm'>");
    webpage += getText("custom");
    webpage += F("</button></a></td></tr>");
    
    webpage += F("<tr><td style='text-align:left;padding-left:30px;'>BiliBili</td><td style='text-align:left;padding-left:30px;'>");
    webpage += F("<a href='https://space.bilibili.com/344470052' target='_blank'><button class='buttonsm'>");
    webpage += getText("tutorial");
    webpage += F("</button></a></td></tr>");
    
    webpage += F("</table>");
    Send_HTML(webpage);
}

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

void saveSysConf(void)
{
    Send_HTML(F("<h1>设置成功! 退出APP或者继续其他设置.</h1>"));

    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"ssid_0",
                            (void *)server.arg("ssid_0").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"password_0",
                            (void *)server.arg("password_0").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"power_mode",
                            (void *)server.arg("power_mode").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"backLight",
                            (void *)server.arg("backLight").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"rotation",
                            (void *)server.arg("rotation").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"mpu_order",
                            (void *)server.arg("mpu_order").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"auto_calibration_mpu",
                            (void *)server.arg("auto_calibration_mpu").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"auto_start_app",
                            (void *)server.arg("auto_start_app").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
}

void saveRgbConf(void)
{
    Send_HTML(F("<h1>设置成功! 退出APP或者继续其他设置.</h1>"));

    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"min_brightness",
                            (void *)server.arg("min_brightness").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"max_brightness",
                            (void *)server.arg("max_brightness").c_str());
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"time",
                            (void *)server.arg("time").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "AppCtrl", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
}

void saveWeatherConf(void)
{
    Send_HTML(F("<h1>設置成功! 退出APP或者繼續其他設置.</h1>"));

    app_controller->send_to(SERVER_APP_NAME, "Weather",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"api_key",
                            (void *)server.arg("api_key").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"city_name",
                            (void *)server.arg("city_name").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"weatherUpdataInterval",
                            (void *)server.arg("weatherUpdataInterval").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"timeUpdataInterval",
                            (void *)server.arg("timeUpdataInterval").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"language",
                            (void *)server.arg("language").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Weather", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
}

void saveWeatherOldConf(void)
{
    Send_HTML(F("<h1>设置成功! 退出APP或者继续其他设置.</h1>"));

    app_controller->send_to(SERVER_APP_NAME, "Weather Old",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"cityname",
                            (void *)server.arg("cityname").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather Old",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"language",
                            (void *)server.arg("language").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather Old",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"weather_key",
                            (void *)server.arg("weather_key").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather Old",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"weatherUpdataInterval",
                            (void *)server.arg("weatherUpdataInterval").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Weather Old",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"timeUpdataInterval",
                            (void *)server.arg("timeUpdataInterval").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Weather Old", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
}

void saveBiliConf(void)
{
    Send_HTML(F("<h1>设置成功! 退出APP或者继续其他设置.</h1>"));
    app_controller->send_to(SERVER_APP_NAME, "Bili",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"bili_uid",
                            (void *)server.arg("bili_uid").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Bili",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"updataInterval",
                            (void *)server.arg("updataInterval").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Bili", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
}

void saveStockConf(void)
{
    Send_HTML(F("<h1>Configuration saved successfully! Exit APP or continue other settings.</h1>"));
    app_controller->send_to(SERVER_APP_NAME, "Stock",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"stock_symbol",
                            (void *)server.arg("stock_symbol").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Stock",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"market_type",
                            (void *)server.arg("market_type").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Stock",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"updataInterval",
                            (void *)server.arg("updataInterval").c_str());
    // Persist data to flash
    app_controller->send_to(SERVER_APP_NAME, "Stock", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
}

void savePictureConf(void)
{
    Send_HTML(F("<h1>设置成功! 退出APP或者继续其他设置.</h1>"));
    app_controller->send_to(SERVER_APP_NAME, "Picture",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"switchInterval",
                            (void *)server.arg("switchInterval").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Picture", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
}

void saveMediaConf(void)
{
    Send_HTML(F("<h1>设置成功! 退出APP或者继续其他设置.</h1>"));
    app_controller->send_to(SERVER_APP_NAME, "Media",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"switchFlag",
                            (void *)server.arg("switchFlag").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Media",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"powerFlag",
                            (void *)server.arg("powerFlag").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Media", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
}

void saveScreenConf(void)
{
    Send_HTML(F("<h1>设置成功! 退出APP或者继续其他设置.</h1>"));
    app_controller->send_to(SERVER_APP_NAME, "Screen share",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"powerFlag",
                            (void *)server.arg("powerFlag").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Screen share", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
}

void saveHeartbeatConf(void)
{
    Send_HTML(F("<h1>设置成功! 退出APP或者继续其他设置.</h1>"));
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"role",
                            (void *)server.arg("role").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"qq_num",
                            (void *)server.arg("qq_num").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"mqtt_server",
                            (void *)server.arg("mqtt_server").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"mqtt_port",
                            (void *)server.arg("mqtt_port").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"mqtt_user",
                            (void *)server.arg("mqtt_user").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"mqtt_password",
                            (void *)server.arg("mqtt_password").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Heartbeat", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
}

void saveAnniversaryConf(void)
{
    Send_HTML(F("<h1>设置成功! 退出APP或者继续其他设置.</h1>"));
    app_controller->send_to(SERVER_APP_NAME, "Anniversary",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"event_name0",
                            (void *)server.arg("event_name0").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Anniversary",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"target_date0",
                            (void *)server.arg("target_date0").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Anniversary",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"event_name1",
                            (void *)server.arg("event_name1").c_str());
    app_controller->send_to(SERVER_APP_NAME, "Anniversary",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"target_date1",
                            (void *)server.arg("target_date1").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "Anniversary", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
}

void savePCResourceConf(void)
{
    Send_HTML(F("<h1>设置成功! 退出APP或者继续其他设置.</h1>"));
    app_controller->send_to(SERVER_APP_NAME, "PC Resource",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"pc_ipaddr",
                            (void *)server.arg("pc_ipaddr").c_str());
    app_controller->send_to(SERVER_APP_NAME, "PC Resource",
                            APP_MESSAGE_SET_PARAM,
                            (void *)"sensorUpdataInterval",
                            (void *)server.arg("sensorUpdataInterval").c_str());
    // 持久化数据
    app_controller->send_to(SERVER_APP_NAME, "PC Resource", APP_MESSAGE_WRITE_CFG,
                            NULL, NULL);
}

void File_Delete()
{
    Send_HTML(
        F("<h3>Enter filename to delete</h3>"
          "<form action='/delete_result' method='post'>"
          "<input type='text' name='delete_filepath' placeHolder='绝对路径 /image/...'><br>"
          "</label><input class=\"btn\" type=\"submit\" name=\"Submie\" value=\"确认删除\"></form>"
          "<a href='/'>[Back]</a>"));
}

void delete_result(void)
{
    String del_file = server.arg("delete_filepath");
    boolean ret = tf.deleteFile(del_file);
    if (ret)
    {
        webpage = "<h3>Delete succ!</h3><a href='/delete'>[Back]</a>";
        tf.listDir("/image", 250);
    }
    else
    {
        webpage = "<h3>Delete fail! Please check up file path.</h3><a href='/delete'>[Back]</a>";
    }
    tf.listDir("/image", 250);
    Send_HTML(webpage);
}

void File_Download()
{ // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments
    if (server.hasArg("download"))
    { // Download argument was received
        sd_file_download(server.arg("download"));
    }
    else
    {
        SelectInput("Enter filename to download", "download", "download");
    }
}

void sd_file_download(const String &filename)
{
    if (sd_present)
    {
        File download = tf.open("/" + filename);
        if (download)
        {
            server.sendHeader("Content-Type", "text/text");
            server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
            server.sendHeader("Connection", "close");
            server.streamFile(download, "application/octet-stream");
            download.close();
        }
        else
            ReportFileNotPresent(String("download"));
    }
    else
        ReportSDNotPresent();
}

void File_Upload()
{
    tf.listDir("/image", 250);

    webpage = webpage_header;
    webpage += "<h3>Select File to Upload</h3>"
               "<FORM action='/fupload' method='post' enctype='multipart/form-data'>"
               "<input class='buttons' style='width:40%' type='file' name='fupload' id = 'fupload' value=''><br>"
               "<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>"
               "<a href='/'>[Back]</a><br><br>";
    webpage += webpage_footer;
    server.send(200, "text/html", webpage);
}

File UploadFile;
void handleFileUpload()
{                                                   // upload a new file to the Filing system
    HTTPUpload &uploadFileStream = server.upload(); // See https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer/srcv
                                                    // For further information on 'status' structure, there are other reasons such as a failed transfer that could be used
    String filename = uploadFileStream.filename;
    if (uploadFileStream.status == UPLOAD_FILE_START)
    {
        // String filename = uploadFileStream.filename;
        // if (!filename.startsWith("/image"))
        filename = "/image/" + filename;
        Serial.print(F("Upload File Name: "));
        Serial.println(filename);
        tf.deleteFile(filename);                    // Remove a previous version, otherwise data is appended the file again
        UploadFile = tf.open(filename, FILE_WRITE); // Open the file for writing in SPIFFS (create it, if doesn't exist)
    }
    else if (uploadFileStream.status == UPLOAD_FILE_WRITE)
    {
        if (UploadFile)
            UploadFile.write(uploadFileStream.buf, uploadFileStream.currentSize); // Write the received bytes to the file
    }
    else if (uploadFileStream.status == UPLOAD_FILE_END)
    {
        if (UploadFile) // If the file was successfully created
        {
            UploadFile.close(); // Close the file again
            Serial.print(F("Upload Size: "));
            Serial.println(uploadFileStream.totalSize);
            webpage = webpage_header;
            webpage += F("<h3>File was successfully uploaded</h3>");
            webpage += F("<h2>Uploaded File Name: ");
            webpage += filename + "</h2>";
            webpage += F("<h2>File Size: ");
            webpage += file_size(uploadFileStream.totalSize) + "</h2><br>";
            webpage += webpage_footer;
            server.send(200, "text/html", webpage);
            tf.listDir("/image", 250);
        }
        else
        {
            ReportCouldNotCreateFile(String("upload"));
        }
    }
}

void SelectInput(String heading, String command, String arg_calling_name)
{
    webpage = F("<h3>");
    webpage += heading + "</h3>";
    webpage += F("<FORM action='/");
    webpage += command + "' method='post'>"; // Must match the calling argument e.g. '/chart' calls '/chart' after selection but with arguments!
    webpage += F("<input type='text' name='");
    webpage += arg_calling_name;
    webpage += F("' value=''><br>");
    webpage += F("<type='submit' name='");
    webpage += arg_calling_name;
    webpage += F("' value=''><br>");
    webpage += F("<a href='/'>[Back]</a>");
    Send_HTML(webpage);
}

void ReportSDNotPresent()
{
    webpage = F("<h3>No SD Card present</h3>");
    webpage += F("<a href='/'>[Back]</a><br><br>");
    Send_HTML(webpage);
}

void ReportFileNotPresent(const String &target)
{
    webpage = F("<h3>File does not exist</h3>");
    webpage += F("<a href='/");
    webpage += target + "'>[Back]</a><br><br>";
    Send_HTML(webpage);
}

void ReportCouldNotCreateFile(const String &target)
{
    webpage = F("<h3>Could Not Create Uploaded File (write-protected?)</h3>");
    webpage += F("<a href='/");
    webpage += target + "'>[Back]</a><br><br>";
    Send_HTML(webpage);
}