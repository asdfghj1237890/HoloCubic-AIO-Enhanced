// Screenshot capture and golden-image diff for the regression harness.
//
// save_screen_png snapshots whatever is on lv_scr_act() and writes a 240x240
// 24-bit PNG. compare_pngs loads both files and counts pixels whose RGB
// distance exceeds a fixed channel tolerance; if the percentage of differing
// pixels exceeds threshold_pct, it returns false and writes a diff
// visualisation (red on differing pixels, dimmed actual on the rest).

#ifndef AIO_HARNESS_SCREENSHOT_H
#define AIO_HARNESS_SCREENSHOT_H

namespace aio_screenshot {

// Snapshot the active LVGL screen, convert RGB565 -> RGB888, write PNG.
// Creates parent directories as needed. Returns false on I/O or LVGL error.
bool save_screen_png(const char *path);

// Load actual_path + golden_path (both 240x240 RGB), compare per-pixel.
// On mismatch beyond threshold_pct (e.g. 0.5 means 0.5%), writes a diff
// visualisation to diff_path and returns false. out_diff_pct is the
// percentage of differing pixels regardless of threshold.
bool compare_pngs(const char *actual_path,
                  const char *golden_path,
                  const char *diff_path,
                  double threshold_pct,
                  double *out_diff_pct);

// Best-effort recursive mkdir along the parent of `file_path` (POSIX).
// Exposed so other tooling can reuse it.
void ensure_parent_dir(const char *file_path);

} // namespace aio_screenshot

#endif
