#ifndef AIO_STUB_SYS_APP_CONTROLLER_H
#define AIO_STUB_SYS_APP_CONTROLLER_H

#include "Arduino.h"
#include "common.h"
#include "interface.h"
#include <list>

#define CTRL_NAME "AppCtrl"
#define APP_MAX_NUM 20
#define WIFI_LIFE_CYCLE 60000
#define MQTT_ALIVE_CYCLE 1000
#define EVENT_LIST_MAX_LENGTH 10
#define APP_CONTROLLER_NAME_LEN 16

struct EVENT_OBJ {
    const APP_OBJ *from;
    APP_MESSAGE_TYPE type;
    void *info;
    uint8_t retryMaxNum;
    uint8_t retryCount;
    unsigned long nextRunTime;
};

class AppController {
public:
    AppController(const char *name = CTRL_NAME);
    ~AppController();
    void init();
    void Display();
    int app_auto_start();
    int app_install(APP_OBJ *app, APP_TYPE app_type = APP_TYPE_REAL_TIME);
    int app_uninstall(const APP_OBJ *app);
    int remove_backgroud_task();
    int main_process(ImuAction *act_info);
    void app_exit();
    int send_to(const char *from, const char *to,
                APP_MESSAGE_TYPE type, void *message, void *ext_info);
    void deal_config(APP_MESSAGE_TYPE, const char *, char *) {}
    int req_event_deal() { return 0; }
    bool wifi_event(APP_MESSAGE_TYPE) { return true; }
    void read_config(SysUtilConfig *) {}
    void write_config(SysUtilConfig *) {}
    void read_config(SysMpuConfig *) {}
    void write_config(SysMpuConfig *) {}
    void read_config(RgbConfig *) {}
    void write_config(RgbConfig *) {}

public:
    SysUtilConfig sys_cfg;
    SysMpuConfig mpu_cfg;
    RgbConfig rgb_cfg;

private:
    char name[APP_CONTROLLER_NAME_LEN];
    APP_OBJ *appList[APP_MAX_NUM];
    APP_TYPE appTypeList[APP_MAX_NUM];
    unsigned int app_num = 0;
    boolean app_exit_flag = 0;
    int cur_app_index = 0;
    int pre_app_index = 0;
};

#endif
