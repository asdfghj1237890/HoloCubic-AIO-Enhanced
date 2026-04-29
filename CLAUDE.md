# CLAUDE.md

Operational notes for AI agents working in this repo. For architecture / how-to walkthroughs see [`Docs/development/`](./Docs/development/README.md).

## What this is

HoloCubic AIO — a third-party firmware for the HoloCubic ESP32 toy. Two main components:
- `AIO_Firmware_PIO/` — ESP32 firmware (PlatformIO + Arduino-core + LVGL 8.3 + ArduinoJson v6)
- `AIO_Tool/` — Windows GUI flasher + remote control (Python 3.11 + customtkinter + uv + PyInstaller)

Plus `lv_simulater_platformio/` for host-side SDL2 GUI simulation, and `test/` for scenario harness.

## Common commands

```bash
# Firmware
cd AIO_Firmware_PIO
pio run -e HoloCubic_AIO_Releases     # production build (CI also runs this)
pio test -e native_unit                # Unity unit tests (~30s)
pio test -e native_ftp                 # FTP harness Unity tests (~30s)

# GUI scenario harness (host SDL2)
cd lv_simulater_platformio
pio run -e native_test                 # builds the SDL2 binary
./.pio/build/native_test/program --scenario ../test/scenarios/<app>/smoke.scn --headless

# AIO_Tool
cd AIO_Tool
make run        # uv run python CubicAIO_Tool.py
make test       # pytest
make lint       # ruff check
make format     # ruff format
make build      # pyinstaller → dist/CubicAIO_Tool.exe
```

## PR workflow

The team uses **branch → PR → CI → squash-merge**. Direct push to main is unusual — only acceptable for emergency build-fix when CI itself can't catch the issue.

```bash
git checkout -b descriptive-branch-name
# ... make changes ...
git commit -m "..."   # use HEREDOC; include Co-Authored-By line
git push -u origin descriptive-branch-name
gh pr create --title "..." --body "..."
# wait for CI, then:
gh pr merge <#> --squash --delete-branch
```

CI auto-monitor: `gh run list --branch <branch> --limit 1 --json databaseId,status` then `gh run watch <id> --exit-status --interval 30`.

## Release process

Tag `v*.*.*` triggers `.github/workflows/release.yml` which builds firmware .bin + AIO_Tool .exe + publishes a GitHub Release.

```bash
# Bump version in AIO_Firmware_PIO/src/common.h first:
#   #define AIO_VERSION "2.6.X"
# (this drives /api/stats's d.version AND the ?v= cache-bust query for static assets)

git tag -a v2.6.X -m "Release v2.6.X"
git push origin v2.6.X
```

## Code conventions (NON-OBVIOUS, ENFORCED)

These are real rules with real reasons (each cited in [`Docs/development/08-refactoring-case-studies.md`](./Docs/development/08-refactoring-case-studies.md)):

**Firmware C/C++**:
- ❌ `strcpy(dst, src)` → ✅ `snprintf(dst, sizeof(dst), "%s", src)`. Never `strcpy` / `sprintf` (no-`n` variants).
- ❌ `doc["field"].as<int>()` → ✅ `doc["field"] | 0` (ArduinoJson v6 `|` fallback). `.as<T>()` crashes on missing/wrong-type; `|` doesn't.
- ❌ `delay(N)` in `main_process` / `message_handle` / LVGL event callback → ✅ `if (millis() - last < N) return;` early return. `delay()` blocks the entire main thread (LVGL + IMU + all apps).
- Use `F("...")` for string literals → keeps strings in flash, not SRAM.
- `Send_HTML(webpage)` for web pages; build via `String webpage; webpage += F(...) + getText(key) + F(...);`.

**AIO_Tool Python**:
- ❌ `widget["text"]` (read or write) on customtkinter widgets → ✅ `widget.cget("text")` / `widget.configure(text=...)`. Dict access bypasses CTkButton's overrides; reads raise TclError, writes silently no-op.
- Translation: every user-visible string MUST exist in all 3 locales (`AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json` for tool, `getText()` cascade in `web_setting.cpp` for firmware).

**Web settings (firmware)**:
- New form fields go through helpers in `AIO_Firmware_PIO/src/app/server/web_setting_forms.cpp`: `emit_form_open` / `emit_text_field` / `emit_pwd_field` / `emit_radio2_field` / `emit_form_close`. All take an i18n key for the label, not a literal.
- Save handler in `web_setting_handlers.cpp` posts back via `app_controller->send_to(... APP_MESSAGE_SET_PARAM ...)` then `APP_MESSAGE_WRITE_CFG`.

## Architecture in one paragraph

Firmware main loop (`HoloCubic_AIO.cpp`) reads IMU once per ~50ms tick → passes `ImuAction` to `AppController->main_process()` → routes to active app's `main_process(sys, act_info)`. Each app is an `APP_OBJ` with 7 callbacks (init/process/background_task/exit/message_handle + name/icon/info). Cross-app comms via `sys->send_to(from, to, type, msg, ext)` which is async (queued) — except `GET_PARAM`/`SET_PARAM` which dispatch synchronously. Both `main_process` and `message_handle` run on main thread → no mutex needed but `delay()` is fatal. Full deep-dive in [`Docs/development/02-firmware-architecture.md`](./Docs/development/02-firmware-architecture.md).

## Test strategy in one paragraph

Four host envs cover different layers: `native_unit` (pure logic — parsers, state machines), `native_ftp` (one stateful protocol class), `native_test` (full GUI scenario with SDL2 + LVGL + screenshot diff), `firmware-build` (real ESP32 compile/link, no execute). Driver layer + WiFi reconnect + flash partition behaviour need real hardware. **Known gaps**: AIO_Tool GUI interactions are NOT tested (CTkButton silent fails escape until users report); long-running memory leaks are NOT tested (stock leak class — see [`Docs/development/09-test-architecture-decomposition.md`](./Docs/development/09-test-architecture-decomposition.md) §8 for the planned fix design).

## Things that LOOK like bugs but aren't

- `AIO_VERSION` in `common.h` lags behind release tags sometimes — this is OK, the tag is source of truth, the constant gates cache-busting on the next bump
- `TOOL_VERSION` in `AIO_Tool/util/common.py` is stuck at "v2.5.0" — same as above; release filenames use `github.ref_name`, this only feeds the release body text
- `dist/HoloCubic_AIO_firmware_v2.6.X.bin` appearing as untracked is fine — local user testing artifact, NOT to be committed
- Many comments in source are zh-cn from the original ClimbSnail upstream — leave them; only translate the ones we change

## When debugging

- CI fail: `gh run view <id> --log-failed | grep -E "error:|FAIL"` first
- Silent UI bug (especially AIO_Tool): the layer that didn't catch it is the place to add coverage; surface-the-error first ([PR #74](https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced/pull/74) is the textbook fix shape)
- Release fail after PR was green: probably a host-stub / Arduino-core divergence — verify both `pio run -e HoloCubic_AIO_Releases` AND `pio run -e native_test` build cleanly. PR #72 added the firmware-build CI job specifically for this class.

## What to ask before destructive actions

Confirm before: force-pushing tags (acceptable when no GitHub release was published, otherwise bump version), `git tag -d` on remote, deleting branches with unmerged commits, modifying CI workflows in ways that change gating semantics. The user expects standard PR flow for everything else.
