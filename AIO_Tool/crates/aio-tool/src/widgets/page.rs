//! Shared page-chrome helpers — every Studio tab uses the same header
//! shape, group-card frame, and toolbar row, so they live in one module
//! to keep the per-tab code focused on its own logic.
//!
//! Layout match:
//!   PageHeader → prototype's `<PageHeader title sub right={chip}/>`
//!   group_card → prototype's `<div style="background: panel, border, R4">`
//!   section_label → prototype's `<div class="font-700 letter-spacing-08em">`

use egui::{RichText, Ui};

use crate::theme;
use crate::widgets::studio::{self, ChipKind};

/// Render a page header — title + sub-label on the left, optional widget
/// (typically a status chip) on the right, 1px BORDER-bottom underneath.
///
/// `right` runs in a right-to-left layout slot so widgets land flush
/// against the right edge without manual padding math.
pub fn page_header(ui: &mut Ui, title: &str, sub: &str, right: impl FnOnce(&mut Ui)) {
    egui::Frame::none()
        .inner_margin(egui::Margin::symmetric(theme::S6, theme::S5))
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                ui.vertical(|ui| {
                    // Title uses the Space Grotesk display family to
                    // match the prototype's `.disp` class.
                    ui.label(
                        RichText::new(title)
                            .font(theme::display_font(21.0))
                            .strong()
                            .color(theme::TEXT),
                    );
                    ui.label(RichText::new(sub).size(13.0).color(theme::TEXT_MUTE));
                });
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), right);
            });
        });
    studio::section_divider(ui);
}

/// Same as `page_header` but with no right-side slot — convenience for
/// pages that only show a title (e.g. Help).
pub fn page_header_simple(ui: &mut Ui, title: &str, sub: &str) {
    page_header(ui, title, sub, |_| {});
}

/// Group card — the rounded `PANEL` frame the prototype wraps each
/// settings group in. Use under a `section_label` to get the prototype's
/// "tiny-uppercase label + card" pattern.
pub fn group_card(ui: &mut Ui, body: impl FnOnce(&mut Ui)) {
    egui::Frame::none()
        .fill(theme::PANEL)
        .stroke(egui::Stroke::new(1.0, theme::BORDER))
        .rounding(egui::Rounding::same(theme::R4))
        .inner_margin(egui::Margin::same(theme::S4))
        .show(ui, body);
}

/// Tiny uppercase muted heading shown above grouped cards. Same as
/// `theme::section_heading` but kept here for discoverability alongside
/// the rest of the page-chrome helpers.
pub fn section_label(ui: &mut Ui, text: impl Into<String>) {
    theme::section_heading(ui, text);
}

/// Connection-status chip shown in the top-right of pages that connect to
/// the device (Params, File Manager).
pub fn connection_chip(ui: &mut Ui, connected: bool, busy: bool, port: Option<&str>) {
    let (kind, label) = match (connected, busy, port) {
        (true, false, Some(p)) => (
            ChipKind::Live,
            format!("{} · {}", aio_i18n::t("status_connected", None), p),
        ),
        (true, false, None) => (ChipKind::Live, aio_i18n::t("status_connected", None)),
        (false, true, _) => (ChipKind::Busy, aio_i18n::t("connecting", None)),
        _ => (ChipKind::Inactive, aio_i18n::t("status_disconnected", None)),
    };
    studio::status_chip(ui, kind, label);
}

/// Outer-frame around the scrollable body of every tab — establishes the
/// `var(--s6)` (30px) page padding the prototype applies inside the main
/// content area.
pub fn body_frame(ui: &mut Ui, body: impl FnOnce(&mut Ui)) {
    egui::Frame::none()
        .inner_margin(egui::Margin::same(theme::S6))
        .show(ui, body);
}
