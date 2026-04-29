
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
// Glass UI CSS (variant C, from claude.ai/design handoff). Kept as one
// big PROGMEM string so init_page_header() can append it in one shot
// instead of 250 individual webpage_header += F("...") calls.
static const char GLASS_CSS[] PROGMEM = R"GLASS(
*{box-sizing:border-box;}
:root{
  --accent:#f5c542; --accent-soft:rgba(245,197,66,0.14); --accent-strong:#e0ad1a;
  --bg:#0a0d14; --bg-2:rgba(20,24,33,0.65); --bg-3:rgba(28,33,44,0.6);
  --panel:rgba(22,26,36,0.55); --panel-strong:rgba(28,33,44,0.85);
  --border:rgba(255,255,255,0.06); --border-strong:rgba(255,255,255,0.12);
  --fg:#e8eaef; --fg-2:#a8aebb; --fg-3:#6c7280;
  --good:#4ade80; --warn:#f59e0b; --bad:#ef4444;
}
body.light{
  --bg:#f5f1e8; --bg-2:rgba(255,255,255,0.65); --bg-3:rgba(255,255,255,0.5);
  --panel:rgba(255,255,255,0.65); --panel-strong:rgba(255,255,255,0.9);
  --border:rgba(0,0,0,0.06); --border-strong:rgba(0,0,0,0.12);
  --fg:#1a1a1a; --fg-2:#5b5b5b; --fg-3:#8a8a8a;
}
html,body{margin:0;height:100%;font-family:'Inter','Noto Sans TC','Noto Sans SC',-apple-system,system-ui,sans-serif;color:var(--fg);background:var(--bg);}
body{overflow:hidden;}
body::before{content:'';position:fixed;inset:0;background:radial-gradient(ellipse 700px 400px at 15% 10%,rgba(245,197,66,0.18) 0%,transparent 55%),radial-gradient(ellipse 600px 500px at 90% 90%,rgba(245,197,66,0.10) 0%,transparent 55%),radial-gradient(ellipse 400px 300px at 60% 40%,rgba(120,80,240,0.06) 0%,transparent 60%);pointer-events:none;z-index:0;}
.grain{position:fixed;inset:0;background-image:repeating-linear-gradient(0deg,transparent 0,transparent 2px,rgba(255,255,255,0.012) 3px),repeating-linear-gradient(90deg,transparent 0,transparent 2px,rgba(255,255,255,0.012) 3px);pointer-events:none;z-index:0;}
.app{position:relative;z-index:1;display:grid;grid-template-columns:240px 1fr;height:100vh;width:100vw;}
.sidebar{background:var(--bg-2);backdrop-filter:blur(24px) saturate(140%);-webkit-backdrop-filter:blur(24px) saturate(140%);border-right:1px solid var(--border);display:flex;flex-direction:column;min-height:0;}
.brand{padding:20px 18px 16px;display:flex;align-items:center;gap:10px;border-bottom:1px solid var(--border);}
.brand-mark{width:30px;height:30px;border-radius:8px;background:linear-gradient(135deg,var(--accent),var(--accent-strong));display:grid;place-items:center;color:#1a1a1a;font-weight:800;font-family:'JetBrains Mono',monospace;font-size:14px;box-shadow:0 0 20px var(--accent-soft);}
.brand-name{font-weight:700;letter-spacing:0.02em;font-size:14px;}
.brand-version{margin-left:auto;font-family:'JetBrains Mono',monospace;font-size:11px;color:var(--fg-3);}
.nav-group{padding:4px 8px;flex:1;overflow-y:auto;}
.nav-label{font-size:10px;font-weight:600;text-transform:uppercase;letter-spacing:0.1em;color:var(--fg-3);padding:12px 10px 6px;}
.nav-item{display:flex;align-items:center;gap:10px;padding:8px 10px;border-radius:8px;color:var(--fg-2);cursor:pointer;font-size:13px;font-weight:500;text-decoration:none;transition:all .15s;border:1px solid transparent;}
.nav-item:hover{background:var(--bg-3);color:var(--fg);}
.nav-item.active{background:linear-gradient(90deg,var(--accent-soft),transparent);color:var(--accent);border-color:rgba(245,197,66,0.3);box-shadow:0 0 16px var(--accent-soft);}
.sidebar-foot{padding:12px 14px;border-top:1px solid var(--border);display:flex;align-items:center;gap:10px;}
.lang-pills{display:inline-flex;background:var(--bg-3);border:1px solid var(--border);border-radius:7px;overflow:hidden;font-family:'JetBrains Mono',monospace;font-size:11px;}
.lang-pills a{background:transparent;border:0;padding:4px 9px;color:var(--fg-2);cursor:pointer;text-decoration:none;}
.lang-pills a.on{background:var(--accent);color:#1a1a1a;font-weight:700;}
.theme-toggle{margin-left:auto;width:34px;height:22px;background:var(--bg-3);border:1px solid var(--border);border-radius:12px;position:relative;cursor:pointer;}
.theme-toggle::after{content:'';position:absolute;width:16px;height:16px;border-radius:50%;background:var(--accent);top:2px;left:2px;transition:left .2s ease;}
body.light .theme-toggle::after{left:14px;}
.main{display:flex;flex-direction:column;min-width:0;min-height:0;}
.topbar{height:56px;border-bottom:1px solid var(--border);display:flex;align-items:center;padding:0 22px;gap:14px;}
.crumbs{color:var(--fg-3);font-size:12px;font-family:'JetBrains Mono',monospace;}
.crumbs strong{color:var(--fg);font-weight:600;}
.status-cluster{margin-left:auto;display:flex;align-items:center;gap:8px;font-size:12px;color:var(--fg-2);}
.pill{display:inline-flex;align-items:center;gap:6px;background:var(--panel);backdrop-filter:blur(12px);border:1px solid var(--border);padding:5px 10px;border-radius:999px;font-family:'JetBrains Mono',monospace;font-size:11px;}
.dot{width:7px;height:7px;border-radius:50%;background:var(--good);box-shadow:0 0 8px var(--good);}
.content{flex:1;overflow-y:auto;padding:28px 32px 40px;min-height:0;}
.page-head{margin-bottom:22px;display:flex;align-items:end;gap:16px;}
.page-title{font-size:28px;font-weight:700;letter-spacing:-0.02em;margin:0;color:var(--fg);}
.page-sub{margin:4px 0 0;color:var(--fg-2);font-size:13px;}
.card{background:var(--panel);backdrop-filter:blur(20px) saturate(140%);-webkit-backdrop-filter:blur(20px) saturate(140%);border:1px solid var(--border);border-radius:14px;margin-bottom:18px;overflow:hidden;box-shadow:0 1px 0 rgba(255,255,255,0.04) inset,0 24px 48px -24px rgba(0,0,0,0.5);position:relative;}
.card::before{content:'';position:absolute;top:0;left:0;right:0;height:1px;background:linear-gradient(90deg,transparent,var(--border-strong),transparent);opacity:0.7;}
.card-head{padding:16px 20px 14px;border-bottom:1px solid var(--border);display:flex;align-items:center;gap:12px;}
.card-title{font-weight:600;font-size:14px;}
.card-sub{color:var(--fg-3);font-size:12px;margin-top:2px;}
.card-body{padding:4px 0;}
.field{display:grid;grid-template-columns:220px 1fr 22px;align-items:center;gap:16px;padding:13px 20px;border-bottom:1px solid var(--border);}
.field:last-child{border-bottom:none;}
.field label{font-weight:500;font-size:13px;color:var(--fg);}
.field .hint{display:block;font-size:11px;color:var(--fg-3);margin-top:2px;font-family:'JetBrains Mono',monospace;font-weight:400;}
.field input[type=text],.field input[type=password],.field input[type=number],.field select{width:100%;height:36px;background:var(--bg-3);border:1px solid var(--border);border-radius:8px;padding:0 12px;color:var(--fg);font-family:inherit;font-size:13px;outline:none;transition:all .15s;}
.field input:focus,.field select:focus{border-color:var(--accent);box-shadow:0 0 0 3px var(--accent-soft);}
.field input.mono{font-family:'JetBrains Mono',monospace;}
.secret-wrap{position:relative;}
.secret-wrap input{padding-right:38px !important;}
.eye-btn{position:absolute;right:6px;top:50%;transform:translateY(-50%);width:28px;height:28px;border:0;background:transparent;color:var(--fg-3);cursor:pointer;border-radius:6px;font-size:14px;}
.eye-btn:hover{color:var(--accent);background:var(--accent-soft);}
.tip{display:inline-grid;place-items:center;width:18px;height:18px;border-radius:50%;border:1px solid var(--border-strong);color:var(--fg-3);font-size:10px;font-weight:600;cursor:help;position:relative;}
.tip:hover{color:var(--accent);border-color:var(--accent);}
.tip:hover::after{content:attr(data-tip);position:absolute;right:calc(100% + 8px);top:50%;transform:translateY(-50%);white-space:nowrap;background:var(--panel-strong);backdrop-filter:blur(12px);border:1px solid var(--border-strong);color:var(--fg);font-weight:400;font-size:11px;padding:6px 10px;border-radius:6px;z-index:10;box-shadow:0 8px 24px rgba(0,0,0,0.4);}
.row-actions{padding:16px 20px;border-top:1px solid var(--border);display:flex;align-items:center;gap:10px;background:var(--bg-3);}
.btn{height:36px;padding:0 18px;border-radius:8px;border:1px solid var(--border-strong);background:var(--bg-3);color:var(--fg);font:inherit;font-size:13px;font-weight:500;cursor:pointer;display:inline-flex;align-items:center;gap:6px;text-decoration:none;transition:all .15s;}
.btn:hover{border-color:var(--accent);color:var(--accent);}
.btn.primary{background:linear-gradient(180deg,var(--accent),var(--accent-strong));color:#1a1a1a;border-color:var(--accent);box-shadow:0 0 20px var(--accent-soft),inset 0 1px 0 rgba(255,255,255,0.3);font-weight:600;}
.btn.primary:hover{box-shadow:0 0 28px rgba(245,197,66,0.5),inset 0 1px 0 rgba(255,255,255,0.3);}
.btn.ghost{background:transparent;border-color:transparent;color:var(--fg-2);}
.btn.ghost:hover{color:var(--fg);background:var(--bg-3);border-color:var(--border);}
.hero{background:linear-gradient(135deg,var(--panel) 0%,rgba(245,197,66,0.08) 100%);backdrop-filter:blur(24px);border:1px solid var(--border-strong);border-radius:16px;padding:24px;margin-bottom:18px;position:relative;overflow:hidden;}
.hero::after{content:'';position:absolute;top:-50%;right:-20%;width:400px;height:400px;background:radial-gradient(circle,var(--accent-soft) 0%,transparent 60%);pointer-events:none;}
.hero h2{margin:0 0 4px;font-size:18px;font-weight:600;display:flex;align-items:center;gap:10px;}
.hero .status-line{color:var(--fg-2);font-size:13px;}
.hero .kpi-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:14px;margin-top:18px;position:relative;z-index:1;}
.hero .kpi .label{font-size:10.5px;color:var(--fg-3);text-transform:uppercase;letter-spacing:0.08em;font-weight:600;}
.hero .kpi .value{font-family:'JetBrains Mono',monospace;font-size:26px;font-weight:600;margin-top:6px;color:var(--fg);letter-spacing:-0.02em;}
.hero .kpi .value .unit{font-size:12px;color:var(--fg-3);margin-left:4px;font-weight:400;}
.hero .kpi .bar{height:4px;border-radius:2px;background:var(--bg-3);margin-top:10px;overflow:hidden;}
.hero .kpi .bar i{display:block;height:100%;background:linear-gradient(90deg,var(--accent-strong),var(--accent));border-radius:2px;}
.toast{position:fixed;bottom:24px;right:24px;background:var(--panel-strong);backdrop-filter:blur(20px);border:1px solid var(--border-strong);border-left:3px solid var(--good);border-radius:10px;padding:12px 16px;display:flex;gap:10px;align-items:center;box-shadow:0 16px 40px rgba(0,0,0,0.4);font-size:13px;z-index:100;animation:toast-in .25s ease;}
@keyframes toast-in{from{transform:translateY(8px);opacity:0;}to{transform:translateY(0);opacity:1;}}
.toast .check{width:18px;height:18px;border-radius:50%;background:var(--good);display:grid;place-items:center;color:#fff;font-size:11px;font-weight:700;}
.quick-card a{display:block;padding:14px 20px;border-bottom:1px solid var(--border);color:var(--fg);text-decoration:none;font-size:13px;}
.quick-card a:hover{background:var(--bg-3);color:var(--accent);}
.quick-card a:last-child{border-bottom:none;}
.scan-list{padding:4px 0;}
.scan-row{display:grid;grid-template-columns:1fr auto auto auto;gap:14px;align-items:center;padding:11px 20px;border-bottom:1px solid var(--border);font-size:13px;cursor:pointer;}
.scan-row:last-child{border-bottom:none;}
.scan-row:hover{background:var(--bg-3);}
.scan-row.current{background:linear-gradient(90deg,var(--accent-soft),transparent);color:var(--accent);}
.scan-row .meta{font-family:'JetBrains Mono',monospace;font-size:11px;color:var(--fg-3);}
.scan-row .dot{width:7px;height:7px;border-radius:50%;background:var(--accent);box-shadow:0 0 8px var(--accent);}
.scan-empty{padding:20px;color:var(--fg-3);text-align:center;font-size:13px;}
)GLASS";
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
    webpage_header += F("<head><meta charset=\"UTF-8\"><title>HoloCubic WebServer</title>");
    webpage_header += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
    // Google Fonts (Inter / JetBrains Mono / Noto Sans TC+SC). The
    // device serves these via the user's WiFi route to googleapis; on
    // captive AP-mode boots they fall back to system fonts gracefully.
    webpage_header += F("<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\"><link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>");
    webpage_header += F("<link href=\"https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&family=JetBrains+Mono:wght@400;500;600&family=Noto+Sans+TC:wght@400;500;700&family=Noto+Sans+SC:wght@400;500;700&display=swap\" rel=\"stylesheet\">");
    // Glass UI CSS now served from /static/glass.css (FU #2) so the
    // browser caches it across page navigations. The PROGMEM blob is
    // still defined above and emitted by serve_glass_css().
    webpage_header += F("<link rel=\"stylesheet\" href=\"/static/glass.css\">");
    // Restore the saved theme (dark by default) before <body> renders so
    // the page doesn't flash dark first when light is selected.
    webpage_header += F("<script>try{if(localStorage.getItem('holocubic-theme')==='light')document.documentElement.classList.add('preload-light');}catch(e){}</script>");
    webpage_header += F("<style>html.preload-light body{background:#f5f1e8;}</style>");
    webpage_header += F("</head><body><div class=\"grain\"></div><div class=\"app\">");

    String langParam = getLangParam();
    String currentUri = server.uri();

    // Sidebar: brand + nav-group + sidebar-foot.
    webpage_header += F("<aside class=\"sidebar\">");
    webpage_header += F("<div class=\"brand\"><div class=\"brand-mark\">H</div><div class=\"brand-name\">HoloCubic</div><div class=\"brand-version\">v");
    webpage_header += F(AIO_VERSION "</div></div>");
    webpage_header += F("<nav class=\"nav-group\">");

    // Helper-style inline lambda would be nicer but C++ lambdas inside
    // the macro-heavy #if APP_*_USE block trip preprocessor expectations,
    // so we expand each nav row by hand. The pattern is identical:
    //   <a class="nav-item [active]" href="/route?lang=..."><span>label</span></a>

    // GENERAL group
    webpage_header += F("<div class=\"nav-label\">");
    webpage_header += getText("general");
    webpage_header += F("</div>");
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/")) webpage_header += F("active");
    webpage_header += F("\" href=\"/");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("home");
    webpage_header += F("</span></a>");
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/sys_setting")) webpage_header += F("active");
    webpage_header += F("\" href=\"/sys_setting");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("system");
    webpage_header += F("</span></a>");
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/rgb_setting")) webpage_header += F("active");
    webpage_header += F("\" href=\"/rgb_setting");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("rgb");
    webpage_header += F("</span></a>");

    // APPS group
    webpage_header += F("<div class=\"nav-label\">");
    webpage_header += getText("apps");
    webpage_header += F("</div>");
#if APP_WEATHER_USE
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/weather_setting")) webpage_header += F("active");
    webpage_header += F("\" href=\"/weather_setting");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("weather");
    webpage_header += F("</span></a>");
#endif
#if APP_WEATHER_OLD_USE
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/weather_old_setting")) webpage_header += F("active");
    webpage_header += F("\" href=\"/weather_old_setting");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("weather_old");
    webpage_header += F("</span></a>");
#endif
#if APP_BILIBILI_FANS_USE
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/bili_setting")) webpage_header += F("active");
    webpage_header += F("\" href=\"/bili_setting");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("bilibili");
    webpage_header += F("</span></a>");
#endif
#if APP_PICTURE_USE
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/picture_setting")) webpage_header += F("active");
    webpage_header += F("\" href=\"/picture_setting");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("picture");
    webpage_header += F("</span></a>");
#endif
#if APP_MEDIA_PLAYER_USE
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/media_setting")) webpage_header += F("active");
    webpage_header += F("\" href=\"/media_setting");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("media");
    webpage_header += F("</span></a>");
#endif
#if APP_SCREEN_SHARE_USE
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/screen_setting")) webpage_header += F("active");
    webpage_header += F("\" href=\"/screen_setting");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("screen");
    webpage_header += F("</span></a>");
#endif
#if APP_HEARTBEAT_USE
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/heartbeat_setting")) webpage_header += F("active");
    webpage_header += F("\" href=\"/heartbeat_setting");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("heartbeat");
    webpage_header += F("</span></a>");
#endif
#if APP_ANNIVERSARY_USE
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/anniversary_setting")) webpage_header += F("active");
    webpage_header += F("\" href=\"/anniversary_setting");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("anniversary");
    webpage_header += F("</span></a>");
#endif
#if APP_STOCK_MARKET_USE
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/stock_setting")) webpage_header += F("active");
    webpage_header += F("\" href=\"/stock_setting");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("stock");
    webpage_header += F("</span></a>");
#endif
#if APP_PC_RESOURCE_USE
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/pc_resource_setting")) webpage_header += F("active");
    webpage_header += F("\" href=\"/pc_resource_setting");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("pc_monitor");
    webpage_header += F("</span></a>");
#endif

    // STORAGE group
    webpage_header += F("<div class=\"nav-label\">");
    webpage_header += getText("storage");
    webpage_header += F("</div>");
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/upload")) webpage_header += F("active");
    webpage_header += F("\" href=\"/upload");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("upload");
    webpage_header += F("</span></a>");
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/download")) webpage_header += F("active");
    webpage_header += F("\" href=\"/download");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("download");
    webpage_header += F("</span></a>");
    webpage_header += F("<a class=\"nav-item ");
    if (currentUri == F("/delete")) webpage_header += F("active");
    webpage_header += F("\" href=\"/delete");
    webpage_header += langParam;
    webpage_header += F("\"><span>");
    webpage_header += getText("delete");
    webpage_header += F("</span></a>");

    webpage_header += F("</nav>");

    // Sidebar foot: language pills + theme toggle.
    String basePath = currentUri;
    webpage_header += F("<div class=\"sidebar-foot\"><div class=\"lang-pills\">");
    webpage_header += F("<a href=\"");
    webpage_header += basePath;
    webpage_header += F("?lang=en\" class=\"");
    if (current_lang == LANG_EN_US) webpage_header += F("on");
    webpage_header += F("\">EN</a>");
    webpage_header += F("<a href=\"");
    webpage_header += basePath;
    webpage_header += F("?lang=cn\" class=\"");
    if (current_lang == LANG_ZH_CN) webpage_header += F("on");
    webpage_header += F("\">\xE7\xAE\x80</a>");  // 简
    webpage_header += F("<a href=\"");
    webpage_header += basePath;
    webpage_header += F("?lang=tw\" class=\"");
    if (current_lang == LANG_ZH_TW) webpage_header += F("on");
    webpage_header += F("\">\xE7\xB9\x81</a>");  // 繁
    webpage_header += F("</div><div class=\"theme-toggle\" id=\"themeToggle\" title=\"Toggle theme\"></div></div>");

    webpage_header += F("</aside>");

    // Main: topbar + open .content
    webpage_header += F("<main class=\"main\"><div class=\"topbar\"><div class=\"crumbs\">HoloCubic &nbsp;/&nbsp; <strong id=\"crumbCurrent\">");
    webpage_header += getText("home");
    webpage_header += F("</strong></div><div class=\"status-cluster\"><span class=\"pill\" id=\"wifiPill\"><span class=\"dot\" id=\"wifiDot\"></span><span id=\"wifiText\">--</span></span><span class=\"pill\" id=\"ipPill\">--</span></div></div>");
    webpage_header += F("<div class=\"content\">");
}

// Glass UI footer JS, kept in PROGMEM. Handles:
//   - theme toggle (#themeToggle): persists to localStorage as
//     'holocubic-theme' = light | dark; toggles body.light class
//   - password / API-key visibility toggle (.eye-btn): swaps the
//     adjacent input's type between password and text
//   - status pill refresh: polls /api/stats every 5s for
//     wifi pill + ip pill text. Dashboard hero (if present on the page)
//     uses the same fetch to populate uptime/heap/flash/temp KPIs.
//   - post-save toast: if the URL has ?saved=1, render the toast for 2.5s
static const char GLASS_JS[] PROGMEM = R"GLASS(
(function(){
  // Theme: restore + toggle
  try{
    var saved = localStorage.getItem('holocubic-theme');
    if(saved==='light'){ document.body.classList.add('light'); }
  }catch(e){}
  document.documentElement.classList.remove('preload-light');
  var t = document.getElementById('themeToggle');
  if(t){
    t.addEventListener('click', function(){
      var nowLight = document.body.classList.toggle('light');
      try{ localStorage.setItem('holocubic-theme', nowLight ? 'light' : 'dark'); }catch(e){}
    });
  }

  // Password / API key visibility toggle
  document.querySelectorAll('.eye-btn').forEach(function(btn){
    btn.addEventListener('click', function(){
      var input = btn.parentElement.querySelector('input');
      if(!input) return;
      input.type = input.type === 'password' ? 'text' : 'password';
      btn.textContent = input.type === 'password' ? '\u{1F441}' : '\u{1F441}‍\u{1F5E8}';
    });
  });

  // Stats: refresh status pills + dashboard KPIs from /api/stats
  function fmtUptime(ms){
    var s = Math.floor(ms/1000), h = Math.floor(s/3600), m = Math.floor((s%3600)/60);
    return ('0'+h).slice(-2)+':'+('0'+m).slice(-2)+':'+('0'+(s%60)).slice(-2);
  }
  function fmtKB(b){ return Math.round(b/1024); }
  function fmtMB(b){ return (b/1024/1024).toFixed(1); }
  function refreshStats(){
    fetch('/api/stats',{cache:'no-store'}).then(function(r){return r.json();}).then(function(d){
      var wifiText = document.getElementById('wifiText');
      var wifiDot  = document.getElementById('wifiDot');
      var ipPill   = document.getElementById('ipPill');
      if(wifiText) wifiText.textContent = d.wifi.connected ? (d.wifi.ssid + ' · ' + d.wifi.rssi + ' dBm') : 'WiFi off';
      if(wifiDot)  wifiDot.style.background = d.wifi.connected ? 'var(--good)' : 'var(--bad)';
      if(ipPill)   ipPill.textContent = d.wifi.connected ? d.wifi.ip : '--';
      var u=document.getElementById('kpiUptime');  if(u) u.innerHTML = fmtUptime(d.uptime_ms) + '<span class="unit">h</span>';
      var fh=document.getElementById('kpiHeap');   if(fh) fh.innerHTML = fmtKB(d.free_heap) + '<span class="unit">/' + fmtKB(d.total_heap) + ' KB</span>';
      var hb=document.getElementById('kpiHeapBar');if(hb) hb.style.width = (100*d.free_heap/d.total_heap).toFixed(0)+'%';
      var fl=document.getElementById('kpiFlash');  if(fl) fl.innerHTML = fmtMB(d.flash_used) + '<span class="unit">/' + fmtMB(d.flash_total) + ' MB</span>';
      var fb=document.getElementById('kpiFlashBar');if(fb)fb.style.width = (100*d.flash_used/d.flash_total).toFixed(0)+'%';
      var tp=document.getElementById('kpiTemp');   if(tp) tp.innerHTML = d.temp_c.toFixed(1) + '<span class="unit">°C</span>';
      var meta=document.getElementById('deviceMeta'); if(meta) meta.textContent = d.chip + ' · Firmware ' + d.version + ' · MAC ' + d.mac;
    }).catch(function(){});
  }
  refreshStats();
  setInterval(refreshStats, 5000);

  // Toast (post-save). Server redirects to /<page>?saved=1 after a save.
  if(/[?&]saved=1/.test(location.search)){
    var t = document.createElement('div'); t.className='toast';
    t.innerHTML = '<span class="check">✓</span> Settings saved';
    document.body.appendChild(t);
    setTimeout(function(){ t.style.opacity=0; t.style.transform='translateY(8px)'; }, 2500);
    setTimeout(function(){ t.remove(); }, 3000);
  }

  // WiFi scan (System page only): bind #rescanBtn → /api/wifi-scan,
  // render rows in #wifiScanList, click row → fill ssid_0 + focus
  // password_0. No-op on pages without the rescan button.
  function escapeHtml(s){
    return s.replace(/[&<>"']/g, function(c){
      return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c];
    });
  }
  function renderScanRows(networks){
    var list = document.getElementById('wifiScanList');
    if(!list) return;
    if(!networks || networks.length === 0){
      list.innerHTML = '<div class="scan-empty">No networks found</div>';
      return;
    }
    networks.sort(function(a,b){ return b.rssi - a.rssi; });
    var html = networks.map(function(n){
      var bars = n.rssi >= -55 ? '▁▃▅▇' : n.rssi >= -65 ? '▁▃▅' : n.rssi >= -75 ? '▁▃' : '▁';
      return '<div class="scan-row ' + (n.current ? 'current' : '') + '" data-ssid="' + escapeHtml(n.ssid) + '">' +
             '<span>' + escapeHtml(n.ssid) + '</span>' +
             '<span class="meta">' + n.sec + '</span>' +
             '<span class="meta">' + n.rssi + ' dBm ' + bars + '</span>' +
             (n.current ? '<span class="dot" title="connected"></span>' : '<span style="width:7px"></span>') +
             '</div>';
    }).join('');
    list.innerHTML = html;
    list.querySelectorAll('.scan-row').forEach(function(row){
      row.addEventListener('click', function(){
        var ssid = row.getAttribute('data-ssid');
        var ssidInput = document.querySelector('input[name="ssid_0"]');
        var pwdInput  = document.querySelector('input[name="password_0"]');
        if(ssidInput){ ssidInput.value = ssid; }
        if(pwdInput){ pwdInput.focus(); }
      });
    });
  }
  var rescanBtn = document.getElementById('rescanBtn');
  if(rescanBtn){
    rescanBtn.addEventListener('click', function(){
      var list = document.getElementById('wifiScanList');
      if(list){ list.innerHTML = '<div class="scan-empty">Scanning…</div>'; }
      rescanBtn.disabled = true;
      fetch('/api/wifi-scan',{cache:'no-store'})
        .then(function(r){ return r.json(); })
        .then(function(d){ renderScanRows(d.networks); })
        .catch(function(){
          if(list){ list.innerHTML = '<div class="scan-empty">Scan failed</div>'; }
        })
        .finally(function(){ rescanBtn.disabled = false; });
    });
  }
})();
)GLASS";

void init_page_footer()
{
    webpage_footer = F("</div></main></div>");  // close .content, .main, .app
    // Glass UI JS now served from /static/glass.js (FU #2) for browser
    // caching; PROGMEM blob still defined above and emitted by
    // serve_glass_js().
    webpage_footer += F("<script src=\"/static/glass.js\"></script>");
    webpage_footer += F("</body></html>");
}

void serve_glass_css(void)
{
    server.sendHeader("Cache-Control", "public, max-age=86400");
    server.sendHeader("Content-Type", "text/css; charset=utf-8");
    // Stream from PROGMEM via FPSTR -> String conversion. ~9KB blob;
    // single send() call is fine for this size.
    String body;
    body += FPSTR(GLASS_CSS);
    server.send(200, "text/css", body);
}

void serve_glass_js(void)
{
    server.sendHeader("Cache-Control", "public, max-age=86400");
    server.sendHeader("Content-Type", "application/javascript; charset=utf-8");
    String body;
    body += FPSTR(GLASS_JS);
    server.send(200, "application/javascript", body);
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

    // Glass UI: nav group labels + dashboard hero
    if (strcmp(key, "general") == 0) {
        if (current_lang == LANG_ZH_CN) return "通用";
        if (current_lang == LANG_ZH_TW) return "通用";
        return "General";
    }
    if (strcmp(key, "apps") == 0) {
        if (current_lang == LANG_ZH_CN) return "应用";
        if (current_lang == LANG_ZH_TW) return "應用";
        return "Apps";
    }
    if (strcmp(key, "storage") == 0) {
        if (current_lang == LANG_ZH_CN) return "存储";
        if (current_lang == LANG_ZH_TW) return "儲存";
        return "Storage";
    }
    if (strcmp(key, "dashboard_sub") == 0) {
        if (current_lang == LANG_ZH_CN) return "HoloCubic 设备实时状态";
        if (current_lang == LANG_ZH_TW) return "HoloCubic 裝置即時狀態";
        return "Live status of your HoloCubic device";
    }
    if (strcmp(key, "device_online") == 0) {
        if (current_lang == LANG_ZH_CN) return "设备在线";
        if (current_lang == LANG_ZH_TW) return "裝置在線";
        return "Device online";
    }
    if (strcmp(key, "uptime") == 0) {
        if (current_lang == LANG_ZH_CN) return "运行时长";
        if (current_lang == LANG_ZH_TW) return "運行時間";
        return "Uptime";
    }
    if (strcmp(key, "free_heap") == 0) {
        if (current_lang == LANG_ZH_CN) return "可用堆";
        if (current_lang == LANG_ZH_TW) return "可用堆積";
        return "Free Heap";
    }
    if (strcmp(key, "flash") == 0) {
        if (current_lang == LANG_ZH_CN) return "闪存";
        if (current_lang == LANG_ZH_TW) return "快閃記憶體";
        return "Flash";
    }
    if (strcmp(key, "temp") == 0) {
        if (current_lang == LANG_ZH_CN) return "温度";
        if (current_lang == LANG_ZH_TW) return "溫度";
        return "Temp";
    }

    // WiFi scan card on the System page
    if (strcmp(key, "wifi_networks") == 0) {
        if (current_lang == LANG_ZH_CN) return "WiFi 网络";
        if (current_lang == LANG_ZH_TW) return "WiFi 網路";
        return "WiFi Networks";
    }
    if (strcmp(key, "scan_subtitle") == 0) {
        if (current_lang == LANG_ZH_CN) return "扫描附近网络";
        if (current_lang == LANG_ZH_TW) return "掃描附近網路";
        return "Scan for nearby networks";
    }
    if (strcmp(key, "rescan") == 0) {
        if (current_lang == LANG_ZH_CN) return "重新扫描";
        if (current_lang == LANG_ZH_TW) return "重新掃描";
        return "Rescan";
    }
    if (strcmp(key, "scan_hint") == 0) {
        if (current_lang == LANG_ZH_CN) return "点击重新扫描以发现网络";
        if (current_lang == LANG_ZH_TW) return "點擊重新掃描以發現網路";
        return "Click Rescan to discover networks";
    }
    if (strcmp(key, "scanning") == 0) {
        if (current_lang == LANG_ZH_CN) return "扫描中…";
        if (current_lang == LANG_ZH_TW) return "掃描中…";
        return "Scanning…";
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

// HomePage: Dashboard hero (live KPIs hooked to /api/stats via the
// shared GLASS_JS in init_page_footer) + a quick-links card. The
// language toggle moved into the sidebar foot, so HomePage no longer
// needs its own EN/简/繁 buttons.
void HomePage()
{
    if (server.hasArg("lang")) {
        String lang = server.arg("lang");
        if (lang == "en") current_lang = LANG_EN_US;
        else if (lang == "cn") current_lang = LANG_ZH_CN;
        else if (lang == "tw") current_lang = LANG_ZH_TW;
    }

    webpage = F("<div class=\"page-head\"><div><h1 class=\"page-title\">");
    webpage += getText("home");
    webpage += F("</h1><p class=\"page-sub\">");
    webpage += getText("dashboard_sub");
    webpage += F("</p></div></div>");

    // Hero with live KPIs — populated by GLASS_JS::refreshStats() via /api/stats.
    webpage += F("<div class=\"hero\">");
    webpage += F("<h2><span style=\"width:8px;height:8px;border-radius:50%;background:var(--good);box-shadow:0 0 12px var(--good);\"></span> ");
    webpage += getText("device_online");
    webpage += F("</h2>");
    webpage += F("<div class=\"status-line\" id=\"deviceMeta\">--</div>");
    webpage += F("<div class=\"kpi-grid\">");
    webpage += F("<div class=\"kpi\"><div class=\"label\">");
    webpage += getText("uptime");
    webpage += F("</div><div class=\"value\" id=\"kpiUptime\">--</div></div>");
    webpage += F("<div class=\"kpi\"><div class=\"label\">");
    webpage += getText("free_heap");
    webpage += F("</div><div class=\"value\" id=\"kpiHeap\">--</div><div class=\"bar\"><i id=\"kpiHeapBar\" style=\"width:0%\"></i></div></div>");
    webpage += F("<div class=\"kpi\"><div class=\"label\">");
    webpage += getText("flash");
    webpage += F("</div><div class=\"value\" id=\"kpiFlash\">--</div><div class=\"bar\"><i id=\"kpiFlashBar\" style=\"width:0%\"></i></div></div>");
    webpage += F("<div class=\"kpi\"><div class=\"label\">");
    webpage += getText("temp");
    webpage += F("</div><div class=\"value\" id=\"kpiTemp\">--</div></div>");
    webpage += F("</div></div>");

    // Quick links card — preserved from the original HomePage.
    webpage += F("<div class=\"card quick-card\">");
    webpage += F("<div class=\"card-head\"><div><div class=\"card-title\">");
    webpage += getText("quick_links");
    webpage += F("</div><div class=\"card-sub\">GitHub · Bilibili</div></div></div>");
    webpage += F("<div class=\"card-body\">");
    webpage += F("<a href=\"https://github.com/ClimbSnail/HoloCubic_AIO\" target=\"_blank\">GitHub · ");
    webpage += getText("original");
    webpage += F("</a>");
    webpage += F("<a href=\"https://github.com/asdfghj1237890/HoloCubic_AIO\" target=\"_blank\">GitHub · ");
    webpage += getText("custom");
    webpage += F("</a>");
    webpage += F("<a href=\"https://space.bilibili.com/344470052\" target=\"_blank\">Bilibili · ");
    webpage += getText("tutorial");
    webpage += F("</a>");
    webpage += F("</div></div>");

    Send_HTML(webpage);
}

