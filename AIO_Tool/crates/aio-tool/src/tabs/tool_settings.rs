//! Tool Settings tab — language selection with live apply.
//!
//! Legacy Python required a restart; egui doesn't — `aio_i18n::t()`
//! resolves per call, so changing the language takes effect on the next
//! frame. We drop both the Apply button and the "restart needed" tip
//! (Plan 9 D7).

use aio_i18n::{config, get_i18n, t, Lang};
use egui::Ui;

/// Tool Settings tab state. Currently only tracks the last save error
/// (if any) for inline display.
#[derive(Default)]
pub struct ToolSettingsState {
    /// `Some(msg)` after a failed save; cleared on the next successful save.
    pub last_save_error: Option<String>,
}

/// Render the Tool Settings tab — title, language tip, and three radio
/// buttons (one per supported locale, using native display names). A
/// radio click immediately applies the new language via `get_i18n()` and
/// persists it via `config::save_language`.
pub fn show(ui: &mut Ui, state: &mut ToolSettingsState) {
    ui.vertical(|ui| {
        ui.heading(t("tool_settings_title", None));
        ui.separator();

        ui.label(t("language_label", None));
        ui.label(t("language_tip", None));

        let current = get_i18n().get_language();
        let mut selected = current;

        for lang in Lang::ALL {
            // Radio label uses the language's native display name so users
            // see "English" / "\u{7b80}\u{4f53}\u{4e2d}\u{6587}" / "\u{7e41}\u{9ad4}\u{4e2d}\u{6587}"
            // regardless of UI locale.
            if ui
                .radio_value(&mut selected, lang, lang.display_name())
                .changed()
                && selected != current
            {
                // Apply immediately (D7).
                get_i18n().set_language(selected);
                // Persist.
                match config::save_language(selected) {
                    Ok(()) => state.last_save_error = None,
                    Err(e) => state.last_save_error = Some(format!("save config: {e}")),
                }
            }
        }

        if let Some(msg) = &state.last_save_error {
            ui.colored_label(egui::Color32::LIGHT_RED, msg);
        }
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
