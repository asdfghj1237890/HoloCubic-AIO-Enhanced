#include "scenario_runner.h"
#include "screenshot.h"

#include "lvgl.h"
#include "common.h"
#include "sys/app_controller.h"
#include "driver/imu.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <string>
#include <vector>

namespace {

enum class StepKind {
    WAIT_MS,
    ACTION,
    SCREENSHOT,
    ASSERT_NO_CRASH,
};

struct Step {
    StepKind kind;
    int int_arg = 0;
    ACTIVE_TYPE action = UNKNOWN;
    std::string str_arg;
    int line_no = 0;
};

ACTIVE_TYPE parse_action(const char *s) {
    if (!strcmp(s, "TURN_LEFT")) return TURN_LEFT;
    if (!strcmp(s, "TURN_RIGHT")) return TURN_RIGHT;
    if (!strcmp(s, "UP")) return UP;
    if (!strcmp(s, "DOWN")) return DOWN;
    if (!strcmp(s, "GO_FORWORD")) return GO_FORWORD;
    if (!strcmp(s, "GO_FORWARD")) return GO_FORWORD; // tolerate the typo-fixed spelling
    if (!strcmp(s, "RETURN")) return RETURN;
    if (!strcmp(s, "SHAKE")) return SHAKE;
    return UNKNOWN;
}

// Strip leading/trailing whitespace in place.
void trim(std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) { s.clear(); return; }
    s = s.substr(a, b - a + 1);
}

// Tick LVGL for ms_total milliseconds in 5 ms slices. SDL_Delay also
// advances the tick thread (see test/harness/main.cpp).
void tick_for(int ms_total) {
    int elapsed = 0;
    while (elapsed < ms_total) {
        lv_timer_handler();
        int slice = ms_total - elapsed > 5 ? 5 : ms_total - elapsed;
        SDL_Delay(slice);
        elapsed += slice;
    }
}

} // namespace

// Derive a scenario "name" from the file path: drop directory components and
// the .scn extension. Example: ".../anniversary/smoke.scn" -> "smoke", and
// the parent dir is "anniversary" — both used to layout golden/results paths.
static void derive_scenario_paths(const char *path,
                                  std::string *scenario_dir,
                                  std::string *scenario_stem) {
    std::string p(path ? path : "");
    // Normalise separators for parsing.
    for (auto &c : p) if (c == '\\') c = '/';

    size_t slash = p.find_last_of('/');
    std::string base = (slash == std::string::npos) ? p : p.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    *scenario_stem = base;

    // Parent directory name (e.g. "anniversary") drives the top-level dir.
    if (slash == std::string::npos) {
        *scenario_dir = "default";
        return;
    }
    std::string parent = p.substr(0, slash);
    size_t pslash = parent.find_last_of('/');
    *scenario_dir = (pslash == std::string::npos) ? parent : parent.substr(pslash + 1);
}

int run_scenario(const char *path,
                 AppController *controller,
                 const ScenarioApp *apps,
                 int app_count,
                 const ScenarioOptions &opts) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[scenario] cannot open '%s'\n", path);
        return 2;
    }

    std::string app_name;
    std::vector<Step> steps;

    char raw[512];
    int line_no = 0;
    while (fgets(raw, sizeof(raw), f)) {
        ++line_no;
        std::string line(raw);
        trim(line);
        if (line.empty() || line[0] == '#') continue;

        // Split into command + remainder.
        size_t sp = line.find_first_of(" \t");
        std::string cmd = (sp == std::string::npos) ? line : line.substr(0, sp);
        std::string arg = (sp == std::string::npos) ? "" : line.substr(sp + 1);
        trim(arg);

        if (cmd == "app") {
            if (!app_name.empty()) {
                fprintf(stderr, "[scenario] line %d: 'app' specified twice\n", line_no);
                fclose(f);
                return 3;
            }
            app_name = arg;
        } else if (cmd == "wait_ms") {
            Step s; s.kind = StepKind::WAIT_MS; s.int_arg = atoi(arg.c_str()); s.line_no = line_no;
            steps.push_back(s);
        } else if (cmd == "action") {
            ACTIVE_TYPE a = parse_action(arg.c_str());
            if (a == UNKNOWN) {
                fprintf(stderr, "[scenario] line %d: unknown action '%s'\n", line_no, arg.c_str());
                fclose(f);
                return 3;
            }
            Step s; s.kind = StepKind::ACTION; s.action = a; s.line_no = line_no;
            steps.push_back(s);
        } else if (cmd == "screenshot") {
            Step s; s.kind = StepKind::SCREENSHOT; s.str_arg = arg; s.line_no = line_no;
            steps.push_back(s);
        } else if (cmd == "assert_no_crash") {
            Step s; s.kind = StepKind::ASSERT_NO_CRASH; s.line_no = line_no;
            steps.push_back(s);
        } else {
            fprintf(stderr, "[scenario] line %d: unknown command '%s'\n", line_no, cmd.c_str());
            fclose(f);
            return 3;
        }
    }
    fclose(f);

    if (app_name.empty()) {
        fprintf(stderr, "[scenario] missing 'app <name>' header in %s\n", path);
        return 3;
    }

    // Find and install the requested app.
    APP_OBJ *target = nullptr;
    for (int i = 0; i < app_count; ++i) {
        if (apps[i].name && !strcmp(apps[i].name, app_name.c_str())) {
            target = apps[i].app;
            break;
        }
    }
    if (!target) {
        fprintf(stderr, "[scenario] app '%s' not registered in this build\n", app_name.c_str());
        return 4;
    }
    controller->app_install(target, APP_TYPE_REAL_TIME);

    // Boot into the app via a synthetic GO_FORWORD before running steps.
    {
        ImuAction a; a.active = GO_FORWORD; a.isValid = true;
        controller->main_process(&a);
    }

    std::string scenario_dir, scenario_stem;
    derive_scenario_paths(path, &scenario_dir, &scenario_stem);
    printf("[scenario] running '%s' against app '%s' (%zu steps, mode=%s)\n",
           path, app_name.c_str(), steps.size(),
           opts.update_golden ? "update-golden" : "compare");

    int failures = 0;
    for (size_t i = 0; i < steps.size(); ++i) {
        const Step &s = steps[i];
        switch (s.kind) {
            case StepKind::WAIT_MS:
                printf("[scenario] step %zu (line %d): wait_ms %d\n", i + 1, s.line_no, s.int_arg);
                tick_for(s.int_arg);
                break;
            case StepKind::ACTION: {
                printf("[scenario] step %zu (line %d): action %s\n",
                       i + 1, s.line_no, active_type_info[s.action]);
                ImuAction a; a.active = s.action; a.isValid = true;
                controller->main_process(&a);
                tick_for(50); // let LVGL settle the screen change
                break;
            }
            case StepKind::SCREENSHOT: {
                char actual_path[512];
                char golden_path[512];
                char diff_path[512];
                snprintf(golden_path, sizeof golden_path,
                         "../test/golden/%s/%s/%s.png",
                         scenario_dir.c_str(), scenario_stem.c_str(), s.str_arg.c_str());
                snprintf(actual_path, sizeof actual_path,
                         "../test/results/%s/%s/%s.png",
                         scenario_dir.c_str(), scenario_stem.c_str(), s.str_arg.c_str());
                snprintf(diff_path, sizeof diff_path,
                         "../test/results/%s/%s/%s_diff.png",
                         scenario_dir.c_str(), scenario_stem.c_str(), s.str_arg.c_str());

                if (opts.update_golden) {
                    if (!aio_screenshot::save_screen_png(golden_path)) {
                        fprintf(stderr, "[scenario] step %zu: could not save golden %s\n",
                                i + 1, golden_path);
                        ++failures;
                    } else {
                        printf("[scenario] step %zu (line %d): screenshot '%s' -> golden saved (%s)\n",
                               i + 1, s.line_no, s.str_arg.c_str(), golden_path);
                    }
                } else {
                    if (!aio_screenshot::save_screen_png(actual_path)) {
                        fprintf(stderr, "[scenario] step %zu: could not save actual %s\n",
                                i + 1, actual_path);
                        ++failures;
                        break;
                    }
                    FILE *gf = fopen(golden_path, "rb");
                    if (!gf) {
                        printf("[scenario] step %zu (line %d): screenshot '%s' -> "
                               "no baseline at %s, candidate saved (artifact upload will collect it)\n",
                               i + 1, s.line_no, s.str_arg.c_str(), golden_path);
                        break;
                    }
                    fclose(gf);
                    double pct = 0.0;
                    bool ok = aio_screenshot::compare_pngs(actual_path, golden_path,
                                                           diff_path, opts.diff_threshold_pct, &pct);
                    if (ok) {
                        printf("[scenario] step %zu (line %d): screenshot '%s' -> match (%.3f%% differ)\n",
                               i + 1, s.line_no, s.str_arg.c_str(), pct);
                    } else {
                        printf("[scenario] step %zu (line %d): screenshot '%s' -> "
                               "DIFF %.3f%% > %.3f%%, see %s\n",
                               i + 1, s.line_no, s.str_arg.c_str(), pct,
                               opts.diff_threshold_pct, diff_path);
                        ++failures;
                    }
                }
                break;
            }
            case StepKind::ASSERT_NO_CRASH:
                printf("[scenario] step %zu (line %d): assert_no_crash — ok\n", i + 1, s.line_no);
                break;
        }
    }

    printf("[scenario] '%s' completed with %d failure(s)\n", path, failures);
    return failures == 0 ? 0 : 1;
}
