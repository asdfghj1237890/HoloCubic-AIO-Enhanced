#ifndef APP_PC_RESOURCE_GUI_H
#define APP_PC_RESOURCE_GUI_H

// 遥感器数据，带一位小数的数据均为扩大10倍后的整数部分
struct PC_Resource
{
    int cpu_usage; // CPU利用率(%)
    int cpu_temp;  // CPU温度(℃)，扩大10倍
    int cpu_freq;  // CPU主频(MHz)
    int cpu_power; // CPU功耗(W)，扩大10倍

    int gpu_usage; // GPU利用率(%)
    int gpu_temp;  // GPU温度(℃)，扩大10倍
    int gpu_power; // GPU功耗(W)，扩大10倍

    int ram_usage; // 内存RAM使用率(%)
    int ram_use;   // 内存RAM使用量(MB)

    int net_upload_speed;   // 网络上行速率(KB/s)，扩大10倍
    int net_download_speed; // 网络下行速率(KB/s)，扩大10倍
};

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
    extern const lv_img_dsc_t app_pc_resource;

    void display_pc_resource_gui_init(void);
    void display_pc_resource_init(void);
    void display_pc_resource(struct PC_Resource sensorInfo);
    void pc_resource_gui_release(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
