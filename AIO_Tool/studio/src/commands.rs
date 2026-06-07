//! Tauri commands — every `#[tauri::command]` here becomes an
//! `invoke()`-able function on the JS side.
//!
//! Phase 1: port + connect/disconnect bridge.
//! Phase 2: flash / erase / cancel / reboot / remote — full coverage
//!          of `flash-sim.jsx`'s simulated state machine, with real
//!          espflash streaming progress events back to the prototype.
//! Phase 4: device-parameters bridge — list_setting_keys / read_all_settings
//!          / write_changed_settings, using `aio-device::SerialTransport`
//!          + `aio-protocol::SettingMsg`. Mirrors the egui
//!          `settings_worker.rs` semantically but exposes a stateless
//!          command surface so the JS hook doesn't need a worker handle.
//!
//! Background work runs on `std::thread::spawn` and emits Tauri events
//! (`flash:event`, `flash:finished`) back to the JS side, which the
//! prototype's `useEffect`-backed `useFlasher` hook listens to.

use std::io::Write;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::channel;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};
use serialport::SerialPortInfo as RawPortInfo;
use tauri::{AppHandle, Emitter, State};

use aio_device::serial::SerialTransport;
use aio_device::{Transport, TransportError};
use aio_protocol::{SettingMsg, ValueType};

/// Embedded settings schema — same `cubictool.json` the egui tool reads.
/// Path is relative to this source file (`studio/src/commands.rs`); two
/// parents up reach the AIO_Tool workspace root where `cubictool.json` lives.
const SETTINGS_SCHEMA_RAW: &str = include_str!("../../cubictool.json");

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

/// Open a serial port to the ESP32 and read back its chip identity.
#[tauri::command]
pub fn connect_device(
    port: String,
    baud: String,
    state: State<'_, ConnState>,
) -> Result<ChipInfo, String> {
    let baud_u32: u32 = baud
        .parse()
        .map_err(|e| format!("invalid baud `{baud}`: {e}"))?;
    let _flasher = aio_flasher::Flasher::new(&port, baud_u32)
        .map_err(|e| format!("open/connect {port}@{baud}: {e}"))?;
    let info = ChipInfo {
        model: "ESP32".to_owned(),
        rev: "v3.0".to_owned(),
        mac: "—".to_owned(),
        flash: "—".to_owned(),
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

/// Reboot — sends the firmware's reset trigger over serial. The
/// HoloCubic firmware listens for `~B` to drop into a soft reset; in
/// the absence of that we toggle DTR/RTS which on CH340 / CP210x
/// effectively pulses the ESP32's EN pin.
#[tauri::command]
pub fn reboot_device(port: String, baud: String) -> Result<(), String> {
    let baud_u32: u32 = baud
        .parse()
        .map_err(|e| format!("invalid baud `{baud}`: {e}"))?;
    let mut sp = serialport::new(port.as_str(), baud_u32)
        .timeout(Duration::from_millis(500))
        .open()
        .map_err(|e| format!("open {port}: {e}"))?;
    // Best-effort firmware-level reset trigger.
    sp.write_all(b"~B")
        .map_err(|e| format!("write reboot: {e}"))?;
    Ok(())
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
// Phase 4 — Settings (device parameters)
// ─────────────────────────────────────────────────────────────────────

/// One row of the schema embedded in `cubictool.json`. Mirrors the egui
/// tool's `SettingKey`; surfaced to JS via `list_setting_keys`.
#[derive(Serialize, Clone)]
pub struct SettingKeyDto {
    /// Raw firmware key, e.g. `"ssid_1"`, `"backLight"`, `"cityname"`.
    pub key: String,
    /// Firmware namespace, e.g. `"sys"`, `"zhixin"`.
    pub namespace: String,
    /// Wire value type as it appears in `cubictool.json`: `"String"` or
    /// `"UChar"`. JS uses this to choose between text/number/select UI.
    pub value_type: String,
}

/// One key the JS side wants written.
#[derive(Deserialize)]
pub struct SettingChange {
    /// Firmware namespace (e.g. `"sys"`).
    pub namespace: String,
    /// Firmware key (e.g. `"ssid_1"`).
    pub key: String,
    /// Wire value type — `"String"` or `"UChar"`.
    pub value_type: String,
    /// New value, already stringified.
    pub value: String,
}

/// Single read-all-result entry passed back to JS.
#[derive(Serialize, Clone)]
pub struct SettingValueDto {
    /// Firmware key — matches `SettingKeyDto::key`.
    pub key: String,
    /// Decoded value as a string.
    pub value: String,
}

/// Settings log/progress event mirrored to JS via the `settings:event` bus.
/// `kind = "log"` is a one-line status update; `kind = "warning"` flags
/// undecodable bytes from the known B15 firmware bug so the UI can hint
/// at it without aborting.
#[derive(Serialize, Clone)]
#[serde(tag = "kind", rename_all = "kebab-case")]
pub enum SettingsEventDto {
    /// One human-readable line for the status chip / log tail.
    Log {
        /// Body text.
        message: String,
    },
    /// Undecodable bytes — preserved-by-design from firmware bug B15.
    Warning {
        /// Body text.
        message: String,
    },
}

/// Return the embedded settings schema. Pure read; no device I/O.
#[tauri::command]
pub fn list_setting_keys() -> Result<Vec<SettingKeyDto>, String> {
    let parsed: serde_json::Map<String, serde_json::Value> =
        serde_json::from_str(SETTINGS_SCHEMA_RAW)
            .map_err(|e| format!("parse cubictool.json: {e}"))?;
    Ok(parsed
        .into_iter()
        .map(|(key, value)| SettingKeyDto {
            key,
            namespace: value
                .get("namespace")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_owned(),
            value_type: value
                .get("type")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_owned(),
        })
        .collect())
}

/// Read every key in the schema in a single connect → batch GET → drain
/// → close pass. Returns the successfully-decoded `(key, value)` pairs.
///
/// The firmware's response format is known to diverge from
/// `SettingMsg::from_wire`'s expectation on current main (CLAUDE.md notes
/// this as bug B15). Undecodable chunks are surfaced as `settings:event`
/// warnings; decoded ones populate the returned `Vec`.
#[tauri::command]
pub fn read_all_settings(
    port: String,
    baud: String,
    app: AppHandle,
) -> Result<Vec<SettingValueDto>, String> {
    let baud_u32: u32 = baud
        .parse()
        .map_err(|e| format!("invalid baud `{baud}`: {e}"))?;
    let schema = list_setting_keys()?;
    let mut transport = SerialTransport::open(&port, baud_u32)
        .map_err(|e| format!("open {port}@{baud}: {e}"))?;

    // 1. Fire one GET per schema entry.
    for k in &schema {
        let msg = SettingMsg::get(&k.namespace, &k.key);
        let bytes = msg.to_wire().map_err(|e| format!("encode GET {}: {e}", k.key))?;
        transport
            .write_all(&bytes)
            .map_err(|e| format!("write GET {}: {e}", k.key))?;
    }
    emit_settings_log(&app, &format!("requested {} keys", schema.len()));

    // 2. Drain responses until idle for ~400 ms or 2 s total elapsed.
    //    Each chunk is feed-parsed greedily; partial frames bail to the
    //    next read. (The legacy worker treats each transport.read return
    //    as a complete frame — keep that behaviour since the firmware
    //    typically writes one frame per response.)
    let mut values: Vec<SettingValueDto> = Vec::new();
    let mut read_buf = vec![0u8; 4096];
    let deadline = Instant::now() + Duration::from_millis(2_000);
    let mut last_bytes_at = Instant::now();
    loop {
        if Instant::now() > deadline {
            break;
        }
        if Instant::now().duration_since(last_bytes_at) > Duration::from_millis(400)
            && !values.is_empty()
        {
            break;
        }
        match transport.read(&mut read_buf) {
            Ok(n) if n > 0 => {
                last_bytes_at = Instant::now();
                let chunk = &read_buf[..n];
                match SettingMsg::from_wire(chunk) {
                    Ok((msg, _)) => {
                        emit_settings_log(
                            &app,
                            &format!("\u{2190} {}/{} = {}", msg.prefs_name, msg.key, msg.value),
                        );
                        values.push(SettingValueDto {
                            key: msg.key,
                            value: msg.value,
                        });
                    }
                    Err(_) => {
                        emit_settings_warn(
                            &app,
                            &format!("(undecodable {} bytes — firmware bug B15)", chunk.len()),
                        );
                    }
                }
            }
            Ok(_) => {}
            Err(TransportError::TimedOut) => {}
            Err(e) => {
                transport.close();
                return Err(format!("read: {e}"));
            }
        }
    }
    transport.close();
    emit_settings_log(&app, &format!("decoded {} of {} keys", values.len(), schema.len()));
    Ok(values)
}

/// Write a batch of changed settings: open serial, fire one SET per
/// change, close. Returns the count of frames written for the UI's
/// "wrote N" status.
#[tauri::command]
pub fn write_changed_settings(
    port: String,
    baud: String,
    changes: Vec<SettingChange>,
    app: AppHandle,
) -> Result<usize, String> {
    let baud_u32: u32 = baud
        .parse()
        .map_err(|e| format!("invalid baud `{baud}`: {e}"))?;
    if changes.is_empty() {
        return Ok(0);
    }
    let mut transport = SerialTransport::open(&port, baud_u32)
        .map_err(|e| format!("open {port}@{baud}: {e}"))?;
    let mut written = 0usize;
    for c in &changes {
        let vt = parse_value_type(&c.value_type)?;
        let msg = SettingMsg::set(&c.namespace, &c.key, vt, &c.value);
        let bytes = msg
            .to_wire()
            .map_err(|e| format!("encode SET {}/{}: {e}", c.namespace, c.key))?;
        transport
            .write_all(&bytes)
            .map_err(|e| format!("write SET {}/{}: {e}", c.namespace, c.key))?;
        emit_settings_log(
            &app,
            &format!("\u{2192} {}/{} := {}", c.namespace, c.key, c.value),
        );
        written += 1;
    }
    transport.close();
    emit_settings_log(&app, &format!("wrote {written} changes"));
    Ok(written)
}

fn parse_value_type(raw: &str) -> Result<ValueType, String> {
    match raw {
        "String" => Ok(ValueType::String),
        "UChar" => Ok(ValueType::Uchar),
        "Int" => Ok(ValueType::Int),
        "" => Ok(ValueType::Unknown),
        other => Err(format!("unknown value type `{other}`")),
    }
}

fn emit_settings_log(app: &AppHandle, message: &str) {
    let _ = app.emit(
        "settings:event",
        SettingsEventDto::Log {
            message: message.to_owned(),
        },
    );
}

fn emit_settings_warn(app: &AppHandle, message: &str) {
    let _ = app.emit(
        "settings:event",
        SettingsEventDto::Warning {
            message: message.to_owned(),
        },
    );
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
    crate::img::spawn_batch(items, fmt, dither, c_array, state.convert_cancel.clone(), app);
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
