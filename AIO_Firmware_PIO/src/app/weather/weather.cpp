// Weather app — APP_OBJ glue, config persistence, lifecycle hooks,
// and message dispatch. Fetcher bodies live in weather_api.cpp; NTP
// helpers in weather_time.cpp; LVGL rendering in weather_gui.c.
//
// PR-3.2a split: this file dropped from 1081 to ~330 LOC. cfg_data
// and run_data are *defined* here and *declared* in weather_internal.h
// so the api/time files can reach them without going through getters.

#include "weather.h"
#include "weather_gui.h"
#include "weather_internal.h"
#include "sys/app_controller.h"
#include "aio_network.h"
#include "common.h"

#define WEATHER_APP_NAME "Weather"

#define WEATHER_PAGE_SIZE 2

WT_Config cfg_data;
WeatherAppRunData *run_data = NULL;

enum WEA_EVENT_ID
{
    UPDATE_NOW,
    UPDATE_NTP,
    UPDATE_DAILY
};

void weather_write_config(WT_Config *cfg)
{
    char tmp[16];
    // 将配置数据保存在文件中（持久化）
    String w_data;
    w_data = w_data + cfg->api_key + "\n";
    w_data = w_data + cfg->city_name + "\n";
    w_data = w_data + cfg->location_key + "\n";
    memset(tmp, 0, 16);
    snprintf(tmp, 16, "%lu\n", cfg->weatherUpdataInterval);
    w_data += tmp;
    memset(tmp, 0, 16);
    snprintf(tmp, 16, "%lu\n", cfg->timeUpdataInterval);
    w_data += tmp;
    memset(tmp, 0, 16);
    snprintf(tmp, 16, "%d\n", cfg->language);
    w_data += tmp;
    g_flashCfg.writeFile(WEATHER_CONFIG_PATH, w_data.c_str());
}

static void read_config(WT_Config *cfg)
{
    // 如果有需要持久化配置文件 可以调用此函数将数据存在flash中
    // 配置文件名最好以APP名为开头 以".cfg"结尾，以免多个APP读取混乱
    char info[256] = {0};
    uint16_t size = g_flashCfg.readFile(WEATHER_CONFIG_PATH, (uint8_t *)info);
    info[size] = 0;
    if (size == 0)
    {
        // 默认值
        cfg->api_key = WEATHER_API_KEY_PLACEHOLDER;
        cfg->city_name = "Beijing";          // Default to Beijing (change via WebServer if needed)
        cfg->location_key = "";              // Will be fetched on first run
        cfg->weatherUpdataInterval = 900000; // 天气更新的时间间隔900000(900s)
        cfg->timeUpdataInterval = 900000;    // 日期时钟更新的时间间隔900000(900s)
        cfg->language = 0;                   // Default to Simplified Chinese
        weather_write_config(cfg);
    }
    else
    {
        // 解析数据
        char *param[6] = {0};
        analyseParam(info, 6, param);
        cfg->api_key = param[0];
        cfg->city_name = param[1];
        cfg->location_key = param[2];
        cfg->weatherUpdataInterval = atol(param[3]);
        cfg->timeUpdataInterval = atol(param[4]);
        cfg->language = (param[5] != NULL) ? atoi(param[5]) : 0; // Default to Simplified Chinese if not present
    }
}

static void task_update(void *parameter); // 异步更新任务

static int weather_init(AppController *sys)
{
    tft->setSwapBytes(true);
    weather_gui_init();
    // 获取配置信息
    read_config(&cfg_data);
    // 设置UI语言
    weather_set_language((WeatherLanguage)cfg_data.language);

    // 初始化运行时参数
    run_data = (WeatherAppRunData *)calloc(1, sizeof(WeatherAppRunData));
    memset((char *)&run_data->wea, 0, sizeof(Weather));
    run_data->preNetTimestamp = 1577808000000; // 上一次的网络时间戳 初始化为2020-01-01 00:00:00
    run_data->errorNetTimestamp = 2;
    run_data->preLocalTimestamp = GET_SYS_MILLIS(); // 上一次的本地机器时间戳
    run_data->clock_page = 0;
    run_data->preWeatherMillis = 0;
    run_data->preTimeMillis = 0;
    // 强制更新
    run_data->coactusUpdateFlag = 0x01;
    run_data->update_type = 0x00; // 表示什么也不需要更新

    // 目前更新数据的任务栈大小5000够用，4000不够用
    // 为了后期迭代新功能 当前设置为8000, 任务可能会导致卡死
    run_data->xReturned_task_update = pdFAIL;
    // run_data->xReturned_task_update = xTaskCreate(
    //     task_update,                     /*任务函数*/
    //     "Task_update",                   /*带任务名称的字符串*/
    //     8000,                            /*堆栈大小，单位为字节*/
    //     NULL,                            /*作为任务输入传递的参数*/
    //     1,                               /*任务的优先级*/
    //     &run_data->xHandle_task_update); /*任务句柄*/

    return 0;
}

static void weather_process(AppController *sys,
                            const ImuAction *act_info)
{
    lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;

    if (RETURN == act_info->active)
    {
        sys->app_exit();
        return;
    }
    else if (GO_FORWORD == act_info->active)
    {
        // 间接强制更新
        run_data->coactusUpdateFlag = 0x01;
        delay(500); // 以防间接强制更新后，生产很多请求 使显示卡顿
    }
    else if (TURN_RIGHT == act_info->active)
    {
        anim_type = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
        run_data->clock_page = (run_data->clock_page + 1) % WEATHER_PAGE_SIZE;
    }
    else if (TURN_LEFT == act_info->active)
    {
        anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;
        // 以下等效与 clock_page = (clock_page + WEATHER_PAGE_SIZE - 1) % WEATHER_PAGE_SIZE;
        // +3为了不让数据溢出成负数，而导致取模逻辑错误
        run_data->clock_page = (run_data->clock_page + WEATHER_PAGE_SIZE - 1) % WEATHER_PAGE_SIZE;
    }

    // 界面刷新
    if (run_data->clock_page == 0)
    {
        display_weather(run_data->wea, anim_type);
        if (0x01 == run_data->coactusUpdateFlag || doDelayMillisTime(cfg_data.weatherUpdataInterval, &run_data->preWeatherMillis, false))
        {
            sys->send_to(WEATHER_APP_NAME, CTRL_NAME,
                         APP_MESSAGE_WIFI_CONN, (void *)UPDATE_NOW, NULL);
            sys->send_to(WEATHER_APP_NAME, CTRL_NAME,
                         APP_MESSAGE_WIFI_CONN, (void *)UPDATE_DAILY, NULL);
        }

        if (0x01 == run_data->coactusUpdateFlag || doDelayMillisTime(cfg_data.timeUpdataInterval, &run_data->preTimeMillis, false))
        {
            // 尝试同步网络上的时钟
            sys->send_to(WEATHER_APP_NAME, CTRL_NAME,
                         APP_MESSAGE_WIFI_CONN, (void *)UPDATE_NTP, NULL);
        }
        else if (GET_SYS_MILLIS() - run_data->preLocalTimestamp > 400)
        {
            updateTime_RTC(get_timestamp());
        }
        run_data->coactusUpdateFlag = 0x00; // 取消强制更新标志
        display_space();
        // (was: delay(30) — pure throttle removed; AppController already
        //  rate-limits main_process via its 200ms loop timer.)
    }
    else if (run_data->clock_page == 1)
    {
        // 仅在切换界面时获取一次未来天气
        display_curve(run_data->wea.daily_max, run_data->wea.daily_min, anim_type);
        // (was: delay(300) — pure throttle removed; same reasoning.)
    }
}

static void weather_background_task(AppController *sys,
                                    const ImuAction *act_info)
{
    // 本函数为后台任务，主控制器会间隔一分钟调用此函数
    // 本函数尽量只调用"常驻数据",其他变量可能会因为生命周期的缘故已经释放
}

static int weather_exit_callback(void *param)
{
    weather_gui_del();

    // 查杀异步任务
    if (run_data->xReturned_task_update == pdPASS)
    {
        vTaskDelete(run_data->xHandle_task_update);
    }
    run_data->xReturned_task_update = pdFAIL;

    // 释放运行数据
    if (NULL != run_data)
    {
        free(run_data);
        run_data = NULL;
    }
    return 0;
}

static void task_update(void *parameter)
{
    // 数据更新任务
    while (true)
    {
        if (run_data->update_type & UPDATE_WEATHER)
        {
            get_weather();
            run_data->update_type &= (~UPDATE_WEATHER);
        }
        if (run_data->update_type & UPDATE_TIME)
        {
            get_timestamp(TIME_API); // nowapi时间API
            run_data->update_type &= (~UPDATE_TIME);
        }
        if (run_data->update_type & UPDATE_DALIY_WEATHER)
        {
            get_daliyWeather(run_data->wea.daily_max, run_data->wea.daily_min);
            run_data->update_type &= (~UPDATE_DALIY_WEATHER);
        }
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}

static void weather_message_handle(const char *from, const char *to,
                                   APP_MESSAGE_TYPE type, void *message,
                                   void *ext_info)
{
    switch (type)
    {
    case APP_MESSAGE_WIFI_CONN:
    {
        Serial.println(F("----->weather_event_notification"));
        int event_id = (int)message;
        switch (event_id)
        {
        case UPDATE_NOW:
        {
            Serial.print(F("weather update.\n"));
            run_data->update_type |= UPDATE_WEATHER;

            // 更新过程，使用如下代码或者替换成异步任务
            get_weather();
        };
        break;
        case UPDATE_NTP:
        {
            Serial.print(F("ntp update.\n"));
            run_data->update_type |= UPDATE_TIME;

            // 更新过程，使用如下代码或者替换成异步任务
            long long timestamp = get_timestamp(TIME_API); // nowapi时间API
        };
        break;
        case UPDATE_DAILY:
        {
            Serial.print(F("daliy update.\n"));
            run_data->update_type |= UPDATE_DALIY_WEATHER;

            // 更新过程，使用如下代码或者替换成异步任务
            get_daliyWeather(run_data->wea.daily_max, run_data->wea.daily_min);
        };
        break;
        default:
            break;
        }
    }
    break;
    case APP_MESSAGE_GET_PARAM:
    {
        char *param_key = (char *)message;
        if (!strcmp(param_key, "api_key"))
        {
            snprintf((char *)ext_info, 128, "%s", cfg_data.api_key.c_str());
        }
        else if (!strcmp(param_key, "city_name"))
        {
            snprintf((char *)ext_info, 64, "%s", cfg_data.city_name.c_str());
        }
        else if (!strcmp(param_key, "location_key"))
        {
            snprintf((char *)ext_info, 64, "%s", cfg_data.location_key.c_str());
        }
        else if (!strcmp(param_key, "weatherUpdataInterval"))
        {
            snprintf((char *)ext_info, 32, "%lu", cfg_data.weatherUpdataInterval);
        }
        else if (!strcmp(param_key, "timeUpdataInterval"))
        {
            snprintf((char *)ext_info, 32, "%lu", cfg_data.timeUpdataInterval);
        }
        else if (!strcmp(param_key, "language"))
        {
            snprintf((char *)ext_info, 32, "%d", cfg_data.language);
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
        if (!strcmp(param_key, "api_key"))
        {
            cfg_data.api_key = param_val;
        }
        else if (!strcmp(param_key, "city_name"))
        {
            cfg_data.city_name = param_val;
            // Clear location key to force re-fetch when city changes
            cfg_data.location_key = "";
        }
        else if (!strcmp(param_key, "location_key"))
        {
            cfg_data.location_key = param_val;
        }
        else if (!strcmp(param_key, "weatherUpdataInterval"))
        {
            cfg_data.weatherUpdataInterval = atol(param_val);
        }
        else if (!strcmp(param_key, "timeUpdataInterval"))
        {
            cfg_data.timeUpdataInterval = atol(param_val);
        }
        else if (!strcmp(param_key, "language"))
        {
            cfg_data.language = atoi(param_val);
            // 立即更新UI语言
            weather_set_language((WeatherLanguage)cfg_data.language);
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
        weather_write_config(&cfg_data);
    }
    break;
    default:
        break;
    }
}

APP_OBJ weather_app = {WEATHER_APP_NAME, &app_weather, "",
                       weather_init, weather_process, weather_background_task,
                       weather_exit_callback, weather_message_handle};
