# B15 fix — Studio Settings via HTTP, delete dead serial-settings code

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Studio's broken serial-based Settings tab with HTTP calls to the firmware's existing web-settings flow (which already persists to SPIFFS `.cfg` files). Delete the dead serial-settings module on both sides.

**Architecture:** Three sequential PRs.
1. **PR1 (firmware, additive)**: Add `GET /api/settings` route returning the live `sys_cfg` + `rgb_cfg` config structs as JSON. Pure addition, no deletions, no behaviour change for existing flows.
2. **PR2 (tool, cleanup)**: Studio Settings tab fetches `GET /api/settings`, displays rows, on Save POSTs form-encoded to existing `/saveSysConf` / `/saveRgbConf` handlers. Delete Tauri commands `read_all_settings` / `write_changed_settings` / `list_setting_keys`, delete `cubictool.json`, delete `aio-protocol/src/setting.rs`, delete egui tool's settings tab.
3. **PR3 (firmware, dead-code cleanup)**: Delete `app/settings/settings.cpp`, delete `SettingsMsg` from `message.{h,cpp}`, remove from app registry. Must merge AFTER PR2 ships (so no tool version expects the serial protocol).

**Tech Stack:** ESP32 Arduino-core + ArduinoJson v6 (firmware side, already linked); `ureq` 2.x (Rust HTTP client, added to `aio-studio`); standard JSX/React Tauri command bridge.

**Why this order:** PR1 is additive and risk-free → ship first, deploy to test device. PR2 depends on PR1 being on the test device. PR3 is pure deletion that requires PR2 to be the published tool version (otherwise an old tool would silently get no response — same UX as today's "decoded 0" but more confusing).

---

## File Structure

### PR1 (firmware additive)

| Action | Path | Responsibility |
|---|---|---|
| Modify | `AIO_Firmware_PIO/src/app/server/web_api.cpp` | Add `api_settings()` handler — pure JSON serialiser reading `sys_cfg`, `rgb_cfg` |
| Modify | `AIO_Firmware_PIO/src/app/server/web_api.h` | Declare `api_settings()` |
| Modify | `AIO_Firmware_PIO/src/app/server/server.cpp:83` (after `api_stats`) | Register route `server.on("/api/settings", HTTP_GET, api_settings)` |
| Create | `AIO_Firmware_PIO/test/native_unit/test_api_settings.cpp` | Unit test for the pure JSON-build function |

### PR2 (tool)

| Action | Path | Responsibility |
|---|---|---|
| Modify | `AIO_Tool/studio/Cargo.toml` | Add `ureq` dep |
| Modify | `AIO_Tool/studio/src/commands.rs` | Add `fetch_settings_http(host)` + `save_settings_http(host, category, fields)`; delete `list_setting_keys` / `read_all_settings` / `write_changed_settings` / `SETTINGS_SCHEMA_RAW` |
| Modify | `AIO_Tool/studio/src/lib.rs:44` (invoke_handler list) | Register new commands, remove old |
| Delete | `AIO_Tool/cubictool.json` | Replaced by HTTP schema endpoint |
| Modify | `Docs/design/studio-flasher/studio-pages.jsx` (settings tab) | Replace serial-based hook with HTTP fetch + form POST |
| Delete | `AIO_Tool/crates/aio-protocol/src/setting.rs` | Dead — only used by deleted Studio commands |
| Modify | `AIO_Tool/crates/aio-protocol/src/lib.rs` | Remove `pub mod setting;` and re-exports |
| Delete | `AIO_Tool/crates/aio-protocol/tests/golden_setting.rs` | Tests for deleted code |
| Modify | `AIO_Tool/crates/aio-tool/src/tabs/settings_schema.rs` | egui tool Settings tab — switch to same HTTP path OR delete the tab entirely (decision in Task 14) |
| Modify | `CLAUDE.md` | Remove B15 reference, document `/api/settings` |
| Modify | `AIO_Tool/crates/aio-tool/BUGS.md` | Remove B15 row; add a row noting it was fixed in PR2 |
| Modify | `Docs/development/studio-hardware-verification.md` | Update Settings tab section — new acceptance criteria |

### PR3 (firmware dead-code cleanup)

| Action | Path | Responsibility |
|---|---|---|
| Delete | `AIO_Firmware_PIO/src/app/settings/settings.cpp` | Whole serial-settings module |
| Delete | `AIO_Firmware_PIO/src/app/settings/settings.h` | Header for same |
| Delete | `AIO_Firmware_PIO/src/app/settings/settings_gui.{cpp,h}` | Verify no cross-refs first (Task 18) |
| Modify | `AIO_Firmware_PIO/src/message.h:64-80` | Delete `SettingsMsg` class declaration |
| Modify | `AIO_Firmware_PIO/src/message.cpp:58-200` (approx, verify line range) | Delete `SettingsMsg::*` definitions |
| Modify | `AIO_Firmware_PIO/src/sys/app_controller.cpp` (or wherever apps are registered) | Remove `&settings_app` from app list |
| Modify | `AIO_Firmware_PIO/src/common.h` or wherever | Remove any `SettingsMsg`-related enum members referenced only by the deleted module (verify with grep first) |

---

## PR1 — Firmware: `/api/settings` JSON endpoint

### Task 1: Discover the exact `sys_cfg` / `rgb_cfg` struct field names

**Files:**
- Read: `AIO_Firmware_PIO/src/sys/app_controller.h` (struct defs)
- Read: `AIO_Firmware_PIO/src/sys/app_controller_config.cpp` (write/read serialisation)

- [ ] **Step 1: Grep for struct definitions**

Run:
```bash
grep -n "struct SYS_CONFIG\|struct RGB_CONFIG\|SysConfig\|RgbConfig\|sys_cfg;\|rgb_cfg;" AIO_Firmware_PIO/src/sys/*.h AIO_Firmware_PIO/src/sys/*.cpp
```

Expected: locations of struct decls and the global instances `extern SYS_CONFIG sys_cfg;` etc.

- [ ] **Step 2: Read the exact field list**

Write down (in a scratch note, not a file) the complete field list for both structs. We need this to make the JSON output match struct member names verbatim. Sample expected for `sys_cfg` (from grep evidence already gathered): `ssid_0`, `password_0`, `ssid_1`, `password_1`, `ssid_2`, `password_2`, `power_mode`, `backLight`, `rotation`, `auto_calibration_mpu`, `mpu_order`, `auto_start_app`.

- [ ] **Step 3: Commit nothing** — discovery only

### Task 2: Write the failing unit test for `build_settings_json`

**Files:**
- Create: `AIO_Firmware_PIO/test/native_unit/test_api_settings.cpp`

- [ ] **Step 1: Add the test file**

```cpp
// test/native_unit/test_api_settings.cpp
#include <unity.h>
#include <ArduinoJson.h>
#include "app/server/web_api.h"  // declares build_settings_json

// Minimal host-side stubs for the structs we read (full struct defs are not
// available at native_unit link time; we declare just what build_settings_json
// touches and link against a stub provider).
extern "C" void set_test_sys_cfg(const char* ssid_0, unsigned backLight);
extern "C" void set_test_rgb_cfg(unsigned time);

void test_build_settings_json_emits_sys_block(void)
{
    set_test_sys_cfg("MyHomeWiFi", 80);
    char buf[1024];
    size_t n = build_settings_json(buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, buf, n);
    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_EQUAL_STRING("MyHomeWiFi", doc["sys"]["ssid_0"]);
    TEST_ASSERT_EQUAL(80, doc["sys"]["backLight"].as<int>());
}

void test_build_settings_json_emits_rgb_block(void)
{
    set_test_rgb_cfg(50);
    char buf[1024];
    size_t n = build_settings_json(buf, sizeof(buf));
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, buf, n);
    TEST_ASSERT_EQUAL(50, doc["rgb"]["time"].as<int>());
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_build_settings_json_emits_sys_block);
    RUN_TEST(test_build_settings_json_emits_rgb_block);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the test to confirm it fails**

Run:
```bash
cd AIO_Firmware_PIO
pio test -e native_unit -f test_api_settings
```

Expected: link error — `build_settings_json` unresolved, `set_test_sys_cfg` unresolved.

- [ ] **Step 3: Commit nothing** — only commit once green

### Task 3: Implement `build_settings_json` + host stubs

**Files:**
- Modify: `AIO_Firmware_PIO/src/app/server/web_api.h`
- Modify: `AIO_Firmware_PIO/src/app/server/web_api.cpp`
- Create: `AIO_Firmware_PIO/test/native_unit/stub_api_settings_cfg.cpp`

- [ ] **Step 1: Declare the pure function in the header**

Add to `web_api.h`:

```cpp
// Build a JSON document describing all current config in a fixed buffer.
// Returns the byte count written (excluding NUL). Pure: reads sys_cfg /
// rgb_cfg globals and serialises via ArduinoJson; no I/O, no allocations
// on the heap beyond ArduinoJson's StaticJsonDocument.
size_t build_settings_json(char* buf, size_t buf_len);

// HTTP handler that calls build_settings_json + Send_HTML.
void api_settings();
```

- [ ] **Step 2: Implement `build_settings_json` in `web_api.cpp`**

```cpp
#include "web_api.h"
#include <ArduinoJson.h>
#include "sys/app_controller.h"  // for extern sys_cfg, rgb_cfg

size_t build_settings_json(char* buf, size_t buf_len)
{
    StaticJsonDocument<1024> doc;
    JsonObject sys = doc.createNestedObject("sys");
    sys["ssid_0"]               = sys_cfg.ssid_0;
    sys["password_0"]           = sys_cfg.password_0;
    sys["ssid_1"]               = sys_cfg.ssid_1;
    sys["password_1"]           = sys_cfg.password_1;
    sys["ssid_2"]               = sys_cfg.ssid_2;
    sys["password_2"]           = sys_cfg.password_2;
    sys["power_mode"]           = sys_cfg.power_mode;
    sys["backLight"]            = sys_cfg.backLight;
    sys["rotation"]             = sys_cfg.rotation;
    sys["auto_calibration_mpu"] = sys_cfg.auto_calibration_mpu;
    sys["mpu_order"]            = sys_cfg.mpu_order;
    sys["auto_start_app"]       = sys_cfg.auto_start_app;

    JsonObject rgb = doc.createNestedObject("rgb");
    rgb["min_value_0"]    = rgb_cfg.min_value_0;
    rgb["min_value_1"]    = rgb_cfg.min_value_1;
    rgb["min_value_2"]    = rgb_cfg.min_value_2;
    rgb["max_value_0"]    = rgb_cfg.max_value_0;
    rgb["max_value_1"]    = rgb_cfg.max_value_1;
    rgb["max_value_2"]    = rgb_cfg.max_value_2;
    rgb["step_0"]         = rgb_cfg.step_0;
    rgb["step_1"]         = rgb_cfg.step_1;
    rgb["step_2"]         = rgb_cfg.step_2;
    rgb["min_brightness"] = rgb_cfg.min_brightness;
    rgb["max_brightness"] = rgb_cfg.max_brightness;
    rgb["brightness_step"]= rgb_cfg.brightness_step;
    rgb["time"]           = rgb_cfg.time;

    return serializeJson(doc, buf, buf_len);
}

void api_settings()
{
    extern WebServer server;  // already declared in server.cpp
    char buf[1024];
    size_t n = build_settings_json(buf, sizeof(buf));
    server.send(200, F("application/json"), String(buf, n));
}
```

- [ ] **Step 3: Write the host stub for `sys_cfg` / `rgb_cfg`**

```cpp
// test/native_unit/stub_api_settings_cfg.cpp
// Native host stubs for sys_cfg / rgb_cfg so build_settings_json links
// against test code without dragging in the entire AppController.
#include <string>

// Minimal mirrors of the real structs. Field names must match the real
// SYS_CONFIG / RGB_CONFIG declarations in sys/app_controller.h verbatim,
// otherwise build_settings_json won't compile.
struct SYS_CONFIG {
    std::string ssid_0, password_0, ssid_1, password_1, ssid_2, password_2;
    unsigned power_mode = 0, backLight = 0, rotation = 0;
    unsigned auto_calibration_mpu = 0, mpu_order = 0;
    std::string auto_start_app;
};
struct RGB_CONFIG {
    unsigned min_value_0 = 0, min_value_1 = 0, min_value_2 = 0;
    unsigned max_value_0 = 0, max_value_1 = 0, max_value_2 = 0;
    unsigned step_0 = 0, step_1 = 0, step_2 = 0;
    unsigned min_brightness = 0, max_brightness = 0;
    unsigned brightness_step = 0, time = 0;
};

SYS_CONFIG sys_cfg;
RGB_CONFIG rgb_cfg;

extern "C" void set_test_sys_cfg(const char* ssid_0, unsigned backLight) {
    sys_cfg.ssid_0 = ssid_0;
    sys_cfg.backLight = backLight;
}
extern "C" void set_test_rgb_cfg(unsigned time) {
    rgb_cfg.time = time;
}
```

- [ ] **Step 4: Update `platformio.ini` native_unit env to include the new test source + ArduinoJson lib**

Verify `[env:native_unit]` already includes ArduinoJson; if not, add `lib_deps = bblanchon/ArduinoJson@^6.21.0`.

- [ ] **Step 5: Run the test, expect PASS**

Run:
```bash
cd AIO_Firmware_PIO
pio test -e native_unit -f test_api_settings
```

Expected: 2 tests PASS, total time <30s.

- [ ] **Step 6: Commit**

```bash
git add AIO_Firmware_PIO/src/app/server/web_api.{cpp,h} \
        AIO_Firmware_PIO/test/native_unit/test_api_settings.cpp \
        AIO_Firmware_PIO/test/native_unit/stub_api_settings_cfg.cpp \
        AIO_Firmware_PIO/platformio.ini
git commit -m "$(cat <<'EOF'
firmware: add build_settings_json + /api/settings endpoint scaffolding

Pure JSON encoder reads sys_cfg / rgb_cfg into an ArduinoJson document.
Host-side native_unit test covers the encoder; HTTP route registration
in the next commit.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### Task 4: Register the HTTP route

**Files:**
- Modify: `AIO_Firmware_PIO/src/app/server/server.cpp:83-84`

- [ ] **Step 1: Add the route registration**

After the existing `server.on("/api/stats", HTTP_GET, api_stats);` line, add:

```cpp
    server.on("/api/settings", HTTP_GET, api_settings);
```

- [ ] **Step 2: Build firmware for ESP32**

Run:
```bash
cd AIO_Firmware_PIO
pio run -e HoloCubic_AIO_Releases
```

Expected: build succeeds, output `firmware.bin` ~1.85-2.0 MB.

- [ ] **Step 3: Commit**

```bash
git add AIO_Firmware_PIO/src/app/server/server.cpp
git commit -m "firmware: register GET /api/settings route"
```

### Task 5: Hardware verification — PR1 acceptance test

**Files:**
- Modify: `Docs/development/studio-hardware-verification.md` (add a section)

- [ ] **Step 1: Flash the test firmware to a HoloCubic**

Use existing Studio Flasher tab. Verify device boots into firmware.

- [ ] **Step 2: Connect device to WiFi, note its IP**

- [ ] **Step 3: Curl the new endpoint**

Run from the same network:
```bash
curl -v http://<device-ip>/api/settings
```

Expected: HTTP 200, `Content-Type: application/json`, body is a JSON object with `sys` and `rgb` top-level keys, all the fields populated from the device's actual config.

- [ ] **Step 4: Verify field values match the device's actual state**

Cross-check: navigate to `http://<device-ip>/sys_setting` in a browser — the form fields shown there should match the JSON values returned by `/api/settings`.

- [ ] **Step 5: Add this test to the hardware-verification doc**

Append to `Docs/development/studio-hardware-verification.md` under a new "Settings (HTTP)" section:

```markdown
## Settings (HTTP)

- [ ] **[H-1] GET /api/settings returns live config** — `curl http://<ip>/api/settings`
      returns HTTP 200 + JSON containing `sys` and `rgb` blocks. Values match
      what `/sys_setting` form shows in a browser.
```

- [ ] **Step 6: Commit the doc update**

```bash
git add Docs/development/studio-hardware-verification.md
git commit -m "docs: add Settings (HTTP) hardware verification step H-1"
```

### Task 6: PR1 — open and merge

- [ ] **Step 1: Push branch**

```bash
git push -u origin fix-b15-firmware-api-settings
```

- [ ] **Step 2: Create PR**

```bash
gh pr create --title "firmware: add /api/settings JSON endpoint (PR1 of B15 fix)" --body "$(cat <<'EOF'
## Summary
- New `GET /api/settings` route returns current `sys_cfg` + `rgb_cfg` as JSON.
- Pure encoder `build_settings_json` is unit-tested under `native_unit`.
- Additive only — no existing behaviour changes. No deletions in this PR.

## Why
First of three PRs fixing the long-standing B15 (Studio Settings tab returns "decoded 0 of N keys" against current firmware). Replaces the broken serial protocol with a working HTTP path; the new endpoint is the read side. SET will continue to use the existing `/save<Foo>Conf` form POST handlers.

## Test plan
- [x] `pio test -e native_unit -f test_api_settings` — 2 tests PASS
- [x] `pio run -e HoloCubic_AIO_Releases` — firmware builds
- [ ] Hardware H-1 from `studio-hardware-verification.md`

## Next PRs
- PR2: Studio Settings tab → HTTP, delete serial commands & dead code
- PR3: Delete firmware serial-settings module (after PR2 ships)

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 3: Wait for CI**

```bash
gh run list --branch fix-b15-firmware-api-settings --limit 1 --json databaseId,status
gh run watch <id> --exit-status --interval 30
```

- [ ] **Step 4: Squash-merge after green + hardware H-1 passes**

```bash
gh pr merge <#> --squash --delete-branch
```

---

## PR2 — Studio: HTTP Settings tab + delete dead Rust + delete cubictool.json

### Task 7: Add `ureq` dep to studio

**Files:**
- Modify: `AIO_Tool/studio/Cargo.toml`

- [ ] **Step 1: Add the dep**

Append to `[dependencies]`:

```toml
# HTTP client for the new /api/settings flow. ureq picked over reqwest for
# minimal binary size (no tokio runtime) — the Studio settings flow is
# request/response sync, no streaming.
ureq = { version = "2.10", default-features = false, features = ["tls"] }
```

- [ ] **Step 2: Verify it builds**

```bash
cd AIO_Tool/studio
cargo +stable build --bin aio-studio
```

Expected: build succeeds. May take a few minutes for first ureq build.

- [ ] **Step 3: Commit**

```bash
git add AIO_Tool/studio/Cargo.toml AIO_Tool/studio/Cargo.lock
git commit -m "studio: add ureq dep for HTTP settings flow"
```

### Task 8: Add new Tauri commands `fetch_settings_http` + `save_settings_http`

**Files:**
- Modify: `AIO_Tool/studio/src/commands.rs`

- [ ] **Step 1: Add at end of `commands.rs` (above the test/scratch area if any)**

```rust
// =====================================================================
// HTTP Settings — replaces the serial-based read_all_settings / write_changed_settings.
// =====================================================================

#[derive(Serialize)]
pub struct SettingsResponse {
    /// Full JSON object returned by GET /api/settings. Studio's JS side
    /// renders it; the Rust bridge is intentionally pass-through so adding
    /// new sections firmware-side doesn't need a tool rebuild.
    pub json: serde_json::Value,
}

/// Fetch current device settings via HTTP. `host` is the bare IP or hostname
/// (no scheme). Times out after 3 seconds — settings dump is sub-kB and
/// should be near-instant on the local network.
#[tauri::command]
pub fn fetch_settings_http(host: String) -> Result<SettingsResponse, String> {
    let url = format!("http://{host}/api/settings");
    let resp = ureq::get(&url)
        .timeout(Duration::from_secs(3))
        .call()
        .map_err(|e| format!("GET {url}: {e}"))?;
    let json: serde_json::Value = resp
        .into_json()
        .map_err(|e| format!("parse JSON from {url}: {e}"))?;
    Ok(SettingsResponse { json })
}

#[derive(Deserialize)]
pub struct SaveField {
    pub key: String,
    pub value: String,
}

/// POST a category's changed fields to the existing form handler.
/// `category` is one of `"sys"` / `"rgb"` / `"weather"` / etc.; the handler
/// URL is `/save<Cat>Conf` with the first letter uppercased.
/// Fields are form-encoded — matches what the browser submit does.
#[tauri::command]
pub fn save_settings_http(
    host: String,
    category: String,
    fields: Vec<SaveField>,
) -> Result<(), String> {
    let mut handler = match category.as_str() {
        "sys" => "saveSysConf",
        "rgb" => "saveRgbConf",
        "weather" => "saveWeatherConf",
        "weather_old" => "saveWeatherOldConf",
        "bili" => "saveBiliConf",
        "stock" => "saveStockConf",
        "picture" => "savePictureConf",
        "media" => "saveMediaConf",
        "screen" => "saveScreenConf",
        "heartbeat" => "saveHeartbeatConf",
        "anniversary" => "saveAnniversaryConf",
        "pc_resource" => "savePCResourceConf",
        other => return Err(format!("unknown settings category `{other}`")),
    }
    .to_owned();
    let url = format!("http://{host}/{handler}");
    handler.clear(); // silence unused after this point

    let pairs: Vec<(&str, &str)> = fields
        .iter()
        .map(|f| (f.key.as_str(), f.value.as_str()))
        .collect();
    ureq::post(&url)
        .timeout(Duration::from_secs(3))
        .send_form(&pairs)
        .map_err(|e| format!("POST {url}: {e}"))?;
    Ok(())
}
```

- [ ] **Step 2: Verify `serde_json::Value` is reachable**

Check imports at top of `commands.rs`. Add `use serde_json;` if missing. `serde_json` is already a transitive dep via `serde` — verify in `Cargo.toml`. If absent, add `serde_json = "1"` to studio's `[dependencies]`.

- [ ] **Step 3: Build**

```bash
cargo +stable build --bin aio-studio
```

Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
git add AIO_Tool/studio/src/commands.rs AIO_Tool/studio/Cargo.toml AIO_Tool/studio/Cargo.lock
git commit -m "studio: add fetch_settings_http + save_settings_http Tauri commands"
```

### Task 9: Register the new commands, remove the old

**Files:**
- Modify: `AIO_Tool/studio/src/lib.rs` (the `invoke_handler!` list near line 44)

- [ ] **Step 1: Open the file and locate the `tauri::generate_handler!` block**

It will look like (current state):

```rust
.invoke_handler(tauri::generate_handler![
    commands::list_ports,
    commands::connect_device,
    commands::disconnect_device,
    commands::send_remote,
    commands::reboot_device,
    commands::pick_partition_bin,
    commands::start_flash,
    commands::start_erase,
    commands::cancel_op,
    commands::list_setting_keys,        // ← delete
    commands::read_all_settings,        // ← delete
    commands::write_changed_settings,   // ← delete
    // ... fm / img / video commands
])
```

- [ ] **Step 2: Remove the three `commands::*_settings*` lines**

- [ ] **Step 3: Add the two new commands**

```rust
    commands::fetch_settings_http,
    commands::save_settings_http,
```

- [ ] **Step 4: Build**

```bash
cargo +stable build --bin aio-studio
```

Expected: now fails with "unused function `list_setting_keys`" etc. — these are about to be deleted in Task 10.

- [ ] **Step 5: Do not commit yet** — Task 10 must complete the deletion to get clean build.

### Task 10: Delete the dead serial-settings commands from Rust

**Files:**
- Modify: `AIO_Tool/studio/src/commands.rs` — delete `list_setting_keys`, `read_all_settings`, `write_changed_settings`, the `SETTINGS_SCHEMA_RAW` constant, the `SettingKeyDto` / `SettingValueDto` / `SettingChange` structs, `parse_value_type` helper, `emit_settings_log` / `emit_settings_warn` helpers (if only used by deleted code — verify with grep first)

- [ ] **Step 1: Grep all usages**

```bash
grep -n "list_setting_keys\|read_all_settings\|write_changed_settings\|SETTINGS_SCHEMA_RAW\|SettingKeyDto\|SettingValueDto\|SettingChange\|parse_value_type\|emit_settings_log\|emit_settings_warn" AIO_Tool/studio/src/*.rs
```

Expected: all hits are inside the to-be-deleted code paths. If any helper is used elsewhere, leave that helper.

- [ ] **Step 2: Delete the function bodies + types**

Delete inclusive of the doc comments. Resulting `commands.rs` should be ~150 lines shorter.

- [ ] **Step 3: Remove the now-unused imports**

`use aio_protocol::{SettingMsg, ValueType};` and `use aio_device::serial::SerialTransport;` — verify by grep there are no other uses. (Serial flasher commands still use SerialTransport indirectly via aio_flasher — likely NO uses left in studio after Task 10. If there are, leave the import.)

- [ ] **Step 4: Build**

```bash
cargo +stable build --bin aio-studio
```

Expected: success, no warnings about unused imports/functions.

- [ ] **Step 5: Commit**

```bash
git add AIO_Tool/studio/src/commands.rs AIO_Tool/studio/src/lib.rs
git commit -m "$(cat <<'EOF'
studio: replace serial settings commands with HTTP-based equivalents

Deletes list_setting_keys / read_all_settings / write_changed_settings
and the embedded cubictool.json schema. The new fetch_settings_http /
save_settings_http commands use the firmware's existing web-settings flow
which actually persists to SPIFFS. Settings tab JSX wiring in next commit.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### Task 11: Delete `cubictool.json`

**Files:**
- Delete: `AIO_Tool/cubictool.json`

- [ ] **Step 1: Confirm no remaining references**

```bash
grep -rn "cubictool.json\|cubictool\\.json" AIO_Tool/ Docs/
```

Expected: zero matches after Task 10's deletions land. If the egui tool (`crates/aio-tool/src/tabs/settings_schema.rs`) still references it, that's handled in Task 14.

- [ ] **Step 2: Delete the file (only if grep returned empty)**

```bash
git rm AIO_Tool/cubictool.json
```

- [ ] **Step 3: Commit**

```bash
git commit -m "studio: delete cubictool.json — superseded by GET /api/settings"
```

### Task 12: Rewrite the Settings tab JSX

**Files:**
- Modify: `Docs/design/studio-flasher/studio-pages.jsx` (search for the Settings tab component — likely `function StudioSettings` or similar)

- [ ] **Step 1: Locate the Settings tab component**

```bash
grep -n "讀取設定\|讀取所有\|read_all_settings\|寫入修改\|write_changed_settings\|參數設定" Docs/design/studio-flasher/studio-pages.jsx
```

Take note of the component's start/end line range.

- [ ] **Step 2: Replace the component body** with the new HTTP-driven version. Template:

```jsx
function StudioSettings({ deviceHost, log }) {
  const { useState, useEffect, useCallback } = React;
  const invoke = window.__TAURI__?.core?.invoke;
  const [host, setHost] = useState(deviceHost || "");
  const [snapshot, setSnapshot] = useState(null);       // {sys: {...}, rgb: {...}}
  const [edits, setEdits] = useState({});               // {key: newValue} — flat across categories
  const [status, setStatus] = useState("idle");         // idle | loading | error | saving

  const fetchAll = useCallback(async () => {
    if (!host) { setStatus("error"); return; }
    setStatus("loading");
    try {
      const { json } = await invoke("fetch_settings_http", { host });
      setSnapshot(json);
      setEdits({});
      setStatus("idle");
      log(`fetched settings from ${host}`, "ok");
    } catch (e) {
      setStatus("error");
      log(`fetch ${host}: ${e}`, "err");
    }
  }, [host, log]);

  const dirtyCount = Object.keys(edits).length;

  const save = useCallback(async () => {
    if (!dirtyCount) return;
    setStatus("saving");
    // Group edits by category. snapshot tells us which category each key lives in.
    const byCategory = {};
    for (const [key, value] of Object.entries(edits)) {
      const category = Object.keys(snapshot).find((cat) => key in snapshot[cat]);
      if (!category) continue;
      (byCategory[category] ||= []).push({ key, value });
    }
    try {
      for (const [category, fields] of Object.entries(byCategory)) {
        await invoke("save_settings_http", { host, category, fields });
        log(`saved ${fields.length} ${category} field(s)`, "ok");
      }
      await fetchAll();  // re-fetch to confirm
    } catch (e) {
      setStatus("error");
      log(`save: ${e}`, "err");
    }
  }, [edits, snapshot, host, dirtyCount, fetchAll, log]);

  return (
    <div className="settings-panel">
      <div className="row">
        <input
          className="fld"
          placeholder="192.168.x.x"
          value={host}
          onChange={(e) => setHost(e.target.value)}
        />
        <button className="btn primary" onClick={fetchAll} disabled={status === "loading"}>
          {status === "loading" ? "讀取中…" : "讀取設定"}
        </button>
        <button className="btn" onClick={save} disabled={!dirtyCount || status === "saving"}>
          {status === "saving" ? "寫入中…" : `寫入修改 (${dirtyCount})`}
        </button>
      </div>
      {snapshot && Object.entries(snapshot).map(([category, fields]) => (
        <fieldset key={category}>
          <legend>{category}</legend>
          {Object.entries(fields).map(([key, value]) => (
            <div className="row" key={key}>
              <label>{key}</label>
              <input
                className="fld"
                value={key in edits ? edits[key] : String(value)}
                onChange={(e) =>
                  setEdits((prev) => ({ ...prev, [key]: e.target.value }))
                }
              />
            </div>
          ))}
        </fieldset>
      ))}
    </div>
  );
}
```

- [ ] **Step 3: Verify it compiles in-browser (Babel transpile)**

Open a fresh static-server preview:
```bash
( cd Docs/design/studio-flasher && python -m http.server 8765 )
```

Browse to `http://localhost:8765`. The Settings tab should render (without working buttons since `invoke` is undefined in browser preview mode). No JSX parse errors.

- [ ] **Step 4: Run Studio against the test device**

```bash
cd AIO_Tool/studio
cargo run --bin aio-studio
```

In the Studio window: enter device IP, click 讀取設定, verify fields populate with real values. Edit one, click 寫入修改, verify save log appears and re-fetch shows the new value.

- [ ] **Step 5: Commit**

```bash
git add Docs/design/studio-flasher/studio-pages.jsx
git commit -m "studio: rewrite Settings tab to use HTTP /api/settings flow"
```

### Task 13: Delete `aio-protocol/src/setting.rs` + golden test

**Files:**
- Delete: `AIO_Tool/crates/aio-protocol/src/setting.rs`
- Delete: `AIO_Tool/crates/aio-protocol/tests/golden_setting.rs`
- Modify: `AIO_Tool/crates/aio-protocol/src/lib.rs`

- [ ] **Step 1: Confirm no remaining users**

```bash
grep -rn "SettingMsg\|aio_protocol::SettingMsg\|protocol::SettingMsg\|use.*setting::" AIO_Tool/
```

Expected: zero hits (PR2's earlier tasks removed all consumers). If the egui tool (`tabs/settings_schema.rs`) still imports it, this delete depends on Task 14.

- [ ] **Step 2: Delete the file**

```bash
git rm AIO_Tool/crates/aio-protocol/src/setting.rs
git rm AIO_Tool/crates/aio-protocol/tests/golden_setting.rs
```

- [ ] **Step 3: Remove the module from `lib.rs`**

Open `AIO_Tool/crates/aio-protocol/src/lib.rs` and delete the `pub mod setting;` line + any `pub use setting::*;` re-exports.

- [ ] **Step 4: Build the workspace**

```bash
cd AIO_Tool
cargo +1.82.0 build --workspace
cargo +1.82.0 test --workspace
```

Expected: build + tests pass. No unresolved imports.

- [ ] **Step 5: Commit**

```bash
git add AIO_Tool/crates/aio-protocol/src/lib.rs
git commit -m "aio-protocol: delete SettingMsg — superseded by HTTP /api/settings"
```

### Task 14: Decide egui tool's Settings tab fate

**Files:**
- Read: `AIO_Tool/crates/aio-tool/src/tabs/settings.rs`, `AIO_Tool/crates/aio-tool/src/tabs/settings_schema.rs`

- [ ] **Step 1: Read the egui Settings tab**

Decide between two paths:

**Path A: Mirror Studio — switch egui to HTTP too.**
- Pro: feature parity, both tools work
- Con: doubles the work; need to port the HTTP flow to egui too

**Path B: Delete the egui Settings tab.**
- Pro: smaller diff; egui is a legacy UI that release.yml still ships but Studio is the future
- Con: feature regression for users who still use the egui binary

Recommended: **Path B**. The Studio is the documented direction (per recent commits + screenshot refresh). Path B is the smaller change.

- [ ] **Step 2: If Path B, delete the tab module + remove from tab list**

Search for the tab registration:
```bash
grep -n "Settings\|settings_schema\|SettingsTab" AIO_Tool/crates/aio-tool/src/app.rs AIO_Tool/crates/aio-tool/src/tabs/mod.rs
```

Delete the tab variant + its module file(s) + the `mod settings_schema;` line.

- [ ] **Step 3: Verify the i18n keys for the deleted tab are also removed**

```bash
grep -rn "tab_settings\|setting_read_all\|setting_write_changed" AIO_Tool/i18n/
```

Remove matching keys from all three locale files (`en_US.json`, `zh_CN.json`, `zh_TW.json`) — `aio-i18n/build.rs` panics if they diverge.

- [ ] **Step 4: Build + test**

```bash
cargo +1.82.0 build --bin aio-tool
cargo +1.82.0 test --workspace
```

Expected: success. Locale key counts in build log should show e.g. "all 3 locales share 197 keys" (3 fewer than before).

- [ ] **Step 5: Commit**

```bash
git commit -am "aio-tool (egui): remove Settings tab — use Studio (HTTP) instead"
```

### Task 15: Update CLAUDE.md + BUGS.md + hardware verification doc

**Files:**
- Modify: `CLAUDE.md` (remove B15 row from "Things that LOOK like bugs but aren't", document `/api/settings`)
- Modify: `AIO_Tool/crates/aio-tool/BUGS.md` (mark B15 fixed)
- Modify: `Docs/development/studio-hardware-verification.md` (Settings tab section — new acceptance criteria for HTTP flow)

- [ ] **Step 1: Edit `CLAUDE.md`**

Find the bullet:
> Settings tab "Read All" logs "(undecodable N bytes)" against current firmware — this is B15 from the Plan 1 Discovery doc; firmware-side fix is queued separately from the Rust rewrite

Delete it.

In the "Architecture in one paragraph" section, add a sentence (after the existing Studio mention):

> Settings tab speaks HTTP to the firmware's existing web flow (`GET /api/settings` reads `sys_cfg`/`rgb_cfg` as JSON; `POST /save<Cat>Conf` writes form-encoded fields — same handlers the browser-based settings pages use, which persist to SPIFFS `.cfg` files).

- [ ] **Step 2: Edit `AIO_Tool/crates/aio-tool/BUGS.md`**

Change the B15 row's last cell from "Tracked for a firmware-repo fix; Plan 7 ships send-side functional." to "**FIXED 2026-06-09** — Studio Settings tab rewritten to use HTTP `/api/settings` + form-POST handlers. Serial-side `SettingsMsg` deleted (firmware module deletion in PR3)."

- [ ] **Step 3: Edit `Docs/development/studio-hardware-verification.md`**

Replace the existing Settings tab section's "B15 expected" warning with a positive checklist tied to the HTTP flow:

```markdown
## Settings tab

- [ ] **[S-1] 讀取設定** — entering the device IP and clicking 讀取設定 populates
      all fields within ~1s. Values match those visible in
      `http://<ip>/sys_setting` browser page.
- [ ] **[S-2] 寫入修改** — editing `backLight`, clicking 寫入修改 (1), then
      讀取設定 again — the new value persists. Brightness on the physical
      device changes accordingly.
- [ ] **[S-3] Offline error** — turning off device WiFi, clicking 讀取設定
      shows a clear "fetch X: timeout" error in log within 3s (the ureq
      timeout setting).
```

- [ ] **Step 4: Commit**

```bash
git add CLAUDE.md AIO_Tool/crates/aio-tool/BUGS.md Docs/development/studio-hardware-verification.md
git commit -m "docs: mark B15 fixed — Studio Settings via HTTP"
```

### Task 16: PR2 — open and merge

- [ ] **Step 1: Push branch**

```bash
git push -u origin fix-b15-studio-http-settings
```

- [ ] **Step 2: Open PR**

```bash
gh pr create --title "studio: Settings tab via HTTP, delete serial-settings code (PR2 of B15 fix)" --body "$(cat <<'EOF'
## Summary
- Studio Settings tab fetches `GET /api/settings` (added in PR1), saves via existing `POST /save<Cat>Conf` form handlers.
- Deletes the serial-based `SettingMsg` protocol (aio-protocol/src/setting.rs + golden test).
- Deletes Studio's `read_all_settings` / `write_changed_settings` / `list_setting_keys` Tauri commands + `cubictool.json` embed.
- Deletes egui tool's Settings tab (Path B — Studio is the documented future).
- Updates CLAUDE.md / BUGS.md to mark B15 fixed.

## Why
B15 was never just a wire-format mismatch — firmware NVS storage was never actually wired up. The web flow IS the real storage path (writes to SPIFFS `.cfg` files). This PR makes Studio use that path.

## Test plan
- [x] `cargo +1.82.0 test --workspace` — ~196 tests pass (3 fewer than before — golden_setting.rs deleted)
- [x] `cargo +1.82.0 clippy --all-targets --workspace -- -D warnings`
- [x] `cargo +1.82.0 fmt --all -- --check`
- [ ] Hardware S-1, S-2, S-3 from `studio-hardware-verification.md`

## Next PR
- PR3: Delete firmware serial-settings module (`app/settings/settings.cpp` + `SettingsMsg`).

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 3: Wait for CI green**

- [ ] **Step 4: Hardware S-1/S-2/S-3 pass**

- [ ] **Step 5: Squash-merge**

```bash
gh pr merge <#> --squash --delete-branch
```

---

## PR3 — Firmware: delete dead serial-settings module

**Pre-requisite:** PR2 merged AND a Studio release tag is out with the HTTP-based Settings tab. Otherwise old Studio versions in the wild will see the serial protocol disappear without explanation.

### Task 17: Verify no live users of `SettingsMsg`

**Files:** (read-only discovery)

- [ ] **Step 1: Grep**

```bash
grep -rn "SettingsMsg\|settings_app\|settings_init\|settings_process\|settings_message_handle" AIO_Firmware_PIO/src/
```

Expected: only matches inside `app/settings/settings.cpp` and its `.h`, plus the registration site (likely `sys/app_controller.cpp` or `HoloCubic_AIO.cpp`). No other apps reference it.

- [ ] **Step 2: Grep for the action types**

```bash
grep -rn "AT_SETTING_SET\|AT_SETTING_GET" AIO_Firmware_PIO/src/
```

Expected: only matches inside the to-be-deleted module + the enum definition in `message.h`.

### Task 18: Delete the module

**Files:**
- Delete: `AIO_Firmware_PIO/src/app/settings/settings.cpp`
- Delete: `AIO_Firmware_PIO/src/app/settings/settings.h`
- Conditional delete: `AIO_Firmware_PIO/src/app/settings/settings_gui.{cpp,h}` — only if not referenced anywhere else (Task 17 grep confirms)
- Modify: app registration site (likely `sys/app_controller.cpp` or `HoloCubic_AIO.cpp`) — remove `&settings_app` from the list

- [ ] **Step 1: Delete the module files**

```bash
git rm AIO_Firmware_PIO/src/app/settings/settings.cpp \
       AIO_Firmware_PIO/src/app/settings/settings.h \
       AIO_Firmware_PIO/src/app/settings/settings_gui.cpp \
       AIO_Firmware_PIO/src/app/settings/settings_gui.h
```

(If `settings_gui.{cpp,h}` are referenced elsewhere, leave them and update the rmdir below.)

- [ ] **Step 2: Find the registration**

```bash
grep -rn "settings_app\|&settings_app" AIO_Firmware_PIO/src/sys/ AIO_Firmware_PIO/src/HoloCubic_AIO.cpp
```

- [ ] **Step 3: Delete the registration line**

- [ ] **Step 4: Build firmware to verify nothing is left dangling**

```bash
cd AIO_Firmware_PIO
pio run -e HoloCubic_AIO_Releases
```

Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git commit -m "$(cat <<'EOF'
firmware: delete dead app/settings module

The serial-based settings module (SettingsMsg::decode/encode, app/settings/*.cpp)
was born commented out in d47c88d (2022-10) and never wired to actual storage.
Studio's Settings tab now uses HTTP /api/settings (added in PR1, wired in PR2),
which talks to the working SPIFFS-backed web flow.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### Task 19: Delete `SettingsMsg` from `message.{h,cpp}`

**Files:**
- Modify: `AIO_Firmware_PIO/src/message.h:55-80` — delete `enum VALUE_TYPE` + `class SettingsMsg`
- Modify: `AIO_Firmware_PIO/src/message.cpp:58-200` (approx; verify) — delete `SettingsMsg::SettingsMsg`, `::decode`, `::encode`, `::isLegal`
- Modify: `AIO_Firmware_PIO/src/message.h:16-35` — keep `AT_SETTING_SET` / `AT_SETTING_GET` in the enum? Only delete if no other file uses them. Task 17's grep is the source of truth.

- [ ] **Step 1: Delete `class SettingsMsg` block in `message.h`**

- [ ] **Step 2: Delete `enum VALUE_TYPE` block** — verify it's not referenced elsewhere first

```bash
grep -rn "VALUE_TYPE\|VALUE_TYPE_INT\|VALUE_TYPE_UCHAR\|VALUE_TYPE_STRING" AIO_Firmware_PIO/src/
```

If only inside message.{h,cpp}, delete.

- [ ] **Step 3: Delete the `AT_SETTING_*` enum entries if unused outside**

- [ ] **Step 4: Delete all `SettingsMsg::*` definitions in `message.cpp`**

- [ ] **Step 5: Build firmware**

```bash
pio run -e HoloCubic_AIO_Releases
```

- [ ] **Step 6: Commit**

```bash
git commit -am "firmware: delete SettingsMsg from message.{h,cpp}"
```

### Task 20: PR3 — open and merge

- [ ] **Step 1: Push branch**

```bash
git push -u origin cleanup-b15-firmware-deadcode
```

- [ ] **Step 2: Open PR**

```bash
gh pr create --title "firmware: delete dead serial-settings module (PR3 of B15 fix)" --body "$(cat <<'EOF'
## Summary
- Deletes `app/settings/settings.cpp`/`.h` (+ `settings_gui.*` if unreferenced).
- Deletes `SettingsMsg` class + `VALUE_TYPE` enum + `AT_SETTING_*` action types.
- Removes `&settings_app` from the app registry.

## Why
Final cleanup of B15. PR1 added the working HTTP `/api/settings` flow; PR2 made Studio use it and removed all serial-protocol consumers on the tool side. With no consumers left, the firmware-side serial-settings code is provably dead.

## Risk
None — the deleted code's `prefs.*` calls have been commented out since the module's birth (d47c88d, 2022-10), so no behaviour was actually wired. Any tool version old enough to still send the serial protocol would have received garbage anyway.

## Test plan
- [x] `pio run -e HoloCubic_AIO_Releases` — clean build
- [x] `pio test -e native_unit` — all tests pass
- [ ] Hardware: device boots, browse to `/sys_setting` and `/api/settings` still work (PR1 endpoint unchanged), Settings app no longer appears in the device's app list

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 3: Wait for CI + hardware**

- [ ] **Step 4: Squash-merge**

```bash
gh pr merge <#> --squash --delete-branch
```

---

## Self-review notes

**Spec coverage check:**
- ✅ Firmware adds `/api/settings` GET endpoint (Tasks 1-5)
- ✅ Studio uses HTTP for read + write (Tasks 7-12)
- ✅ Deletes `cubictool.json` (Task 11)
- ✅ Deletes `aio-protocol/setting.rs` + tests (Task 13)
- ✅ Deletes Studio's serial-settings Tauri commands (Task 10)
- ✅ Decides egui tool's Settings tab fate (Task 14)
- ✅ Updates CLAUDE.md + BUGS.md + hardware-verification doc (Task 15)
- ✅ Deletes firmware's `app/settings` module (Tasks 17-19)
- ✅ Three sequential PRs each shippable

**Type consistency check:**
- `SettingsResponse { json: serde_json::Value }` — matches JS-side `const { json } = await invoke(...)`
- `SaveField { key, value }` matches JS-side `{key, value}` pairs in the categorised array
- HTTP category strings (`"sys"`, `"rgb"`, ...) match between Rust handler map and JSON top-level keys

**Known gaps the engineer must close at execution time:**
- Task 1's grep for exact struct field names — the plan assumed names from existing grep evidence; verify before writing JSON serialiser
- Task 12's component location in `studio-pages.jsx` — line range not pinned; locate via grep first
- Task 14's tab registration location in egui tool — likely in `app.rs` or `tabs/mod.rs`
- Task 18's `&settings_app` registration site — confirm with grep before editing
