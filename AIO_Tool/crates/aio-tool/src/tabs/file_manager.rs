//! File Manager tab — Studio layout: page header + connection bar + file
//! tree wrapped in a group card. The remote tree itself is still rendered
//! with `egui::CollapsingHeader` (Plan 8 baseline); a flat-list +
//! breadcrumb rewrite to fully mirror `StudioFiles` would be a separate
//! plan and would change keyboard/click semantics user-facing.

use std::net::SocketAddr;
use std::sync::atomic::AtomicBool;
use std::sync::mpsc::Sender;
use std::sync::Arc;

use aio_i18n::t;
use egui::{RichText, ScrollArea, Ui};

use crate::bus::AppEventTx;
use crate::file_manager_worker::{self, FmCmd};
use crate::tabs::fs_node::FsNode;
use crate::tabs::settings::DeviceState;
use crate::theme;
use crate::widgets::operation_log::OperationLog;
use crate::widgets::page;

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
    let connected = matches!(state.state, DeviceState::Connected);
    let connecting = matches!(state.state, DeviceState::Connecting);

    page::page_header(
        ui,
        &t("files_title", None),
        &t("files_subtitle", None),
        |ui| {
            let addr = if connected {
                format!("{}:{}", state.ip, state.port)
            } else {
                String::new()
            };
            page::connection_chip(
                ui,
                connected,
                connecting,
                if connected { Some(&addr) } else { None },
            );
        },
    );

    // Connection bar — IP + ":" + port + connect/disconnect.
    egui::Frame::none()
        .inner_margin(egui::Margin::symmetric(theme::S6, theme::S3))
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                ui.add(
                    egui::TextEdit::singleline(&mut state.ip)
                        .hint_text("IP")
                        .desired_width(150.0),
                );
                ui.label(RichText::new(":").color(theme::TEXT_MUTE));
                ui.add(
                    egui::TextEdit::singleline(&mut state.port)
                        .hint_text("port")
                        .desired_width(80.0),
                );
                if !connected && !connecting {
                    if theme::primary_button(ui, t("connect", None)).clicked() {
                        match format!("{}:{}", state.ip, state.port).parse::<SocketAddr>() {
                            Ok(addr) => {
                                state.state = DeviceState::Connecting;
                                state
                                    .cancel
                                    .store(false, std::sync::atomic::Ordering::Relaxed);
                                let (cmd_tx, cancel) =
                                    file_manager_worker::spawn(addr, bus_tx.clone());
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
                } else if theme::ghost_button(ui, t("disconnect", None)).clicked() {
                    state
                        .cancel
                        .store(true, std::sync::atomic::Ordering::Relaxed);
                    state.log.push("Disconnect requested.");
                }
            });
        });

    if let DeviceState::Error(msg) = &state.state {
        ui.label(
            RichText::new(format!("Error: {msg}"))
                .color(theme::ERR)
                .size(12.5),
        );
    }

    crate::widgets::studio::section_divider(ui);

    // Body — tree wrapped in a group card, plus the operation log below.
    ScrollArea::vertical()
        .auto_shrink([false; 2])
        .show(ui, |ui| {
            page::body_frame(ui, |ui| {
                if !connected {
                    ui.vertical_centered(|ui| {
                        ui.add_space(theme::S6);
                        ui.label(
                            RichText::new(t("files_not_connected_hint", None))
                                .size(13.0)
                                .color(theme::TEXT_MUTE),
                        );
                    });
                } else {
                    page::section_label(ui, t("files_tree_label", None));
                    ui.add_space(theme::S2);
                    page::group_card(ui, |ui| {
                        render_node(
                            ui,
                            &mut state.tree,
                            &state.cmd_tx,
                            &mut state.last_read_path,
                            &mut state.log,
                        );
                    });
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

/// Recursively render an `FsNode` — same logic as Plan 8 baseline; only
/// the surrounding chrome moved.
fn render_node(
    ui: &mut Ui,
    node: &mut FsNode,
    cmd_tx: &Option<Sender<FmCmd>>,
    last_read_path: &mut Option<String>,
    log: &mut OperationLog,
) {
    if node.is_dir {
        let response = egui::CollapsingHeader::new(&node.name)
            .id_salt(&node.path)
            .show(ui, |ui| {
                for child in &mut node.children {
                    render_node(ui, child, cmd_tx, last_read_path, log);
                }
            });
        if response.openness > 0.5 && !node.loaded {
            if let Some(tx) = cmd_tx {
                let _ = tx.send(FmCmd::ListDir {
                    path: node.path.clone(),
                });
                log.push(format!("\u{2192} ListDir {}", node.path));
                node.loaded = true;
            }
        }
    } else {
        let resp = ui.monospace(&node.name);
        resp.context_menu(|ui| {
            if ui.button(t("download", None)).clicked() {
                if let Some(tx) = cmd_tx {
                    *last_read_path = Some(node.path.clone());
                    let _ = tx.send(FmCmd::ReadFile {
                        path: node.path.clone(),
                    });
                    log.push(format!("\u{2192} ReadFile {}", node.path));
                }
                ui.close_menu();
            }
            if ui.button(t("delete", None)).clicked() {
                if let Some(tx) = cmd_tx {
                    let _ = tx.send(FmCmd::RemoveFile {
                        name: node.path.clone(),
                    });
                    log.push(format!("\u{2192} RemoveFile {}", node.path));
                    let parent_path = match node.path.rsplit_once('/') {
                        Some(("", _)) => "/".to_owned(),
                        Some((p, _)) => p.to_owned(),
                        None => "/".to_owned(),
                    };
                    let _ = tx.send(FmCmd::ListDir {
                        path: parent_path.clone(),
                    });
                    log.push(format!(
                        "\u{2192} ListDir {} (refresh after delete)",
                        parent_path
                    ));
                }
                ui.close_menu();
            }
            if ui.button(t("rename", None)).clicked() {
                if let Some(tx) = cmd_tx {
                    let _ = tx.send(FmCmd::RenameFile {
                        name: node.path.clone(),
                    });
                    log.push(format!(
                        "\u{2192} RenameFile {} (B1 preserved bug \u{2014} no actual rename)",
                        node.path
                    ));
                }
                ui.close_menu();
            }
            if ui.button(t("properties", None)).clicked() {
                if let Some(tx) = cmd_tx {
                    let _ = tx.send(FmCmd::GetFileInfo {
                        name: node.path.clone(),
                    });
                    log.push(format!("\u{2192} GetFileInfo {}", node.path));
                }
                ui.close_menu();
            }
        });
    }
}
