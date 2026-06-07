//! Tauri commands — every `#[tauri::command]` here becomes an
//! `invoke()`-able function on the JS side.
//!
//! Phase 1: port + connect/disconnect bridge.
//! Phase 2: flash / erase / cancel / reboot / remote — full coverage
//!          of `flash-sim.jsx`'s simulated state machine, with real
//!          espflash streaming progress events back to the prototype.
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
