//! Tauri commands — every `#[tauri::command]` here becomes an
//! `invoke()`-able function on the JS side.
//!
//! Phase 1 ships the bridge surface that `flash-sim.jsx` will use to
//! replace its in-browser mock state machine: `list_ports` and
//! `connect_device`. The full flasher / file-manager / converter
//! command set comes incrementally — each one drops one chunk of
//! simulation from the prototype hook.

use std::sync::Mutex;

use serde::Serialize;
use serialport::SerialPortInfo as RawPortInfo;
use tauri::State;

/// A serial port name surfaced to JS via the `list_ports` command.
#[derive(Serialize)]
pub struct SerialPortInfo {
    pub name: String,
    pub description: String,
}

/// Connection metadata returned by `connect_device` — the prototype's
/// `useFlasher` hook stores this in its `chip` state and renders it
/// inside the sidebar's device card.
#[derive(Serialize)]
pub struct ChipInfo {
    pub model: String,
    pub rev: String,
    pub mac: String,
    pub flash: String,
}

/// Mutex-protected per-window state holding the open serial port name
/// once a `connect_device` succeeded. Per-port detail (cancel flag,
/// background thread handle) lives on the corresponding worker thread
/// once `start_flash` / `start_erase` add them.
#[derive(Default)]
pub struct ConnState {
    pub open_port: Mutex<Option<OpenPort>>,
}

/// Active serial connection. `Flasher::new()` already opened-and-closed
/// the port to read the chip header, so we just remember which port we
/// announced as "connected" — the next flash / erase / remote operation
/// re-opens it.
#[derive(Clone)]
#[allow(dead_code)] // `baud` is consumed by Phase-2 flash/erase commands
pub struct OpenPort {
    pub name: String,
    pub baud: u32,
}

/// Enumerate the system's serial ports — replaces the prototype's
/// `FLASH_PORTS` hard-coded array in `flash-sim.jsx`.
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
/// Mirrors the prototype's mock connect: returns `ChipInfo`, then JS
/// flips the UI to "connected" and enables Step 2.
///
/// Uses `aio_flasher::Flasher::new(port, baud)` which under the hood
/// drives espflash's `Flasher::connect` → reads the magic, eFuse MAC,
/// and flash size. The handle is then dropped (closing the port) —
/// flashing later re-opens via the standard path. Phase-2 will keep
/// the handle alive in `ConnState` so chip queries stay cheap.
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
    // espflash 3.3 doesn't surface chip metadata back through
    // `Flasher::new`; for now we report best-known defaults matching
    // the HoloCubic's ESP32-D0WD-V3. A follow-up will read the chip
    // info via `Flasher::chip_info()` once we expose it.
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

/// Drop the recorded open port — the actual port handle is per-thread,
/// so this just resets the UI-level state.
#[tauri::command]
pub fn disconnect_device(state: State<'_, ConnState>) {
    *state.open_port.lock().unwrap() = None;
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
