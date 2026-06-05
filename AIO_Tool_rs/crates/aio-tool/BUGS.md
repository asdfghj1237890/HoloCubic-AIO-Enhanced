# Preserved bugs / quirks ledger — aio-tool

| ID | Site | Description | Source |
|----|------|-------------|--------|
| B11/B12/B13 | (omitted from `aio-tool`) | Plan 6's `download_debug.py` improvements: ctypes `_async_raise` → cooperative cancel via `Arc<AtomicBool>`; silent "params 错误" → visible error log; estimated-from-baudrate progress → real espflash `ProgressCallbacks` events. Not byte-format bugs; documented for cross-reference. | `download_debug.py` |
| B14 | (omitted from `aio-tool`) | Python `setting.py:212` calls `set_param("ssid_1", "12345678")` inside `print_log` — debug leftover that leaks "12345678" to the device every time anything is logged. **NOT preserved**; the Rust port omits it entirely. | `setting.py:210-212` |
| B15 | `tabs/settings.rs` Read All | Firmware `SettingsMsg::decode` expects `prefs\0key\0<value_type_u8><pad>...` but Python (and our Rust port preserving Python wire compat) emits `prefs\0key\0type_string\0value\r\n`. End-to-end Read All round-trip fails to decode firmware replies — the `SettingsReceived` arm in `App::drain_events` gracefully falls back to `"(undecodable N bytes)"`. Tracked for a firmware-repo fix; Plan 7 ships send-side functional. | Plan 1 D4, Plan 7 D3 |
