//! Flasher tab — partition list + erase / flash / cancel + operation log.
//!
//! Plan 6 Task 4: lays out the UI and holds state. Tasks 5-8 wire actions.

use aio_i18n::t;
use egui::{ComboBox, Grid, Ui};
use serialport::available_ports;

use crate::widgets::operation_log::OperationLog;

/// Refresh `state.available_ports` from the OS port enumeration.
pub fn refresh_ports(state: &mut FlasherState) {
    state.available_ports = available_ports()
        .map(|ports| ports.into_iter().map(|p| p.port_name).collect())
        .unwrap_or_default();
}

/// Standard ESP32 flash addresses for the 4 partitions.
pub const PARTITION_ADDRESSES: [u32; 4] = [
    aio_flasher::PARTITION_BOOTLOADER,
    aio_flasher::PARTITION_PARTITIONS,
    aio_flasher::PARTITION_BOOTAPP0,
    aio_flasher::PARTITION_FIRMWARE,
];

/// i18n keys for the per-partition "Select bin file" button labels. Matches
/// the legacy tool's `download_debug.py:85-97` placeholder strings so users
/// see "Select Bootloader bin file" instead of a generic "Choose File".
pub const PARTITION_BUTTON_KEYS: [&str; 4] = [
    "choose_bootloader",
    "choose_partitions",
    "choose_boot_app0",
    "choose_firmware",
];

/// Per-partition path + enabled flag.
#[derive(Default, Clone)]
pub struct PartitionEntry {
    /// Absolute path the user picked. Empty until they hit the file picker.
    pub path: String,
    /// Whether this partition is included in the flash operation.
    pub enabled: bool,
}

/// Flasher tab state.
pub struct FlasherState {
    /// Currently selected COM port (e.g. `"COM3"`, `/dev/ttyUSB0`).
    pub port: String,
    /// Available port names (populated by `refresh_ports`).
    pub available_ports: Vec<String>,
    /// Baud rate string.
    pub baud: String,
    /// One entry per `PARTITION_ADDRESSES[i]`.
    pub partitions: [PartitionEntry; 4],
    /// Scrollback for operation messages.
    pub log: OperationLog,
    /// Set while erase or flash is running; suppresses re-clicks.
    pub busy: bool,
    /// Cancel handle shared with the background thread.
    pub cancel: std::sync::Arc<std::sync::atomic::AtomicBool>,
}

impl Default for FlasherState {
    fn default() -> Self {
        Self {
            port: String::new(),
            available_ports: Vec::new(),
            baud: "115200".to_owned(),
            partitions: Default::default(),
            log: OperationLog::default(),
            busy: false,
            cancel: std::sync::Arc::new(std::sync::atomic::AtomicBool::new(false)),
        }
    }
}

/// Render the Flasher tab. Action wiring lands in Tasks 5-8.
pub fn show(ui: &mut Ui, state: &mut FlasherState, bus_tx: &crate::bus::AppEventTx) {
    // Lazy first-time port enumeration so the dropdown isn't empty before
    // the user clicks ⟳.
    if state.available_ports.is_empty() && state.port.is_empty() {
        refresh_ports(state);
        if let Some(first) = state.available_ports.first() {
            state.port = first.clone();
        }
    }

    ui.vertical(|ui| {
        // Top section: serial config row.
        ui.horizontal(|ui| {
            ui.label(t("port_number", None));
            ComboBox::from_id_salt("flasher_port")
                .selected_text(if state.port.is_empty() {
                    "—".to_owned()
                } else {
                    state.port.clone()
                })
                .show_ui(ui, |ui| {
                    for p in &state.available_ports {
                        ui.selectable_value(&mut state.port, p.clone(), p);
                    }
                });
            if ui.button("⟳").clicked() {
                refresh_ports(state);
            }
            ui.add_space(20.0);
            ui.label(t("baud_rate", None));
            ComboBox::from_id_salt("flasher_baud")
                .selected_text(&state.baud)
                .show_ui(ui, |ui| {
                    for b in [
                        "9600", "38400", "57600", "115200", "230400", "460800", "576000", "921600",
                        "1152000",
                    ] {
                        ui.selectable_value(&mut state.baud, b.to_owned(), b);
                    }
                });
        });

        ui.separator();

        // Partitions section: 4 rows of (enabled / address / path / pick).
        Grid::new("flasher_partitions")
            .num_columns(4)
            .show(ui, |ui| {
                for (i, part) in state.partitions.iter_mut().enumerate() {
                    ui.checkbox(&mut part.enabled, "");
                    ui.monospace(format!("0x{:08x}", PARTITION_ADDRESSES[i]));
                    ui.text_edit_singleline(&mut part.path);
                    if ui.button(t(PARTITION_BUTTON_KEYS[i], None)).clicked() {
                        if let Some(path) = rfd::FileDialog::new()
                            .add_filter("Binary", &["bin"])
                            .pick_file()
                        {
                            if let Some(s) = path.to_str() {
                                part.path = s.to_owned();
                                part.enabled = true;
                            }
                        }
                    }
                    ui.end_row();
                }
            });

        ui.separator();

        // Action buttons row.
        ui.horizontal(|ui| {
            // --- Erase (Task 7) -----------------------------------------
            if ui
                .add_enabled(!state.busy, egui::Button::new(t("clear_flash", None)))
                .clicked()
            {
                state.busy = true;
                state
                    .cancel
                    .store(false, std::sync::atomic::Ordering::Relaxed);
                let port = state.port.clone();
                let baud: u32 = state.baud.parse().unwrap_or(115_200);
                let bus_tx_outer = bus_tx.clone();
                let cancel = state.cancel.clone();
                state
                    .log
                    .push(format!("Starting chip erase on {port} @ {baud} baud..."));
                std::thread::spawn(move || {
                    let bus_tx = bus_tx_outer;
                    let result: Result<(), String> = (|| {
                        let mut flasher = aio_flasher::Flasher::new(&port, baud)
                            .map_err(|e| format!("open/connect: {e}"))?;
                        let (op_tx, op_rx) = std::sync::mpsc::channel::<aio_flasher::FlashEvent>();
                        // Forwarder thread re-wraps FlashEvent into AppEvent::Flash.
                        {
                            let bus_tx_fwd = bus_tx.clone();
                            std::thread::spawn(move || {
                                while let Ok(evt) = op_rx.recv() {
                                    let _ = bus_tx_fwd.send(crate::bus::AppEvent::Flash(evt));
                                }
                            });
                        }
                        flasher
                            .erase(op_tx, cancel)
                            .map_err(|e| format!("erase: {e}"))?;
                        Ok(())
                    })();
                    let _ = bus_tx.send(crate::bus::AppEvent::FlashFinished(result));
                });
            }

            // --- Flash Firmware (Task 8) -------------------------------
            if ui
                .add_enabled(!state.busy, egui::Button::new(t("flash_firmware", None)))
                .clicked()
            {
                // Preflight: collect enabled, valid partitions and read their
                // .bin files. Done on the egui thread so file IO errors land
                // in the log immediately. Errors are buffered into a Vec
                // first to avoid simultaneous borrows of state.partitions
                // (read) and state.log (mutate).
                let mut parts: Vec<aio_flasher::Partition> = Vec::new();
                let mut io_errors: Vec<String> = Vec::new();
                for (i, p) in state.partitions.iter().enumerate() {
                    if !p.enabled || p.path.is_empty() {
                        continue;
                    }
                    match std::fs::read(&p.path) {
                        Ok(data) => parts.push(aio_flasher::Partition {
                            address: PARTITION_ADDRESSES[i],
                            data,
                        }),
                        Err(e) => {
                            io_errors.push(format!("Skipping partition {i} ({}): {e}", p.path))
                        }
                    }
                }
                for line in io_errors {
                    state.log.push(line);
                }
                if parts.is_empty() {
                    state.log.push("No partitions selected with valid paths.");
                } else {
                    state.busy = true;
                    state
                        .cancel
                        .store(false, std::sync::atomic::Ordering::Relaxed);
                    let port = state.port.clone();
                    let baud: u32 = state.baud.parse().unwrap_or(115_200);
                    let bus_tx_outer = bus_tx.clone();
                    let cancel = state.cancel.clone();
                    let n = parts.len();
                    state.log.push(format!(
                        "Flashing {n} partition(s) on {port} @ {baud} baud..."
                    ));
                    std::thread::spawn(move || {
                        let bus_tx = bus_tx_outer;
                        let result: Result<(), String> = (|| {
                            let mut flasher = aio_flasher::Flasher::new(&port, baud)
                                .map_err(|e| format!("open/connect: {e}"))?;
                            let (op_tx, op_rx) =
                                std::sync::mpsc::channel::<aio_flasher::FlashEvent>();
                            {
                                let bus_tx_fwd = bus_tx.clone();
                                std::thread::spawn(move || {
                                    while let Ok(evt) = op_rx.recv() {
                                        let _ = bus_tx_fwd.send(crate::bus::AppEvent::Flash(evt));
                                    }
                                });
                            }
                            flasher
                                .write_partitions(parts, op_tx, cancel)
                                .map_err(|e| format!("flash: {e}"))?;
                            Ok(())
                        })();
                        let _ = bus_tx.send(crate::bus::AppEvent::FlashFinished(result));
                    });
                }
            }

            // --- Cancel (Task 8) ---------------------------------------
            if ui
                .add_enabled(state.busy, egui::Button::new(t("cancel_flash", None)))
                .clicked()
            {
                state
                    .cancel
                    .store(true, std::sync::atomic::Ordering::Relaxed);
                state.log.push("Cancellation requested...");
            }
        });

        ui.separator();

        // Operation log fills remaining vertical space.
        ui.heading(t("operation_log", None));
        state.log.show(ui);
    });
}
