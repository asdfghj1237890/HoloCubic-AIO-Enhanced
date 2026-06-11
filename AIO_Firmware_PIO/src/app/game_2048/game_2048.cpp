#include "game_2048.h"
#include "game_2048_gui.h"
#include "game2048_contorller.h"
#include "sys/app_controller.h"
#include "common.h"
#include "freertos/semphr.h"

#define G2048_APP_NAME "2048"
#define G2048_MOVE_ANIM_MS 700UL
#define G2048_BORN_ANIM_MS 300UL

void taskOne(void *parameter)
{
    while (1)
    {
        // 心跳任务
        //  lv_tick_inc(5); // todo
        delay(5);
    }
    Serial.println("Ending task 1");
    vTaskDelete(NULL);
}

void taskTwo(void *parameter)
{
    while (1)
    {
        // LVGL任务主函数
        AIO_LVGL_OPERATE_LOCK(lv_task_handler();)
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
    Serial.println("Ending lv_task_handler");
    vTaskDelete(NULL);
}

GAME2048 game;

struct Game2048AppRunData
{
    int Normal = 0;       // 记录移动的方向
    int BornLocation = 0; // 记录新棋子的位置
    int *pBoard;
    int *moveRecord;
    BaseType_t xReturned_task_one = pdFALSE;
    TaskHandle_t xHandle_task_one = NULL;
    BaseType_t xReturned_task_two = pdFALSE;
    TaskHandle_t xHandle_task_two = NULL;
    bool pendingNewBorn;
    unsigned long newBornDueMillis;
    unsigned long inputLockedUntilMillis;
};

static Game2048AppRunData *run_data = NULL;

static void game_2048_schedule_newborn()
{
    if (NULL == run_data)
    {
        return;
    }

    run_data->pendingNewBorn = true;
    run_data->newBornDueMillis = GET_SYS_MILLIS() + G2048_MOVE_ANIM_MS;
    run_data->inputLockedUntilMillis = 0;
}

static bool game_2048_waiting_newborn()
{
    if (NULL == run_data)
    {
        return false;
    }

    if (run_data->pendingNewBorn)
    {
        if ((long)(GET_SYS_MILLIS() - run_data->newBornDueMillis) < 0)
        {
            return true;
        }

        run_data->pendingNewBorn = false;
        AIO_LVGL_OPERATE_LOCK(showNewBorn(game.addRandom(), run_data->pBoard);)
        run_data->inputLockedUntilMillis = GET_SYS_MILLIS() + G2048_BORN_ANIM_MS;
        return true;
    }

    if (run_data->inputLockedUntilMillis != 0 &&
        (long)(GET_SYS_MILLIS() - run_data->inputLockedUntilMillis) < 0)
    {
        return true;
    }

    run_data->inputLockedUntilMillis = 0;
    return false;
}

static int game_2048_init(AppController *sys)
{
    // 初始化运行时的参数
    game_2048_gui_init();

    randomSeed(analogRead(25));
    // 初始化运行时参数
    run_data = (Game2048AppRunData *)calloc(1, sizeof(Game2048AppRunData));
    game.init();
    run_data->pBoard = game.getBoard();
    run_data->moveRecord = game.getMoveRecord();

    // run_data->xReturned_task_one = xTaskCreate(
    //     taskOne,                      /*任务函数*/
    //     "TaskOne",                    /*带任务名称的字符串*/
    //     10000,                        /*堆栈大小，单位为字节*/
    //     NULL,                         /*作为任务输入传递的参数*/
    //     1,                            /*任务的优先级*/
    //     &run_data->xHandle_task_one); /*任务句柄*/

    run_data->xReturned_task_two = xTaskCreate(
        taskTwo,                      /*任务函数*/
        "TaskTwo",                    /*带任务名称的字符串*/
        8 * 1024,                     /*堆栈大小，单位为字节*/
        NULL,                         /*作为任务输入传递的参数*/
        1,                            /*任务的优先级*/
        &run_data->xHandle_task_two); /*任务句柄*/
    if (pdPASS != run_data->xReturned_task_two)
    {
        Serial.printf("[2048] xTaskCreate(TaskTwo) failed (rc=%d) — running without async helper\n",
                      run_data->xReturned_task_two);
    }

    // 刷新棋盘显示
    int new1 = game.addRandom();
    int new2 = game.addRandom();
    AIO_LVGL_OPERATE_LOCK(showBoard(run_data->pBoard);)
    // 棋子出生动画
    AIO_LVGL_OPERATE_LOCK(born(new1);)
    AIO_LVGL_OPERATE_LOCK(born(new2);)
    // 防止进入游戏时，误触发了向上
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    return 0;
}

static void game_2048_process(AppController *sys,
                              const ImuAction *act_info)
{
    ACTIVE_TYPE action = act_info->active;
    if (RETURN == action && act_info->long_time)
    {
        action = DOWN;
    }

    if (RETURN == action)
    {
        sys->app_exit(); // 退出APP
        return;
    }

    if (game_2048_waiting_newborn())
    {
        return;
    }

    // 具体操作
    if (TURN_RIGHT == action)
    {
        game.moveRight();
        if (game.comparePre() == 0)
        {
            AIO_LVGL_OPERATE_LOCK(showAnim(run_data->moveRecord, 4);)
            game_2048_schedule_newborn();
        }
    }
    else if (TURN_LEFT == action)
    {
        game.moveLeft();
        if (game.comparePre() == 0)
        {
            AIO_LVGL_OPERATE_LOCK(showAnim(run_data->moveRecord, 3);)
            game_2048_schedule_newborn();
        }
    }
    else if (UP == action)
    {
        game.moveUp();
        if (game.comparePre() == 0)
        {
            AIO_LVGL_OPERATE_LOCK(showAnim(run_data->moveRecord, 1);)
            game_2048_schedule_newborn();
        }
    }
    else if (DOWN == action)
    {
        game.moveDown();
        if (game.comparePre() == 0)
        {
            AIO_LVGL_OPERATE_LOCK(showAnim(run_data->moveRecord, 2);)
            game_2048_schedule_newborn();
        }
    }

    if (game.judge() == 1)
    {
        //   rgb.setRGB(0, 255, 0);
        Serial.println("you win!");
    }
    else if (game.judge() == 2)
    {
        //   rgb.setRGB(255, 0, 0);
        Serial.println("you lose!");
    }

    // Movement animation completion is handled by game_2048_waiting_newborn()
    // on later loop ticks instead of blocking the whole controller here.
}

static void game_2048_background_task(AppController *sys,
                                      const ImuAction *act_info)
{
    // 本函数为后台任务，主控制器会间隔一分钟调用此函数
    // 本函数尽量只调用"常驻数据",其他变量可能会因为生命周期的缘故已经释放
}

static int game_2048_exit_callback(void *param)
{
    // 查杀任务
    if (run_data->xReturned_task_one == pdPASS)
    {
        vTaskDelete(run_data->xHandle_task_one);
    }
    if (run_data->xReturned_task_two == pdPASS)
    {
        vTaskDelete(run_data->xHandle_task_two);
    }

    xSemaphoreGive(lvgl_mutex);

    game_2048_gui_del();

    // 释放运行数据
    if (NULL != run_data)
    {
        free(run_data);
        run_data = NULL;
    }
    return 0;
}

static void game_2048_message_handle(const char *from, const char *to,
                                     APP_MESSAGE_TYPE type, void *message,
                                     void *ext_info)
{
    // 目前事件主要是wifi开关类事件（用于功耗控制）
    switch (type)
    {
    case APP_MESSAGE_WIFI_CONN:
    {
        // todo
    }
    break;
    case APP_MESSAGE_WIFI_AP:
    {
        // todo
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

APP_OBJ game_2048_app = {G2048_APP_NAME, &app_game_2048, "",
                         game_2048_init, game_2048_process, game_2048_background_task,
                         game_2048_exit_callback, game_2048_message_handle};
