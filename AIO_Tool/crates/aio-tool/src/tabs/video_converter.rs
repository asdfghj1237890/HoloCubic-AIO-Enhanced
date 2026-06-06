//! Video Converter tab — file picker + output settings + ffmpeg subprocess.
//!
//! Q4 design decision: detect ffmpeg up-front and show a friendly error
//! when missing, instead of silently failing when the user clicks Start.

use aio_i18n::t;
use egui::{ComboBox, Ui};

use crate::bus::AppEventTx;
use crate::video_converter_worker::{self, Job, VideoFormat};
use crate::widgets::operation_log::OperationLog;

/// Video Converter tab state.
pub struct VideoConverterState {
    /// Source video path (text-edit-backed).
    pub src: String,
    /// Output directory (text-edit-backed). Defaults to `<cwd>/OutFile` to
    /// match the legacy Python tool (common.py:39).
    pub dst_dir: String,
    /// Custom-radio: false = Default (fields disabled), true = Custom.
    pub custom: bool,
    /// Output width in px (string-backed for the text field).
    pub width: String,
    /// Output height in px (string-backed for the text field).
    pub height: String,
    /// Frames per second (string-backed for the text field).
    pub fps: String,
    /// ffmpeg `-q:v` value 1-9 (string-backed for the combo box).
    pub quality: String,
    /// Index into `FORMAT_LABELS` — 0 = MJPEG, 1 = rgb565be.
    pub format_idx: usize,
    /// `None` = not yet probed; `Some(false)` = absent.
    pub ffmpeg_available: Option<bool>,
    /// True while a conversion job is in flight. Gates Start / Cancel buttons.
    pub pending: bool,
    /// Cancel flag shared with the worker. Reset before each spawn.
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
        // Default output dir: <cwd>/OutFile to match the Python tool's
        // default ROOT_PATH (common.py:39).
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
    // Lazy probe on first render.
    if state.ffmpeg_available.is_none() {
        state.ffmpeg_available = Some(video_converter_worker::ffmpeg_present());
    }

    ui.vertical(|ui| {
        // --- ffmpeg detection banner --------------------------------------
        if state.ffmpeg_available == Some(false) {
            ui.horizontal(|ui| {
                ui.colored_label(egui::Color32::LIGHT_RED, t("ffmpeg_missing", None));
                ui.hyperlink_to(
                    t("install_ffmpeg", None),
                    "https://ffmpeg.org/download.html",
                );
                if ui.button(t("recheck", None)).clicked() {
                    state.ffmpeg_available = None; // re-probe next frame
                }
            });
            ui.separator();
        }

        // --- Path row -----------------------------------------------------
        ui.horizontal(|ui| {
            ui.label("Source:");
            ui.text_edit_singleline(&mut state.src);
            if ui.button(t("select_video", None)).clicked() {
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
        ui.horizontal(|ui| {
            ui.label("Output dir:");
            ui.text_edit_singleline(&mut state.dst_dir);
            if ui.button(t("output_path", None)).clicked() {
                if let Some(p) = rfd::FileDialog::new().pick_folder() {
                    state.dst_dir = p.display().to_string();
                }
            }
        });

        ui.separator();

        // --- Output settings panel ---------------------------------------
        ui.heading(t("output_settings", None));
        ui.horizontal(|ui| {
            ui.radio_value(&mut state.custom, false, t("default_option", None));
            ui.radio_value(&mut state.custom, true, t("custom_option", None));
        });
        ui.add_enabled_ui(state.custom, |ui| {
            ui.horizontal(|ui| {
                ui.label(t("resolution", None));
                ui.text_edit_singleline(&mut state.width);
                ui.text_edit_singleline(&mut state.height);
            });
            ui.horizontal(|ui| {
                ui.label(t("fps", None));
                ui.text_edit_singleline(&mut state.fps);
                ui.label(t("quality", None));
                ComboBox::from_id_salt("vid_quality")
                    .selected_text(&state.quality)
                    .show_ui(ui, |ui| {
                        for q in ["1", "2", "3", "4", "5", "6", "7", "8", "9"] {
                            ui.selectable_value(&mut state.quality, q.to_owned(), q);
                        }
                    });
            });
            ui.horizontal(|ui| {
                ui.label(t("format", None));
                ComboBox::from_id_salt("vid_format")
                    .selected_text(FORMAT_LABELS[state.format_idx].0)
                    .show_ui(ui, |ui| {
                        for (i, (label, _)) in FORMAT_LABELS.iter().enumerate() {
                            ui.selectable_value(&mut state.format_idx, i, *label);
                        }
                    });
            });
        });

        ui.separator();

        // --- Convert / cancel buttons ------------------------------------
        ui.horizontal(|ui| {
            let can_start =
                !state.pending && state.ffmpeg_available == Some(true) && !state.src.is_empty();
            if ui
                .add_enabled(can_start, egui::Button::new(t("start_conversion", None)))
                .clicked()
            {
                // Parse numeric fields with safe fallbacks (D8 discretionary:
                // no popup for invalid input — just use defaults).
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
            if state.pending
                && ui
                    .add(egui::Button::new(t("cancel_button", Some("Cancel"))))
                    .clicked()
            {
                state
                    .cancel
                    .store(true, std::sync::atomic::Ordering::Relaxed);
                state.log.push("Cancellation requested...");
            }
        });

        ui.separator();
        ui.heading(t("operation_log", None));
        state.log.show(ui);
    });
}
