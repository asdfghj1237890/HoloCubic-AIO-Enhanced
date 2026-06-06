//! Help tab — render the `help_info` i18n blob with clickable URLs.
//!
//! D13: legacy Python renders the blob as plain text. egui's `hyperlink_to`
//! lets us upgrade URLs to clickable for free; we parse line-by-line and
//! render hyperlinks for lines that look like URLs.

use aio_i18n::t;
use egui::{ScrollArea, Ui};

/// Render the Help tab — scrollable view of the `help_info` i18n blob,
/// with lines starting with `https://` or `http://` (after stripping
/// leading whitespace) rendered as clickable hyperlinks.
pub fn show(ui: &mut Ui) {
    let info = t("help_info", None);
    ScrollArea::vertical()
        .auto_shrink([false; 2])
        .show(ui, |ui| {
            for raw_line in info.lines() {
                let trimmed = raw_line.trim_start();
                if trimmed.starts_with("https://") || trimmed.starts_with("http://") {
                    // Preserve leading whitespace for indented links.
                    let indent: String =
                        raw_line.chars().take_while(|c| c.is_whitespace()).collect();
                    ui.horizontal(|ui| {
                        ui.monospace(&indent);
                        ui.hyperlink(trimmed);
                    });
                } else {
                    ui.monospace(raw_line);
                }
            }
        });
}
