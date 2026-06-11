// HoloCubic_AIO regression-test harness — host entry point.
//
// Initialises LVGL + the built-in SDL2 display driver and either
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
#include "src/drivers/sdl/lv_sdl_window.h"

#include "Arduino.h"
#include "common.h"
#include "sys/app_controller.h"
#include "driver/imu.h"
#include "app/anniversary/anniversary.h"
#include "app/tomato/tomato.h"
#include "app/game_2048/game_2048.h"
#include "app/heartbeat/heartbeat.h"
// app/settings/settings.h removed in B15 fix PR3 — module was dead code.
#include "app/game_snake/game_snake.h"
#include "app/example/example.h"
#include "app/bilibili_fans/bilibili.h"
#include "app/weather/weather.h"
#include "app/weather_old/weather_old.h"
#include "app/stockmarket/stockmarket.h"
#include "app/file_manager/file_manager.h"
#include "app/server/server.h"
#include "app/idea_anim/idea.h"
#include "app/LHLXW/LHLXW.h"
#include "app/screen_share/screen_share.h"
#include "app/pc_resource/pc_resource.h"
#include "app/picture/picture.h"
#include "app/media_player/media_player.h"

#include "scenario_runner.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>

static int tick_thread(void *) {
    while (1) {
        SDL_Delay(5);
        lv_tick_inc(5);
    }
    return 0;
}

// Print a glibc backtrace on SIGSEGV/SIGABRT so silent firmware crashes
// surface a call site in CI logs. backtrace_symbols_fd is async-signal-safe;
// addresses are unmangled but pinpoint the crashing function in most cases.
static void crash_handler(int sig) {
    // Flush stdout first so buffered diagnostic prints survive the crash —
    // CI runs stdout fully-buffered to a pipe, so plain printf()s upstream
    // would otherwise be lost. (fflush is not strictly async-signal-safe
    // but is what we have; the alternative is silent diagnostics.)
    fflush(stdout);
    fprintf(stderr, "\n[harness] caught signal %d — backtrace:\n", sig);
    void *frames[64];
    int n = backtrace(frames, 64);
    backtrace_symbols_fd(frames, n, STDERR_FILENO);
    fflush(stderr);
    _exit(128 + sig);
}

static void install_crash_handler() {
    struct sigaction sa;
    sa.sa_handler = crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
}

#define DISP_HOR_RES 240
#define DISP_VER_RES 240
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
    { "tomato",      &tomato_app },
    { "2048",        &game_2048_app },
    { "heartbeat",   &heartbeat_app },
    // settings app removed in B15 fix PR3 — module was dead code.
    { "snake",       &game_snake_app },
    { "example",     &example_app },
    { "bilibili",    &bilibili_app },
    { "weather",     &weather_app },
    { "weather_old", &weather_old_app },
    { "stockmarket", &stockmarket_app },
    { "file_manager",&file_manager_app },
    { "server",      &server_app },
    { "idea",        &idea_app },
    { "LHLXW",       &LHLXW_app },  // init_only smoke — see scenario file
    { "screen_share",&screen_share_app },
    { "pc_resource", &pc_resource_app },
    { "picture",     &picture_app },
    { "media",       &media_app },
};
static const int kRegisteredAppCount =
    sizeof(kRegisteredApps) / sizeof(kRegisteredApps[0]);

int main(int argc, char **argv) {
    install_crash_handler();
    // Line-buffer stdout so diagnostic printfs in firmware code aren't
    // lost in the pipe buffer when a downstream SIGSEGV kills us.
    setvbuf(stdout, nullptr, _IOLBF, 0);
    Args args = parse_args(argc, argv);
    printf("[harness] HoloCubic_AIO regression harness "
           "(scenario=%s headless=%d)\n",
           args.scenario ? args.scenario : "<interactive>",
           args.headless ? 1 : 0);

    lv_init();

    lv_display_t *display = lv_sdl_window_create(DISP_HOR_RES, DISP_VER_RES);
    lv_sdl_window_set_title(display, "HoloCubic AIO Regression");
    SDL_CreateThread(tick_thread, "tick", NULL);

    g_controller = new AppController("AppCtrl");
    g_controller->init();
    extern AppController *app_controller;
    app_controller = g_controller;

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
