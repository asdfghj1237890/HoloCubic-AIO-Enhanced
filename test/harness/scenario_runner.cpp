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
#include <dirent.h>
#include <unistd.h>
#include <utility>

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
    // init_only: skip the second main_process(UNKNOWN) call below. Apps like
    // LHLXW take over the main thread inside main_process with their own
    // while(1) tick loop and never return; we can only verify their init
    // path under that constraint.
    bool init_only = false;
    // flash_seed entries: (path, content) pairs written into FlashFS
    // before app_init runs. Lets a scenario boot the app under test
    // with non-default config (e.g. CN-market stockmarket) without
    // reaching for a per-test write_config call.
    std::vector<std::pair<std::string, std::string>> flash_seeds;
    // http_fixture entries: (url-substring, fixture-relpath) pairs. Lets a
    // scenario route specific URLs to a per-scenario fixture instead of the
    // URL-derived default — needed when (a) the URL has the parameter in
    // the query string (which the harness strips before fixture lookup,
    // so different params share one fixture: bilibili uid) or (b) the
    // scenario wants to inject a malformed body for a negative-path test
    // without disturbing the happy-path golden.
    std::vector<std::pair<std::string, std::string>> http_fixtures;
    // Per-scenario diff-threshold override. Defaults to opts.diff_threshold_pct
    // (0.5%); a scenario can opt to a wider threshold via `threshold N.N`
    // when its render legitimately has subpixel variance — e.g. pc_resource
    // whose FontAwesome glyphs and 4-arc layout don't always settle to the
    // same byte sequence even after wait_ms padding.
    double scenario_threshold = opts.diff_threshold_pct;

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
        } else if (cmd == "init_only") {
            init_only = true;
        } else if (cmd == "flash_seed") {
            // Format: flash_seed <path> <content>
            // <content> may contain literal "\n" / "\\" / "\"" escapes.
            size_t sp2 = arg.find_first_of(" \t");
            if (sp2 == std::string::npos) {
                fprintf(stderr, "[scenario] line %d: flash_seed needs <path> <content>\n", line_no);
                fclose(f);
                return 3;
            }
            std::string seed_path = arg.substr(0, sp2);
            std::string raw_content = arg.substr(sp2 + 1);
            trim(raw_content);
            // Strip surrounding double quotes if present.
            if (raw_content.size() >= 2 &&
                raw_content.front() == '"' && raw_content.back() == '"') {
                raw_content = raw_content.substr(1, raw_content.size() - 2);
            }
            // Decode \n / \\ / \" escapes into the actual chars.
            std::string content;
            for (size_t k = 0; k < raw_content.size(); ++k) {
                if (raw_content[k] == '\\' && k + 1 < raw_content.size()) {
                    char nx = raw_content[k + 1];
                    if (nx == 'n')      content.push_back('\n');
                    else if (nx == 't') content.push_back('\t');
                    else                content.push_back(nx);
                    ++k;
                } else {
                    content.push_back(raw_content[k]);
                }
            }
            flash_seeds.emplace_back(std::move(seed_path), std::move(content));
        } else if (cmd == "threshold") {
            // Format: threshold <pct>  (e.g. `threshold 3.0` for 3% tolerance)
            scenario_threshold = atof(arg.c_str());
            if (scenario_threshold <= 0.0) {
                fprintf(stderr, "[scenario] line %d: threshold needs a positive number, got '%s'\n",
                        line_no, arg.c_str());
                fclose(f);
                return 3;
            }
        } else if (cmd == "http_fixture") {
            // Format: http_fixture <url-substring> <fixture-relpath>
            // The fixture-relpath is relative to test/fixtures/http/.
            size_t sp2 = arg.find_first_of(" \t");
            if (sp2 == std::string::npos) {
                fprintf(stderr, "[scenario] line %d: http_fixture needs <url-substring> <fixture-relpath>\n", line_no);
                fclose(f);
                return 3;
            }
            std::string url_pattern = arg.substr(0, sp2);
            std::string relpath = arg.substr(sp2 + 1);
            trim(relpath);
            http_fixtures.emplace_back(std::move(url_pattern), std::move(relpath));
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
    // Wipe the FlashFS fixture dir so each scenario boots with the
    // default-init code paths unless it explicitly seeds otherwise.
    // Without this, a previous scenario's writeFile (e.g. a default
    // config write on first boot) would leak into the next.
    // Path matches FLASH_FIXTURE_DIR in test/stubs/stubs_runtime.cpp.
    {
        const char *flash_dir = "../test/fixtures/flash";
        DIR *dh = opendir(flash_dir);
        if (dh) {
            struct dirent *ent;
            while ((ent = readdir(dh)) != nullptr) {
                if (ent->d_name[0] == '.') continue;
                std::string p(flash_dir);
                p += "/";
                p += ent->d_name;
                unlink(p.c_str());
            }
            closedir(dh);
        }
    }

    // Apply flash_seed directives so app_init's read_config sees the
    // intended state instead of writing fresh defaults.
    for (auto const &seed : flash_seeds) {
        g_flashCfg.writeFile(seed.first.c_str(), seed.second.c_str());
    }

    // Apply http_fixture directives. The setters live in
    // test/stubs/stubs_runtime.cpp — declared here to avoid pulling a
    // new header in just for two function signatures.
    extern void clear_http_fixture_overrides();
    extern void set_http_fixture_override(const std::string &, const std::string &);
    clear_http_fixture_overrides();
    for (auto const &ovr : http_fixtures) {
        set_http_fixture_override(ovr.first, ovr.second);
    }

    controller->app_install(target, APP_TYPE_REAL_TIME);

    // Boot into the app the way the real firmware does: a GO_FORWORD frame
    // enters the app and runs app_init, then a follow-up UNKNOWN frame is
    // what actually invokes the app's own main_process for the first time.
    // Without this second tick, the first screenshot captures the default
    // LVGL screen (black) rather than the app's real first frame.
    //
    // init_only mode skips the UNKNOWN frame so apps whose main_process is
    // an internal while(1) loop (LHLXW & friends) don't hang the harness.
    {
        ImuAction a; a.active = GO_FORWORD; a.isValid = true;
        controller->main_process(&a);
        if (!init_only) {
            ImuAction b; b.active = UNKNOWN; b.isValid = false;
            controller->main_process(&b);
        }
    }
    tick_for(50);

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
                                                           diff_path, scenario_threshold, &pct);
                    if (ok) {
                        printf("[scenario] step %zu (line %d): screenshot '%s' -> match (%.3f%% differ)\n",
                               i + 1, s.line_no, s.str_arg.c_str(), pct);
                    } else {
                        printf("[scenario] step %zu (line %d): screenshot '%s' -> "
                               "DIFF %.3f%% > %.3f%%, see %s\n",
                               i + 1, s.line_no, s.str_arg.c_str(), pct,
                               scenario_threshold, diff_path);
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
