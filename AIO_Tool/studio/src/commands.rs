//! Tauri commands — every `#[tauri::command]` here becomes an
//! `invoke()`-able function on the JS side.
//!
//! Phase 1: port + connect/disconnect bridge.
//! Phase 2: flash / erase / cancel / reboot / remote — full coverage
//!          of `flash-sim.jsx`'s simulated state machine, with real
//!          espflash streaming progress events back to the prototype.
//! Phase 4: device parameters via HTTP. Studio's Settings tab fetches
//!          `GET /api/settings` (firmware route in `web_api.cpp`) and
//!          posts changes back to the existing `/save<Cat>Conf` form
//!          handlers — the same path the browser-based settings pages
//!          use, which actually persists to SPIFFS `.cfg` files. The
//!          old serial-based SettingMsg flow was deleted with B15 fix
//!          (commit fix-b15-studio-http-settings).
//!
//! Background work runs on `std::thread::spawn` and emits Tauri events
//! (`flash:event`, `flash:finished`) back to the JS side, which the
//! prototype's `useEffect`-backed `useFlasher` hook listens to.

use std::io::Write;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::channel;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use serde::{Deserialize, Serialize};
use serialport::SerialPortInfo as RawPortInfo;
use tauri::{AppHandle, Emitter, State};

/// A serial port name surfaced to JS via the `list_ports` command.
#[derive(Serialize)]
pub struct SerialPortInfo {
    pub name: String,
    pub description: String,
}

/// Connection metadata returned by `connect_device`.
#[derive(Serialize)]
pub struct ChipInfo {
    pub model: String,
    pub rev: String,
    pub mac: String,
    pub flash: String,
}

/// Per-window state shared between commands.
#[derive(Default)]
pub struct ConnState {
    /// The last-connected port + baud — `connect_device` writes here,
    /// later commands read it as a default. The prototype already passes
    /// port/baud explicitly to every action, so this is only used as a
    /// sanity check that connect was clicked first.
    pub open_port: Mutex<Option<OpenPort>>,
    /// Shared cancellation flag for the active flash / erase. The
    /// background thread polls it at every partition boundary; the JS
    /// cancel button flips it via the `cancel_op` command.
    pub cancel: Arc<AtomicBool>,
    /// True while a flash or erase background thread is running. Wrapped
    /// in `Arc` so the worker thread can flip it back to `false` on
    /// completion without holding a reference to `state`.
    pub busy: Arc<Mutex<bool>>,
    /// Handle to the File Manager worker thread when connected.
    pub fm: Mutex<Option<FmHandle>>,
    /// Shared cancellation flag for the image-converter batch. Independent
    /// from `cancel` (flasher) so cancelling one doesn't affect the other.
    pub convert_cancel: Arc<AtomicBool>,
    /// Shared cancellation flag for the in-flight video conversion. Also
    /// independent so the JS Cancel buttons can't accidentally cross-talk.
    pub video_cancel: Arc<AtomicBool>,
}

/// Live handle to a File-Manager worker — `fm_connect` stores it,
/// per-op commands borrow `cmd_tx`, `fm_disconnect` flips `cancel`.
pub struct FmHandle {
    /// Command sender to the worker thread.
    pub cmd_tx: std::sync::mpsc::Sender<crate::fm::FmCmd>,
    /// Cancellation flag — flipped by `fm_disconnect`.
    pub cancel: Arc<AtomicBool>,
}

/// Active serial connection.
#[derive(Clone)]
#[allow(dead_code)] // baud will be consumed once we keep the port handle alive
pub struct OpenPort {
    pub name: String,
    pub baud: u32,
}

/// One partition the JS side wants written — `{address, path}`.
#[derive(Deserialize)]
pub struct FlashPartition {
    pub address: u32,
    pub path: String,
}

/// FlashEvent serialised for the JS side. Mirrors
/// `aio_flasher::FlashEvent` 1:1 so the prototype's `useFlasher` hook
/// can match on `event.kind`. `Clone` is required by Tauri's
/// `Emitter::emit` — payloads are cloned per subscriber.
#[derive(Serialize, Clone)]
#[serde(tag = "kind", rename_all = "kebab-case")]
pub enum FlashEventDto {
    EraseStart,
    EraseDone,
    PartitionStart { index: usize, total_bytes: u64 },
    Progress { index: usize, bytes_written: u64 },
    PartitionDone { index: usize },
}

/// Payload for the `flash:finished` event — Ok or Err with message.
#[derive(Serialize, Clone)]
pub struct FlashFinishedDto {
    pub ok: bool,
    pub cancelled: bool,
    pub error: Option<String>,
}

/// Open a native file picker for a single `.bin` partition image.
///
/// `initial_name` becomes the default selection in the dialog (e.g.
/// "bootloader_qio_80m.bin") so the user can match the partition row
/// to the right file at a glance. Returns the absolute path on
/// confirm, or `None` on cancel — the JS side stores the absolute
/// path in `parts[i].file` and that's what `start_flash` reads.
#[tauri::command]
pub fn pick_partition_bin(initial_name: Option<String>) -> Option<String> {
    let mut dlg = rfd::FileDialog::new().add_filter("Firmware (.bin)", &["bin"]);
    if let Some(name) = initial_name {
        dlg = dlg.set_file_name(&name);
    }
    dlg.pick_file().map(|p| p.to_string_lossy().into_owned())
}

/// Enumerate the system's serial ports.
#[tauri::command]
pub fn list_ports() -> Vec<SerialPortInfo> {
    serialport::available_ports()
        .map(|list| {
            list.into_iter()
                .map(|p: RawPortInfo| SerialPortInfo {
                    description: describe(&p),
                    name: p.port_name,
                })
                .collect()
        })
        .unwrap_or_default()
}

/// Open a serial port to the ESP32, read back its chip identity, then
/// reboot the chip into its firmware before returning.
///
/// Returns model / revision / MAC / flash size as queried from the chip —
/// these populate the right-side info panel in the UI.
///
/// **Why the reboot.** `aio_flasher::Flasher::new` calls espflash's
/// `DefaultReset` to enter the ROM bootloader so chip identity can be
/// read. The Flasher is dropped (releasing the serial port) at the end
/// of the inner block below, but — as `aio_flasher::reboot`'s doc-comment
/// at `crates/aio-flasher/src/flasher.rs` spells out — closing the port
/// does NOT undo the bootloader entry: the ESP32 stays parked until
/// something pulses EN. While parked, the HoloCubic firmware (LCD, IMU,
/// app loop) is not running, so the device's display goes blank. The
/// explicit `aio_flasher::reboot` after dropping the Flasher pulses the
/// reset line via RTS and brings the firmware back. Adds ~1.5 s to a
/// connect; the alternative is a permanently blank screen until the
/// user manually clicks Reboot. See PR comment on the connect-reboot
/// fix for the user-reported reproduction.
///
/// `start_flash` re-opens its own espflash session (and handles the
/// post-flash reset via espflash's own `reset_after`), so we don't keep
/// a live handle across this call.
#[tauri::command]
pub fn connect_device(
    port: String,
    baud: String,
    state: State<'_, ConnState>,
) -> Result<ChipInfo, String> {
    let baud_u32: u32 = baud
        .parse()
        .map_err(|e| format!("invalid baud `{baud}`: {e}"))?;
    // Inner block so the Flasher is dropped (and the serial port
    // released) before reboot() reopens the port at 115_200 to toggle
    // RTS — opening the same port twice concurrently would fail with
    // "device or resource busy" on Linux / "Access denied" on Windows.
    let summary = {
        let mut flasher = aio_flasher::Flasher::new(&port, baud_u32)
            .map_err(|e| format!("open/connect {port}@{baud}: {e}"))?;
        flasher
            .device_info()
            .map_err(|e| format!("read chip info {port}: {e}"))?
    };
    aio_flasher::reboot(&port).map_err(|e| format!("post-connect reboot {port}: {e}"))?;
    let info = ChipInfo {
        model: summary.chip,
        rev: summary.revision,
        mac: summary.mac,
        flash: summary.flash_size,
    };
    *state.open_port.lock().unwrap() = Some(OpenPort {
        name: port,
        baud: baud_u32,
    });
    Ok(info)
}

/// Reset the recorded open port.
#[tauri::command]
pub fn disconnect_device(state: State<'_, ConnState>) {
    *state.open_port.lock().unwrap() = None;
}

/// Cancel an in-flight flash or erase. The background thread polls the
/// cancel flag at partition boundaries (`aio_flasher`'s contract).
#[tauri::command]
pub fn cancel_op(state: State<'_, ConnState>) {
    state.cancel.store(true, Ordering::Relaxed);
}

/// Spawn a background thread that flashes the given partitions, streaming
/// `flash:event` to the JS side and finishing with `flash:finished`.
///
/// Each partition `path` is read from disk on the worker thread — files
/// are kept off the egui thread so we don't block UI on large reads.
#[tauri::command]
pub fn start_flash(
    parts: Vec<FlashPartition>,
    port: String,
    baud: String,
    app: AppHandle,
    state: State<'_, ConnState>,
) -> Result<(), String> {
    let baud_u32: u32 = baud
        .parse()
        .map_err(|e| format!("invalid baud `{baud}`: {e}"))?;
    let mut busy = state.busy.lock().unwrap();
    if *busy {
        return Err("flash already in progress".to_owned());
    }
    *busy = true;
    drop(busy);
    state.cancel.store(false, Ordering::Relaxed);
    let cancel = state.cancel.clone();
    let busy_handle = state.busy.clone();

    std::thread::spawn(move || {
        let result: Result<(), String> = (|| {
            // Read each partition's data off disk on this background
            // thread — espflash takes Vec<u8>, not Read.
            let mut payload: Vec<aio_flasher::Partition> = Vec::with_capacity(parts.len());
            for fp in parts {
                let data = std::fs::read(&fp.path).map_err(|e| format!("read {}: {e}", fp.path))?;
                payload.push(aio_flasher::Partition {
                    address: fp.address,
                    data,
                });
            }
            let mut flasher = aio_flasher::Flasher::new(&port, baud_u32)
                .map_err(|e| format!("open/connect: {e}"))?;
            let (tx, rx) = channel::<aio_flasher::FlashEvent>();
            // Forwarder thread bridges aio_flasher's mpsc → Tauri event
            // bus, on the original Tauri app handle (clone-able).
            let app_for_fwd = app.clone();
            std::thread::spawn(move || {
                while let Ok(evt) = rx.recv() {
                    let _ = app_for_fwd.emit("flash:event", encode_event(evt));
                }
            });
            flasher
                .write_partitions(payload, tx, cancel)
                .map_err(|e| format!("flash: {e}"))?;
            Ok(())
        })();

        emit_finished(&app, result.as_ref().map(|_| ()).map_err(|s| s.clone()));
        *busy_handle.lock().unwrap() = false;
    });
    Ok(())
}

/// Spawn a background thread that erases the whole chip.
#[tauri::command]
pub fn start_erase(
    port: String,
    baud: String,
    app: AppHandle,
    state: State<'_, ConnState>,
) -> Result<(), String> {
    let baud_u32: u32 = baud
        .parse()
        .map_err(|e| format!("invalid baud `{baud}`: {e}"))?;
    let mut busy = state.busy.lock().unwrap();
    if *busy {
        return Err("operation already in progress".to_owned());
    }
    *busy = true;
    drop(busy);
    state.cancel.store(false, Ordering::Relaxed);
    let cancel = state.cancel.clone();
    let busy_handle = state.busy.clone();

    std::thread::spawn(move || {
        let result: Result<(), String> = (|| {
            let mut flasher = aio_flasher::Flasher::new(&port, baud_u32)
                .map_err(|e| format!("open/connect: {e}"))?;
            let (tx, rx) = channel::<aio_flasher::FlashEvent>();
            let app_for_fwd = app.clone();
            std::thread::spawn(move || {
                while let Ok(evt) = rx.recv() {
                    let _ = app_for_fwd.emit("flash:event", encode_event(evt));
                }
            });
            flasher
                .erase(tx, cancel)
                .map_err(|e| format!("erase: {e}"))?;
            Ok(())
        })();

        emit_finished(&app, result.as_ref().map(|_| ()).map_err(|s| s.clone()));
        *busy_handle.lock().unwrap() = false;
    });
    Ok(())
}

/// Send a 2-byte remote control command (`~U` / `~L` / `~R` / `~F` /
/// `~H`) over the serial port. Mirrors the firmware's remote protocol
/// — opens, writes, drops the handle. No long-lived port held.
#[tauri::command]
pub fn send_remote(port: String, baud: String, dir: String) -> Result<(), String> {
    let baud_u32: u32 = baud
        .parse()
        .map_err(|e| format!("invalid baud `{baud}`: {e}"))?;
    let cmd: &[u8] = match dir.as_str() {
        "up" => b"~U",
        "left" => b"~L",
        "right" => b"~R",
        "ok" => b"~F",
        "home" => b"~H",
        other => return Err(format!("unknown D-pad dir `{other}`")),
    };
    let mut sp = serialport::new(port.as_str(), baud_u32)
        .timeout(Duration::from_millis(500))
        .open()
        .map_err(|e| format!("open {port}: {e}"))?;
    sp.write_all(cmd)
        .map_err(|e| format!("write {}: {e}", String::from_utf8_lossy(cmd)))?;
    Ok(())
}

/// Reboot the connected device into its firmware.
///
/// Pulses the chip's EN pin via the USB-serial adapter's RTS line
/// (`aio_flasher::reboot`). The firmware has no serial "reboot" opcode —
/// its remote protocol only knows the `~U/~D/~L/~R/~H/~F` D-pad codes —
/// so a control-line reset is both the firmware-agnostic path and the only
/// way to bring the chip back out of the ROM bootloader it's parked in
/// after `connect_device`. Baud is irrelevant to a line-toggle reset, so
/// it isn't taken here.
#[tauri::command]
pub fn reboot_device(port: String) -> Result<(), String> {
    aio_flasher::reboot(&port).map_err(|e| format!("{e}"))
}

fn encode_event(evt: aio_flasher::FlashEvent) -> FlashEventDto {
    match evt {
        aio_flasher::FlashEvent::EraseStart => FlashEventDto::EraseStart,
        aio_flasher::FlashEvent::EraseDone => FlashEventDto::EraseDone,
        aio_flasher::FlashEvent::PartitionStart { index, total_bytes } => {
            FlashEventDto::PartitionStart { index, total_bytes }
        }
        aio_flasher::FlashEvent::Progress {
            index,
            bytes_written,
        } => FlashEventDto::Progress {
            index,
            bytes_written,
        },
        aio_flasher::FlashEvent::PartitionDone { index } => FlashEventDto::PartitionDone { index },
    }
}

fn emit_finished(app: &AppHandle, result: Result<(), String>) {
    let dto = match result {
        Ok(()) => FlashFinishedDto {
            ok: true,
            cancelled: false,
            error: None,
        },
        Err(msg) => {
            let cancelled = msg.to_lowercase().contains("cancel");
            FlashFinishedDto {
                ok: false,
                cancelled,
                error: Some(msg),
            }
        }
    };
    let _ = app.emit("flash:finished", dto);
}

/// Friendly description for a serialport entry.
fn describe(p: &RawPortInfo) -> String {
    match &p.port_type {
        serialport::SerialPortType::UsbPort(info) => {
            let manufacturer = info.manufacturer.as_deref().unwrap_or("USB Serial");
            let product = info.product.as_deref().unwrap_or("");
            format!("{manufacturer} {product} ({})", p.port_name)
                .trim()
                .to_owned()
        }
        serialport::SerialPortType::PciPort => format!("PCI ({})", p.port_name),
        serialport::SerialPortType::BluetoothPort => format!("Bluetooth ({})", p.port_name),
        serialport::SerialPortType::Unknown => p.port_name.clone(),
    }
}

// ─────────────────────────────────────────────────────────────────────
// Phase 4 — Settings (device parameters via HTTP)
// ─────────────────────────────────────────────────────────────────────

/// JSON snapshot returned by `GET /api/settings`. The shape is firmware-
/// controlled (see `AIO_Firmware_PIO/src/app/server/web_api.cpp`'s
/// `api_settings()`); we pass it through to the JS side as a generic
/// JSON tree so adding firmware-side sections doesn't need a tool rebuild.
#[derive(Serialize)]
pub struct SettingsResponse {
    /// Top-level JSON object — keyed by category (`sys`, `rgb`, `mpu`, …),
    /// each holding key→value pairs as the firmware emits them.
    pub json: serde_json::Value,
}

/// One field the JS side wants written, paired into a form-data POST body.
#[derive(Deserialize)]
pub struct SaveField {
    /// Firmware-side form field name (e.g. `"ssid_0"`, `"backLight"`).
    pub key: String,
    /// New value stringified — firmware handlers parse with `atoi`/`atol`/
    /// `.toInt()` or take the raw string depending on field type.
    pub value: String,
}

/// Fetch the device's current settings via the firmware's `/api/settings`
/// JSON endpoint. `host` is the bare IP / hostname without scheme. Sub-3s
/// timeout — the payload is sub-kB and the device is on the local network.
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

/// POST the changed fields of one settings category to the firmware's
/// existing `/save<Cat>Conf` form handler — the same URL the browser
/// submits to. Form-encoded body so the firmware-side handlers can read
/// values via `server.arg("key")` without a JSON parser change.
#[tauri::command]
pub fn save_settings_http(
    host: String,
    category: String,
    fields: Vec<SaveField>,
) -> Result<(), String> {
    // Category → form handler path. Only the categories whose form pages
    // are currently registered in `server.cpp` are accepted — submitting to
    // a disabled #if APP_X_USE branch would return 404 and confuse the user.
    let handler = match category.as_str() {
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
    };
    let url = format!("http://{host}/{handler}");
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

// ─────────────────────────────────────────────────────────────────────
// Phase 5 — File Manager (SD card over TCP/WiFi)
// ─────────────────────────────────────────────────────────────────────

/// Spawn the File-Manager worker. Idempotent on the same `(ip, port)`
/// — repeat calls disconnect the existing handle first. Failures to
/// resolve the address surface synchronously; transport-level failures
/// arrive later as `fm:event` `Finished` payloads.
#[tauri::command]
pub fn fm_connect(
    ip: String,
    port: String,
    app: AppHandle,
    state: State<'_, ConnState>,
) -> Result<(), String> {
    let port_u16: u16 = port
        .parse()
        .map_err(|e| format!("invalid TCP port `{port}`: {e}"))?;
    let addr_str = format!("{ip}:{port_u16}");
    let addr: std::net::SocketAddr = addr_str
        .parse()
        .map_err(|e| format!("resolve {addr_str}: {e}"))?;

    // Tear down the previous worker if any.
    {
        let mut guard = state.fm.lock().unwrap();
        if let Some(prev) = guard.take() {
            prev.cancel.store(true, Ordering::Relaxed);
        }
    }

    let (tx, cancel) = crate::fm::spawn(addr, app);
    *state.fm.lock().unwrap() = Some(FmHandle { cmd_tx: tx, cancel });
    Ok(())
}

/// Flip the worker's cancel flag. Worker drops the transport at the
/// next loop boundary and emits a final `Finished` event.
#[tauri::command]
pub fn fm_disconnect(state: State<'_, ConnState>) {
    if let Some(prev) = state.fm.lock().unwrap().take() {
        prev.cancel.store(true, Ordering::Relaxed);
    }
}

/// Send `FmCmd::ListDir` to the worker. Caller listens for the
/// corresponding `fm:event` `DirListed` response.
#[tauri::command]
pub fn fm_list_dir(path: String, state: State<'_, ConnState>) -> Result<(), String> {
    send_fm_cmd(&state, crate::fm::FmCmd::ListDir { path })
}

/// Send `FmCmd::ReadFile`. Response arrives as `fm:event` `FileBytes`.
#[tauri::command]
pub fn fm_read_file(path: String, state: State<'_, ConnState>) -> Result<(), String> {
    send_fm_cmd(&state, crate::fm::FmCmd::ReadFile { path })
}

/// Send `FmCmd::RemoveFile`. No response (firmware doesn't send one).
#[tauri::command]
pub fn fm_remove(name: String, state: State<'_, ConnState>) -> Result<(), String> {
    send_fm_cmd(&state, crate::fm::FmCmd::RemoveFile { name })
}

/// Send `FmCmd::RenameFile`. Preserved-bug B1 — see aio-protocol.
#[tauri::command]
pub fn fm_rename(name: String, state: State<'_, ConnState>) -> Result<(), String> {
    send_fm_cmd(&state, crate::fm::FmCmd::RenameFile { name })
}

/// Send `FmCmd::GetFileInfo`. Response arrives as `fm:event` `Properties`.
#[tauri::command]
pub fn fm_get_info(name: String, state: State<'_, ConnState>) -> Result<(), String> {
    send_fm_cmd(&state, crate::fm::FmCmd::GetFileInfo { name })
}

fn send_fm_cmd(state: &State<'_, ConnState>, cmd: crate::fm::FmCmd) -> Result<(), String> {
    let guard = state.fm.lock().unwrap();
    let handle = guard
        .as_ref()
        .ok_or_else(|| "fm: not connected".to_owned())?;
    handle
        .cmd_tx
        .send(cmd)
        .map_err(|e| format!("fm: worker gone: {e}"))
}

// ─────────────────────────────────────────────────────────────────────
// Phase 6 — Image Converter
// ─────────────────────────────────────────────────────────────────────

/// Open a native file picker and return picked image metadata. Synchronous
/// — `rfd` runs the dialog on the Tauri command thread.
#[tauri::command]
pub fn convert_pick_images() -> Vec<crate::img::PickedImageDto> {
    crate::img::pick_images()
}

/// Run a batch conversion in the background; stream `convert:event`
/// payloads to JS. Returns immediately. `convert_image_cancel` flips
/// the shared `AtomicBool` and the converter aborts at the next row
/// boundary.
#[tauri::command]
pub fn convert_image_batch(
    items: Vec<crate::img::ConvertItem>,
    format: String,
    dither: bool,
    c_array: bool,
    app: AppHandle,
    state: State<'_, ConnState>,
) -> Result<(), String> {
    let fmt = crate::img::parse_format(&format)?;
    crate::img::spawn_batch(
        items,
        fmt,
        dither,
        c_array,
        state.convert_cancel.clone(),
        app,
    );
    Ok(())
}

/// Flip the convert cancel flag. The encoder polls at row boundaries.
#[tauri::command]
pub fn convert_image_cancel(state: State<'_, ConnState>) {
    state.convert_cancel.store(true, Ordering::Relaxed);
}

// ─────────────────────────────────────────────────────────────────────
// Phase 7 — Video Converter (ffmpeg subprocess)
// ─────────────────────────────────────────────────────────────────────

/// Probe whether ffmpeg is on PATH. Cheap; matches the egui tool's
/// `ffmpeg_present()` helper. Called from the UI on tab init / refresh.
#[tauri::command]
pub fn video_ffmpeg_check() -> bool {
    crate::video::ffmpeg_present()
}

/// Open a native picker for the source video.
#[tauri::command]
pub fn video_pick_source() -> Option<crate::video::VideoSourceDto> {
    crate::video::pick_source()
}

/// Open a save-as dialog. `srcPath` + `w`/`h`/`format` are used to
/// suggest the default filename matching the egui worker's convention.
#[tauri::command]
#[allow(non_snake_case)] // Tauri forwards JS camelCase verbatim.
pub fn video_pick_output(srcPath: String, w: u32, h: u32, format: String) -> Option<String> {
    let default = crate::video::default_output_name(&srcPath, w, h, &format);
    crate::video::pick_output(&default)
}

/// Spawn the two-step ffmpeg pipeline. Streams `video:event` payloads.
#[tauri::command]
pub fn video_run(
    job: crate::video::VideoJobDto,
    app: AppHandle,
    state: State<'_, ConnState>,
) -> Result<(), String> {
    crate::video::spawn_job(job, state.video_cancel.clone(), app)
}

/// Flip the video cancel flag — the watcher thread kills the child.
#[tauri::command]
pub fn video_cancel(state: State<'_, ConnState>) {
    state.video_cancel.store(true, Ordering::Relaxed);
}
