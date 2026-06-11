#include "screenshot.h"

#include "lvgl.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#define AIO_MKDIR(p) _mkdir(p)
#else
#define AIO_MKDIR(p) mkdir((p), 0755)
#endif

namespace aio_screenshot {

void ensure_parent_dir(const char *file_path) {
    if (!file_path) return;
    std::string p(file_path);
    // Walk forward, creating each prefix that contains a '/'.
    for (size_t i = 1; i < p.size(); ++i) {
        if (p[i] == '/' || p[i] == '\\') {
            char saved = p[i];
            p[i] = '\0';
            AIO_MKDIR(p.c_str()); // ignore errors (e.g. EEXIST)
            p[i] = saved;
        }
    }
}

// LVGL renders RGB565 in little-endian when LV_COLOR_DEPTH=16 and
// LV_COLOR_16_SWAP=0 (the simulator default). Map each 565 word to a
// gamma-naive RGB888 triple via bit replication.
static inline void rgb565_to_rgb888(uint16_t px, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t r5 = (px >> 11) & 0x1F;
    uint8_t g6 = (px >> 5)  & 0x3F;
    uint8_t b5 = px         & 0x1F;
    *r = (uint8_t)((r5 << 3) | (r5 >> 2));
    *g = (uint8_t)((g6 << 2) | (g6 >> 4));
    *b = (uint8_t)((b5 << 3) | (b5 >> 2));
}

bool save_screen_png(const char *path) {
    if (!path) return false;
    lv_obj_t *scr = lv_scr_act();
    if (!scr) {
        fprintf(stderr, "[screenshot] no active LVGL screen\n");
        return false;
    }

    lv_draw_buf_t *snap = lv_snapshot_take(scr, LV_COLOR_FORMAT_RGB565);
    if (!snap) {
        fprintf(stderr, "[screenshot] lv_snapshot_take failed (LV_USE_SNAPSHOT not enabled?)\n");
        return false;
    }

    int w = snap->header.w;
    int h = snap->header.h;
    if (w <= 0 || h <= 0) {
        lv_draw_buf_destroy(snap);
        fprintf(stderr, "[screenshot] bad snapshot dims %dx%d\n", w, h);
        return false;
    }

    std::vector<uint8_t> rgb((size_t)w * h * 3);
    const uint8_t *src = snap->data;
    uint32_t stride = snap->header.stride ? snap->header.stride : (uint32_t)w * 2;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t src_i = (size_t)y * stride + (size_t)x * 2;
            size_t dst_i = ((size_t)y * w + (size_t)x) * 3;
            uint16_t px = (uint16_t)src[src_i] | ((uint16_t)src[src_i + 1] << 8);
            rgb565_to_rgb888(px, &rgb[dst_i], &rgb[dst_i + 1], &rgb[dst_i + 2]);
        }
    }

    ensure_parent_dir(path);
    int ok = stbi_write_png(path, w, h, 3, rgb.data(), w * 3);
    lv_draw_buf_destroy(snap);

    if (!ok) {
        fprintf(stderr, "[screenshot] stbi_write_png failed for %s\n", path);
        return false;
    }
    return true;
}

bool compare_pngs(const char *actual_path,
                  const char *golden_path,
                  const char *diff_path,
                  double threshold_pct,
                  double *out_diff_pct) {
    int aw, ah, ac, gw, gh, gc;
    uint8_t *a = stbi_load(actual_path, &aw, &ah, &ac, 3);
    if (!a) {
        fprintf(stderr, "[screenshot] cannot load actual '%s'\n", actual_path);
        return false;
    }
    uint8_t *g = stbi_load(golden_path, &gw, &gh, &gc, 3);
    if (!g) {
        fprintf(stderr, "[screenshot] cannot load golden '%s'\n", golden_path);
        stbi_image_free(a);
        return false;
    }
    if (aw != gw || ah != gh) {
        fprintf(stderr, "[screenshot] size mismatch: actual %dx%d vs golden %dx%d\n",
                aw, ah, gw, gh);
        stbi_image_free(a);
        stbi_image_free(g);
        return false;
    }

    const int total = aw * ah;
    int differing = 0;
    const int channel_tol = 6; // small allowance for 565<->888 round-trip noise

    std::vector<uint8_t> diff_rgb((size_t)total * 3);
    for (int i = 0; i < total; ++i) {
        int dr = (int)a[3 * i]     - (int)g[3 * i];
        int dg = (int)a[3 * i + 1] - (int)g[3 * i + 1];
        int db = (int)a[3 * i + 2] - (int)g[3 * i + 2];
        int max_d = (dr < 0 ? -dr : dr);
        int adg = (dg < 0 ? -dg : dg); if (adg > max_d) max_d = adg;
        int adb = (db < 0 ? -db : db); if (adb > max_d) max_d = adb;

        if (max_d > channel_tol) {
            ++differing;
            diff_rgb[3 * i]     = 255;
            diff_rgb[3 * i + 1] = 0;
            diff_rgb[3 * i + 2] = 0;
        } else {
            // Dim matching pixels so red stands out.
            diff_rgb[3 * i]     = a[3 * i]     / 4;
            diff_rgb[3 * i + 1] = a[3 * i + 1] / 4;
            diff_rgb[3 * i + 2] = a[3 * i + 2] / 4;
        }
    }
    stbi_image_free(a);
    stbi_image_free(g);

    double pct = (100.0 * differing) / (double)total;
    if (out_diff_pct) *out_diff_pct = pct;

    if (pct > threshold_pct) {
        ensure_parent_dir(diff_path);
        if (!stbi_write_png(diff_path, aw, ah, 3, diff_rgb.data(), aw * 3)) {
            fprintf(stderr, "[screenshot] stbi_write_png failed for diff %s\n", diff_path);
        }
        return false;
    }
    return true;
}

} // namespace aio_screenshot
