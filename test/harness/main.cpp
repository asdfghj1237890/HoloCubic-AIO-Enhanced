// HoloCubic_AIO regression-test harness — host entry point.
//
// Initialises LVGL + the SDL2 monitor driver from lv_drivers, registers the
// firmware's anniversary app, and ticks LVGL in a loop while pumping SDL
// events. Phase 1 walking skeleton: interactive only, no scenario runner yet.
//
// Build: see lv_simulater_platformio/platformio.ini env:native_test

#define LV_LVGL_H_INCLUDE_SIMPLE
#define SDL_MAIN_HANDLED
#include "lvgl.h"
#include "sdl/sdl.h"

#include "Arduino.h"
#include "common.h"
#include "sys/app_controller.h"
#include "driver/imu.h"
#include "app/anniversary/anniversary.h"

#include <SDL2/SDL.h>
#include <stdio.h>

static int tick_thread(void *) {
    while (1) {
        SDL_Delay(5);
        lv_tick_inc(5);
    }
    return 0;
}

#define DISP_HOR_RES 240
#define DISP_VER_RES 240
#define DISP_BUF_LINES 80

static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf1[DISP_HOR_RES * DISP_BUF_LINES];
static lv_color_t buf2[DISP_HOR_RES * DISP_BUF_LINES];
static lv_disp_drv_t disp_drv;

static lv_indev_drv_t kb_indev_drv;
static lv_indev_t *kb_indev;

static AppController *g_controller = nullptr;
static ImuAction g_action;

static ACTIVE_TYPE map_sdl_key(SDL_Keycode k) {
    switch (k) {
        case SDLK_RIGHT:    return TURN_RIGHT;
        case SDLK_LEFT:     return TURN_LEFT;
        case SDLK_UP:       return UP;
        case SDLK_DOWN:     return DOWN;
        case SDLK_RETURN:
        case SDLK_SPACE:    return GO_FORWORD;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE: return RETURN;
        case SDLK_s:        return SHAKE;
        default:            return UNKNOWN;
    }
}

static void process_sdl_input() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_KEYDOWN) {
            ACTIVE_TYPE a = map_sdl_key(e.key.keysym.sym);
            if (a != UNKNOWN) {
                g_action.active = a;
                g_action.isValid = true;
                printf("[harness] SDL keydown -> action=%s\n", active_type_info[a]);
            }
        }
        if (e.type == SDL_QUIT) {
            exit(0);
        }
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("[harness] HoloCubic_AIO regression harness — Phase 1 walking skeleton\n");

    lv_init();

    // SDL display from lv_drivers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISP_HOR_RES * DISP_BUF_LINES);
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = DISP_HOR_RES;
    disp_drv.ver_res = DISP_VER_RES;
    lv_disp_drv_register(&disp_drv);

    // Keyboard / mouse via SDL — used for LVGL focus/input later. For now we
    // also poll SDL ourselves to translate keys into ImuAction.
    lv_indev_drv_init(&kb_indev_drv);
    kb_indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_indev_drv.read_cb = sdl_keyboard_read;
    kb_indev = lv_indev_drv_register(&kb_indev_drv);

    sdl_init();
    SDL_CreateThread(tick_thread, "tick", NULL);

    // Hand-off to firmware: install the anniversary app and run its init.
    g_controller = new AppController("AppCtrl");
    g_controller->init();
    g_controller->app_install(&anniversary_app, APP_TYPE_REAL_TIME);

    // For the walking skeleton, auto-enter the first app on startup so the
    // user immediately sees rendering rather than a black screen.
    g_action.active = GO_FORWORD;
    g_action.isValid = true;
    g_controller->main_process(&g_action);

    printf("[harness] entering main loop — keys: arrows / Enter / Esc / S / Q to quit\n");

    // Main tick loop
    while (true) {
        process_sdl_input();
        if (g_action.isValid) {
            g_controller->main_process(&g_action);
        }
        lv_timer_handler();
        SDL_Delay(5);
    }
    return 0;
}
