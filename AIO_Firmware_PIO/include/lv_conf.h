/**
 * Project LVGL configuration for firmware and host GUI regression builds.
 *
 * Keep this file small and explicit. LVGL 9's defaults enable many widgets,
 * while the project-specific choices here preserve the old 16-bit RGB565,
 * stdlib-backed allocation, required Montserrat fonts, snapshot support on
 * host, and SDL only for native harnesses.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

#define LV_DEF_REFR_PERIOD 33
#define LV_DPI_DEF 130

#define LV_DRAW_BUF_STRIDE_ALIGN 1
#define LV_DRAW_BUF_ALIGN 4
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE (24 * 1024)
#define LV_DRAW_LAYER_MAX_MEMORY 0

#define LV_USE_DRAW_SW 1
#if LV_USE_DRAW_SW
    #define LV_DRAW_SW_SUPPORT_RGB565 1
    #define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED 1
    #define LV_DRAW_SW_SUPPORT_RGB565A8 1
    #define LV_DRAW_SW_SUPPORT_RGB888 1
    #define LV_DRAW_SW_SUPPORT_XRGB8888 1
    #define LV_DRAW_SW_SUPPORT_ARGB8888 1
    #define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 1
    #define LV_DRAW_SW_SUPPORT_L8 1
    #define LV_DRAW_SW_SUPPORT_AL88 1
    #define LV_DRAW_SW_SUPPORT_A8 1
    #define LV_DRAW_SW_SUPPORT_I1 1
    #define LV_DRAW_SW_DRAW_UNIT_CNT 1
    #define LV_DRAW_SW_COMPLEX 1
    #if LV_DRAW_SW_COMPLEX
        #define LV_DRAW_SW_SHADOW_CACHE_SIZE 0
        #define LV_DRAW_SW_CIRCLE_CACHE_SIZE 4
    #endif
#endif

#if defined(AIO_NATIVE_TEST) || defined(AIO_LVGL_SIMULATOR)
    #define LV_USE_LOG 1
    #define LV_LOG_PRINTF 1
    #define LV_USE_SNAPSHOT 1
    // Host-test-only style checking: lv_style_init() on an already-inited
    // style logs "Potential memory leak" (the scenario runner fails the run
    // on it — see scenario_lv_log_watch_install), and LVGL asserts abort()
    // instead of the default while(1) so a bad style surfaces as a caught
    // SIGABRT + backtrace rather than a CI timeout. Never set on-device.
    #define LV_USE_ASSERT_STYLE 1
    #define LV_ASSERT_HANDLER_INCLUDE <stdlib.h>
    #define LV_ASSERT_HANDLER abort();
#else
    #define LV_USE_LOG 0
    #define LV_USE_SNAPSHOT 0
#endif

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_30 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_48 1

#if defined(AIO_NATIVE_TEST) || defined(AIO_LVGL_SIMULATOR)
    #define LV_USE_SDL 1
    #if LV_USE_SDL
        #define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
        #define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_DIRECT
        #define LV_SDL_BUF_COUNT 1
        #define LV_SDL_ACCELERATED 0
        #define LV_SDL_FULLSCREEN 0
        #define LV_SDL_DIRECT_EXIT 1
        #define LV_SDL_MOUSEWHEEL_MODE LV_SDL_MOUSEWHEEL_MODE_ENCODER
    #endif
#else
    #define LV_USE_SDL 0
#endif

#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS 0

#endif /* LV_CONF_H */
