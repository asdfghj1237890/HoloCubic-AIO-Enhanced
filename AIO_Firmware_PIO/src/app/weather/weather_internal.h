// Internal header shared between weather.cpp (lifecycle / message dispatch),
// weather_api.cpp (HTTP + JSON fetchers), and weather_time.cpp (NTP / RTC).
// Not in the public weather.h because nothing outside this folder needs to
// touch the WT_Config struct, the run-time state, or the AccuWeather URLs.
//
// PR-3.2a split: pulled out so weather.cpp can drop ~400 LOC of fetcher
// bodies without losing the cfg_data / run_data wiring.

#ifndef APP_WEATHER_INTERNAL_H
#define APP_WEATHER_INTERNAL_H

#include <Arduino.h>
#include "ESP32Time.h"
#include "weather_gui.h"  // Weather, FORECAST_DAYS, TimeStr

// Placeholder default for the AccuWeather API key. Real keys are supplied at
// runtime via the web settings page; this string acts as both the seeded
// default in fresh flash configs and as the sentinel that gates HTTP calls.
#define WEATHER_API_KEY_PLACEHOLDER "YOUR_ACCUWEATHER_API_KEY"

// Persisted config path on flash.
#define WEATHER_CONFIG_PATH "/weather_accu.cfg"

// Update-type bits set by message_handle and consumed by task_update.
#define UPDATE_WEATHER       0x01
#define UPDATE_DALIY_WEATHER 0x02
#define UPDATE_TIME          0x04

// AccuWeather REST endpoints. Format args (in order):
//   LOCATION_IP_API:        api_key
//   LOCATION_SEARCH_API:    api_key, city_name
//   WEATHER_CURRENT_API:    location_key, api_key
//   WEATHER_FORECAST_API:   location_key, api_key
#define LOCATION_IP_API     "https://dataservice.accuweather.com/locations/v1/cities/ipaddress?apikey=%s&language=zh-TW"
#define LOCATION_SEARCH_API "https://dataservice.accuweather.com/locations/v1/cities/search?apikey=%s&q=%s&language=zh-TW"
#define WEATHER_CURRENT_API "https://dataservice.accuweather.com/currentconditions/v1/%s?apikey=%s&language=zh-CN&details=true"
#define WEATHER_FORECAST_API "https://dataservice.accuweather.com/forecasts/v1/daily/5day/%s?apikey=%s&language=zh-CN&details=true&metric=true"

// Taobao timestamp endpoint used by weather_time.cpp.
#define TIME_API "https://acs.m.taobao.com/gw/mtop.common.getTimestamp/"

struct WT_Config
{
    String api_key;                      // AccuWeather API key
    String city_name;                    // City name for location search
    String location_key;                 // AccuWeather location key (cached)
    unsigned long weatherUpdataInterval; // 天气更新的时间间隔(s)
    unsigned long timeUpdataInterval;    // 日期时钟更新的时间间隔(s)
    int language;                        // UI language: 0=Simplified, 1=Traditional
};

struct WeatherAppRunData
{
    unsigned long preWeatherMillis; // 上一回更新天气时的毫秒数
    unsigned long preTimeMillis;    // 更新时间计数器
    long long preNetTimestamp;      // 上一次的网络时间戳
    long long errorNetTimestamp;    // 网络到显示过程中的时间误差
    long long preLocalTimestamp;    // 上一次的本地机器时间戳
    unsigned int coactusUpdateFlag; // 强制更新标志
    int clock_page;
    unsigned int update_type;       // 更新类型的标志位

    BaseType_t xReturned_task_update; // 更新数据的异步任务
    TaskHandle_t xHandle_task_update;

    ESP32Time g_rtc;
    Weather wea;
};

// Shared state owned by weather.cpp; api/time files read+write through these.
extern WT_Config cfg_data;
extern WeatherAppRunData *run_data;

// Implemented in weather.cpp; called from weather_api.cpp after a successful
// location-key fetch so the new key gets persisted to flash.
void weather_write_config(WT_Config *cfg);

// weather_api.cpp
bool weather_api_key_configured(void);
bool get_location_key(void);
void get_weather(void);
void get_daliyWeather(short maxT[], short minT[]);

// weather_time.cpp
long long get_timestamp(void);
long long get_timestamp(String url);
void updateTime_RTC(long long timestamp);

#endif
