// NTP fetch + RTC display update.
//
// Split out of weather.cpp in PR-3.2a. Pure mechanical extraction —
// every function body is byte-for-byte the original. The Taobao
// timestamp endpoint is the only external surface; the rest of this
// file just decodes the resulting epoch into a TimeStr for the GUI.

#include <Arduino.h>
#include "aio_network.h"
#include "common.h"
#include "http_util.h"
#include "weather_internal.h"

long long get_timestamp(void)
{
    // 使用本地的机器时钟
    run_data->preNetTimestamp = run_data->preNetTimestamp + (GET_SYS_MILLIS() - run_data->preLocalTimestamp);
    run_data->preLocalTimestamp = GET_SYS_MILLIS();
    return run_data->preNetTimestamp;
}

long long get_timestamp(String url)
{
    if (WL_CONNECTED != WiFi.status())
        return 0;

    String payload;
    int httpCode = http_fetch_string(url.c_str(), payload, 1000);
    if (httpCode == HTTP_CODE_OK)
    {
        Serial.println(payload);
        int time_index = payload.indexOf("\"t\":\"") + 5;       // 找到 "t":" 后的索引，+5 跳过 "t":" 的长度
        int time_end_index = payload.indexOf("\"", time_index); // 查找结束引号的位置
        String time = payload.substring(time_index, time_end_index); // 提取时间戳

        // 以网络时间戳为准
        run_data->preNetTimestamp = atoll(time.c_str()) + run_data->errorNetTimestamp + TIMEZERO_OFFSIZE;
        run_data->preLocalTimestamp = GET_SYS_MILLIS();
    }
    else
    {
        Serial.printf("[HTTP] GET... failed (code=%d)\n", httpCode);
        // 得不到网络时间戳时
        run_data->preNetTimestamp = run_data->preNetTimestamp + (GET_SYS_MILLIS() - run_data->preLocalTimestamp);
        run_data->preLocalTimestamp = GET_SYS_MILLIS();
    }

    return run_data->preNetTimestamp;
}

void updateTime_RTC(long long timestamp)
{
    struct TimeStr t;
    run_data->g_rtc.setTime(timestamp / 1000);
    t.month = run_data->g_rtc.getMonth() + 1;
    t.day = run_data->g_rtc.getDay();
    t.hour = run_data->g_rtc.getHour(true);
    t.minute = run_data->g_rtc.getMinute();
    t.second = run_data->g_rtc.getSecond();
    t.weekday = run_data->g_rtc.getDayofWeek();
    // Serial.printf("time : %d-%d-%d\n",t.hour, t.minute, t.second);
    display_time(t, LV_SCR_LOAD_ANIM_NONE);
}
