//! Settings tab — Studio layout: page header + toolbar + group cards per
//! firmware namespace, with per-field diff dots and a "Write Changes (N)"
//! counter button. Mirrors `StudioParams` in `studio-pages.jsx`.

use std::collections::HashMap;
use std::sync::atomic::AtomicBool;
use std::sync::mpsc::Sender;
use std::sync::Arc;

use aio_i18n::t;
use egui::{ComboBox, RichText, ScrollArea, Ui};

use crate::settings_worker::{self, SettingsCmd};
use crate::tabs::settings_schema::{schema, SettingKey};
use crate::theme;
use crate::widgets::operation_log::OperationLog;
use crate::widgets::{page, studio};

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
    /// Per-key edited values.
    pub values: HashMap<String, String>,
    /// Original values from the last Read All. Diff against `values` to
    /// compute "changed since read" set for the Write Changes counter.
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

/// Friendly section title for a firmware namespace.
fn namespace_label(namespace: &str) -> &'static str {
    match namespace {
        "sys" => "WiFi / System",
        "zhixin" => "Weather (Seniverse)",
        "tianqi" => "Weather feed",
        "other" => "Other",
        _ => "—",
    }
}

/// Render the Settings tab.
pub fn show(ui: &mut Ui, state: &mut SettingsState, bus_tx: &crate::bus::AppEventTx) {
    let connected = matches!(state.state, DeviceState::Connected);
    let connecting = matches!(state.state, DeviceState::Connecting);

    page::page_header(
        ui,
        &t("params_title", None),
        &t("params_subtitle", None),
        |ui| {
            let port_opt = if connected {
                Some(state.port.as_str())
            } else {
                None
            };
            page::connection_chip(ui, connected, connecting, port_opt);
        },
    );

    // Toolbar — connection + Read All + Write Changes.
    let changes: Vec<(String, String)> = pending_changes(state);
    egui::Frame::none()
        .inner_margin(egui::Margin::symmetric(theme::S6, theme::S3))
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                ui.add(
                    egui::TextEdit::singleline(&mut state.port)
                        .hint_text(t("port_number", None))
                        .desired_width(150.0),
                );
                ComboBox::from_id_salt("settings_baud")
                    .selected_text(&state.baud)
                    .show_ui(ui, |ui| {
                        for b in ["9600", "115200", "230400", "921600"] {
                            ui.selectable_value(&mut state.baud, b.to_owned(), b);
                        }
                    });

                if !connected && !connecting {
                    if theme::primary_button(ui, t("connect", None)).clicked()
                        && !state.port.is_empty()
                    {
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
                } else if theme::ghost_button(ui, t("disconnect", None)).clicked() {
                    state
                        .cancel
                        .store(true, std::sync::atomic::Ordering::Relaxed);
                    state.log.push("Disconnect requested.");
                }

                // Push the action buttons to the right.
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    let write_label = if changes.is_empty() {
                        t("write_changes", None)
                    } else {
                        format!("{} ({})", t("write_changes", None), changes.len())
                    };
                    if ui
                        .add_enabled(
                            connected && !changes.is_empty(),
                            egui::Button::new(write_label),
                        )
                        .clicked()
                    {
                        write_changes(state, &changes);
                    }
                    if ui
                        .add_enabled(connected, egui::Button::new(t("read_settings", None)))
                        .clicked()
                    {
                        read_all(state);
                    }
                });
            });
        });

    if let DeviceState::Error(msg) = &state.state {
        ui.label(
            RichText::new(format!("Error: {msg}"))
                .color(theme::ERR)
                .size(12.5),
        );
    }

    studio::section_divider(ui);

    // Body — group cards per namespace + log at the bottom.
    ScrollArea::vertical()
        .auto_shrink([false; 2])
        .show(ui, |ui| {
            page::body_frame(ui, |ui| {
                ui.set_max_width(760.0);

                // Bucket schema rows by namespace, preserving JSON order.
                let mut groups: Vec<(String, Vec<&SettingKey>)> = Vec::new();
                for k in schema() {
                    if let Some((_, v)) = groups.iter_mut().find(|(ns, _)| ns == &k.namespace) {
                        v.push(k);
                    } else {
                        groups.push((k.namespace.clone(), vec![k]));
                    }
                }

                for (namespace, keys) in &groups {
                    page::section_label(ui, namespace_label(namespace));
                    ui.add_space(theme::S2);
                    page::group_card(ui, |ui| {
                        let enabled = connected;
                        for k in keys {
                            param_row(ui, state, k, enabled);
                        }
                    });
                    ui.add_space(theme::S4);
                }

                ui.add_space(theme::S5);
                page::section_label(ui, t("operation_log", None));
                ui.add_space(theme::S2);
                page::group_card(ui, |ui| {
                    let remaining = ui.available_height().max(120.0);
                    ui.allocate_ui(egui::Vec2::new(ui.available_width(), remaining), |ui| {
                        state.log.show(ui);
                    });
                });
            });
        });
}

/// One row inside a group card — label + text field + amber diff dot.
fn param_row(ui: &mut Ui, state: &mut SettingsState, key: &SettingKey, enabled: bool) {
    let changed = match (state.values.get(&key.key), state.baseline.get(&key.key)) {
        (Some(cur), Some(base)) => cur != base,
        _ => false,
    };
    ui.horizontal(|ui| {
        // Label column (150px wide to match the prototype).
        ui.scope(|ui| {
            ui.set_width(150.0);
            ui.horizontal(|ui| {
                ui.label(RichText::new(&key.key).size(13.0).color(if enabled {
                    theme::TEXT_DIM
                } else {
                    theme::TEXT_MUTE
                }));
                if changed {
                    let (dot_rect, _) =
                        ui.allocate_exact_size(egui::Vec2::splat(8.0), egui::Sense::hover());
                    ui.painter()
                        .circle_filled(dot_rect.center(), 3.5, theme::WARN);
                }
            });
        });
        // Value control.
        let value = state.values.entry(key.key.clone()).or_default();
        ui.add_enabled(
            enabled,
            egui::TextEdit::singleline(value).desired_width(280.0),
        );
        ui.label(
            RichText::new(&key.value_type)
                .monospace()
                .size(11.0)
                .color(theme::TEXT_MUTE),
        );
    });
    ui.add_space(theme::S1);
}

fn pending_changes(state: &SettingsState) -> Vec<(String, String)> {
    schema()
        .iter()
        .filter_map(|k| {
            let current = state.values.get(&k.key)?;
            let baseline = state.baseline.get(&k.key);
            if Some(current) == baseline {
                None
            } else {
                Some((k.key.clone(), current.clone()))
            }
        })
        .collect()
}

fn read_all(state: &mut SettingsState) {
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

fn write_changes(state: &mut SettingsState, changes: &[(String, String)]) {
    if let Some(tx) = &state.cmd_tx {
        let key_to_def: HashMap<&str, &SettingKey> =
            schema().iter().map(|k| (k.key.as_str(), k)).collect();

        let mut sent = 0usize;
        for (key, value) in changes {
            let def = match key_to_def.get(key.as_str()) {
                Some(d) => *d,
                None => continue,
            };
            let value_type = match def.value_type.as_str() {
                "String" => aio_protocol::ValueType::String,
                "UChar" => aio_protocol::ValueType::Uchar,
                _ => aio_protocol::ValueType::Unknown,
            };
            if tx
                .send(SettingsCmd::Set {
                    prefs_name: def.namespace.clone(),
                    key: key.clone(),
                    value_type,
                    value: value.clone(),
                })
                .is_ok()
            {
                sent += 1;
            }
        }
        state.log.push(format!("Sent {sent} Set commands."));

        // Update baseline so subsequent Write Changes only sends NEW diffs.
        for (key, value) in changes {
            state.baseline.insert(key.clone(), value.clone());
        }
    }
}
