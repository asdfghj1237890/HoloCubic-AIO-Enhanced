
// 参考代码 https://github.com/G6EJD/ESP32-8266-File-Upload

#include "network.h"
#include "common.h"
#include "server.h"
#include "web_setting.h"
#include "web_setting_internal.h"
#include "app/app_conf.h"
#include "FS.h"
#include "HardwareSerial.h"
#include <esp32-hal.h>

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
#define SETING_CSS ".input{display:block;margin:18px 0;background:rgba(10,14,39,0.8);padding:15px;border:2px solid #00ff41;box-shadow:0 0 15px rgba(0,255,65,0.3),inset 0 0 10px rgba(0,255,65,0.05);transition:all 0.3s;position:relative;z-index:2;}" \
                   ".input:hover{box-shadow:0 0 25px rgba(0,255,65,0.6),inset 0 0 15px rgba(0,255,65,0.1);border-color:#00ff41;}" \
                   ".input span{width:320px;float:left;height:42px;line-height:42px;color:#00ff41;font-weight:700;font-size:15px;text-transform:uppercase;letter-spacing:1px;text-shadow:0 0 8px rgba(0,255,65,0.6);font-family:'Courier New',monospace;}" \
                   ".input input[type='text']{height:40px;width:240px;border:2px solid #00d9ff;background:rgba(0,217,255,0.05);color:#00d9ff;padding:0 12px;font-size:14px;font-family:'Courier New',monospace;transition:all 0.3s;}" \
                   ".input input[type='text']:focus{outline:none;border-color:#00ff41;background:rgba(0,255,65,0.1);box-shadow:0 0 20px rgba(0,255,65,0.6);color:#00ff41;}" \
                   ".input .radio{height:18px;width:18px;margin:0 10px;cursor:pointer;accent-color:#00ff41;filter:drop-shadow(0 0 5px #00ff41);}" \
                   ".btn{min-width:160px;height:45px;background:rgba(0,255,65,0.1);border:2px solid #00ff41;color:#00ff41;font-size:16px;font-weight:700;cursor:pointer;margin-top:25px;box-shadow:0 0 20px rgba(0,255,65,0.6);transition:all 0.3s;text-transform:uppercase;letter-spacing:2px;font-family:'Courier New',monospace;}" \
                   ".btn:hover{background:rgba(0,255,65,0.2);box-shadow:0 0 35px rgba(0,255,65,0.9);transform:scale(1.05);}" \
                   "form{background:rgba(10,14,39,0.95);padding:30px;border:3px solid #00ff41;box-shadow:0 0 30px rgba(0,255,65,0.5),inset 0 0 30px rgba(0,255,65,0.05);margin:25px auto;max-width:850px;position:relative;z-index:2;}"
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

