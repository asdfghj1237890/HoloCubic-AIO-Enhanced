#include "app_controller.h"
#include "app_controller_gui.h"
#include "common.h"
#include "interface.h"
#include "send_to_dispatch.h"
#include "Arduino.h"

const char *app_event_type_info[] = {"APP_MESSAGE_WIFI_CONN", "APP_MESSAGE_WIFI_AP",
                                     "APP_MESSAGE_WIFI_ALIVE", "APP_MESSAGE_WIFI_DISCONN",
                                     "APP_MESSAGE_UPDATE_TIME", "APP_MESSAGE_MQTT_DATA",
                                     "APP_MESSAGE_GET_PARAM", "APP_MESSAGE_SET_PARAM",
                                     "APP_MESSAGE_READ_CFG", "APP_MESSAGE_WRITE_CFG",
                                     "APP_MESSAGE_NONE"};

volatile static bool isRunEventDeal = false;

// TickType_t mainFormRefreshLastTime;
// const TickType_t xDelay500ms = pdMS_TO_TICKS(500);
// mainFormRefreshLastTime = xTaskGetTickCount();
// vTaskDelayUntil(&mainFormRefreshLastTime, xDelay500ms);

void eventDealHandle(TimerHandle_t xTimer)
{
    isRunEventDeal = true;
}

AppController::AppController(const char *name)
{
    strncpy(this->name, name, APP_CONTROLLER_NAME_LEN);
    app_num = 0;
    app_exit_flag = 0;
    cur_app_index = 0;
    pre_app_index = 0;
    // appList = new APP_OBJ[APP_MAX_NUM];
    m_wifi_status = false;
    m_preWifiReqMillis = GET_SYS_MILLIS();
    m_preBackgroundTaskMillis = GET_SYS_MILLIS();

    // 定义一个事件处理定时器
    xTimerEventDeal = xTimerCreate("Event Deal",
                                   300 / portTICK_PERIOD_MS,
                                   pdTRUE, (void *)0, eventDealHandle);
    // 启动事件处理定时器
    xTimerStart(xTimerEventDeal, 0);
}

void AppController::init(void)
{
    // 设置CPU主频
    if (1 == this->sys_cfg.power_mode)
    {
        setCpuFrequencyMhz(240);
    }
    else
    {
        setCpuFrequencyMhz(80);
    }
    // uint32_t freq = getXtalFrequencyMhz(); // In MHz
    Serial.print(F("CpuFrequencyMhz: "));
    Serial.println(getCpuFrequencyMhz());

    app_control_gui_init();
    appList[0] = new APP_OBJ();
    appList[0]->app_image = &app_loading;
    appList[0]->app_name = "Loading...";
    appTypeList[0] = APP_TYPE_REAL_TIME;
    app_control_display_scr(appList[cur_app_index]->app_image,
                            appList[cur_app_index]->app_name,
                            LV_SCR_LOAD_ANIM_NONE, true);
    // Display();
}

void AppController::Display()
{
    // appList[0].app_image = &app_loading;
    app_control_display_scr(appList[cur_app_index]->app_image,
                            appList[cur_app_index]->app_name,
                            LV_SCR_LOAD_ANIM_NONE, true);
}

AppController::~AppController()
{
    rgb_stop();
}

int AppController::app_is_legal(const APP_OBJ *app_obj)
{
    // APP的合法性检测
    if (NULL == app_obj)
        return 1;
    if (APP_MAX_NUM <= app_num)
        return 2;
    return 0;
}

// 将APP安装到app_controller中
int AppController::app_install(APP_OBJ *app, APP_TYPE app_type)
{
    int ret_code = app_is_legal(app);
    if (0 != ret_code)
    {
        return ret_code;
    }

    appList[app_num] = app;
    appTypeList[app_num] = app_type;
    ++app_num;
    return 0; // 安装成功
}

// 将APP的后台任务从任务队列中移除(自能通过APP退出的时候，移除自身的后台任务)
int AppController::remove_backgroud_task(void)
{
    if (cur_app_index < 0 || cur_app_index >= (int)app_num || NULL == appList[cur_app_index])
    {
        return 1;
    }

    appList[cur_app_index]->background_task = NULL;
    return 0;
}

// 将APP从app_controller中卸载（删除）
int AppController::app_uninstall(const APP_OBJ *app)
{
    if (NULL == app)
    {
        return 1;
    }

    int app_index = -1;
    for (int pos = 0; pos < (int)app_num; ++pos)
    {
        if (appList[pos] == app)
        {
            app_index = pos;
            break;
        }
    }

    if (app_index < 0)
    {
        return 2;
    }

    if (1 == app_exit_flag && cur_app_index == app_index)
    {
        app_exit();
    }

    for (int pos = app_index; pos < (int)app_num - 1; ++pos)
    {
        appList[pos] = appList[pos + 1];
        appTypeList[pos] = appTypeList[pos + 1];
    }

    appList[app_num - 1] = NULL;
    appTypeList[app_num - 1] = APP_TYPE_NONE;
    --app_num;

    if (0 == app_num)
    {
        cur_app_index = 0;
        pre_app_index = 0;
        app_exit_flag = 0;
        return 0;
    }

    if (cur_app_index > app_index)
    {
        --cur_app_index;
    }
    if (pre_app_index > app_index)
    {
        --pre_app_index;
    }
    if (cur_app_index >= (int)app_num)
    {
        cur_app_index = app_num - 1;
    }
    if (pre_app_index >= (int)app_num)
    {
        pre_app_index = cur_app_index;
    }

    if (0 == app_exit_flag && false == app_is_launchable(cur_app_index))
    {
        int next_app_index = get_next_launchable_app_idx(cur_app_index, 1);
        if (next_app_index >= 0)
        {
            cur_app_index = next_app_index;
        }
    }

    return 0;
}

bool AppController::app_is_launchable(int app_index) const
{
    if (app_index < 0 || app_index >= (int)app_num)
    {
        return false;
    }

    return NULL != appList[app_index] && APP_TYPE_REAL_TIME == appTypeList[app_index];
}

int AppController::get_next_launchable_app_idx(int start_index, int step)
{
    if (0 == app_num)
    {
        return -1;
    }

    int app_index = start_index;
    for (unsigned int count = 0; count < app_num; ++count)
    {
        app_index = (app_index + step + app_num) % app_num;
        if (app_is_launchable(app_index))
        {
            return app_index;
        }
    }

    return -1;
}

int AppController::run_background_tasks(const ImuAction *act_info)
{
    for (unsigned int pos = 0; pos < app_num; ++pos)
    {
        if (NULL == appList[pos] || NULL == appList[pos]->background_task)
        {
            continue;
        }

        if (APP_TYPE_REAL_TIME == appTypeList[pos] && 1 == app_exit_flag && (int)pos == cur_app_index)
        {
            continue;
        }

        (*(appList[pos]->background_task))(this, act_info);
    }

    return 0;
}

int AppController::app_auto_start()
{
    // APP自启动
    int index = this->getAppIdxByName(sys_cfg.auto_start_app.c_str());
    if (index < 0 || false == app_is_launchable(index))
    {
        // 没找到相关的APP
        return 0;
    }
    // 进入自启动的APP
    app_exit_flag = 1; // 进入app, 如果已经在
    cur_app_index = index;
    (*(appList[cur_app_index]->app_init))(this); // 执行APP初始化
    return 0;
}

int AppController::main_process(ImuAction *act_info)
{
    if (ACTIVE_TYPE::UNKNOWN != act_info->active)
    {
        Serial.print(F("[Operate]\tact_info->active: "));
        Serial.println(active_type_info[act_info->active]);
    }

    if (isRunEventDeal)
    {
        isRunEventDeal = false;
        // 扫描事件
        this->req_event_deal();
    }

    // wifi自动关闭(在节能模式下)
    if (0 == sys_cfg.power_mode && true == m_wifi_status && doDelayMillisTime(WIFI_LIFE_CYCLE, &m_preWifiReqMillis, false))
    {
        send_to(CTRL_NAME, CTRL_NAME, APP_MESSAGE_WIFI_DISCONN, 0, NULL);
    }

    if (doDelayMillisTime(BACKGROUND_TASK_CYCLE, &m_preBackgroundTaskMillis, false))
    {
        run_background_tasks(act_info);
    }

    if (0 == app_exit_flag)
    {
        if (false == app_is_launchable(cur_app_index))
        {
            int next_app_index = get_next_launchable_app_idx(cur_app_index, 1);
            if (next_app_index >= 0)
            {
                cur_app_index = next_app_index;
            }
        }

        // 当前没有进入任何app
        lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;
        if (ACTIVE_TYPE::TURN_LEFT == act_info->active)
        {
            anim_type = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
            pre_app_index = cur_app_index;
            int next_app_index = get_next_launchable_app_idx(cur_app_index, 1);
            if (next_app_index >= 0)
            {
                cur_app_index = next_app_index;
            }
            Serial.println(String("Current App: ") + appList[cur_app_index]->app_name);
        }
        else if (ACTIVE_TYPE::TURN_RIGHT == act_info->active)
        {
            anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;
            pre_app_index = cur_app_index;
            int next_app_index = get_next_launchable_app_idx(cur_app_index, -1);
            if (next_app_index >= 0)
            {
                cur_app_index = next_app_index;
            }
            Serial.println(String("Current App: ") + appList[cur_app_index]->app_name);
        }
        else if (ACTIVE_TYPE::GO_FORWORD == act_info->active)
        {
            if (app_is_launchable(cur_app_index))
            {
                app_exit_flag = 1; // 进入app
                if (NULL != appList[cur_app_index]->app_init)
                {
                    (*(appList[cur_app_index]->app_init))(this); // 执行APP初始化
                }
            }
        }

        if (ACTIVE_TYPE::GO_FORWORD != act_info->active) // && UNKNOWN != act_info->active
        {
            app_control_display_scr(appList[cur_app_index]->app_image,
                                    appList[cur_app_index]->app_name,
                                    anim_type, false);
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }
    }
    else
    {
        app_control_display_scr(appList[cur_app_index]->app_image,
                                appList[cur_app_index]->app_name,
                                LV_SCR_LOAD_ANIM_NONE, false);
        // 运行APP进程 等效于把控制权交给当前APP
        (*(appList[cur_app_index]->main_process))(this, act_info);
    }
    act_info->active = ACTIVE_TYPE::UNKNOWN;
    act_info->isValid = 0;
    act_info->long_time = false;
    return 0;
}

APP_OBJ *AppController::getAppByName(const char *name)
{
    for (int pos = 0; pos < app_num; ++pos)
    {
        if (!strcmp(name, appList[pos]->app_name))
        {
            return appList[pos];
        }
    }

    return NULL;
}

int AppController::getAppIdxByName(const char *name)
{
    for (int pos = 0; pos < app_num; ++pos)
    {
        if (!strcmp(name, appList[pos]->app_name))
        {
            return pos;
        }
    }

    return -1;
}

// 通信中心（消息转发）
int AppController::send_to(const char *from, const char *to,
                           APP_MESSAGE_TYPE type, void *message,
                           void *ext_info)
{
    APP_OBJ *fromApp = getAppByName(from); // 来自谁 有可能为空
    APP_OBJ *toApp = getAppByName(to);     // 发送给谁 有可能为空

    // The queue/dispatch logic lives in send_to_dispatch.cpp so the
    // host-side Unity unit test (test/native/test_app_controller) can
    // exercise it without standing up the full AppController. Side
    // effects that need controller state (Serial logging, deal_config
    // routing) stay here.
    int rc = send_to_dispatch(fromApp, toApp, from, to,
                              type, message, ext_info, &eventList);

    if (type <= APP_MESSAGE_MQTT_DATA && rc == 0) {
        Serial.print("[EVENT]\tAdd -> " + String(app_event_type_info[type]));
        Serial.print(F("\tEventList Size: "));
        Serial.println(eventList.size());
    } else if (type > APP_MESSAGE_MQTT_DATA) {
        if (rc == 0 && toApp != NULL) {
            Serial.print("[Massage]\tFrom " + String(fromApp ? fromApp->app_name : "")
                         + "\tTo " + String(toApp->app_name) + "\n");
        } else if (rc == 2 && !strcmp(to, CTRL_NAME)) {
            Serial.print("[Massage]\tFrom " + String(fromApp ? fromApp->app_name : "")
                         + "\tTo " + CTRL_NAME + "\n");
            deal_config(type, (const char *)message, (char *)ext_info);
            rc = 0;
        }
    }
    return rc == 2 ? 0 : rc;
}

int AppController::req_event_deal(void)
{
    // 请求事件的处理
    for (std::list<EVENT_OBJ>::iterator event = eventList.begin(); event != eventList.end();)
    {
        if ((*event).nextRunTime > GET_SYS_MILLIS())
        {
            ++event;
            continue;
        }
        // 后期可以拓展其他事件的处理
        bool ret = wifi_event((*event).type);
        if (false == ret)
        {
            // 本事件没处理完成
            (*event).retryCount += 1;
            if ((*event).retryCount >= (*event).retryMaxNum)
            {
                // 多次重试失败
                Serial.print("[EVENT]\tDelete -> " + String(app_event_type_info[(*event).type]));
                event = eventList.erase(event); // 删除该响应事件
                Serial.print(F("\tEventList Size: "));
                Serial.println(eventList.size());
            }
            else
            {
                // 下次重试
                (*event).nextRunTime = GET_SYS_MILLIS() + 4000;
                ++event;
            }
            continue;
        }

        // 事件回调
        if (NULL != (*event).from && NULL != (*event).from->message_handle)
        {
            (*((*event).from->message_handle))(CTRL_NAME, (*event).from->app_name,
                                               (*event).type, (*event).info, NULL);
        }
        Serial.print("[EVENT]\tDelete -> " + String(app_event_type_info[(*event).type]));
        event = eventList.erase(event); // 删除该响应完成的事件
        Serial.print(F("\tEventList Size: "));
        Serial.println(eventList.size());
    }
    return 0;
}

/**
 *  WiFi event handling
 *  Returns true if event handled successfully, false otherwise
 * */
bool AppController::wifi_event(APP_MESSAGE_TYPE type)
{
    switch (type)
    {
    case APP_MESSAGE_WIFI_CONN:
    {
        // Update request timestamp
        m_preWifiReqMillis = GET_SYS_MILLIS();
        
        // Check actual WiFi connection status (not just internal flag)
        // This fixes reconnection bug when WiFi drops after being connected
        if (WiFi.status() != WL_CONNECTED)
        {
            // WiFi not connected - always try to start connection
            // start_conn_wifi() internally checks if already connecting
            g_network.start_conn_wifi(sys_cfg.ssid_0.c_str(), sys_cfg.password_0.c_str());
            m_wifi_status = true;
            
            // Check connection result
            int conn_result = g_network.end_conn_wifi();
            if (CONN_TIMEOUT == conn_result)
            {
                // Connection timeout - stop to avoid continuous retry loop
                Serial.println(F("[WiFi] Connection timeout. Stopping attempts."));
                g_network.close_wifi();
                m_wifi_status = false;
                return true; // Mark as handled to prevent infinite retry
            }
            else if (CONN_SUCC != conn_result)
            {
                // Still trying to connect, event will be retried
                return false;
            }
        }
        // WiFi connected successfully
        m_wifi_status = true;
    }
    break;
    case APP_MESSAGE_WIFI_AP:
    {
        // 更新请求
        g_network.open_ap(AP_SSID);
        m_wifi_status = true;
        m_preWifiReqMillis = GET_SYS_MILLIS();
    }
    break;
    case APP_MESSAGE_WIFI_ALIVE:
    {
        // wifi开关的心跳 持续收到心跳 wifi才不会被关闭
        m_wifi_status = true;
        // 更新请求
        m_preWifiReqMillis = GET_SYS_MILLIS();
    }
    break;
    case APP_MESSAGE_WIFI_DISCONN:
    {
        g_network.close_wifi();
        m_wifi_status = false; // 标志位
        // m_preWifiReqMillis = GET_SYS_MILLIS() - WIFI_LIFE_CYCLE;
    }
    break;
    case APP_MESSAGE_UPDATE_TIME:
    {
    }
    break;
    case APP_MESSAGE_MQTT_DATA:
    {
        Serial.println("APP_MESSAGE_MQTT_DATA");
        if (app_exit_flag == 1 && cur_app_index != getAppIdxByName("Heartbeat")) // 在其他app中
        {
            app_exit_flag = 0;
            (*(appList[cur_app_index]->exit_callback))(NULL); // 退出当前app
        }
        if (app_exit_flag == 0)
        {
            app_exit_flag = 1; // 进入app, 如果已经在
            cur_app_index = getAppIdxByName("Heartbeat");
            (*(getAppByName("Heartbeat")->app_init))(this); // 执行APP初始化
        }
    }
    break;
    default:
        break;
    }

    return true;
}

void AppController::app_exit()
{
    app_exit_flag = 0; // 退出APP

    // 清空该对象的所有请求
    for (std::list<EVENT_OBJ>::iterator event = eventList.begin(); event != eventList.end();)
    {
        if (appList[cur_app_index] == (*event).from)
        {
            event = eventList.erase(event); // 删除该响应事件
        }
        else
        {
            ++event;
        }
    }

    if (NULL != appList[cur_app_index]->exit_callback)
    {
        // 执行APP退出回调
        (*(appList[cur_app_index]->exit_callback))(NULL);
    }
    app_control_display_scr(appList[cur_app_index]->app_image,
                            appList[cur_app_index]->app_name,
                            LV_SCR_LOAD_ANIM_NONE, true);

    // 恢复RGB灯  HSV色彩模式
    RgbConfig *cfg = &rgb_cfg;
    RgbParam rgb_setting = {LED_MODE_HSV,
                            cfg->min_value_0, cfg->min_value_1, cfg->min_value_2,
                            cfg->max_value_0, cfg->max_value_1, cfg->max_value_2,
                            cfg->step_0, cfg->step_1, cfg->step_2,
                            cfg->min_brightness, cfg->max_brightness,
                            cfg->brightness_step, cfg->time};
    set_rgb_and_run(&rgb_setting);

    // 设置CPU主频
    if (1 == this->sys_cfg.power_mode)
    {
        setCpuFrequencyMhz(240);
    }
    else
    {
        setCpuFrequencyMhz(80);
    }
    Serial.print(F("CpuFrequencyMhz: "));
    Serial.println(getCpuFrequencyMhz());
}
