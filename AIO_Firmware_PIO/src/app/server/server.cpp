#include "server.h"
#include "server_gui.h"
#include "web_setting.h"
#include "web_api.h"
#include "sys/app_controller.h"
#include "app/app_conf.h"
#include "network.h"
#include "common.h"
#include "web_auth.h"

#define SERVER_REFLUSH_INTERVAL 5000UL // 配置界面重新刷新时间(5s)
#define DNS_PORT 53                    // DNS端口
WebServer server(80);

// DNSServer dnsServer;

struct ServerAppRunData
{
    boolean web_start;                    // 标志是否开启web server服务，0为关闭 1为开启
    boolean req_sent;                     // 标志是否发送wifi请求服务，0为关闭 1为开启
    unsigned long serverReflushPreMillis; // 上一回更新的时间
};

static ServerAppRunData *run_data = NULL;

// Wrap a route handler in the HTTP Basic auth gate so every endpoint
// registered in start_web_config() rejects unauthenticated clients with a
// 401 — without editing each handler body in web_setting_handlers.cpp /
// web_api.cpp. Method-qualified routes (HTTP_GET / HTTP_POST) use an inline
// lambda instead, since that overload's signature differs across cores.
static void auth_on(const char *uri, WebServer::THandlerFunction handler)
{
    server.on(uri, [handler]() {
        if (!web_require_auth())
            return;
        handler();
    });
}

void start_web_config()
{
    // 首页
    server.on("/", HTTP_GET, []() { if (!web_require_auth()) return; HomePage(); });

    // Glass UI static assets (FU #2). Served once + cached by the
    // browser, so the main HTML pages no longer carry the ~30KB
    // CSS/JS payload inline on every navigation.
    server.on("/static/glass.css", HTTP_GET, []() { if (!web_require_auth()) return; serve_glass_css(); });
    server.on("/static/glass.js",  HTTP_GET, []() { if (!web_require_auth()) return; serve_glass_js(); });

    init_page_header();
    init_page_footer();
    auth_on("/download", File_Download);
    auth_on("/upload", File_Upload);
    auth_on("/delete", File_Delete);
    auth_on("/delete_result", delete_result);

    auth_on("/sys_setting", sys_setting);
    auth_on("/rgb_setting", rgb_setting);
#if APP_WEATHER_USE
    auth_on("/weather_setting", weather_setting);
#endif
#if APP_WEATHER_OLD_USE
    auth_on("/weather_old_setting", weather_old_setting);
#endif
#if APP_BILIBILI_FANS_USE
    auth_on("/bili_setting", bili_setting);
#endif
#if APP_STOCK_MARKET_USE
    auth_on("/stock_setting", stock_setting);
#endif
#if APP_PICTURE_USE
    auth_on("/picture_setting", picture_setting);
#endif
#if APP_MEDIA_PLAYER_USE
    auth_on("/media_setting", media_setting);
#endif
#if APP_SCREEN_SHARE_USE
    auth_on("/screen_setting", screen_setting);
#endif
#if APP_HEARTBEAT_USE
    auth_on("/heartbeat_setting", heartbeat_setting);
#endif
#if APP_ANNIVERSARY_USE
    auth_on("/anniversary_setting", anniversary_setting);
#endif
#if APP_PC_RESOURCE_USE
    auth_on("/pc_resource_setting", pc_resource_setting);
#endif

    server.on(
        "/fupload", HTTP_POST,
        []()
        { if (!web_require_auth()) return; server.send(200); },
        handleFileUpload);

    // JSON API endpoints for the Glass UI dashboard + system page.
    server.on("/api/stats", HTTP_GET, []() { if (!web_require_auth()) return; api_stats(); });
    server.on("/api/wifi-scan", HTTP_GET, []() { if (!web_require_auth()) return; api_wifi_scan(); });
    server.on("/api/settings", HTTP_GET, []() { if (!web_require_auth()) return; api_settings(); });

    // 连接
    auth_on("/saveSysConf", saveSysConf);
    auth_on("/saveRgbConf", saveRgbConf);
#if APP_WEATHER_USE
    auth_on("/saveWeatherConf", saveWeatherConf);
#endif
#if APP_WEATHER_OLD_USE
    auth_on("/saveWeatherOldConf", saveWeatherOldConf);
#endif
#if APP_BILIBILI_FANS_USE
    auth_on("/saveBiliConf", saveBiliConf);
#endif
#if APP_STOCK_MARKET_USE
    auth_on("/saveStockConf", saveStockConf);
#endif
#if APP_PICTURE_USE
    auth_on("/savePictureConf", savePictureConf);
#endif
#if APP_MEDIA_PLAYER_USE
    auth_on("/saveMediaConf", saveMediaConf);
#endif
#if APP_SCREEN_SHARE_USE
    auth_on("/saveScreenConf", saveScreenConf);
#endif
#if APP_HEARTBEAT_USE
    auth_on("/saveHeartbeatConf", saveHeartbeatConf);
#endif
#if APP_ANNIVERSARY_USE
    auth_on("/saveAnniversaryConf", saveAnniversaryConf);
#endif
#if APP_PC_RESOURCE_USE
    auth_on("/savePCResourceConf", savePCResourceConf);
#endif

    server.begin();
    // MDNS.addService("http", "tcp", 80);
    Serial.println("HTTP server started");

    // dnsServer.start(DNS_PORT, "*", gateway);
}

void stop_web_config()
{
    run_data->web_start = 0;
    run_data->req_sent = 0;
    server.stop();
    server.close();
}

static int server_init(AppController *sys)
{
    server_gui_init();
    // 初始化运行时参数
    run_data = (ServerAppRunData *)malloc(sizeof(ServerAppRunData));
    run_data->web_start = 0;
    run_data->req_sent = 0;
    run_data->serverReflushPreMillis = 0;
    return 0;
}

static void server_process(AppController *sys,
                           const ImuAction *action)
{
    lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;

    if (RETURN == action->active)
    {
        stop_web_config();
        sys->app_exit();
        return;
    }

    if (0 == run_data->web_start && 0 == run_data->req_sent)
    {
        // 预显示
        String web_login = String(WEB_AUTH_USER) + "/" + web_auth_password();
        display_setting(
            "WebServer Start",
            web_login.c_str(),
            "Wait...", "Wait...",
            // "", "",
            LV_SCR_LOAD_ANIM_NONE);
        // 如果web服务没有开启 且 ap开启的请求没有发送 message这边没有作用（填0）
        sys->send_to(SERVER_APP_NAME, CTRL_NAME,
                     APP_MESSAGE_WIFI_AP, NULL, NULL);
        run_data->req_sent = 1; // 标志为 ap开启请求已发送
    }
    else if (1 == run_data->web_start)
    {
        server.handleClient(); // 一定需要放在循环里扫描
        // dnsServer.processNextRequest();
        if (doDelayMillisTime(SERVER_REFLUSH_INTERVAL, &run_data->serverReflushPreMillis, false) == true)
        {
            // 发送wifi维持的心跳
            sys->send_to(SERVER_APP_NAME, CTRL_NAME,
                         APP_MESSAGE_WIFI_ALIVE, NULL, NULL);

            String web_login = String(WEB_AUTH_USER) + "/" + web_auth_password();
            display_setting(
                "WebServer Start",
                web_login.c_str(),
                WiFi.localIP().toString().c_str(),
                WiFi.softAPIP().toString().c_str(),
                LV_SCR_LOAD_ANIM_NONE);
        }
    }
}

static void server_background_task(AppController *sys,
                                   const ImuAction *act_info)
{
    // 本函数为后台任务，主控制器会间隔一分钟调用此函数
    // 本函数尽量只调用"常驻数据",其他变量可能会因为生命周期的缘故已经释放
}

static int server_exit_callback(void *param)
{
    setting_gui_del();

    // 释放运行数据
    if (NULL != run_data)
    {
        free(run_data);
        run_data = NULL;
    }
    return 0;
}

static void server_message_handle(const char *from, const char *to,
                                  APP_MESSAGE_TYPE type, void *message,
                                  void *ext_info)
{
    switch (type)
    {
    case APP_MESSAGE_WIFI_AP:
    {
        Serial.print(F("APP_MESSAGE_WIFI_AP enable\n"));
        String web_login = String(WEB_AUTH_USER) + "/" + web_auth_password();
        display_setting(
            "WebServer Start",
            web_login.c_str(),
            WiFi.localIP().toString().c_str(),
            WiFi.softAPIP().toString().c_str(),
            LV_SCR_LOAD_ANIM_NONE);
        start_web_config();
        run_data->web_start = 1;
    }
    break;
    case APP_MESSAGE_WIFI_ALIVE:
    {
        // wifi心跳维持的响应 可以不做任何处理
    }
    break;
    default:
        break;
    }
}

APP_OBJ server_app = {SERVER_APP_NAME, &app_server, "",
                      server_init, server_process, server_background_task,
                      server_exit_callback, server_message_handle};
