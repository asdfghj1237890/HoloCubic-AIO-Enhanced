// HoloCubic_AIO regression-test harness — host entry point.
//
// Initialises LVGL + the SDL2 monitor driver from lv_drivers and either
// (a) runs a scripted scenario from disk and exits, or (b) drops into an
// interactive loop with keyboard input. Used by CI for non-interactive
// regression and by developers locally to poke at the UI.
//
// Usage:
//   program                              interactive (no exit)
//   program --scenario PATH              run scenario, exit with its status
//   program --scenario PATH --headless   same, but won't open a window if
//                                        SDL_VIDEODRIVER=dummy is set
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

#include "scenario_runner.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

struct Args {
    const char *scenario = nullptr;
    bool headless = false;
    bool update_golden = false;
    double threshold_pct = 0.5;
};

static Args parse_args(int argc, char **argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--scenario") && i + 1 < argc) {
            a.scenario = argv[++i];
        } else if (!strcmp(argv[i], "--headless")) {
            a.headless = true;
        } else if (!strcmp(argv[i], "--update-golden")) {
            a.update_golden = true;
        } else if (!strcmp(argv[i], "--threshold") && i + 1 < argc) {
            a.threshold_pct = strtod(argv[++i], nullptr);
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("Usage: %s [--scenario PATH] [--headless] "
                   "[--update-golden] [--threshold PCT]\n", argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "[harness] unknown arg '%s'\n", argv[i]);
            exit(2);
        }
    }
    return a;
}

// Apps registered with the harness — keep this in sync with build_src_filter
// in platformio.ini (each entry must have its sources actually compiled in).
static const ScenarioApp kRegisteredApps[] = {
    { "anniversary", &anniversary_app },
};
static const int kRegisteredAppCount =
    sizeof(kRegisteredApps) / sizeof(kRegisteredApps[0]);

int main(int argc, char **argv) {
    Args args = parse_args(argc, argv);
    printf("[harness] HoloCubic_AIO regression harness "
           "(scenario=%s headless=%d)\n",
           args.scenario ? args.scenario : "<interactive>",
           args.headless ? 1 : 0);

    lv_init();

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISP_HOR_RES * DISP_BUF_LINES);
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = DISP_HOR_RES;
    disp_drv.ver_res = DISP_VER_RES;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&kb_indev_drv);
    kb_indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_indev_drv.read_cb = sdl_keyboard_read;
    kb_indev = lv_indev_drv_register(&kb_indev_drv);

    sdl_init();
    SDL_CreateThread(tick_thread, "tick", NULL);

    g_controller = new AppController("AppCtrl");
    g_controller->init();

    if (args.scenario) {
        ScenarioOptions opts;
        opts.update_golden = args.update_golden;
        opts.diff_threshold_pct = args.threshold_pct;
        int rc = run_scenario(args.scenario, g_controller,
                              kRegisteredApps, kRegisteredAppCount, opts);
        printf("[harness] scenario exit rc=%d\n", rc);
        return rc;
    }

    // Interactive mode: install all known apps, auto-enter the first one,
    // then sit in the input loop until the window is closed.
    for (int i = 0; i < kRegisteredAppCount; ++i) {
        g_controller->app_install(kRegisteredApps[i].app, APP_TYPE_REAL_TIME);
    }
    g_action.active = GO_FORWORD;
    g_action.isValid = true;
    g_controller->main_process(&g_action);

    long max_frames = -1;
    if (const char *env = getenv("AIO_HARNESS_FRAMES")) {
        max_frames = strtol(env, nullptr, 10);
    }
    printf("[harness] entering interactive loop — arrows / Enter / Esc / S "
           "(max_frames=%ld)\n", max_frames);

    long frame = 0;
    while (max_frames < 0 || frame < max_frames) {
        process_sdl_input();
        if (g_action.isValid) {
            g_controller->main_process(&g_action);
        }
        lv_timer_handler();
        SDL_Delay(5);
        ++frame;
    }
    printf("[harness] frame cap reached, exiting cleanly\n");
    return 0;
}
