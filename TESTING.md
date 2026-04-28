# HoloCubic_AIO Test Framework

End-state record of the host-side regression framework built on top of the
firmware. The how-to-run-it operational notes live in
[`test/README.md`](test/README.md); this document captures the design, the
mechanisms, and the boundaries — what the framework does cover, what it
deliberately doesn't, and where to extend it.

## Goal

Catch UI regressions and pure-logic regressions on a developer machine
without flashing real hardware. CI on every PR runs the full suite in
under 2 minutes and uploads candidate screenshots / Unity test results as
artifacts.

The framework is **not** a substitute for on-device smoke testing — TFT
SPI/DMA timing, WiFi reconnection, MPU6050 calibration drift, OTA
partitions etc. all need real hardware. Expectation is "5 minutes after a
PR push you know if UI broke or logic broke", not "ship without burning a
device".

## Two-track architecture

Two independent CI jobs run in parallel.

### Track A — GUI regression (SDL2 host harness)

Builds the actual firmware app GUI code into a desktop binary via
PlatformIO `env:native_test` in
[`lv_simulater_platformio/platformio.ini`](lv_simulater_platformio/platformio.ini).
LVGL renders to an SDL2 surface; `SDL_VIDEODRIVER=dummy` runs it
headless on CI.

Key components:

- **Stubs** (`test/stubs/`) — header shims that take precedence over
  firmware drivers. Cover Arduino, WiFi/HTTP/SD/FastLED/MPU6050/etc.
  surface so firmware code links + runs against a desktop target.
  `stubs_runtime.cpp` provides the singleton instances and a slim
  `AppController` replacement.
- **Harness** (`test/harness/`) — `main.cpp` boots LVGL+SDL,
  `scenario_runner.cpp` parses + runs `.scn` files, `screenshot.cpp`
  snapshots `lv_scr_act()` to PNG and diff-compares against committed
  baselines.
- **Scenarios** (`test/scenarios/<app>/<case>.scn`) — line-based
  scripts. Each `.scn` declares one app under test plus a sequence of
  `wait_ms` / `action` / `screenshot` / `assert_no_crash` steps.
  Optional directives: `init_only` (skip the second main_process tick;
  used for apps whose main_process is an internal `while(1)` that never
  returns) and `flash_seed <path> <content>` (pre-write a config file
  into FlashFS so app_init's read_config picks up custom state).
- **Goldens** (`test/golden/<app>/<case>/<step>.png`) — committed
  baselines. CI fails on >0.5% pixel diff; `actions/upload-artifact`
  uploads candidate + diff PNGs for review.

### Track B — Unity unit tests (native)

Pure-logic firmware modules unit-tested without LVGL or SDL2. Lives at
[`AIO_Firmware_PIO/test/native/`](AIO_Firmware_PIO/test/native/),
build env `[env:native_unit]` in
[`AIO_Firmware_PIO/platformio.ini`](AIO_Firmware_PIO/platformio.ini).
Stubs are kept separate
([`AIO_Firmware_PIO/test/stubs_unit/`](AIO_Firmware_PIO/test/stubs_unit/))
so the unit-test binary stays small — no LVGL, no SDL2, no full Arduino
String.

Modules covered:

| Test | Target firmware code | What it covers |
|---|---|---|
| `test_imu_action` | `src/driver/imu.cpp::IMU::getAction` | v_ax / v_ay threshold table, 3-consecutive-samples long-press promotion |
| `test_config` | `src/driver/analyse_param.cpp` | Line splitter shared by every config parser; basic / empty-line / partial-argc behaviour |
| `test_app_controller` | `src/sys/send_to_dispatch.cpp` | Event-queue path (cap, push), dispatch path (handler invocation, NULL handler, missing toApp) |
| `test_game_2048` | `src/app/game_2048/game2048_contorller.cpp::GAME2048` | init zeros, 4 move directions (slide + merge once), `judge()` 0/1/2 returns |

Three small firmware refactors enabled testability without touching
behaviour:
- `analyseParam` extracted from `flash_fs.cpp` into its own TU
- `send_to_dispatch` extracted from `app_controller.cpp::send_to`
- `game_2048::judge()` off-by-one boundary fix (was a real bug — see
  bug log)

## Coverage snapshot

- **19 / 19 firmware apps** have at least a smoke scenario in Track A
- **18 / 19 apps** have committed visual goldens (30 screenshots total).
  Three documented opt-outs:
  - `weather` page 0 — clock label ticks every render, fundamentally
    non-deterministic
  - `settings` — version label uses an auto-scrolling marquee whose
    offset depends on wall-clock tick scheduling
  - `idea` — scenario only asserts no-crash; no screenshot step
- **30 unit test cases** in Track B across 4 modules

## Fixture mechanisms

Each fixture type has a single resolution rule and falls back to "no
fixture = old behaviour" so adding a fixture is opt-in per app.

### HTTP fixtures (`test/fixtures/http/<host>/<path>.json`)

`HTTPClient::begin(url)` records the URL; `GET()` strips the query string
and looks up a fixture file at `test/fixtures/http/<host><path>.json`.
File found → return 200 + populate buffer. Not found → return -1 (the
old "always offline" sentinel). Used by bilibili (Bilibili stat
endpoint), weather (3-step AccuWeather chain), stockmarket (both Yahoo
US and Sina CN parsers).

### Socket fixtures (`test/fixtures/socket/<host>.txt`)

For apps that go via raw `WiFiClient::connect` (rather than HTTPClient).
`connect(host, port)` loads `test/fixtures/socket/<host>.txt` into the
client's read buffer; `find()` / `readStringUntil()` / `read()` walk it
in place of a real socket. Currently used by pc_resource (HTTP-style
SSE reply).

`screen_share` doesn't get one — its visible state is "Connect succ"
either way (already covered via WIFI_CONN routing) and the JPEG decoder
+ `tft->pushImageDMA` are stubbed to no-op, so faking the MJPEG byte
stream wouldn't add visual coverage.

### SD fixtures (`test/fixtures/sd/<dir>/...`)

`SdCard::listDir(dirname)` scans `test/fixtures/sd/<dirname>/` on the
host filesystem and builds a `File_Info` linked list mirroring the
firmware's circular doubly-linked layout. Used by picture (`/image/`)
and media (`/movie/`).

### Flash fixtures (`flash_seed` scenario directive + `test/fixtures/flash/`)

The slim FlashFS implementation persists `g_flashCfg.writeFile` calls
to `../test/fixtures/flash/<path>` (relative to
`lv_simulater_platformio/`, so it lands on the committed dir at the
repo root). Each scenario wipes that dir at start so per-scenario
state can't leak across the suite.

A scenario can pre-seed config with a directive:

```
flash_seed /stockmarket.cfg "603019\nCN\n10000\n"
```

Used by `test/scenarios/stockmarket/cn_smoke.scn` to swap in a CN-market
config so the app routes to the Sina parser instead of the default Yahoo
path. `\n` / `\t` / `\\` / `\"` escapes get decoded; surrounding double
quotes are stripped.

### WIFI_CONN routing (no fixture, harness behaviour)

Real firmware queues WIFI_CONN messages and `req_event_deal` invokes
the sender's callback after `wifi_event()` succeeds. The harness
short-circuits: when `send_to(from_app, "AppCtrl", WIFI_CONN/AP, ...)`
is called, the slim `AppController::send_to` synchronously invokes
`from_app->message_handle("AppCtrl", from_app_name, type, message,
NULL)`. Apps that gate fetches on this callback (bilibili / weather /
stockmarket / pc_resource / file_manager / screen_share) all therefore
exercise their fetch path during scenarios.

### SIGSEGV handler + addr2line decode

[`test/harness/main.cpp`](test/harness/main.cpp) installs a glibc
backtrace handler on SIGSEGV/SIGABRT/SIGBUS/SIGFPE. The CI workflow
adds an addr2line post-pass on the raw `+0xN` offsets so a crash trace
in the log shows function name + file:line directly, without needing
to download the binary.

## How to add a new …

### Scenario for an existing app

1. Drop `.scn` under `test/scenarios/<app_name>/<case>.scn` mirroring
   one of the existing patterns (see bilibili / stockmarket for typical
   "fetch + render after action UP" flow).
2. Push, let CI save a candidate, review the artifact PNG, copy under
   `test/golden/<app_name>/<case>/<step>.png` if it looks right.

### App that's not yet under test

1. Add `+<../../AIO_Firmware_PIO/src/app/<name>>` to `build_src_filter`
   in `lv_simulater_platformio/platformio.ini`.
2. Add the `#include` and `kRegisteredApps` entry in
   `test/harness/main.cpp`.
3. Iteratively extend `test/stubs/` for any unresolved symbols the
   linker complains about.
4. Write the scenario as above.

### HTTP fixture for a new endpoint

1. Drop the canned reply at
   `test/fixtures/http/<host>/<path-with-slashes-preserved>.json`.
   Query strings are stripped from the URL before lookup, so different
   query params for the same endpoint share the fixture.
2. Update the relevant scenario to either pre-trigger the fetch or
   assert the rendered post-fetch state.

### Track B test for a new pure-logic module

1. Extract the testable function into its own TU if it's currently
   wedged inside a class with heavy dependencies (see
   `analyseParam` / `send_to_dispatch` for examples).
2. Create `AIO_Firmware_PIO/test/native/test_<name>/test_main.cpp`
   following the existing `Unity` patterns.
3. Add the source to `[env:native_unit]`'s `build_src_filter` in
   `AIO_Firmware_PIO/platformio.ini`.

## Known limitations

- Goldens are 240×240 PNGs with a 0.5% pixel-diff tolerance. Anything
  that varies on absolute wall-clock time (clock labels, marquee
  animations, scroll-state-dependent rendering) is excluded from the
  golden suite — see the three opt-outs above.
- The slim host AppController in `test/stubs/stubs_runtime.cpp` doesn't
  model the real firmware's event queue + `req_event_deal`. The
  WIFI_CONN synchronous callback shortcut is a reasonable approximation
  for fetch-on-WiFi-up flows, but anything that relies on retry timing,
  multiple queued events, or the controller's screen-load animations
  for app transitions is undertested by Track A.
- Track B's `stubs_unit/` is intentionally minimal. Any new unit-test
  module pulling in firmware code that touches FreeRTOS timers, LVGL
  rendering, or driver globals will need either a refactor (preferred)
  or further stub additions.

## Reference

The original plan (Hybrid two-track architecture, four-phase rollout,
scope-out list) lives at
`~/.claude/plans/full-regression-wise-axolotl.md`. Every phase of that
plan is delivered. The bugs the framework flushed out are documented
in [`TESTING-BUGS.md`](TESTING-BUGS.md).
