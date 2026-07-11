#ifndef APP_TOMATO_CALC_H
#define APP_TOMATO_CALC_H

// Pure countdown-display helpers, kept LVGL-free so native_unit can test them.
// time_mode values (tomato.cpp time_switch): 0=focus25, 1=focus45,
// -1=break5, 2=break15.

static inline int tomato_total_seconds(int minute, int second)
{
    return minute * 60 + second;
}

// Elapsed percentage of the countdown, clamped to [0,100].
static inline int tomato_progress_pct(int total_sec, int remain_sec)
{
    if (total_sec <= 0)
        return 100;
    if (remain_sec < 0)
        remain_sec = 0;
    if (remain_sec > total_sec)
        remain_sec = total_sec;
    return (int)(100L * (total_sec - remain_sec) / total_sec);
}

static inline int tomato_is_focus(int time_mode)
{
    return time_mode == 0 || time_mode == 1;
}

static inline int tomato_next_minutes(int time_mode)
{
    switch (time_mode)
    {
    case 0:
        return 5;
    case 1:
        return 15;
    case -1:
        return 25;
    case 2:
        return 45;
    default:
        return 5;
    }
}

#endif
