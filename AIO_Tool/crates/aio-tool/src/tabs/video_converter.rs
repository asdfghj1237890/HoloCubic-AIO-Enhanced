//! Video Converter tab — Studio layout: page header + ffmpeg-status chip,
//! source/output group card, preset/custom card, Convert / Cancel actions,
//! and an operation log. Mirrors `StudioVideo` in `studio-convert.jsx`.

use aio_i18n::t;
use egui::{ComboBox, RichText, ScrollArea, Ui};

use crate::bus::AppEventTx;
use crate::theme;
use crate::video_converter_worker::{self, Job, VideoFormat};
use crate::widgets::operation_log::OperationLog;
use crate::widgets::page;
use crate::widgets::studio::{self, ChipKind};

/// Video Converter tab state.
pub struct VideoConverterState {
    /// Source video path (text-edit-backed).
    pub src: String,
    /// Output directory (text-edit-backed).
    pub dst_dir: String,
    /// Custom-radio: false = Default (fields disabled), true = Custom.
    pub custom: bool,
    /// Output width in px.
    pub width: String,
    /// Output height in px.
    pub height: String,
    /// Frames per second.
    pub fps: String,
    /// ffmpeg `-q:v` value 1-9.
    pub quality: String,
    /// Index into `FORMAT_LABELS`.
    pub format_idx: usize,
    /// `None` = not yet probed; `Some(false)` = absent.
    pub ffmpeg_available: Option<bool>,
    /// True while a conversion job is in flight.
    pub pending: bool,
    /// Cancel flag shared with the worker.
    pub cancel: std::sync::Arc<std::sync::atomic::AtomicBool>,
    /// Scrollable operation log.
    pub log: OperationLog,
}

const FORMAT_LABELS: &[(&str, VideoFormat)] = &[
    ("MJPEG", VideoFormat::Mjpeg),
    ("rgb565be", VideoFormat::Rgb565be),
];

impl Default for VideoConverterState {
    fn default() -> Self {
        let default_dst = std::env::current_dir()
            .map(|p| p.join("OutFile").display().to_string())
            .unwrap_or_else(|_| "OutFile".to_owned());
        Self {
            src: String::new(),
            dst_dir: default_dst,
            custom: false,
            width: "240".to_owned(),
            height: "240".to_owned(),
            fps: "20".to_owned(),
            quality: "5".to_owned(),
            format_idx: 0,
            ffmpeg_available: None,
            pending: false,
            cancel: std::sync::Arc::new(std::sync::atomic::AtomicBool::new(false)),
            log: OperationLog::default(),
        }
    }
}

/// Render the Video Converter tab.
pub fn show(ui: &mut Ui, state: &mut VideoConverterState, bus_tx: &AppEventTx) {
    if state.ffmpeg_available.is_none() {
        state.ffmpeg_available = Some(video_converter_worker::ffmpeg_present());
    }

    page::page_header(
        ui,
        &t("video_title", None),
        &t("video_subtitle", None),
        |ui| {
            let (kind, label) = match state.ffmpeg_available {
                Some(true) => (ChipKind::Live, "ffmpeg ready".to_owned()),
                Some(false) => (ChipKind::Inactive, t("ffmpeg_missing", None)),
                None => (ChipKind::Busy, "ffmpeg checking…".to_owned()),
            };
            studio::status_chip(ui, kind, label);
        },
    );

    ScrollArea::vertical()
        .auto_shrink([false; 2])
        .show(ui, |ui| {
            page::body_frame(ui, |ui| {
                ui.set_max_width(720.0);

                // ── ffmpeg missing banner ──
                if state.ffmpeg_available == Some(false) {
                    egui::Frame::none()
                        .fill(theme::ERR_WEAK)
                        .stroke(egui::Stroke::new(1.0, theme::ERR))
                        .rounding(egui::Rounding::same(theme::R3))
                        .inner_margin(egui::Margin::same(theme::S4))
                        .show(ui, |ui| {
                            ui.horizontal(|ui| {
                                ui.label(
                                    RichText::new(t("ffmpeg_missing", None))
                                        .color(theme::ERR)
                                        .size(13.0),
                                );
                                ui.with_layout(
                                    egui::Layout::right_to_left(egui::Align::Center),
                                    |ui| {
                                        if theme::ghost_button(ui, t("recheck", None)).clicked() {
                                            state.ffmpeg_available = None;
                                        }
                                        ui.hyperlink_to(
                                            t("install_ffmpeg", None),
                                            "https://ffmpeg.org/download.html",
                                        );
                                    },
                                );
                            });
                        });
                    ui.add_space(theme::S4);
                }

                // ── Source and output ──
                page::section_label(ui, t("video_source_output", None));
                ui.add_space(theme::S2);
                page::group_card(ui, |ui| {
                    settings_row(ui, "Source", |ui| {
                        ui.add(egui::TextEdit::singleline(&mut state.src).desired_width(280.0));
                        if theme::ghost_button(ui, t("select_video", None)).clicked() {
                            if let Some(p) = rfd::FileDialog::new()
                                .add_filter(
                                    t("common_formats", None),
                                    &["mp4", "MP4", "avi", "AVI", "mov", "MOV", "gif", "GIF"],
                                )
                                .pick_file()
                            {
                                state.src = p.display().to_string();
                            }
                        }
                    });
                    settings_row(ui, "Output dir", |ui| {
                        ui.add(egui::TextEdit::singleline(&mut state.dst_dir).desired_width(280.0));
                        if theme::ghost_button(ui, t("output_path", None)).clicked() {
                            if let Some(p) = rfd::FileDialog::new().pick_folder() {
                                state.dst_dir = p.display().to_string();
                            }
                        }
                    });
                });

                ui.add_space(theme::S4);

                // ── Output settings ──
                page::section_label(ui, t("output_settings", None));
                ui.add_space(theme::S2);
                page::group_card(ui, |ui| {
                    // Preset / custom pill toggle.
                    ui.horizontal(|ui| {
                        for (val, label) in [
                            (false, t("default_option", None)),
                            (true, t("custom_option", None)),
                        ] {
                            let active = state.custom == val;
                            let fill = if active {
                                theme::ACCENT
                            } else {
                                theme::PANEL_3
                            };
                            let fg = if active {
                                theme::ACCENT_INK
                            } else {
                                theme::TEXT_MUTE
                            };
                            if ui
                                .add(
                                    egui::Button::new(
                                        RichText::new(label).size(12.5).color(fg).strong(),
                                    )
                                    .fill(fill)
                                    .rounding(egui::Rounding::same(theme::RPILL))
                                    .min_size(egui::Vec2::new(0.0, 24.0)),
                                )
                                .clicked()
                            {
                                state.custom = val;
                            }
                        }
                    });
                    ui.add_space(theme::S3);

                    ui.add_enabled_ui(state.custom, |ui| {
                        settings_row(ui, &t("resolution", None), |ui| {
                            ui.add(
                                egui::TextEdit::singleline(&mut state.width).desired_width(64.0),
                            );
                            ui.label("×");
                            ui.add(
                                egui::TextEdit::singleline(&mut state.height).desired_width(64.0),
                            );
                        });
                        settings_row(ui, &t("fps", None), |ui| {
                            ui.add(egui::TextEdit::singleline(&mut state.fps).desired_width(80.0));
                        });
                        settings_row(ui, &t("quality", None), |ui| {
                            ComboBox::from_id_salt("vid_quality")
                                .selected_text(&state.quality)
                                .show_ui(ui, |ui| {
                                    for q in ["1", "2", "3", "4", "5", "6", "7", "8", "9"] {
                                        ui.selectable_value(&mut state.quality, q.to_owned(), q);
                                    }
                                });
                        });
                        settings_row(ui, &t("format", None), |ui| {
                            ComboBox::from_id_salt("vid_format")
                                .selected_text(FORMAT_LABELS[state.format_idx].0)
                                .show_ui(ui, |ui| {
                                    for (i, (label, _)) in FORMAT_LABELS.iter().enumerate() {
                                        ui.selectable_value(&mut state.format_idx, i, *label);
                                    }
                                });
                        });
                    });
                });

                ui.add_space(theme::S5);

                // ── Convert / cancel actions ──
                ui.horizontal(|ui| {
                    let can_start = !state.pending
                        && state.ffmpeg_available == Some(true)
                        && !state.src.is_empty();
                    if state.pending {
                        if theme::danger_button(ui, t("cancel_button", Some("Cancel"))).clicked() {
                            state
                                .cancel
                                .store(true, std::sync::atomic::Ordering::Relaxed);
                            state.log.push("Cancellation requested...");
                        }
                    } else if theme::primary_button(ui, t("start_conversion", None)).clicked()
                        && can_start
                    {
                        start_convert(state, bus_tx);
                    }
                });

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

fn settings_row(ui: &mut Ui, label: &str, control: impl FnOnce(&mut Ui)) {
    ui.horizontal(|ui| {
        ui.scope(|ui| {
            ui.set_width(120.0);
            ui.label(RichText::new(label).size(13.0).color(theme::TEXT_DIM));
        });
        control(ui);
    });
    ui.add_space(theme::S2);
}

fn start_convert(state: &mut VideoConverterState, bus_tx: &AppEventTx) {
    let width: u32 = state.width.parse().unwrap_or(240);
    let height: u32 = state.height.parse().unwrap_or(240);
    let fps: u32 = state.fps.parse().unwrap_or(20);
    let quality: u32 = state.quality.parse().unwrap_or(5);
    let format = FORMAT_LABELS[state.format_idx].1;
    let dst_dir = std::path::PathBuf::from(&state.dst_dir);
    let cache_dir = dst_dir.join("Cache");
    let _ = std::fs::create_dir_all(&dst_dir);
    let job = Job {
        src: std::path::PathBuf::from(&state.src),
        dst_dir,
        width,
        height,
        fps,
        quality,
        format,
        cache_dir,
    };
    state
        .cancel
        .store(false, std::sync::atomic::Ordering::Relaxed);
    state.pending = true;
    state.log.push(format!(
        "\u{2192} converting {} \u{2192} {}x{} {:?} q={}",
        state.src, width, height, format, quality
    ));
    video_converter_worker::spawn(job, bus_tx.clone(), state.cancel.clone());
}
