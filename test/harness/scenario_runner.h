// Scenario runner: parses a small line-based scenario file and drives the
// AppController + LVGL through a fixed sequence of waits, actions, and
// screenshot points. Used for non-interactive regression runs in CI.
//
// Scenario format (see test/scenarios/*.scn):
//   # comments and blank lines are ignored
//   app <name>            (required, exactly once, before any step)
//   wait_ms <integer>     advance app + LVGL N milliseconds
//   action <ACTIVE_TYPE>  inject one ImuAction (TURN_LEFT, TURN_RIGHT, UP,
//                         DOWN, GO_FORWORD, RETURN, SHAKE)
//   screenshot <name>     Phase 3 hook — currently just logs
//   assert_no_crash       sanity marker, prints to stdout
//
// Returns 0 on success, non-zero on parse error or scenario failure.

#ifndef AIO_HARNESS_SCENARIO_RUNNER_H
#define AIO_HARNESS_SCENARIO_RUNNER_H

class AppController;
struct APP_OBJ;

struct ScenarioApp {
    const char *name;
    APP_OBJ *app;
};

struct ScenarioOptions {
    // When true, screenshot steps overwrite test/golden/<...>.png and
    // skip comparison. When false (default), screenshots are saved to
    // test/results/<...>.png and compared against the matching golden.
    bool update_golden = false;
    // Maximum percentage of differing pixels tolerated before a step fails.
    double diff_threshold_pct = 0.5;
};

int run_scenario(const char *path,
                 AppController *controller,
                 const ScenarioApp *apps,
                 int app_count,
                 const ScenarioOptions &opts = ScenarioOptions{});

// Install the LVGL log bridge: mirrors every LVGL log line to stdout and
// counts style double-init warnings ("Style might be already inited."),
// which lv_style_init() emits under LV_USE_ASSERT_STYLE when a style is
// re-initialised without lv_style_reset — each one is a leaked style
// property array (the long-uptime leak class; re-entering an app re-runs
// its *_gui_init). run_scenario() fails any scenario that produced one.
// Call once from main() after lv_init().
void scenario_lv_log_watch_install(void);

#endif
