//! Left vertical rail navigation — the Studio prototype's defining
//! visual feature, replacing egui's default top tab bar.
//!
//! Layout (mirrors the prototype's `<Rail>` in `index.html`):
//!
//! ```text
//!  ┌──────┐
//!  │ logo │  ← HoloCubic chip badge (accent-filled)
//!  ├──────┤
//!  │ ⚡   │
//!  │ Flash│
//!  ├──────┤
//!  │ ☰   │
//!  │ Params│
//!  ├──────┤
//!  │  ⋮  │
//!  ├──────┤  ← spacer pushes "Tools" to the bottom
//!  │ ⚙   │
//!  │ Tools│
//!  └──────┘
//! ```
//!
//! Each entry is a 60-wide button with icon-on-top + label-below. The
//! active entry gets an `ACCENT_WEAK` background + `ACCENT` color; idle
//! entries are transparent with `TEXT_MUTE` text.

use egui::{Align2, Color32, FontId, Pos2, Response, Rounding, Sense, Ui, Vec2};

use crate::tabs::Tab;
use crate::theme::{self, ACCENT, ACCENT_INK, ACCENT_WEAK, BORDER, PANEL, S1, S4, TEXT_MUTE};
use crate::widgets::icons::{self, Icon};

/// Width of the rail panel — matches the prototype's 76px.
pub const RAIL_WIDTH: f32 = 76.0;

/// One nav entry on the rail. `icon` selects the painter-drawn icon.
struct RailItem {
    tab: Tab,
    icon: Icon,
    label_key: &'static str,
}

/// Top group — main features.
const NAV_TOP: &[RailItem] = &[
    RailItem {
        tab: Tab::Flasher,
        icon: Icon::Bolt,
        label_key: "tab_download_debug_short",
    },
    RailItem {
        tab: Tab::Settings,
        icon: Icon::Sliders,
        label_key: "tab_setting_short",
    },
    RailItem {
        tab: Tab::FileManager,
        icon: Icon::Folder,
        label_key: "tab_file_manager_short",
    },
    RailItem {
        tab: Tab::ImageConverter,
        icon: Icon::Image,
        label_key: "tab_image_converter_short",
    },
    RailItem {
        tab: Tab::VideoConverter,
        icon: Icon::Play,
        label_key: "tab_video_converter_short",
    },
    RailItem {
        tab: Tab::Help,
        icon: Icon::Help,
        label_key: "tab_help_short",
    },
];

/// Bottom group — appearance / language live here in the prototype.
const NAV_BOTTOM: &[RailItem] = &[RailItem {
    tab: Tab::ToolSettings,
    icon: Icon::Wrench,
    label_key: "tab_tool_settings_short",
}];

/// Render the rail in a left `SidePanel`. Mutates `active` if the user
/// clicks an entry.
///
/// `vertical_centered` puts every child on the rail's center axis, so
/// the 36 px brand chip, the 60 px nav buttons, and the bottom-anchored
/// "tool" button all share one vertical column.
pub fn show(ui: &mut Ui, active: &mut Tab) {
    ui.vertical_centered(|ui| {
        ui.add_space(S4);
        paint_brand_chip(ui);
        ui.add_space(S4);

        for item in NAV_TOP {
            if rail_button(ui, item, *active == item.tab).clicked() {
                *active = item.tab;
            }
            ui.add_space(S1);
        }

        // Push the bottom group to the rail's bottom edge.
        ui.with_layout(egui::Layout::bottom_up(egui::Align::Center), |ui| {
            ui.add_space(S4);
            for item in NAV_BOTTOM {
                if rail_button(ui, item, *active == item.tab).clicked() {
                    *active = item.tab;
                }
                ui.add_space(S1);
            }
        });
    });
}

/// Brand chip — 36×36 accent-filled rounded square with the IC-chip mark
/// drawn inside (matches the prototype's `<Icon d={ICON.chip}>`).
///
/// The prototype's `box-shadow: 0 4px 14px var(--accent-weak)` is a soft
/// Gaussian-blurred glow. egui has no built-in blur, so we approximate
/// with three increasingly large, increasingly transparent rounded
/// rectangles stacked behind the chip — close enough to suggest a glow
/// without looking like a duplicated chip.
fn paint_brand_chip(ui: &mut Ui) {
    let size = 36.0;
    let (rect, _) = ui.allocate_exact_size(Vec2::splat(size), Sense::hover());
    let p = ui.painter();
    // Three layered "blurs" — each shifted 4 px down, expanded a bit
    // wider, and faded to a fraction of accent_weak.
    let shadow_offset = egui::Vec2::new(0.0, 4.0);
    for (expand, alpha_scale) in [(8.0, 0.35), (5.0, 0.5), (2.5, 0.7)] {
        let halo = rect.translate(shadow_offset).expand(expand);
        let mut color = ACCENT_WEAK;
        let a = (color.a() as f32 * alpha_scale) as u8;
        color = egui::Color32::from_rgba_unmultiplied(color.r(), color.g(), color.b(), a);
        p.rect_filled(halo, Rounding::same(theme::R2 + expand * 0.6), color);
    }
    p.rect_filled(rect, Rounding::same(theme::R2), ACCENT);
    let icon_rect = rect.shrink(8.0);
    icons::paint(p, icon_rect, Icon::Chip, ACCENT_INK);
}

/// One rail entry — icon glyph stacked over a small label, accent
/// background when active.
fn rail_button(ui: &mut Ui, item: &RailItem, active: bool) -> Response {
    let width = 60.0;
    let height = 52.0;
    let (rect, response) = ui.allocate_exact_size(Vec2::new(width, height), Sense::click());

    let bg = if active {
        ACCENT_WEAK
    } else if response.hovered() {
        Color32::from_rgba_premultiplied(255, 255, 255, 8)
    } else {
        Color32::TRANSPARENT
    };
    let fg = if active { ACCENT } else { TEXT_MUTE };

    let p = ui.painter();
    p.rect_filled(rect, Rounding::same(theme::R2), bg);

    // Icon — painter-drawn, 20×20 centered in the upper portion.
    let icon_size = 20.0;
    let icon_rect = egui::Rect::from_center_size(
        Pos2::new(rect.center().x, rect.top() + 13.0),
        Vec2::splat(icon_size),
    );
    icons::paint(p, icon_rect, item.icon, fg);

    // Label below the icon.
    let label = aio_i18n::t(item.label_key, None);
    p.text(
        Pos2::new(rect.center().x, rect.bottom() - 10.0),
        Align2::CENTER_CENTER,
        &label,
        FontId::proportional(10.5),
        fg,
    );

    if response.hovered() {
        response.clone().on_hover_text(rail_long_label(item.tab));
    }
    response
}

/// Full label shown in the hover tooltip — reuses the existing tab keys.
fn rail_long_label(tab: Tab) -> String {
    aio_i18n::t(tab.i18n_key(), None)
}

/// Frame the rail with the Studio's panel surface + 1px right-edge border.
/// `egui::Frame::stroke` paints all four sides, so we draw the right edge
/// separately as part of `show()` instead.
pub fn frame() -> egui::Frame {
    egui::Frame::none()
        .fill(PANEL)
        .inner_margin(egui::Margin::ZERO)
}

/// Paint the rail's right-edge border. Called by `app.rs` after the rail
/// `SidePanel` renders, using the absolute panel rect.
pub fn paint_right_border(painter: &egui::Painter, rect: egui::Rect) {
    let line_rect = egui::Rect::from_min_max(
        egui::Pos2::new(rect.right() - 1.0, rect.top()),
        egui::Pos2::new(rect.right(), rect.bottom()),
    );
    painter.rect_filled(line_rect, egui::Rounding::ZERO, BORDER);
}
