# aio-tool

HoloCubic AIO Tool — egui binary that consumes the five backend crates
(`aio-protocol`, `aio-i18n`, `aio-device`, `aio-flasher`, `aio-converter`)
and presents the user-facing GUI.

## Build + run

```sh
cd AIO_Tool_rs
cargo run --bin aio-tool
```

The first build pulls a substantial dep graph (eframe, egui, glow,
winit, wgpu, zbus, ashpd, …). Plan on 5-10 minutes for the first
compile; warm incremental builds are under 5 seconds.

### Linux build requirements

`serialport`'s `libudev` feature is enabled in this crate (overriding
the workspace-default-off) so the Flasher tab can enumerate COM ports.
Linux runners need:

```sh
sudo apt install libudev-dev   # Debian / Ubuntu
sudo dnf install systemd-devel # Fedora
```

PR #92's CI workflow will install this in the Ubuntu runner.

## What works in Plan 7

- 1200×720 dark egui window with **7 tabs** across the top (Remote Control
  removed; folded into the Flasher tab)
- Tab order matches the legacy `CubicAIO_Tool.py:88-114` minus the dropped
  Screen Share placeholder and the folded-in Remote Control tab
- **Flasher tab** fully functional:
  - COM port dropdown + ⟳ refresh button (lazy auto-init)
  - Baud rate dropdown (9 standard rates, default 115200)
  - 4-partition list: enabled checkbox + read-only address + editable path +
    native file picker (rfd) per row
  - Action buttons (greyed out when busy):
    - Clear Flash — `aio_flasher::Flasher::erase` on background thread
    - Flash Firmware — `aio_flasher::Flasher::write_partitions`
    - Cancel Flash — sets shared `Arc<AtomicBool>`; the cancel-during-erase
      UX (Plan 7 Task 1) logs "Operation cancelled (chip erase already
      completed)." when cancel happened mid-erase
  - **Remote Control panel** (Plan 7 fold-in): 5 buttons (↑ ← → ✓ 🏠)
    between action row and log. Each click spawns a transient thread that
    opens the serial port, writes the 2-byte `~X` command, and closes.
  - Scrollable operation log (sticks to bottom, monospace)
- **Settings tab** functional (send-side):
  - Independent port + baud selection
  - **Connect / Disconnect** buttons drive a long-lived worker thread that
    owns a `SerialTransport` (see `settings_worker.rs`)
  - `DeviceState` enum (Disconnected / Connecting / Connected / Error)
    drives button enable/disable and red-colored error banner
  - 15 settings rendered as text fields, grouped by namespace (sys /
    zhixin / tianqi / other) from the embedded `cubictool.json` schema
  - **Read All** sends a Get for every key (15 commands)
  - **Write Changes** computes the diff vs the baseline (last Read All)
    and sends Set only for fields whose value differs; button label shows
    the live count ("Write Changes (3)")
  - **Caveat (BUGS.md B15)**: Plan 1 Discovery D4 documents that the
    legacy Python tool's `SettingMsg` payload format doesn't match the
    firmware's `SettingsMsg::decode` expectation. Until the firmware is
    updated, Read All replies will arrive but log as "(undecodable N
    bytes)". Write Changes is fire-and-forget on the device side. The
    firmware-side fix is tracked separately from this Rust rewrite.
- **File Manager tab** functional for browse + 4 right-click ops (Plan 8):
  - IP + Port connection bar; default `192.168.0.165:6677`
  - TCP transport via `aio_device::TcpTransport` (inline reconnect, 500 ms timeout paces worker loop)
  - Tree view via egui `CollapsingHeader` (no new deps); expanding a folder triggers `DirList` over the wire and populates children on response
  - Right-click context menu on file rows:
    - **Download** — `FileRead` → `rfd` native save dialog (filename hint from path)
    - **Delete** — `FileRemove` + auto-refresh of parent dir (no optimistic tree mutation — Plan 7 reviewer S1 carry-over)
    - **Rename** — `FileRename` **B1 preserved bug**: action_type sent as `DirRename`, both name fields are the input path — no actual rename happens; log line says "B1 preserved bug — no actual rename" so users understand
    - **Properties** — `FileGetInfo` **B2 preserved bug**: action_type sent as `DirList`; firmware response logged as hex preview. Worker uses a pending-request FIFO to disambiguate `FileGetInfo` responses from real `DirList` responses (both come back with `action_type=DirList` per B2)
  - 4 folder ops (upload / create subfolder / rename / delete) NOT yet wired — Python tool also leaves them as `pass`; needs firmware-side verification before shipping

## What's a stub

4 tabs render "Coming in Plan N":

| Tab | Plan |
|-----|------|
| Image Converter | Plan 9 |
| Video Converter | Plan 9 |
| Tool Settings | Plan 9 |
| Help | Plan 9 |

## Background-op pattern

Every long op (flash, future convert / socket-recv) follows:

1. egui frame thread spawns a worker (`std::thread::spawn`).
2. Worker constructs a typed `mpsc::Sender<XxxEvent>` for that op
   and hands it to the lib (e.g. `Flasher::erase(progress_tx, cancel)`).
3. A 1-line forwarder thread re-wraps each `XxxEvent` into the matching
   `AppEvent::Xxx(_)` variant and sends through the bus channel.
4. Worker gets a final `Result`, sends `AppEvent::XxxFinished(result)`.
5. `App::update` polls the bus in a `try_recv` loop each frame, matches
   on the variant, updates the active tab's state (log line, busy flag,
   progress percent), and triggers a repaint.

Repaint is forced every 100 ms via `ctx.request_repaint_after(...)`
so background events don't wait for user mouse-move.

See `src/bus.rs` for the union enum, `src/tabs/flasher.rs::show` for
example spawn closures (Erase + Flash), `src/app.rs::App::update` for
the receive loop.

## Cancellation contract

The `Arc<AtomicBool>` cancel flag is checked at row boundaries inside
`aio-flasher` and `aio-converter`. Latency is bounded by:

- **Flash partition write**: cancel takes effect at the next partition
  boundary (espflash 3.3.0's `ProgressCallbacks` can't return an error,
  so an in-flight `write_bin_to_flash` completes before the loop
  responds).
- **Chip erase**: cancel does NOT interrupt mid-erase (~10s on 4 MB
  ESP32). The trailing `EraseDone` event WILL fire even if the user
  clicked Cancel — see `aio-flasher` docs. A future UX iteration could
  suppress the trailing log line by remembering "cancel was clicked";
  Plan 6 doesn't.

## i18n keys

All UI strings come from `aio_i18n::t(key, None)` (the global
singleton). Keys used:

| Key | Context |
|-----|---------|
| `tab_*` (8 keys) | Tab labels |
| `port_number` / `baud_rate` | Serial config row |
| `choose_bootloader` / `choose_partitions` / `choose_boot_app0` / `choose_firmware` | Per-partition file picker labels |
| `clear_flash` / `flash_firmware` / `cancel_flash` | Action buttons |
| `operation_log` | Log section heading |

All 11 keys exist in `AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json` (verified
by `aio-i18n`'s build.rs key-parity check).

## Bug ledger

`BUGS.md` is empty — Plan 6 introduces no preserved-from-Python bugs.
The legacy `download_debug.py` had three behaviors Plan 6 actively
improves on (`_async_raise` ctypes hack → cooperative cancel; silent
"params 错误" → visible log; estimated progress → real espflash
progress). None are byte-format bugs.
