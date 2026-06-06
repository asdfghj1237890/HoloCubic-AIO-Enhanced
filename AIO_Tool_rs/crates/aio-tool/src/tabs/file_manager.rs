//! File Manager tab — TCP browse + right-click ops on the remote SD card.

use std::net::SocketAddr;
use std::sync::atomic::AtomicBool;
use std::sync::mpsc::Sender;
use std::sync::Arc;

use aio_i18n::t;
use egui::{ScrollArea, Ui};

use crate::bus::AppEventTx;
use crate::file_manager_worker::{self, FmCmd};
use crate::tabs::fs_node::FsNode;
use crate::tabs::settings::DeviceState;
use crate::widgets::operation_log::OperationLog;

/// File Manager tab state.
pub struct FileManagerState {
    /// User-typed IP address (e.g. "192.168.0.165").
    pub ip: String,
    /// User-typed port (string for ComboBox / text-edit; parsed on Connect).
    pub port: String,
    /// Lifecycle state (shared shape with Settings tab).
    pub state: DeviceState,
    /// Root of the remote tree; nodes filled in lazily on DirList replies.
    pub tree: FsNode,
    /// Command channel to the worker (None when Disconnected).
    pub cmd_tx: Option<Sender<FmCmd>>,
    /// Cancel flag shared with worker.
    pub cancel: Arc<AtomicBool>,
    /// Most recent ReadFile path — used to label the save dialog when
    /// `FileManagerFileBytes` arrives (the wire response doesn't echo).
    pub last_read_path: Option<String>,
    /// Scrollable operation log.
    pub log: OperationLog,
}

impl Default for FileManagerState {
    fn default() -> Self {
        Self {
            ip: "192.168.0.165".to_owned(),
            port: "6677".to_owned(),
            state: DeviceState::Disconnected,
            tree: FsNode::root(),
            cmd_tx: None,
            cancel: Arc::new(AtomicBool::new(false)),
            last_read_path: None,
            log: OperationLog::default(),
        }
    }
}

/// Render the File Manager tab.
pub fn show(ui: &mut Ui, state: &mut FileManagerState, bus_tx: &AppEventTx) {
    ui.vertical(|ui| {
        // Connection bar.
        ui.horizontal(|ui| {
            ui.label(t("ip_address", None));
            ui.text_edit_singleline(&mut state.ip);
            ui.label(t("port_number", None));
            ui.text_edit_singleline(&mut state.port);

            let connected = matches!(state.state, DeviceState::Connected);
            let connecting = matches!(state.state, DeviceState::Connecting);

            if !connected && !connecting {
                if ui.button(t("connect", None)).clicked() {
                    match format!("{}:{}", state.ip, state.port).parse::<SocketAddr>() {
                        Ok(addr) => {
                            state.state = DeviceState::Connecting;
                            state
                                .cancel
                                .store(false, std::sync::atomic::Ordering::Relaxed);
                            let (cmd_tx, cancel) = file_manager_worker::spawn(addr, bus_tx.clone());
                            state.cmd_tx = Some(cmd_tx);
                            state.cancel = cancel;
                            state.log.push(format!("Connecting to {addr}..."));
                        }
                        Err(_) => {
                            state
                                .log
                                .push(format!("Invalid IP:port {}:{}", state.ip, state.port));
                        }
                    }
                }
            } else if ui.button(t("disconnect", None)).clicked() {
                // Cancel flag is the sole shutdown signal (Plan 7 reviewer I2).
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

        // Tree. Tasks 4-7 wire right-click context menus.
        ScrollArea::vertical().show(ui, |ui| {
            render_node(ui, &mut state.tree, &state.cmd_tx, &mut state.log);
        });

        ui.separator();
        ui.heading(t("operation_log", None));
        state.log.show(ui);
    });
}

/// Recursively render an `FsNode`. Directories use `CollapsingHeader`;
/// expanding a not-yet-loaded directory triggers a `ListDir` command.
/// Files render as a plain monospace label (context menu lands in Group D).
fn render_node(
    ui: &mut Ui,
    node: &mut FsNode,
    cmd_tx: &Option<Sender<FmCmd>>,
    log: &mut OperationLog,
) {
    if node.is_dir {
        let response = egui::CollapsingHeader::new(&node.name)
            .id_salt(&node.path)
            .show(ui, |ui| {
                for child in &mut node.children {
                    render_node(ui, child, cmd_tx, log);
                }
            });
        // On first expansion of a directory, send a ListDir over the worker.
        // CollapsingHeader's openness ranges 0.0..=1.0; >0.5 means "opening
        // or open". `node.loaded` prevents re-firing every frame.
        if response.openness > 0.5 && !node.loaded {
            if let Some(tx) = cmd_tx {
                let _ = tx.send(FmCmd::ListDir {
                    path: node.path.clone(),
                });
                log.push(format!("\u{2192} ListDir {}", node.path));
                node.loaded = true; // optimistic; reply will populate children
            }
        }
    } else {
        // File entry — Group D adds the context menu.
        ui.monospace(&node.name);
    }
}
