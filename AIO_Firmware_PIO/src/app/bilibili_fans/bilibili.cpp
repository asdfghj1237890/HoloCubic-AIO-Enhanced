#include "bilibili.h"
#include "bilibili_gui.h"
#include "sys/app_controller.h"
#include "../../common.h"
#include "../../http_util.h"

#define FANS_API "https://api.bilibili.com/x/relation/stat?vmid="
#define OTHER_API "https://api.bilibili.com/x/space/upstat?mid="

// Bilibili的持久化配置
#define B_CONFIG_PATH "/bilibili.cfg"
struct B_Config
{
    String bili_uid;              // bilibili的uid
    unsigned long updataInterval; // 更新的时间间隔(s)
};

static void write_config(const B_Config *cfg)
{
    char tmp[16];
    // 将配置数据保存在文件中（持久化）
    String w_data;
    w_data = w_data + cfg->bili_uid + "\n";
    memset(tmp, 0, 16);
    snprintf(tmp, 16, "%lu\n", cfg->updataInterval);
    w_data += tmp;
    g_flashCfg.writeFile(B_CONFIG_PATH, w_data.c_str());
}

static void read_config(B_Config *cfg)
{
    // 如果有需要持久化配置文件 可以调用此函数将数据存在flash中
    // 配置文件名最好以APP名为开头 以".cfg"结尾，以免多个APP读取混乱
    char info[128] = {0};
    uint16_t size = g_flashCfg.readFile(B_CONFIG_PATH, (uint8_t *)info, sizeof(info));
    info[size] = 0;
    if (size == 0)
    {
        // 默认值
        cfg->bili_uid = "344470052";  // B站的用户ID
        cfg->updataInterval = 900000; // 更新的时间间隔900000(900s)
        write_config(cfg);
    }
    else
    {
        // 解析数据
        char *param[2] = {0};
        analyseParam(info, 2, param);
        cfg->bili_uid = param[0];
        cfg->updataInterval = atol(param[1]);
    }
}

struct BilibiliAppRunData
{
    unsigned int fans_num;
    unsigned int follow_num;
    unsigned int refresh_status;
    unsigned long refresh_time_millis;
};

struct MyHttpResult
{
    int httpCode = 0;
    String httpResponse = "";
};

static B_Config cfg_data;
static BilibiliAppRunData *run_data = NULL;

static MyHttpResult http_request(String uid = "344470052")
{
    // String url = "http://www.dtmb.top/api/fans/index?id=" + uid;
    MyHttpResult result;
    String url = FANS_API + uid;
    result.httpCode = http_fetch_string(url.c_str(), result.httpResponse, 1000);
    return result;
}

static int bilibili_init(AppController *sys)
{
    bilibili_gui_init();
    // 获取配置信息
    read_config(&cfg_data);
    // 初始化运行时参数
    run_data = (BilibiliAppRunData *)malloc(sizeof(BilibiliAppRunData));
    run_data->fans_num = 0;
    run_data->follow_num = 0;
    run_data->refresh_status = 0;
    run_data->refresh_time_millis = GET_SYS_MILLIS() - cfg_data.updataInterval;
    return 0;
}

static void bilibili_process(AppController *sys,
                             const ImuAction *act_info)
{
    lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;
    if (RETURN == act_info->active)
    {
        sys->app_exit(); // 退出APP
        return;
    }

    char fans_num[20] = {0};
    char follow_num[20] = {0};
    if (run_data->fans_num >= 10000)
    {
        // 粉丝过万的
        snprintf(fans_num, 20, "%3.1fw", run_data->fans_num * 1.0 / 10000);
    }
    else
    {
        snprintf(fans_num, 20, "%d", run_data->fans_num);
    }

    if (run_data->follow_num >= 10000)
    {
        // 关注过万的
        snprintf(follow_num, 20, "%3.1fw", run_data->follow_num * 1.0 / 10000);
    }
    else
    {
        snprintf(follow_num, 20, "%d", run_data->follow_num);
    }

    if (0 == run_data->refresh_status)
    {
        // 以下减少网络请求的压力
        if (doDelayMillisTime(cfg_data.updataInterval, &run_data->refresh_time_millis, false))
        {
            sys->send_to(BILI_APP_NAME, CTRL_NAME,
                         APP_MESSAGE_WIFI_CONN, NULL, NULL);
        }
    }

    display_bilibili("bilibili", anim_type, fans_num, follow_num);

    delay(300);
}

static void bilibili_background_task(AppController *sys,
                                     const ImuAction *act_info)
{
    // 本函数为后台任务，主控制器会间隔一分钟调用此函数
    // 本函数尽量只调用"常驻数据",其他变量可能会因为生命周期的缘故已经释放
}

static int bilibili_exit_callback(void *param)
{
    bilibili_gui_del();

    // 释放运行数据
    if (NULL != run_data)
    {
        free(run_data);
        run_data = NULL;
    }
    return 0;
}

static void update_fans_num()
{
    MyHttpResult result = http_request(cfg_data.bili_uid);
    if (-1 == result.httpCode)
    {
        Serial.println("[HTTP] Http request failed.");
        return;
    }
    if (result.httpCode > 0)
    {
        if (result.httpCode == HTTP_CODE_OK || result.httpCode == HTTP_CODE_MOVED_PERMANENTLY)
        {
            String payload = result.httpResponse;
            Serial.println("[HTTP] OK");
            Serial.println(payload);
            // Bilibili response shape: { "data": { "follower": N, "following": M, ... } }
            // The +10 / +11 offsets skip past the literal `follower":` / `following":`.
            // Guard each marker so a missing key (changed API, partial body)
            // bails out instead of feeding negative indices into substring.
            int follower_marker  = payload.indexOf("follower");
            int following_marker = payload.indexOf("following");
            if (follower_marker < 0 || following_marker < 0)
            {
                Serial.println("[Bilibili] follower/following marker missing — skipping refresh");
            }
            else
            {
                int startIndex_1 = follower_marker + 10;
                int endIndex_1   = payload.indexOf('}', startIndex_1);
                int startIndex_2 = following_marker + 11;
                int endIndex_2   = payload.indexOf(',', startIndex_2);
                if (endIndex_1 > startIndex_1 && endIndex_2 > startIndex_2)
                {
                    run_data->fans_num   = payload.substring(startIndex_1, endIndex_1).toInt();
                    run_data->follow_num = payload.substring(startIndex_2, endIndex_2).toInt();
                    run_data->refresh_status = 1;
                }
                else
                {
                    Serial.println("[Bilibili] follower/following terminator missing — skipping refresh");
                }
            }
        }
    }
    else
    {
        Serial.println("[HTTP] ERROR");
    }
}

static void bilibili_message_handle(const char *from, const char *to,
                                    APP_MESSAGE_TYPE type, void *message,
                                    void *ext_info)
{
    switch (type)
    {
    case APP_MESSAGE_WIFI_CONN:
    {
        Serial.print(GET_SYS_MILLIS());
        Serial.println("[SYS] bilibili_event_notification");
        if (0 == run_data->refresh_status)
        {
            update_fans_num();
        }
    }
    break;
    case APP_MESSAGE_GET_PARAM:
    {
        char *param_key = (char *)message;
        if (!strcmp(param_key, "bili_uid"))
        {
            snprintf((char *)ext_info, 32, "%s", cfg_data.bili_uid.c_str());
        }
        else if (!strcmp(param_key, "updataInterval"))
        {
            snprintf((char *)ext_info, 32, "%lu", cfg_data.updataInterval);
        }
        else
        {
            snprintf((char *)ext_info, 32, "%s", "NULL");
        }
    }
    break;
    case APP_MESSAGE_SET_PARAM:
    {
        char *param_key = (char *)message;
        char *param_val = (char *)ext_info;
        if (!strcmp(param_key, "bili_uid"))
        {
            cfg_data.bili_uid = param_val;
        }
        else if (!strcmp(param_key, "updataInterval"))
        {
            cfg_data.updataInterval = atol(param_val);
        }
    }
    break;
    case APP_MESSAGE_READ_CFG:
    {
        read_config(&cfg_data);
    }
    break;
    case APP_MESSAGE_WRITE_CFG:
    {
        write_config(&cfg_data);
    }
    break;
    default:
        break;
    }
}

APP_OBJ bilibili_app = {BILI_APP_NAME, &app_bilibili, "", bilibili_init,
                        bilibili_process, bilibili_background_task,
                        bilibili_exit_callback, bilibili_message_handle};
