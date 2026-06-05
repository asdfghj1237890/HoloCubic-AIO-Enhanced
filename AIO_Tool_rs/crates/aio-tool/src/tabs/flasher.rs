//! Flasher tab — partition list + erase / flash / cancel + operation log.
//!
//! Plan 6 Task 4: lays out the UI and holds state. Tasks 5-8 wire actions.

use aio_i18n::t;
use egui::{ComboBox, Grid, Ui};

use crate::widgets::operation_log::OperationLog;

/// Standard ESP32 flash addresses for the 4 partitions.
pub const PARTITION_ADDRESSES: [u32; 4] = [
    aio_flasher::PARTITION_BOOTLOADER,
    aio_flasher::PARTITION_PARTITIONS,
    aio_flasher::PARTITION_BOOTAPP0,
    aio_flasher::PARTITION_FIRMWARE,
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
}

impl Default for FlasherState {
    fn default() -> Self {
        Self {
            port: String::new(),
            available_ports: Vec::new(),
            baud: "115200".to_owned(),
            partitions: Default::default(),
            log: OperationLog::default(),
        }
    }
}

/// Render the Flasher tab. Action wiring lands in Tasks 5-8.
pub fn show(ui: &mut Ui, state: &mut FlasherState) {
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
                // Refresh action wired in Task 5.
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
                    if ui.button(t("choose_file", None)).clicked() {
                        // File picker wired in Task 6.
                    }
                    ui.end_row();
                }
            });

        ui.separator();

        // Action buttons row.
        ui.horizontal(|ui| {
            if ui.button(t("erase_flash", None)).clicked() {
                // Wired in Task 7.
            }
            if ui.button(t("flash_firmware", None)).clicked() {
                // Wired in Task 8.
            }
            if ui.button(t("cancel_flash", None)).clicked() {
                // Wired in Task 8.
            }
        });

        ui.separator();

        // Operation log fills remaining vertical space.
        ui.heading(t("operation_log", None));
        state.log.show(ui);
    });
}
