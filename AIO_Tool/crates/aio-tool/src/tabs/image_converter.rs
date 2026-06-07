//! Image Converter tab — Studio layout: page header + format toolbar +
//! input-list card + Convert / Cancel actions + operation log.

use aio_converter::ColorFormat;
use aio_i18n::t;
use egui::{ComboBox, RichText, ScrollArea, Ui};

use crate::bus::AppEventTx;
use crate::image_converter_worker::{self, Job, Output};
use crate::theme;
use crate::widgets::operation_log::OperationLog;
use crate::widgets::page;

/// All format choices the dropdown offers, in render order.
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
    /// Number of images we've started encoding in this batch.
    pub pending: usize,
    /// Cancel flag shared across all in-flight encodes in a batch.
    pub cancel: std::sync::Arc<std::sync::atomic::AtomicBool>,
    /// Scrollable operation log.
    pub log: OperationLog,
    /// Most-recent `Output::Bin` save target queue.
    pub save_targets: std::collections::VecDeque<std::path::PathBuf>,
}

impl Default for ImageConverterState {
    fn default() -> Self {
        Self {
            inputs: Vec::new(),
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
    page::page_header(
        ui,
        &t("image_title", None),
        &t("image_subtitle", None),
        |ui| {
            let label = if state.inputs.is_empty() {
                t("image_no_files", None)
            } else {
                format!("{} files", state.inputs.len())
            };
            ui.label(RichText::new(label).size(12.5).color(theme::TEXT_DIM));
        },
    );

    // Toolbar — format select + flags.
    egui::Frame::none()
        .inner_margin(egui::Margin::symmetric(theme::S6, theme::S3))
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                ui.label(
                    RichText::new(t("color_format", None))
                        .size(12.0)
                        .color(theme::TEXT_MUTE),
                );
                ComboBox::from_id_salt("img_format")
                    .selected_text(FORMATS[state.format_idx].0)
                    .show_ui(ui, |ui| {
                        for (i, (label, _)) in FORMATS.iter().enumerate() {
                            ui.selectable_value(&mut state.format_idx, i, *label);
                        }
                    });
                ui.checkbox(&mut state.c_array, "C-array (.c)");
                ui.checkbox(&mut state.dither, "Dither (RGB)");

                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    let busy = state.pending > 0;
                    if busy {
                        if theme::danger_button(ui, t("cancel_button", Some("Cancel"))).clicked() {
                            state
                                .cancel
                                .store(true, std::sync::atomic::Ordering::Relaxed);
                            state.log.push("Cancellation requested...");
                        }
                    } else if theme::primary_button(ui, t("start_convert", None)).clicked()
                        && !state.inputs.is_empty()
                    {
                        start_convert(state, bus_tx);
                    }
                });
            });
        });

    crate::widgets::studio::section_divider(ui);

    ScrollArea::vertical()
        .auto_shrink([false; 2])
        .show(ui, |ui| {
            page::body_frame(ui, |ui| {
                ui.set_max_width(860.0);

                // Drop / select zone — top of the body.
                let response = egui::Frame::none()
                    .fill(theme::PANEL)
                    .stroke(egui::Stroke::new(1.5, theme::BORDER_STRONG))
                    .rounding(egui::Rounding::same(theme::R4))
                    .inner_margin(egui::Margin::same(theme::S5))
                    .show(ui, |ui| {
                        ui.vertical_centered(|ui| {
                            ui.label(RichText::new("↑").size(20.0).color(theme::TEXT_MUTE));
                            ui.label(
                                RichText::new(t("image_drop_hint", None))
                                    .size(13.0)
                                    .color(theme::TEXT_MUTE),
                            );
                        });
                    });
                if response.response.interact(egui::Sense::click()).clicked() {
                    if let Some(paths) = rfd::FileDialog::new()
                        .add_filter(t("image_files", None), &["png", "jpg", "jpeg", "bmp"])
                        .pick_files()
                    {
                        state.inputs = paths;
                    }
                }
                ui.add_space(theme::S4);

                // Selected-files card.
                if state.inputs.is_empty() {
                    ui.vertical_centered(|ui| {
                        ui.label(
                            RichText::new(t("image_no_files", None))
                                .size(13.0)
                                .color(theme::TEXT_MUTE),
                        );
                    });
                } else {
                    page::section_label(ui, t("image_inputs_label", None));
                    ui.add_space(theme::S2);
                    page::group_card(ui, |ui| {
                        for p in &state.inputs {
                            ui.label(
                                RichText::new(p.display().to_string())
                                    .monospace()
                                    .size(12.0)
                                    .color(theme::TEXT_DIM),
                            );
                        }
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

fn start_convert(state: &mut ImageConverterState, bus_tx: &AppEventTx) {
    state
        .cancel
        .store(false, std::sync::atomic::Ordering::Relaxed);
    let (_, fmt) = FORMATS[state.format_idx];
    let dither = state.dither;
    let c_array = state.c_array;
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
        image_converter_worker::spawn(job, bus_tx.clone(), state.cancel.clone());
        state.log.push(format!(
            "\u{2192} converting {} \u{2192} {}",
            input.display(),
            save_to.display()
        ));
    }
}
