//! Flasher tab — Studio-style 3-step guided flow.
//!
//! Layout:
//!   ┌─ header: page title + status chip ─────────────────────────────────┐
//!   ├─ left column ──────────────┬─ right sidebar ────────────────────────┤
//!   │ Step 1: Connect            │ Device card (placeholder + chip info)  │
//!   │ Step 2: Firmware           │ D-pad (remote keys, keyboard ↑←→/Enter)│
//!   │ Step 3: Flash              │ Operation log (autoscroll)             │
//!   └────────────────────────────┴────────────────────────────────────────┘
//!
//! Backend wiring is preserved verbatim from the previous flasher.rs —
//! erase / flash / remote each spawn a transient serial connection so the
//! "connect" step here is a UI-level affordance (Plan 1 baseline; a real
//! persistent connection would be a separate plan).
//!
//! Plan 6 Task 4 originally laid out the dense partition grid; this is the
//! Studio redesign (Plan 11 — porting the HTML reference design to egui).

use aio_i18n::t;
use egui::{ComboBox, RichText, ScrollArea, Ui};
use serialport::available_ports;

use crate::theme::{self, ProgressState};
use crate::widgets::operation_log::OperationLog;
use crate::widgets::studio::{
    self, dpad_cluster, partition_row, progress_bar, status_chip, step_circle, step_connector,
    success_banner, ChipKind, DpadKey,
};

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

/// Flasher tab state. Public fields are part of the test seam — see
/// `tests/app_drain_events.rs`.
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

    /// UI-level "connected to device" flag — drives the step-2/3 enable
    /// state and the success banner reset. The hardware-side connection is
    /// per-operation (still opened+closed inside each thread).
    pub connected: bool,
    /// Whether step 2's advanced-partition panel is expanded.
    pub show_advanced: bool,
    /// True when the last flash run reported success; clears on next start.
    pub last_flash_succeeded: bool,
    /// Per-partition progress fractions captured from FlashEvent stream —
    /// drained at the top of every frame from `log` is not enough since
    /// `log` is text. We keep separate state here so the checklist UI can
    /// render percent indicators without parsing log lines.
    pub partition_percent: [f32; 4],
    /// Index of the partition currently being written (or `None`).
    pub active_partition: Option<usize>,
    /// Maps FlashEvent's `index` (position in the enabled-partition list
    /// passed to `Flasher::write_partitions`) back to its address slot
    /// (0..4 in `PARTITION_ADDRESSES`).
    pub flash_slot_map: Vec<usize>,
    /// Total bytes per FlashEvent index — captured from PartitionStart so
    /// Progress events can convert `bytes_written` to a fraction.
    pub flash_total_bytes: Vec<u64>,
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
            connected: false,
            show_advanced: false,
            last_flash_succeeded: false,
            partition_percent: [0.0; 4],
            active_partition: None,
            flash_slot_map: Vec::new(),
            flash_total_bytes: Vec::new(),
        }
    }
}

/// Common baud rates surfaced by the dropdown — matches the prototype's
/// 9-entry list.
const BAUD_RATES: [&str; 9] = [
    "9600", "38400", "57600", "115200", "230400", "460800", "576000", "921600", "1152000",
];

/// Spawn a transient thread that opens the serial port, writes the 2-byte
/// remote-control command, and closes. On error, route through the bus as
/// a FlashFinished(Err) so the user sees the failure in the log.
fn spawn_remote_send(state: &mut FlasherState, bus_tx: &crate::bus::AppEventTx, cmd: &[u8]) {
    let port = state.port.clone();
    let baud: u32 = state.baud.parse().unwrap_or(115_200);
    let bus_tx = bus_tx.clone();
    let cmd_bytes: Vec<u8> = cmd.to_vec();
    state
        .log
        .push(format!("→ {}", String::from_utf8_lossy(&cmd_bytes)));
    std::thread::spawn(move || {
        use aio_device::Transport;
        let result: Result<(), String> = (|| {
            let mut transport = aio_device::serial::SerialTransport::open(&port, baud)
                .map_err(|e| format!("open: {e}"))?;
            transport
                .write_all(&cmd_bytes)
                .map_err(|e| format!("write: {e}"))?;
            transport.close();
            Ok(())
        })();
        if let Err(msg) = result {
            let _ = bus_tx.send(crate::bus::AppEvent::FlashFinished(Err(format!(
                "remote {}: {msg}",
                String::from_utf8_lossy(&cmd_bytes),
            ))));
        }
    });
}

fn send_dpad(state: &mut FlasherState, bus_tx: &crate::bus::AppEventTx, key: DpadKey) {
    let cmd: &[u8] = match key {
        DpadKey::Up => b"~U",
        DpadKey::Left => b"~L",
        DpadKey::Right => b"~R",
        DpadKey::Ok => b"~F",
        DpadKey::Home => b"~H",
    };
    spawn_remote_send(state, bus_tx, cmd);
}

/// Render the Flasher tab.
pub fn show(ui: &mut Ui, state: &mut FlasherState, bus_tx: &crate::bus::AppEventTx) {
    // Lazy first-time port enumeration so the dropdown isn't empty before
    // the user clicks refresh.
    if state.available_ports.is_empty() && state.port.is_empty() {
        refresh_ports(state);
        if let Some(first) = state.available_ports.first() {
            state.port = first.clone();
        }
    }

    // Keyboard remote shortcuts: ↑ ← → Enter Home — only when connected.
    if state.connected && !state.busy {
        let pressed = ui.input(|i| {
            if i.key_pressed(egui::Key::ArrowUp) {
                Some(DpadKey::Up)
            } else if i.key_pressed(egui::Key::ArrowLeft) {
                Some(DpadKey::Left)
            } else if i.key_pressed(egui::Key::ArrowRight) {
                Some(DpadKey::Right)
            } else if i.key_pressed(egui::Key::Enter) {
                Some(DpadKey::Ok)
            } else if i.key_pressed(egui::Key::Home) {
                Some(DpadKey::Home)
            } else {
                None
            }
        });
        if let Some(key) = pressed {
            send_dpad(state, bus_tx, key);
        }
    }

    // ─── Header — proper padding + border-bottom matching the prototype ─
    egui::Frame::none()
        .inner_margin(egui::Margin::symmetric(theme::S6, theme::S5))
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                ui.vertical(|ui| {
                    ui.label(
                        RichText::new(t("flasher_title", None))
                            .font(theme::display_font(21.0))
                            .strong()
                            .color(theme::TEXT),
                    );
                    ui.label(
                        RichText::new(t("flasher_subtitle", None))
                            .size(13.0)
                            .color(theme::TEXT_MUTE),
                    );
                });
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    let (kind, label) = status_chip_text(state);
                    status_chip(ui, kind, label);
                });
            });
        });
    // Header's 1px `borderBottom: var(--border)` from the prototype.
    studio::section_divider(ui);

    // ─── Two-column body. Capture remaining height BEFORE the horizontal
    //     split — `ui.available_height()` inside the row collapses to 0
    //     because egui's horizontal layout sizes to content height.
    //
    //     Prototype caps the main steps column at `maxWidth: 720` with
    //     `padding: var(--s6)` (30px) — we mirror that here so the
    //     content doesn't sprawl on wide monitors.
    let total_w = ui.available_width();
    let body_h = ui.available_height();
    // Sidebar — wider than the prototype's fixed 360 px so it carries
    // visual weight on high-DPI / wide monitors where 360 px would be
    // a thin strip relative to the main column's empty padding.
    let sidebar_w = 400.0_f32.min(total_w * 0.36);
    let divider_w = 1.0;
    let main_w = (total_w - sidebar_w - divider_w).max(360.0);
    let main_content_max_w = 720.0;

    ui.horizontal_top(|ui| {
        // ── Left column: 3-step flow, fills full body height. The inner
        //    content is capped at `main_content_max_w` per the prototype.
        ui.allocate_ui_with_layout(
            egui::Vec2::new(main_w, body_h),
            egui::Layout::top_down(egui::Align::LEFT),
            |ui| {
                ScrollArea::vertical()
                    .id_salt("flasher_steps")
                    .auto_shrink([false, false])
                    .show(ui, |ui| {
                        egui::Frame::none()
                            .inner_margin(egui::Margin::same(theme::S6))
                            .show(ui, |ui| {
                                ui.set_max_width(main_content_max_w);
                                show_step1(ui, state, bus_tx);
                                // Step 2/3 are pending until connected;
                                // matching the prototype's `opacity: 0.5`
                                // on the whole step block.
                                if !state.connected {
                                    ui.scope(|ui| {
                                        ui.set_opacity(0.5);
                                        show_step2(ui, state);
                                        show_step3(ui, state, bus_tx);
                                    });
                                } else {
                                    show_step2(ui, state);
                                    show_step3(ui, state, bus_tx);
                                }
                            });
                    });
            },
        );

        // ── Vertical divider matching the prototype's border-left ──
        let (divider_rect, _) =
            ui.allocate_exact_size(egui::Vec2::new(divider_w, body_h), egui::Sense::hover());
        ui.painter()
            .rect_filled(divider_rect, egui::Rounding::ZERO, theme::BORDER);
        ui.add_space(theme::S5);

        // ── Right sidebar with the lighter PANEL background that the
        //     prototype uses to visually delimit the device column ──
        ui.allocate_ui_with_layout(
            egui::Vec2::new(sidebar_w, body_h),
            egui::Layout::top_down(egui::Align::LEFT),
            |ui| {
                egui::Frame::none()
                    .fill(theme::PANEL)
                    .inner_margin(egui::Margin::ZERO)
                    .show(ui, |ui| {
                        // Force the Frame to fill the full sidebar height.
                        ui.set_min_height(body_h);
                        ui.set_min_width(sidebar_w);
                        show_sidebar(ui, state, bus_tx);
                    });
            },
        );
    });
}

fn status_chip_text(state: &FlasherState) -> (ChipKind, String) {
    if state.busy {
        if state.last_flash_succeeded {
            // Shouldn't happen — defensive default.
            (ChipKind::Live, t("status_connected", None))
        } else {
            (ChipKind::Busy, t("status_flashing", None))
        }
    } else if state.connected {
        let port = if state.port.is_empty() {
            String::new()
        } else {
            format!(" · {}", state.port)
        };
        (
            ChipKind::Live,
            format!("{}{}", t("status_connected", None), port),
        )
    } else {
        (ChipKind::Inactive, t("status_disconnected", None))
    }
}

// ─── Step 1: connect ──────────────────────────────────────────────────────

fn show_step1(ui: &mut Ui, state: &mut FlasherState, bus_tx: &crate::bus::AppEventTx) {
    let step_state = if state.connected {
        ProgressState::Done
    } else {
        ProgressState::Active
    };
    step_header(
        ui,
        1,
        step_state,
        &t("step1_title", None),
        &t("step1_sub", None),
    );

    ui.horizontal_wrapped(|ui| {
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
        let refresh_btn = ui.add_sized(
            egui::Vec2::splat(28.0),
            egui::Button::new("").fill(theme::PANEL_2),
        );
        let icon_rect =
            egui::Rect::from_center_size(refresh_btn.rect.center(), egui::Vec2::splat(16.0));
        crate::widgets::icons::paint(
            ui.painter(),
            icon_rect,
            crate::widgets::icons::Icon::Refresh,
            theme::TEXT_DIM,
        );
        if refresh_btn.clicked() {
            refresh_ports(state);
        }
        ComboBox::from_id_salt("flasher_baud")
            .selected_text(&state.baud)
            .show_ui(ui, |ui| {
                for b in BAUD_RATES {
                    ui.selectable_value(&mut state.baud, b.to_owned(), b);
                }
            });
        if !state.connected {
            let label = if state.busy {
                t("connecting", None)
            } else {
                t("connect", None)
            };
            if theme::primary_button(ui, label)
                .on_disabled_hover_text("port required")
                .clicked()
                && !state.port.is_empty()
            {
                state.connected = true;
                state.last_flash_succeeded = false;
                state.partition_percent = [0.0; 4];
                state.active_partition = None;
                state.log.push(format!(
                    "→ Connected (UI) on {} @ {}",
                    state.port, state.baud
                ));
            }
        } else {
            if theme::ghost_button(ui, t("disconnect", None)).clicked() {
                state.connected = false;
                state.last_flash_succeeded = false;
                state.partition_percent = [0.0; 4];
                state.active_partition = None;
                state.log.push("→ Disconnected (UI)");
            }
            if theme::ghost_button(ui, t("reboot", None)).clicked() {
                spawn_remote_send(state, bus_tx, b"~B");
            }
        }
    });
    ui.add_space(theme::S2);
    ui.label(
        RichText::new(t("port_driver_hint", None))
            .size(11.5)
            .color(theme::TEXT_MUTE),
    );
    ui.add_space(theme::S6);
}

// ─── Step 2: choose firmware ──────────────────────────────────────────────

fn show_step2(ui: &mut Ui, state: &mut FlasherState) {
    let step_state = if !state.connected {
        ProgressState::Pending
    } else if state.last_flash_succeeded {
        ProgressState::Done
    } else {
        ProgressState::Active
    };
    step_header(
        ui,
        2,
        step_state,
        &t("step2_title", None),
        &t("step2_sub", None),
    );

    // Recommended-firmware highlight card — mirrors the prototype's
    // `HoloCubic AIO 韌體 v2.6.7 / N 個分割區 · X · 推薦給多數使用者 / ● 最新`.
    let enabled_n = state.partitions.iter().filter(|p| p.enabled).count();
    let total_bytes: u64 = state
        .partitions
        .iter()
        .enumerate()
        .filter(|(_, p)| p.enabled)
        .map(|(i, _)| state.flash_total_bytes.get(i).copied().unwrap_or(0))
        .sum();
    let total_label = if total_bytes == 0 {
        format!(
            "{} partitions · {}",
            enabled_n,
            t("recommended_firmware", None)
        )
    } else {
        format!(
            "{} partitions · {} · {}",
            enabled_n,
            fmt_bytes(total_bytes),
            t("recommended_firmware", None)
        )
    };
    egui::Frame::none()
        .fill(theme::ACCENT_WEAK)
        .stroke(egui::Stroke::new(1.0, theme::ACCENT_LINE))
        .rounding(egui::Rounding::same(theme::R3))
        .inner_margin(egui::Margin::same(theme::S4))
        .show(ui, |ui| {
            // Bound the row to the available column width and force
            // shrink-to-content vertically — without this, the inner
            // `right_to_left` layout grabbed both axes and the card
            // ballooned to fill the scroll area.
            let row_w = ui.available_width();
            ui.allocate_ui_with_layout(
                egui::Vec2::new(row_w, 0.0),
                egui::Layout::left_to_right(egui::Align::Center),
                |ui| {
                    // Painter-drawn bolt in an accent square.
                    let (rect, _) =
                        ui.allocate_exact_size(egui::Vec2::splat(42.0), egui::Sense::hover());
                    let p = ui.painter();
                    p.rect_filled(rect, egui::Rounding::same(theme::R2), theme::ACCENT);
                    let icon_rect = rect.shrink(8.0);
                    crate::widgets::icons::paint(
                        p,
                        icon_rect,
                        crate::widgets::icons::Icon::Bolt,
                        theme::ACCENT_INK,
                    );
                    ui.add_space(theme::S3);

                    // Middle column — title row + subtitle. Bounded width
                    // so the right-side chip fits without expanding.
                    let chip_w = 84.0;
                    let middle_w = (ui.available_width() - chip_w - theme::S3).max(140.0);
                    ui.allocate_ui_with_layout(
                        egui::Vec2::new(middle_w, 0.0),
                        egui::Layout::top_down(egui::Align::LEFT),
                        |ui| {
                            ui.horizontal(|ui| {
                                ui.label(
                                    RichText::new("HoloCubic AIO firmware")
                                        .strong()
                                        .size(14.0)
                                        .color(theme::TEXT),
                                );
                                ui.label(
                                    RichText::new("v3.0.0")
                                        .monospace()
                                        .size(13.0)
                                        .color(theme::ACCENT),
                                );
                            });
                            ui.add_space(2.0);
                            ui.label(
                                RichText::new(total_label)
                                    .size(12.0)
                                    .color(theme::TEXT_MUTE),
                            );
                        },
                    );
                    ui.add_space(theme::S3);
                    studio::status_chip(ui, studio::ChipKind::Live, "Latest");
                },
            );
        });
    ui.add_space(theme::S3);

    // Advanced toggle.
    let toggle_label = if state.show_advanced {
        t("advanced_partitions_open", None)
    } else {
        t("advanced_partitions_closed", None)
    };
    if theme::ghost_button(ui, RichText::new(toggle_label).size(12.0)).clicked() {
        state.show_advanced = !state.show_advanced;
    }

    if state.show_advanced {
        ui.add_space(theme::S2);
        theme::card(ui, |ui| {
            for (i, part) in state.partitions.iter_mut().enumerate() {
                ui.horizontal(|ui| {
                    ui.checkbox(&mut part.enabled, "");
                    ui.label(
                        RichText::new(format!("0x{:05x}", PARTITION_ADDRESSES[i]))
                            .monospace()
                            .color(theme::ACCENT)
                            .size(12.0),
                    );
                    ui.add(
                        egui::TextEdit::singleline(&mut part.path)
                            .desired_width(ui.available_width() - 100.0),
                    );
                    if ui
                        .add(egui::Button::new(
                            RichText::new(t(PARTITION_BUTTON_KEYS[i], None)).size(11.5),
                        ))
                        .clicked()
                    {
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
                });
                ui.add_space(theme::S1);
            }
        });
    }
    ui.add_space(theme::S6);
}

// ─── Step 3: flash ────────────────────────────────────────────────────────

fn show_step3(ui: &mut Ui, state: &mut FlasherState, bus_tx: &crate::bus::AppEventTx) {
    let step_state = if !state.connected {
        ProgressState::Pending
    } else if state.last_flash_succeeded {
        ProgressState::Done
    } else {
        ProgressState::Active
    };
    step_header(
        ui,
        3,
        step_state,
        &t("step3_title", None),
        &t("step3_sub", None),
    );

    ui.horizontal(|ui| {
        if state.busy {
            if theme::danger_button(
                ui,
                RichText::new(t("cancel_flash", None)).size(14.0).strong(),
            )
            .clicked()
            {
                state
                    .cancel
                    .store(true, std::sync::atomic::Ordering::Relaxed);
                state.log.push("Cancellation requested...");
            }
        } else if theme::primary_button(ui, RichText::new(t("flash_firmware", None)).size(14.0))
            .clicked()
            && state.connected
        {
            start_flash(state, bus_tx);
        }
        if theme::ghost_button(ui, t("clear_flash", None)).clicked()
            && state.connected
            && !state.busy
        {
            start_erase(state, bus_tx);
        }
    });
    ui.add_space(theme::S3);

    // Per-partition checklist — visible once connected and either flashing
    // or just-finished.
    if state.connected && (state.busy || state.last_flash_succeeded) {
        theme::card(ui, |ui| {
            for (i, part) in state.partitions.iter().enumerate() {
                if !part.enabled {
                    continue;
                }
                let p_state = partition_progress_state(state, i);
                let percent = state.partition_percent[i];
                let display = if part.path.is_empty() {
                    "—".to_owned()
                } else {
                    std::path::Path::new(&part.path)
                        .file_name()
                        .and_then(|n| n.to_str())
                        .unwrap_or(&part.path)
                        .to_owned()
                };
                partition_row(ui, PARTITION_ADDRESSES[i], &display, percent, p_state);
            }
            ui.add_space(theme::S2);
            progress_bar(ui, total_progress(state));
        });
    }

    if state.last_flash_succeeded {
        ui.add_space(theme::S4);
        success_banner(
            ui,
            t("flash_succeeded", None),
            t("flash_succeeded_sub", None),
        );
    }
    ui.add_space(theme::S6);
}

/// Human-readable byte size — used by the recommended firmware card.
fn fmt_bytes(n: u64) -> String {
    if n >= 1024 * 1024 {
        format!("{:.2} MB", n as f64 / 1_048_576.0)
    } else if n >= 1024 {
        format!("{:.1} KB", n as f64 / 1024.0)
    } else {
        format!("{} B", n)
    }
}

// Per-partition state from the active-index + percent counters.
fn partition_progress_state(state: &FlasherState, idx: usize) -> ProgressState {
    use std::cmp::Ordering;
    if state.last_flash_succeeded {
        return ProgressState::Done;
    }
    match state.active_partition {
        Some(active) => match idx.cmp(&active) {
            Ordering::Less => ProgressState::Done,
            Ordering::Equal => ProgressState::Active,
            Ordering::Greater => ProgressState::Pending,
        },
        None => ProgressState::Pending,
    }
}

fn total_progress(state: &FlasherState) -> f32 {
    let enabled: Vec<usize> = (0..4).filter(|&i| state.partitions[i].enabled).collect();
    if enabled.is_empty() {
        return 0.0;
    }
    if state.last_flash_succeeded {
        return 1.0;
    }
    let sum: f32 = enabled
        .iter()
        .map(|&i| state.partition_percent[i] / 100.0)
        .sum();
    sum / enabled.len() as f32
}

// ─── Right sidebar ────────────────────────────────────────────────────────
//
// Mirrors the prototype's right column:
//   background: var(--panel)          ← brighter than main BG
//   sections separated by 1px BORDER  ← borderBottom in JSX
//   padding: var(--s5) per section    ← 22px

fn show_sidebar(ui: &mut Ui, state: &mut FlasherState, bus_tx: &crate::bus::AppEventTx) {
    // Sidebar section padding — S6 (30 px) on both axes for the
    // breathing room the prototype's `padding: var(--s6)` gives at
    // wider sidebar widths. S5 (22) on the left/right made content sit
    // too close to the sidebar's right edge.
    let inner_pad = egui::Margin::same(theme::S6);

    // ── Device card section ──
    egui::Frame::none().inner_margin(inner_pad).show(ui, |ui| {
        // Centered dashed-border frame around a 118×146 inner placeholder,
        // matching the prototype's `<image-slot>` slot.
        //
        // `set_max_width(142)` caps the inset_frame to the 118 + 2*12
        // content+padding width. Without it the Frame stretches to the
        // section's full content area and the dashed border looks like
        // it's pinned to the sidebar edges.
        ui.vertical_centered(|ui| {
            ui.set_max_width(142.0);
            studio::inset_frame(ui, |ui| {
                let (rect, _) =
                    ui.allocate_exact_size(egui::Vec2::new(118.0, 146.0), egui::Sense::hover());
                let p = ui.painter();
                p.rect_filled(rect, egui::Rounding::same(12.0), theme::PANEL_3);
                // Connection state glyph centered, placeholder text below.
                let glyph_pos = egui::Pos2::new(rect.center().x, rect.center().y - 12.0);
                p.text(
                    glyph_pos,
                    egui::Align2::CENTER_CENTER,
                    if state.connected { "●" } else { "○" },
                    egui::FontId::proportional(28.0),
                    if state.connected {
                        theme::OK
                    } else {
                        theme::TEXT_MUTE
                    },
                );
                p.text(
                    egui::Pos2::new(rect.center().x, rect.center().y + 18.0),
                    egui::Align2::CENTER_CENTER,
                    t("device_photo_placeholder", None),
                    egui::FontId::proportional(11.0),
                    theme::TEXT_MUTE,
                );
            });
        });
        ui.add_space(theme::S4);

        if state.connected {
            // 2-col grid (auto | 1fr) matching the prototype's chip readout.
            egui::Grid::new("flasher_chip_info")
                .num_columns(2)
                .spacing(egui::Vec2::new(theme::S3, 4.0))
                .min_col_width(0.0)
                .show(ui, |ui| {
                    for (k, v) in [
                        (t("device_chip", None), "ESP32-D0WD-V3"),
                        (t("device_rev", None), "v3.0"),
                        (t("device_flash", None), "4 MB"),
                        (t("device_mac", None), "—"),
                    ] {
                        ui.label(RichText::new(k).size(12.0).color(theme::TEXT_MUTE));
                        ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                            ui.label(
                                RichText::new(v)
                                    .monospace()
                                    .size(12.0)
                                    .color(theme::TEXT_DIM),
                            );
                        });
                        ui.end_row();
                    }
                });
        } else {
            // Two-line hint, centered.
            ui.vertical_centered(|ui| {
                for line in t("not_connected_hint", None).split('\n') {
                    ui.label(RichText::new(line).size(12.0).color(theme::TEXT_MUTE));
                }
            });
        }
    });
    studio::section_divider(ui);

    // ── D-pad section ──
    egui::Frame::none().inner_margin(inner_pad).show(ui, |ui| {
        theme::section_heading(ui, t("remote_control", None));
        ui.add_space(theme::S3);
        if let Some(key) = dpad_cluster(ui, state.connected && !state.busy) {
            send_dpad(state, bus_tx, key);
        }
        ui.add_space(theme::S3);
        ui.vertical_centered(|ui| {
            ui.label(
                RichText::new(t("dpad_keyboard_hint", None))
                    .size(11.0)
                    .color(theme::TEXT_MUTE),
            );
        });
    });
    studio::section_divider(ui);

    // ── Operation log section — fills remaining sidebar height ──
    egui::Frame::none().inner_margin(inner_pad).show(ui, |ui| {
        ui.horizontal(|ui| {
            theme::section_heading(ui, t("operation_log", None));
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                ui.label(
                    RichText::new(format!("{} · {}", state.port, state.baud))
                        .monospace()
                        .size(10.5)
                        .color(theme::TEXT_MUTE),
                );
            });
        });
        ui.add_space(theme::S2);
        let remaining = ui.available_height().max(120.0);
        ui.allocate_ui(egui::Vec2::new(ui.available_width(), remaining), |ui| {
            state.log.show(ui);
        });
    });
}

// ─── Step header: circle + title + subtitle ───────────────────────────────

fn step_header(ui: &mut Ui, n: u8, state: ProgressState, title: &str, sub: &str) {
    ui.horizontal_top(|ui| {
        ui.vertical(|ui| {
            step_circle(ui, n, state);
            step_connector(ui, state, 40.0);
        });
        ui.add_space(theme::S4);
        ui.vertical(|ui| {
            // Step titles use the Space Grotesk display family
            // (prototype's `.disp` class).
            let title_text = RichText::new(title)
                .font(theme::display_font(17.0))
                .strong()
                .color(if state == ProgressState::Pending {
                    theme::TEXT_MUTE
                } else {
                    theme::TEXT
                });
            ui.label(title_text);
            ui.label(RichText::new(sub).size(12.5).color(theme::TEXT_MUTE));
            ui.add_space(theme::S3);
        });
    });
}

// ─── Erase + Flash thread spawns (logic preserved from previous flasher) ──

fn start_erase(state: &mut FlasherState, bus_tx: &crate::bus::AppEventTx) {
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
            let mut flasher =
                aio_flasher::Flasher::new(&port, baud).map_err(|e| format!("open/connect: {e}"))?;
            let (op_tx, op_rx) = std::sync::mpsc::channel::<aio_flasher::FlashEvent>();
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

fn start_flash(state: &mut FlasherState, bus_tx: &crate::bus::AppEventTx) {
    let mut parts: Vec<aio_flasher::Partition> = Vec::new();
    let mut slot_map: Vec<usize> = Vec::new();
    let mut io_errors: Vec<String> = Vec::new();
    for (i, p) in state.partitions.iter().enumerate() {
        if !p.enabled || p.path.is_empty() {
            continue;
        }
        match std::fs::read(&p.path) {
            Ok(data) => {
                parts.push(aio_flasher::Partition {
                    address: PARTITION_ADDRESSES[i],
                    data,
                });
                slot_map.push(i);
            }
            Err(e) => io_errors.push(format!("Skipping partition {i} ({}): {e}", p.path)),
        }
    }
    for line in io_errors {
        state.log.push(line);
    }
    if parts.is_empty() {
        state.log.push("No partitions selected with valid paths.");
        return;
    }
    state.busy = true;
    state.last_flash_succeeded = false;
    state.partition_percent = [0.0; 4];
    state.flash_total_bytes = vec![0; parts.len()];
    state.flash_slot_map = slot_map.clone();
    state.active_partition = slot_map.first().copied();
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
            let mut flasher =
                aio_flasher::Flasher::new(&port, baud).map_err(|e| format!("open/connect: {e}"))?;
            let (op_tx, op_rx) = std::sync::mpsc::channel::<aio_flasher::FlashEvent>();
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
