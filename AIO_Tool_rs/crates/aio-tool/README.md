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

## What works in Plan 6

- 1200×720 dark egui window with 8 tabs across the top
- Tab order matches the legacy `CubicAIO_Tool.py:88-114` minus the
  dropped Screen Share placeholder
- **Flasher tab** fully functional:
  - COM port dropdown + ⟳ refresh button (lazy auto-init)
  - Baud rate dropdown (9 standard rates, default 115200)
  - 4-partition list:
    - Per-row enabled checkbox
    - Per-row address (read-only, `0x1000`/`0x8000`/`0xe000`/`0x10000`)
    - Per-row path field (editable + populated by file picker)
    - Per-row "Select bin file" button → native file dialog (rfd)
  - Action buttons (greyed out when busy):
    - Clear Flash — `aio_flasher::Flasher::erase` on background thread
    - Flash Firmware — `aio_flasher::Flasher::write_partitions` on
      background thread, reading .bin files preflight on egui thread
    - Cancel Flash — sets the shared `Arc<AtomicBool>` flag
  - Scrollable operation log (sticks to bottom, monospace)

## What's a stub

7 tabs render "Coming in Plan N":

| Tab | Plan |
|-----|------|
| Device Settings | Plan 7 |
| Remote Control | Plan 7 (will fold into Flasher tab) |
| File Manager | Plan 8 |
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
