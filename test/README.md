# HoloCubic_AIO Regression Test Harness

Host-side test harness that builds the actual firmware UI code into an SDL2
desktop window so UI changes can be reviewed, scripted, and diff-tested
without flashing real hardware.

The full plan is at `~/.claude/plans/full-regression-wise-axolotl.md`. This
README covers what is implemented today (Phase 1 walking skeleton) and how to
run it.

## Layout

```
test/
  harness/         Host-side entry: SDL2 + LVGL bring-up, input loop
    main.cpp       Phase 1 entry — installs anniversary, ticks LVGL
  stubs/           Header shims that take precedence over firmware headers
    Arduino.h          millis/delay/Serial/String shim
    WiFi.h, WiFiClient.h, HTTPClient.h, PubSubClient.h
    TFT_eSPI.h, FastLED.h, I2Cdev.h, MPU6050.h, Wire.h, SPI.h
    FS.h, SD.h, SPIFFS.h, esp32-hal-ledc.h
    freertos/{FreeRTOS,task,timers,semphr,queue}.h
    common.h           Replaces firmware common.h (no hardware deps)
    network.h          Replaces firmware network.h
    sys/app_controller.h   Slim stub of AppController
    driver/{imu,rgb_led,sd_card,flash_fs,display,ambient}.h
    stubs_runtime.cpp  Singleton instances + AppController/FlashFS impl
  scenarios/       Scenario files (`.scn`, line-based, see "Scenario format")
  golden/          (Phase 3) golden PNG baselines
  fixtures/
    flash/         FlashFS host-side mount point
    http/          (Phase 4) canned HTTP responses
    sd/            (Phase 4) SD-card fixture files
  results/         Diff PNGs written here on regression failure
```

## Phase status

- Phase 1 walking skeleton — done. CI builds and runs anniversary headless.
- Phase 2 scripted scenarios — done for anniversary. CLI takes `--scenario`
  and `--headless`; CI runs `test/scenarios/anniversary/smoke.scn`.
- Phase 3 golden-image diff — not started. `screenshot` step is currently a
  log placeholder.
- Phase 4 all apps + Unity unit-test track — not started.

## Scenario format

Scenarios are plain text, one step per line. Comment lines start with `#`,
blank lines are ignored. Every scenario has exactly one `app <name>` header
followed by zero or more steps:

```
# Smoke test for anniversary.
app anniversary

wait_ms 200
screenshot 01_initial
assert_no_crash

action TURN_RIGHT
wait_ms 400
screenshot 02_after_turn_right

action RETURN
wait_ms 200
```

Step kinds:

- `wait_ms <int>` — advance LVGL for N milliseconds in 5 ms slices
- `action <NAME>` — inject one ImuAction; valid names are `TURN_LEFT`,
  `TURN_RIGHT`, `UP`, `DOWN`, `GO_FORWORD` (`GO_FORWARD` also accepted),
  `RETURN`, `SHAKE`
- `screenshot <name>` — Phase 3 hook; currently logs a placeholder
- `assert_no_crash` — sanity marker; printed to stdout

Apps available to scenarios are the ones registered in
`test/harness/main.cpp` (`kRegisteredApps`). Adding a new app to scenarios
is two edits: add its `+<...>` line to `build_src_filter` in
`lv_simulater_platformio/platformio.ini` and add it to `kRegisteredApps`.

## Local build (Windows)

The native build needs `gcc` / `g++` plus `libsdl2-dev` headers.

Easiest options on Windows:

1. **MinGW-w64 via MSYS2** — install MSYS2, then
   ```
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2
   ```
   and run from an MSYS2 mingw64 shell.
2. **MinGW-w64 standalone** — e.g. WinLibs UCRT, then download SDL2 dev libs
   from https://github.com/libsdl-org/SDL/releases and place under the
   compiler's `include/` and `lib/`.
3. **WSL Ubuntu** — `sudo apt install build-essential libsdl2-dev`, then build
   the same way but from inside WSL.

After a toolchain is on `PATH`:

```
cd lv_simulater_platformio
pio run -e native_test
./.pio/build/native_test/program
```

Keys (Phase 1):
- Right / Left arrow → TURN_RIGHT / TURN_LEFT
- Up / Down arrow   → UP / DOWN
- Enter / Space     → GO_FORWORD (enter app)
- Esc / Backspace   → RETURN (exit app)
- S                 → SHAKE
- Q or window close → quit

## CI

`.github/workflows/regression.yml` runs the build on `ubuntu-latest` for
every push / PR to `main`. CI installs `libsdl2-dev` and PlatformIO,
builds env:native_test, then runs the harness in headless mode
(`SDL_VIDEODRIVER=dummy`). On failure, diff PNGs in `test/results/` are
uploaded as a GitHub artifact.

## How the stubs work

`build_flags` puts `test/stubs/` first on the include path so any
`#include "common.h"`, `#include "WiFi.h"`, `#include "driver/imu.h"`,
etc. resolves to our shim before the firmware version.

Hardware-coupled firmware sources (`driver/display.cpp`, `driver/imu.cpp`,
`network.cpp`, `sd_card.cpp`, `app_controller.cpp`) are excluded from the
build via `build_src_filter`. The slim AppController in `stubs_runtime.cpp`
plays the role of the firmware's controller for test purposes — it owns the
app list, dispatches `main_process` and `message_handle`, and routes
`send_to` between apps.

## Adding more apps (Phase 4)

1. Add `+<../../AIO_Firmware_PIO/src/app/<name>>` to `build_src_filter` in
   `lv_simulater_platformio/platformio.ini` env:native_test.
2. Add any extra fonts / shared resources the app pulls in.
3. If the app references new symbols not yet stubbed, extend `test/stubs/`
   minimally — add only what links demand.
4. For SD-card-dependent apps, place fixture files under `test/fixtures/sd/`.
   For HTTP-dependent apps, expand `HTTPClient` stub to read canned
   responses from `test/fixtures/http/<endpoint>.json`.

See the plan file for the full phasing and the rationale.
