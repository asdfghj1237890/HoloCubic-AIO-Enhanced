# CLAUDE.md

Operational notes for AI agents working in this repo. For architecture / how-to walkthroughs see [`Docs/development/`](./Docs/development/README.md).

## What this is

HoloCubic AIO — a third-party firmware for the HoloCubic ESP32 toy. Two main components:
- `AIO_Firmware_PIO/` — ESP32 firmware (PlatformIO + Arduino-core + LVGL 8.3 + ArduinoJson v6)
- `AIO_Tool/` — Cross-platform GUI flasher + remote control (Rust 1.82 + egui 0.29 + eframe; 6 workspace crates: aio-protocol / aio-i18n / aio-device / aio-flasher / aio-converter / aio-tool)

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

# AIO_Tool (Rust 1.82 — pinned via rust-toolchain.toml)
cd AIO_Tool
cargo +1.82.0 run --bin aio-tool           # launch the GUI
cargo +1.82.0 test --workspace             # ~199 unit + integration + golden tests
cargo +1.82.0 clippy --all-targets --workspace -- -D warnings
cargo +1.82.0 fmt --all -- --check
cargo +1.82.0 build --release --bin aio-tool   # produces target/release/aio-tool[.exe]
```

Linux build requires `libudev-dev` (Debian/Ubuntu) or `systemd-devel` (Fedora) — `serialport` enumeration uses it.

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

Tag `v*.*.*` triggers `.github/workflows/release.yml` which builds firmware .bin + 3 AIO_Tool binaries (Windows .exe, macOS aarch64 tar.gz, Linux x86_64 tar.gz) + publishes a GitHub Release.

```bash
# Bump version in AIO_Firmware_PIO/src/common.h first:
#   #define AIO_VERSION "3.0.X"
# And in AIO_Tool/Cargo.toml's [workspace.package]:
#   version = "3.0.X"
# The Cargo.toml value is the single source of truth — env!("CARGO_PKG_VERSION")
# bakes it into the binary, and release.yml reads the same line for the release
# body. The TOOL_VERSION drift bug from the Python era is structurally impossible.

git tag -a v3.0.X -m "Release v3.0.X"
git push origin v3.0.X
```

## Code conventions (NON-OBVIOUS, ENFORCED)

These are real rules with real reasons (each cited in [`Docs/development/08-refactoring-case-studies.md`](./Docs/development/08-refactoring-case-studies.md)):

**Firmware C/C++**:
- ❌ `strcpy(dst, src)` → ✅ `snprintf(dst, sizeof(dst), "%s", src)`. Never `strcpy` / `sprintf` (no-`n` variants).
- ❌ `doc["field"].as<int>()` → ✅ `doc["field"] | 0` (ArduinoJson v6 `|` fallback). `.as<T>()` crashes on missing/wrong-type; `|` doesn't.
- ❌ `delay(N)` in `main_process` / `message_handle` / LVGL event callback → ✅ `if (millis() - last < N) return;` early return. `delay()` blocks the entire main thread (LVGL + IMU + all apps).
- Use `F("...")` for string literals → keeps strings in flash, not SRAM.
- `Send_HTML(webpage)` for web pages; build via `String webpage; webpage += F(...) + getText(key) + F(...);`.

**AIO_Tool Rust**:
- Every user-visible string MUST come from `aio_i18n::t("key", None)`. New keys MUST be added to all three locale files (`AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json`); `aio-i18n/build.rs` panics at compile time if the key sets diverge.
- Long-running ops follow the bus + worker pattern (`AIO_Tool/crates/aio-tool/src/{flasher_worker,settings_worker,file_manager_worker,image_converter_worker,video_converter_worker}.rs`):
  - egui frame spawns `std::thread::spawn`.
  - Worker owns its transport / subprocess / encoder.
  - Cancel via shared `Arc<AtomicBool>`; no `Cmd::Stop` enum, no `thread::sleep`.
  - Bus events flow via `AppEventTx` (`mpsc::Sender<AppEvent>`); UI drains in `App::update` with `try_recv` and `ctx.request_repaint_after(100ms)` for liveness.
- Preserved-from-Python wire-format bugs (B1 FileRename, B2 FileGetInfo) live in `aio-protocol` with explicit `// PRESERVED-BUG` comments. Don't "fix" them without a firmware-side update.

**Web settings (firmware)**:
- New form fields go through helpers in `AIO_Firmware_PIO/src/app/server/web_setting_forms.cpp`: `emit_form_open` / `emit_text_field` / `emit_pwd_field` / `emit_radio2_field` / `emit_form_close`. All take an i18n key for the label, not a literal.
- Save handler in `web_setting_handlers.cpp` posts back via `app_controller->send_to(... APP_MESSAGE_SET_PARAM ...)` then `APP_MESSAGE_WRITE_CFG`.

## Architecture in one paragraph

Firmware main loop (`HoloCubic_AIO.cpp`) reads IMU once per ~50ms tick → passes `ImuAction` to `AppController->main_process()` → routes to active app's `main_process(sys, act_info)`. Each app is an `APP_OBJ` with 7 callbacks (init/process/background_task/exit/message_handle + name/icon/info). Cross-app comms via `sys->send_to(from, to, type, msg, ext)` which is async (queued) — except `GET_PARAM`/`SET_PARAM` which dispatch synchronously. Both `main_process` and `message_handle` run on main thread → no mutex needed but `delay()` is fatal. Full deep-dive in [`Docs/development/02-firmware-architecture.md`](./Docs/development/02-firmware-architecture.md). AIO_Tool's architecture is documented in `AIO_Tool/README.md` + per-crate READMEs — six workspace crates layered protocol → i18n → device → flasher / converter → tool (egui binary).

## Test strategy in one paragraph

Four host envs cover firmware layers: `native_unit` (pure logic — parsers, state machines), `native_ftp` (one stateful protocol class), `native_test` (full GUI scenario with SDL2 + LVGL + screenshot diff), `firmware-build` (real ESP32 compile/link, no execute). Driver layer + WiFi reconnect + flash partition behaviour need real hardware. AIO_Tool has ~199 tests across 6 crates: unit, integration, wire-format goldens (hex-compared against the legacy Python tool's output), property tests (proptest), and converter parity tests (byte-identical pixel encoding vs Python). **Known gaps**: GUI smoke tests (`egui_kittest` — spec §6 Layer 5) are not yet implemented; tracked for v3.1. Long-running memory leaks are NOT tested (stock leak class — see [`Docs/development/09-test-architecture-decomposition.md`](./Docs/development/09-test-architecture-decomposition.md) §8 for the planned fix design).

## Things that LOOK like bugs but aren't

- `AIO_VERSION` in `common.h` lags behind release tags sometimes — this is OK, the tag is source of truth, the constant gates cache-busting on the next bump
- `dist/HoloCubic_AIO_firmware_v3.0.X.bin` appearing as untracked is fine — local user testing artifact, NOT to be committed
- Settings tab "Read All" logs "(undecodable N bytes)" against current firmware — this is B15 from the Plan 1 Discovery doc; firmware-side fix is queued separately from the Rust rewrite
- File Manager Rename / Properties don't actually rename / show properties — these are B1 / B2 wire-format bugs preserved verbatim from the Python tool (firmware-side handler bug; out of scope for the tool rewrite)
- Many comments in firmware source are zh-cn from the original ClimbSnail upstream — leave them; only translate the ones we change

## When debugging

- CI fail: `gh run view <id> --log-failed | grep -E "error:|FAIL"` first
- AIO_Tool GUI bug: the bus + worker pattern means UI updates are entirely driven by `AppEvent` variants. Add a log line in the `App::update` `try_recv` arm to see what's flowing. The cancel-flag contract means an apparently-hung worker is usually a missed `cancel.load()` check at a loop boundary.
- Release fail after PR was green: probably a host-stub / Arduino-core divergence (firmware side), or a platform-specific cargo build issue (tool side). Verify `pio run -e HoloCubic_AIO_Releases` AND `pio run -e native_test` build cleanly; for tool issues, `tool-rust.yml`'s 3-OS matrix should have caught it at PR time — if not, that's the first place to add a regression.

## What to ask before destructive actions

Confirm before: force-pushing tags (acceptable when no GitHub release was published, otherwise bump version), `git tag -d` on remote, deleting branches with unmerged commits, modifying CI workflows in ways that change gating semantics. The user expects standard PR flow for everything else.
