//! Image Converter tab — file picker + format dropdown + Convert button.
//!
//! Uses `aio_converter::Converter` via the one-shot worker in
//! `image_converter_worker`. UI is intentionally simpler than the legacy
//! Python tool's two-dropdown system (which only used the Color Format
//! dropdown on the C_array path — see Plan 9 D1 / D4).

use aio_converter::ColorFormat;
use aio_i18n::t;
use egui::{ComboBox, Ui};

use crate::bus::AppEventTx;
use crate::image_converter_worker::{self, Job, Output};
use crate::widgets::operation_log::OperationLog;

/// All format choices the dropdown offers, in render order. Label is the
/// user-facing string; `fmt` is the matching enum variant.
const FORMATS: &[(&str, ColorFormat)] = &[
    ("Binary RGB332", ColorFormat::Rgb332),
    ("Binary RGB565", ColorFormat::Rgb565),
    ("Binary RGB565_SWAP", ColorFormat::Rgb565Swap),
    ("Binary RGB888", ColorFormat::Rgb888),
    ("Binary Alpha 1-bit", ColorFormat::Alpha1),
    ("Binary Alpha 2-bit", ColorFormat::Alpha2),
    ("Binary Alpha 4-bit", ColorFormat::Alpha4),
    ("Binary Alpha 8-bit", ColorFormat::Alpha8),
    ("Binary Indexed 1-bit", ColorFormat::Indexed1),
    ("Binary Indexed 2-bit", ColorFormat::Indexed2),
    ("Binary Indexed 4-bit", ColorFormat::Indexed4),
    ("Binary Indexed 8-bit", ColorFormat::Indexed8),
];

/// Image Converter tab state.
pub struct ImageConverterState {
    /// Input file paths (one entry per file the user picked).
    pub inputs: Vec<std::path::PathBuf>,
    /// Currently selected pixel format index into `FORMATS`.
    pub format_idx: usize,
    /// Floyd-Steinberg dither on RGB encoders.
    pub dither: bool,
    /// If true, emit `.c` instead of `.bin`.
    pub c_array: bool,
    /// Number of images we've started encoding in this batch; counts down
    /// each `ConvertFinished`. Used to gate the "Start Convert" button.
    pub pending: usize,
    /// Cancel flag shared with the worker(s).
    pub cancel: std::sync::Arc<std::sync::atomic::AtomicBool>,
    /// Scrollable operation log.
    pub log: OperationLog,
    /// Most-recent `Output::Bin` save target — set when we kick off a job
    /// so the `ConvertFinished` handler knows where to write the bytes.
    /// Queue, FIFO: one entry per pending job.
    pub save_targets: std::collections::VecDeque<std::path::PathBuf>,
}

impl Default for ImageConverterState {
    fn default() -> Self {
        Self {
            inputs: Vec::new(),
            // Default to RGB565 — by far the most common HoloCubic format.
            format_idx: 1,
            dither: false,
            c_array: false,
            pending: 0,
            cancel: std::sync::Arc::new(std::sync::atomic::AtomicBool::new(false)),
            log: OperationLog::default(),
            save_targets: std::collections::VecDeque::new(),
        }
    }
}

/// Render the Image Converter tab.
pub fn show(ui: &mut Ui, state: &mut ImageConverterState, bus_tx: &AppEventTx) {
    ui.vertical(|ui| {
        // --- Settings row -------------------------------------------------
        ui.horizontal(|ui| {
            ui.label(t("color_format", None));
            ComboBox::from_id_salt("img_format")
                .selected_text(FORMATS[state.format_idx].0)
                .show_ui(ui, |ui| {
                    for (i, (label, _)) in FORMATS.iter().enumerate() {
                        ui.selectable_value(&mut state.format_idx, i, *label);
                    }
                });
            ui.checkbox(&mut state.c_array, "C-array (.c)");
            ui.checkbox(&mut state.dither, "Dither (RGB)");
        });

        ui.separator();

        // --- Input file list ---------------------------------------------
        ui.horizontal(|ui| {
            if ui.button(t("select_button", None)).clicked() {
                if let Some(paths) = rfd::FileDialog::new()
                    .add_filter(t("image_files", None), &["png", "jpg", "jpeg", "bmp"])
                    .pick_files()
                {
                    state.inputs = paths;
                }
            }
            ui.label(format!("{} file(s) selected", state.inputs.len()));
        });

        ui.indent("img_inputs", |ui| {
            for p in &state.inputs {
                ui.monospace(p.display().to_string());
            }
        });

        ui.separator();

        // --- Convert button + status --------------------------------------
        ui.horizontal(|ui| {
            let busy = state.pending > 0;
            let can_start = !busy && !state.inputs.is_empty();
            if ui
                .add_enabled(can_start, egui::Button::new(t("start_convert", None)))
                .clicked()
            {
                state
                    .cancel
                    .store(false, std::sync::atomic::Ordering::Relaxed);
                let (_, fmt) = FORMATS[state.format_idx];
                let dither = state.dither;
                let c_array = state.c_array;
                // Clone inputs to release the borrow before the inner
                // rfd::FileDialog loop mutates `state` (push_back / pending).
                for input in state.inputs.clone() {
                    let stem = input
                        .file_stem()
                        .and_then(|s| s.to_str())
                        .unwrap_or("image");
                    let ext = if c_array { "c" } else { "bin" };
                    let default_name = format!("{stem}.{ext}");
                    let Some(save_to) = rfd::FileDialog::new()
                        .set_file_name(&default_name)
                        .save_file()
                    else {
                        state
                            .log
                            .push(format!("Save cancelled for {}.", input.display()));
                        continue;
                    };
                    let output = if c_array {
                        Output::CArray {
                            ident: crate::image_converter_worker::sanitize_c_identifier(stem),
                        }
                    } else {
                        Output::Bin
                    };
                    state.save_targets.push_back(save_to.clone());
                    state.pending += 1;
                    let job = Job {
                        input_path: input.clone(),
                        format: fmt,
                        dither,
                        output,
                    };
                    let _ = image_converter_worker::spawn(job, bus_tx.clone());
                    state.log.push(format!(
                        "\u{2192} converting {} \u{2192} {}",
                        input.display(),
                        save_to.display()
                    ));
                }
            }
            if busy
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
