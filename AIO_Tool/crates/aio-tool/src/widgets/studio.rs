//! Custom Studio-design widgets: step circles, status chips, partition
//! checklist rows, and the D-pad cluster.
//!
//! These are the pieces the prototype's Flasher screen leans on — none have
//! direct equivalents in stock egui, so they paint directly with the
//! `Painter` API onto reserved regions.

use egui::{
    Align, Color32, FontId, Layout, Pos2, Response, RichText, Rounding, Sense, Stroke, Ui, Vec2,
};

use crate::theme::{
    self, ACCENT, ACCENT_INK, ACCENT_WEAK, BORDER, INSET, OK, PANEL_2, PANEL_3, S2, S3, S4, TEXT,
    TEXT_DIM, TEXT_MUTE,
};

// ─── Step circle ──────────────────────────────────────────────────────────

/// Draw a numbered step circle (Pending = grey N, Active = accent N with
/// halo, Done = green ✓). Returns the response so callers can lay out
/// content alongside it.
pub fn step_circle(ui: &mut Ui, n: u8, state: theme::ProgressState) -> Response {
    let diameter = 34.0;
    let (rect, response) = ui.allocate_exact_size(Vec2::new(diameter, diameter), Sense::hover());
    let painter = ui.painter();
    let (fill, fg) = match state {
        theme::ProgressState::Done => (OK, ACCENT_INK),
        theme::ProgressState::Active => (ACCENT, ACCENT_INK),
        theme::ProgressState::Pending => (PANEL_3, TEXT_MUTE),
    };
    if state == theme::ProgressState::Active {
        // Subtle outer halo so the active step pops.
        painter.circle_filled(rect.center(), diameter / 2.0 + 4.0, ACCENT_WEAK);
    }
    painter.circle_filled(rect.center(), diameter / 2.0, fill);
    let glyph = if state == theme::ProgressState::Done {
        "✓".to_string()
    } else {
        n.to_string()
    };
    painter.text(
        rect.center(),
        egui::Align2::CENTER_CENTER,
        glyph,
        FontId::proportional(15.0),
        fg,
    );
    response
}

/// Paint the vertical connector line between two step circles. Caller is
/// responsible for reserving the slim column it lives in.
pub fn step_connector(ui: &mut Ui, prev_state: theme::ProgressState, height: f32) {
    let (rect, _) = ui.allocate_exact_size(Vec2::new(2.0, height), Sense::hover());
    let color = if prev_state == theme::ProgressState::Done {
        OK
    } else {
        BORDER
    };
    ui.painter().rect_filled(rect, Rounding::same(1.0), color);
}

// ─── Status chip ──────────────────────────────────────────────────────────

/// Connection / busy status — drives the dot + label color in `status_chip`.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum ChipKind {
    /// Greyed dot — "Not connected".
    Inactive,
    /// Accent-colored busy dot — "Connecting / Flashing".
    Busy,
    /// Green live dot — "Connected".
    Live,
}

/// Pill-shaped status indicator: filled dot + label. Mirrors the
/// `<span class="chip"><span class="dot live" /> 已連線</span>` pattern
/// from the prototype.
pub fn status_chip(ui: &mut Ui, kind: ChipKind, label: impl Into<String>) -> Response {
    let label = label.into();
    let font = FontId::proportional(12.5);
    let galley = ui
        .painter()
        .layout_no_wrap(label.clone(), font.clone(), TEXT_DIM);
    let dot_d = 7.0;
    let pad_x = 14.0;
    let pad_y = 6.0;
    let width = pad_x + dot_d + 8.0 + galley.size().x + pad_x;
    let height = galley.size().y + pad_y * 2.0;
    let (rect, response) = ui.allocate_exact_size(Vec2::new(width, height), Sense::hover());
    let painter = ui.painter();
    painter.rect(
        rect,
        Rounding::same(height / 2.0),
        PANEL_2,
        Stroke::new(1.0, BORDER),
    );
    let (dot_color, text_color) = match kind {
        ChipKind::Live => (OK, TEXT),
        ChipKind::Busy => (ACCENT, TEXT),
        ChipKind::Inactive => (TEXT_MUTE, TEXT_DIM),
    };
    let dot_center = Pos2::new(rect.left() + pad_x + dot_d / 2.0, rect.center().y);
    painter.circle_filled(dot_center, dot_d / 2.0, dot_color);
    painter.text(
        Pos2::new(dot_center.x + dot_d / 2.0 + 8.0, rect.center().y),
        egui::Align2::LEFT_CENTER,
        label,
        font,
        text_color,
    );
    response
}

// ─── Partition checklist row ──────────────────────────────────────────────

/// One row of the per-partition flashing checklist. Shown in step 3 after
/// flashing kicks off.
pub fn partition_row(
    ui: &mut Ui,
    addr: u32,
    file: &str,
    percent: f32,
    state: theme::ProgressState,
) {
    let row_h = 32.0;
    let bg = if state == theme::ProgressState::Active {
        ACCENT_WEAK
    } else {
        Color32::TRANSPARENT
    };
    egui::Frame::none()
        .fill(bg)
        .inner_margin(egui::Margin::symmetric(S4, S2))
        .show(ui, |ui| {
            ui.set_min_height(row_h);
            ui.horizontal_centered(|ui| {
                // Status bullet (18px circle).
                let (bullet_rect, _) = ui.allocate_exact_size(Vec2::splat(18.0), Sense::hover());
                let p = ui.painter();
                let (fill, glyph) = match state {
                    theme::ProgressState::Done => (OK, "✓".to_string()),
                    theme::ProgressState::Active => (ACCENT, "…".to_string()),
                    theme::ProgressState::Pending => (PANEL_3, "·".to_string()),
                };
                p.circle_filled(bullet_rect.center(), 9.0, fill);
                p.text(
                    bullet_rect.center(),
                    egui::Align2::CENTER_CENTER,
                    glyph,
                    FontId::proportional(11.0),
                    ACCENT_INK,
                );

                ui.add_space(S3);
                ui.label(
                    RichText::new(format!("0x{:05x}", addr))
                        .monospace()
                        .color(ACCENT)
                        .size(12.0),
                );
                ui.add_space(S3);
                ui.with_layout(Layout::left_to_right(Align::Center), |ui| {
                    ui.set_width(ui.available_width() - 56.0);
                    ui.label(RichText::new(file).color(TEXT_DIM).size(12.0));
                });
                let pct_color = if state == theme::ProgressState::Active {
                    ACCENT
                } else {
                    TEXT_MUTE
                };
                ui.with_layout(Layout::right_to_left(Align::Center), |ui| {
                    ui.label(
                        RichText::new(format!("{:>3.0}%", percent))
                            .monospace()
                            .color(pct_color)
                            .size(12.0),
                    );
                });
            });
        });
}

// ─── D-pad cluster ────────────────────────────────────────────────────────

/// One of the five remote-control buttons on the D-pad. The flasher tab
/// turns each one into a 2-byte serial send.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum DpadKey {
    /// Up arrow.
    Up,
    /// Left arrow.
    Left,
    /// OK / confirm — the centre button.
    Ok,
    /// Right arrow.
    Right,
    /// Home — bottom button.
    Home,
}

/// Render the 3x3 D-pad cluster. Returns `Some(key)` when the user
/// presses a key this frame; `None` otherwise. `enabled = false` greys the
/// whole cluster out and disables interaction.
pub fn dpad_cluster(ui: &mut Ui, enabled: bool) -> Option<DpadKey> {
    let mut pressed: Option<DpadKey> = None;
    let cell = 54.0;
    let gap = S2;
    let total = cell * 3.0 + gap * 2.0;
    let cluster_height = cell * 3.0 + gap * 2.0;

    // Center the cluster horizontally inside the available width by
    // padding each side equally via a Frame's `inner_margin`. Computing
    // padding from `ui.available_width()` proved unreliable — the value
    // sometimes included content that pushed the inner column right —
    // so we measure the parent's clip rect instead.
    let outer_width = ui.clip_rect().width().min(ui.available_width());
    let pad_each = ((outer_width - total) / 2.0).max(0.0) as i8;
    egui::Frame::none()
        .inner_margin(egui::Margin::symmetric(pad_each, 0))
        .show(ui, |ui| {
            ui.vertical(|ui| {
            // Up row: invisible left cell — Up — invisible right cell.
            ui.horizontal(|ui| {
                ui.allocate_space(Vec2::new(cell, cell));
                ui.add_space(gap);
                if dpad_key(
                    ui,
                    DpadGlyph::Icon(crate::widgets::icons::Icon::Up),
                    cell,
                    false,
                    enabled,
                )
                .clicked()
                {
                    pressed = Some(DpadKey::Up);
                }
                ui.add_space(gap);
                ui.allocate_space(Vec2::new(cell, cell));
            });
            ui.add_space(gap);

            // Middle row: Left — OK — Right.
            ui.horizontal(|ui| {
                if dpad_key(
                    ui,
                    DpadGlyph::Icon(crate::widgets::icons::Icon::Left),
                    cell,
                    false,
                    enabled,
                )
                .clicked()
                {
                    pressed = Some(DpadKey::Left);
                }
                ui.add_space(gap);
                if dpad_key(ui, DpadGlyph::Ok, cell, true, enabled).clicked() {
                    pressed = Some(DpadKey::Ok);
                }
                ui.add_space(gap);
                if dpad_key(
                    ui,
                    DpadGlyph::Icon(crate::widgets::icons::Icon::Right),
                    cell,
                    false,
                    enabled,
                )
                .clicked()
                {
                    pressed = Some(DpadKey::Right);
                }
            });
            ui.add_space(gap);

            // Bottom row: invisible left cell — Home — invisible right cell.
            ui.horizontal(|ui| {
                ui.allocate_space(Vec2::new(cell, cell));
                ui.add_space(gap);
                if dpad_key(
                    ui,
                    DpadGlyph::Icon(crate::widgets::icons::Icon::Home),
                    cell,
                    false,
                    enabled,
                )
                .clicked()
                {
                    pressed = Some(DpadKey::Home);
                }
                ui.add_space(gap);
                ui.allocate_space(Vec2::new(cell, cell));
            });
        }); // ui.vertical
    }); // egui::Frame::none()
    pressed
}

/// What goes in the middle of a D-pad key — either a stroke icon or the
/// "OK" text label (which the prototype renders in Space Grotesk).
enum DpadGlyph {
    Icon(crate::widgets::icons::Icon),
    Ok,
}

/// Paint a single key in the cluster. The OK key (`accent = true`) gets the
/// accent ring + fill at all times; arrows get a neutral panel surface.
///
/// `cell_size` is the grid cell allocated (54 in the prototype); arrows
/// paint a smaller 48×48 visual inside their cell, matching the
/// prototype's `width: big ? 54 : 48`.
fn dpad_key(
    ui: &mut Ui,
    glyph: DpadGlyph,
    cell_size: f32,
    accent: bool,
    enabled: bool,
) -> Response {
    let (cell_rect, response) = ui.allocate_exact_size(Vec2::splat(cell_size), Sense::click());
    let hovered = enabled && response.hovered();
    let pressed = enabled && response.is_pointer_button_down_on();

    // Arrows: 48×48 visual centered in a 54×54 cell. OK fills the cell.
    let visual_rect = if accent {
        cell_rect
    } else {
        egui::Rect::from_center_size(cell_rect.center(), Vec2::splat(cell_size - 6.0))
    };

    // OK keeps the accent fill+stroke even when disabled (just dim alpha,
    // matching the prototype's `opacity: 0.4` on the whole cluster).
    let (fill, stroke_col, text_color) = if pressed {
        (ACCENT, ACCENT, ACCENT_INK)
    } else if accent {
        (ACCENT_WEAK, ACCENT, ACCENT)
    } else if hovered {
        (PANEL_3, BORDER, TEXT)
    } else {
        (PANEL_2, BORDER, TEXT_DIM)
    };

    let painter = ui.painter();
    painter.rect(
        visual_rect,
        Rounding::same(theme::R3),
        if enabled {
            fill
        } else {
            fill.gamma_multiply(0.4)
        },
        Stroke::new(
            1.0,
            if enabled {
                stroke_col
            } else {
                stroke_col.gamma_multiply(0.4)
            },
        ),
    );
    let dim_text = if enabled {
        text_color
    } else {
        text_color.gamma_multiply(0.5)
    };
    match glyph {
        DpadGlyph::Icon(icon) => {
            let icon_rect = egui::Rect::from_center_size(visual_rect.center(), Vec2::splat(22.0));
            crate::widgets::icons::paint(painter, icon_rect, icon, dim_text);
        }
        DpadGlyph::Ok => {
            painter.text(
                visual_rect.center(),
                egui::Align2::CENTER_CENTER,
                "OK",
                theme::display_font(13.0),
                dim_text,
            );
        }
    }

    response
}

// ─── inset frame used for the device card placeholder ────────────────────

/// Inset frame with a dashed border — the prototype's
/// `border: 1px dashed var(--border-strong)` wrap around the device-photo
/// `<image-slot>`. We paint the dashes manually because egui's stroke is
/// always solid.
///
/// `R4` (31px) rounding matches the prototype's `borderRadius: var(--r4)`
/// on the outer device-card frame.
pub fn inset_frame(ui: &mut Ui, add_contents: impl FnOnce(&mut Ui)) {
    let response = egui::Frame::none()
        .fill(INSET)
        .rounding(Rounding::same(theme::R4))
        .inner_margin(egui::Margin::same(12.0)) // prototype: padding: 12
        .show(ui, add_contents);
    paint_dashed_rect_stroke(
        ui.painter(),
        response.response.rect,
        theme::BORDER_STRONG,
        5.0,
        3.5,
    );
}

/// Success banner — green-tinted card with a check mark + label. Painted
/// after a successful flash completes.
pub fn success_banner(ui: &mut Ui, title: impl Into<String>, sub: impl Into<String>) {
    egui::Frame::none()
        .fill(theme::OK_WEAK)
        .stroke(Stroke::new(1.0, OK))
        .rounding(Rounding::same(theme::R3))
        .inner_margin(egui::Margin::same(S4))
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                let (rect, _) = ui.allocate_exact_size(Vec2::splat(30.0), Sense::hover());
                let p = ui.painter();
                p.circle_filled(rect.center(), 15.0, OK);
                p.text(
                    rect.center(),
                    egui::Align2::CENTER_CENTER,
                    "✓",
                    FontId::proportional(18.0),
                    ACCENT_INK,
                );
                ui.add_space(S3);
                ui.vertical(|ui| {
                    ui.label(RichText::new(title.into()).color(OK).strong().size(13.5));
                    ui.label(RichText::new(sub.into()).color(TEXT_MUTE).size(12.0));
                });
            });
        });
}

/// Paint a 1px BORDER-colored horizontal rule across the available width.
/// Matches the prototype's per-section `borderBottom: 1px solid var(--border)`.
pub fn section_divider(ui: &mut Ui) {
    let (rect, _) = ui.allocate_exact_size(Vec2::new(ui.available_width(), 1.0), Sense::hover());
    ui.painter()
        .rect_filled(rect, Rounding::ZERO, theme::BORDER);
}

/// Stroke a dashed rectangle outline. egui's `rect_stroke` only paints
/// solid borders, so we segment manually. Rounded corners are approximated
/// — the dashes lie on a straight rectangle and a small rounded fill sits
/// inside, hiding the corner gaps.
pub fn paint_dashed_rect_stroke(
    painter: &egui::Painter,
    rect: egui::Rect,
    color: egui::Color32,
    dash_len: f32,
    gap_len: f32,
) {
    let stroke = egui::Stroke::new(1.0, color);
    // Top
    let mut x = rect.left();
    while x < rect.right() {
        let end = (x + dash_len).min(rect.right());
        painter.line_segment(
            [
                egui::Pos2::new(x, rect.top()),
                egui::Pos2::new(end, rect.top()),
            ],
            stroke,
        );
        x = end + gap_len;
    }
    // Bottom
    let mut x = rect.left();
    while x < rect.right() {
        let end = (x + dash_len).min(rect.right());
        painter.line_segment(
            [
                egui::Pos2::new(x, rect.bottom()),
                egui::Pos2::new(end, rect.bottom()),
            ],
            stroke,
        );
        x = end + gap_len;
    }
    // Left
    let mut y = rect.top();
    while y < rect.bottom() {
        let end = (y + dash_len).min(rect.bottom());
        painter.line_segment(
            [
                egui::Pos2::new(rect.left(), y),
                egui::Pos2::new(rect.left(), end),
            ],
            stroke,
        );
        y = end + gap_len;
    }
    // Right
    let mut y = rect.top();
    while y < rect.bottom() {
        let end = (y + dash_len).min(rect.bottom());
        painter.line_segment(
            [
                egui::Pos2::new(rect.right(), y),
                egui::Pos2::new(rect.right(), end),
            ],
            stroke,
        );
        y = end + gap_len;
    }
}

/// Render a Rect-sized progress bar. Used for the queue total below the
/// per-partition checklist.
pub fn progress_bar(ui: &mut Ui, fraction: f32) {
    let (rect, _) = ui.allocate_exact_size(Vec2::new(ui.available_width(), 8.0), Sense::hover());
    let p = ui.painter();
    p.rect_filled(rect, Rounding::same(4.0), PANEL_3);
    let f = fraction.clamp(0.0, 1.0);
    if f > 0.0 {
        let mut fill_rect = rect;
        fill_rect.set_width(rect.width() * f);
        p.rect_filled(fill_rect, Rounding::same(4.0), ACCENT);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn chip_kind_is_copyable() {
        // Cheap compile-time check that ChipKind/DpadKey/ProgressState stay
        // small and copyable — they're passed by value across many widgets.
        fn assert_copy<T: Copy>() {}
        assert_copy::<ChipKind>();
        assert_copy::<DpadKey>();
        assert_copy::<theme::ProgressState>();
    }
}
