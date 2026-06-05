//! Settings tab — read / write 15 device config keys via SerialTransport.

use std::collections::HashMap;
use std::sync::atomic::AtomicBool;
use std::sync::mpsc::Sender;
use std::sync::Arc;

use aio_i18n::t;
use egui::{ComboBox, ScrollArea, Ui};

use crate::settings_worker::{self, SettingsCmd};
use crate::tabs::settings_schema::schema;
use crate::widgets::operation_log::OperationLog;

/// Lifecycle of the Settings tab's connection to a device.
#[derive(Debug, Clone)]
pub enum DeviceState {
    /// No worker thread running.
    Disconnected,
    /// Worker is opening / handshaking; UI shows a spinner indicator.
    Connecting,
    /// Worker is alive and owning the SerialTransport.
    Connected,
    /// Last error message; UI shows it in red. Clears on next Connect attempt.
    Error(String),
}

/// Settings tab state.
pub struct SettingsState {
    /// Port name (independent from Flasher's selection — Plan 7 doesn't sync).
    pub port: String,
    /// Baud rate (string for ComboBox; parsed on Connect).
    pub baud: String,
    /// Current lifecycle state.
    pub state: DeviceState,
    /// Per-key edited values. Populated by `SettingsReceived` from the bus
    /// when a Get reply decodes successfully, or by the user typing in the
    /// form field. Indexed by key name.
    pub values: HashMap<String, String>,
    /// Original values from the last Read All. Diff against `values` to
    /// compute "changed since read" set for Write Changes (Group D).
    pub baseline: HashMap<String, String>,
    /// Cmd channel to the worker (None when Disconnected).
    pub cmd_tx: Option<Sender<SettingsCmd>>,
    /// Cancel flag shared with worker.
    pub cancel: Arc<AtomicBool>,
    /// Scrollable operation log.
    pub log: OperationLog,
}

impl Default for SettingsState {
    fn default() -> Self {
        Self {
            port: String::new(),
            baud: "115200".to_owned(),
            state: DeviceState::Disconnected,
            values: HashMap::new(),
            baseline: HashMap::new(),
            cmd_tx: None,
            cancel: Arc::new(AtomicBool::new(false)),
            log: OperationLog::default(),
        }
    }
}

/// Render the Settings tab.
pub fn show(ui: &mut Ui, state: &mut SettingsState, bus_tx: &crate::bus::AppEventTx) {
    ui.vertical(|ui| {
        // Connection bar.
        ui.horizontal(|ui| {
            ui.label(t("port_number", None));
            ui.text_edit_singleline(&mut state.port);
            ui.label(t("baud_rate", None));
            ComboBox::from_id_salt("settings_baud")
                .selected_text(&state.baud)
                .show_ui(ui, |ui| {
                    for b in ["9600", "115200", "230400", "921600"] {
                        ui.selectable_value(&mut state.baud, b.to_owned(), b);
                    }
                });

            let connected = matches!(state.state, DeviceState::Connected);
            let connecting = matches!(state.state, DeviceState::Connecting);

            if !connected && !connecting {
                if ui.button(t("connect", None)).clicked() && !state.port.is_empty() {
                    state.state = DeviceState::Connecting;
                    state
                        .cancel
                        .store(false, std::sync::atomic::Ordering::Relaxed);
                    let baud: u32 = state.baud.parse().unwrap_or(115_200);
                    let (cmd_tx, cancel) =
                        settings_worker::spawn(state.port.clone(), baud, bus_tx.clone());
                    state.cmd_tx = Some(cmd_tx);
                    state.cancel = cancel;
                    state
                        .log
                        .push(format!("Connecting to {} @ {}...", state.port, baud));
                }
            } else if ui.button(t("disconnect", None)).clicked() {
                if let Some(tx) = &state.cmd_tx {
                    let _ = tx.send(SettingsCmd::Disconnect);
                }
                state
                    .cancel
                    .store(true, std::sync::atomic::Ordering::Relaxed);
                state.log.push("Disconnect requested.");
            }
        });

        // Surfaced error.
        if let DeviceState::Error(msg) = &state.state {
            ui.colored_label(egui::Color32::LIGHT_RED, format!("Error: {msg}"));
        }

        ui.separator();

        // Read All / Write Changes action row (Group D).
        ui.horizontal(|ui| {
            let connected = matches!(state.state, DeviceState::Connected);

            if ui
                .add_enabled(connected, egui::Button::new(t("read_settings", None)))
                .clicked()
            {
                if let Some(tx) = &state.cmd_tx {
                    let mut sent = 0usize;
                    for key in schema() {
                        if tx
                            .send(SettingsCmd::Get {
                                prefs_name: key.namespace.clone(),
                                key: key.key.clone(),
                            })
                            .is_ok()
                        {
                            sent += 1;
                        }
                    }
                    state.log.push(format!("Sent {sent} Get commands."));
                }
            }
        });

        ui.separator();

        // Per-namespace form. Group D wires read/write actions.
        ScrollArea::vertical().show(ui, |ui| {
            let mut current_ns: Option<String> = None;
            for key in schema() {
                if Some(&key.namespace) != current_ns.as_ref() {
                    ui.heading(&key.namespace);
                    current_ns = Some(key.namespace.clone());
                }
                ui.horizontal(|ui| {
                    ui.label(format!("{}:", key.key));
                    let value = state.values.entry(key.key.clone()).or_default();
                    ui.text_edit_singleline(value);
                    ui.weak(&key.value_type);
                });
            }
        });

        ui.separator();
        ui.heading(t("operation_log", None));
        state.log.show(ui);
    });
}
