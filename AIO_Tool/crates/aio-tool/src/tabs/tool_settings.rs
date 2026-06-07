//! Tool Settings tab — Studio layout: page header + grouped "Appearance"
//! card containing a pill-toggle language switcher. Mirrors `StudioSettings`
//! in `dir-a-pro.jsx` (we don't expose accent/font swatches in the Rust
//! app — those would need new app state, separate plan).

use aio_i18n::{config, get_i18n, t, Lang};
use egui::{RichText, ScrollArea, Stroke, Ui};

use crate::theme;
use crate::widgets::page;

/// Tool Settings tab state. Currently only tracks the last save error
/// (if any) for inline display.
#[derive(Default)]
pub struct ToolSettingsState {
    /// `Some(msg)` after a failed save; cleared on the next successful save.
    pub last_save_error: Option<String>,
}

/// Render the Tool Settings tab.
pub fn show(ui: &mut Ui, state: &mut ToolSettingsState) {
    page::page_header_simple(
        ui,
        &t("tool_settings_title", None),
        &t("tool_settings_subtitle", None),
    );

    ScrollArea::vertical()
        .auto_shrink([false; 2])
        .show(ui, |ui| {
            page::body_frame(ui, |ui| {
                ui.set_max_width(640.0);

                page::section_label(ui, t("appearance_label", None));
                ui.add_space(theme::S2);

                page::group_card(ui, |ui| {
                    setting_row(
                        ui,
                        &t("language_label", None),
                        &t("language_sub", None),
                        |ui| {
                            language_toggle(ui, state);
                        },
                    );
                });

                ui.add_space(theme::S4);
                ui.label(
                    RichText::new(t("appearance_footnote", None))
                        .size(11.5)
                        .color(theme::TEXT_MUTE),
                );

                if let Some(msg) = &state.last_save_error {
                    ui.add_space(theme::S3);
                    ui.label(RichText::new(msg).color(theme::ERR));
                }
            });
        });
}

/// A single labeled row inside the group card. Label + sub on the left,
/// the supplied control on the right.
fn setting_row(ui: &mut Ui, label: &str, sub: &str, control: impl FnOnce(&mut Ui)) {
    ui.horizontal(|ui| {
        ui.vertical(|ui| {
            ui.label(RichText::new(label).strong().size(13.5).color(theme::TEXT));
            ui.label(RichText::new(sub).size(12.0).color(theme::TEXT_MUTE));
        });
        ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), control);
    });
    ui.add_space(theme::S3);
}

/// Pill-toggle language switcher — three buttons inside a pill-shaped
/// PANEL_3 background, the active one filled with ACCENT.
fn language_toggle(ui: &mut Ui, state: &mut ToolSettingsState) {
    let current = get_i18n().get_language();
    egui::Frame::none()
        .fill(theme::PANEL_3)
        .rounding(egui::Rounding::same(theme::RPILL))
        .inner_margin(egui::Margin::same(2.0))
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                ui.spacing_mut().item_spacing.x = 2.0;
                for lang in Lang::ALL {
                    let active = lang == current;
                    let label_color = if active {
                        theme::ACCENT_INK
                    } else {
                        theme::TEXT_MUTE
                    };
                    let fill = if active {
                        theme::ACCENT
                    } else {
                        egui::Color32::TRANSPARENT
                    };
                    let btn = egui::Button::new(
                        RichText::new(lang.display_name())
                            .strong()
                            .size(12.5)
                            .color(label_color),
                    )
                    .fill(fill)
                    .stroke(Stroke::new(0.0, theme::ACCENT))
                    .rounding(egui::Rounding::same(theme::RPILL))
                    .min_size(egui::Vec2::new(0.0, 24.0));
                    if ui.add(btn).clicked() && !active {
                        get_i18n().set_language(lang);
                        match config::save_language(lang) {
                            Ok(()) => state.last_save_error = None,
                            Err(e) => state.last_save_error = Some(format!("save config: {e}")),
                        }
                    }
                }
            });
        });
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_state_has_no_error() {
        let s = ToolSettingsState::default();
        assert!(s.last_save_error.is_none());
    }
}
