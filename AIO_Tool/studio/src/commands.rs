//! Tauri commands — every `#[tauri::command]` here becomes an
//! `invoke()`-able function on the JS side.
//!
//! Phase 1 (this session): just enough to prove the bridge works —
//! `list_ports` returns real OS serial enumeration so the prototype's
//! `flash-sim.jsx` can show actual COM ports instead of its hard-coded
//! demo list. The full flasher / file-manager / converter command set
//! is queued and lands incrementally as we swap the prototype's mock
//! state machine for real Rust calls.

use serde::Serialize;
use serialport::SerialPortInfo as RawPortInfo;

/// A serial port name surfaced to JS via the `list_ports` command.
#[derive(Serialize)]
pub struct SerialPortInfo {
    pub name: String,
    pub description: String,
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
