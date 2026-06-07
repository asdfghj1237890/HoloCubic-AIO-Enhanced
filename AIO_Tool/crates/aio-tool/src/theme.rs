//! Studio design system — color tokens, spacing/radius ramps, and egui
//! visuals + button-style helpers.
//!
//! Tokens mirror `Docs/design/studio-flasher/theme.css [data-theme="dark"]`
//! so the Rust UI stays in lockstep with the HTML reference design.

use egui::epaint::Stroke;
use egui::style::{Selection, WidgetVisuals, Widgets};
use egui::{Color32, Margin, RichText, Rounding, Ui, Visuals};

/// Initial window dimensions matching the legacy tool.
pub const INITIAL_WIDTH: f32 = 1200.0;
/// Initial window height.
pub const INITIAL_HEIGHT: f32 = 720.0;
/// Minimum window dimensions allowing the layout to remain usable.
pub const MIN_WIDTH: f32 = 1000.0;
/// Minimum window height.
pub const MIN_HEIGHT: f32 = 600.0;

// ─── color tokens (mirror theme.css [data-theme="dark"]) ──────────────────

/// Page background.
pub const BG: Color32 = Color32::from_rgb(0x0d, 0x11, 0x17);
/// Default panel surface (rails, sidebars).
pub const PANEL: Color32 = Color32::from_rgb(0x15, 0x1b, 0x24);
/// Slightly lighter panel (button rest, group headers).
pub const PANEL_2: Color32 = Color32::from_rgb(0x1a, 0x21, 0x2c);
/// Subdued panel (placeholders, disabled chips).
pub const PANEL_3: Color32 = Color32::from_rgb(0x21, 0x2b, 0x39);
/// Inset surface used for text inputs and log frames.
pub const INSET: Color32 = Color32::from_rgb(0x0b, 0x0f, 0x15);

/// Primary text.
pub const TEXT: Color32 = Color32::from_rgb(0xe8, 0xee, 0xf6);
/// Secondary text (sub-labels, hints).
pub const TEXT_DIM: Color32 = Color32::from_rgb(0xaa, 0xb6, 0xc5);
/// Tertiary text (placeholders, metadata).
pub const TEXT_MUTE: Color32 = Color32::from_rgb(0x6b, 0x76, 0x86);

/// Default 1px border.
pub const BORDER: Color32 = Color32::from_rgb(0x22, 0x2b, 0x38);
/// Stronger border for hover / focus.
pub const BORDER_STRONG: Color32 = Color32::from_rgb(0x31, 0x3d, 0x4e);

/// Brand accent — sampled from the original egui steel-blue.
pub const ACCENT: Color32 = Color32::from_rgb(0x1f, 0x6a, 0xa5);
/// Translucent accent (18% opacity) — fills for active step rings and
/// progress chips.
///
/// egui's `Color32` stores premultiplied alpha. The original constants
/// used `from_rgba_premultiplied(0x1f, 0x6a, 0xa5, 0x2e)` with the
/// **non-premultiplied** RGB, which made the fill far too saturated.
/// `from_rgba_unmultiplied` isn't `const fn` in egui 0.29, so we
/// precompute the premultiplied bytes: `RGB * (A/255)`.
pub const ACCENT_WEAK: Color32 = Color32::from_rgba_premultiplied(0x06, 0x13, 0x1e, 0x2e);
/// Brighter translucent accent (45% opacity) for focus outlines.
///
/// Premultiplied: (0x1f, 0x6a, 0xa5) × (0x73/255) → (0x0e, 0x2f, 0x4a).
pub const ACCENT_LINE: Color32 = Color32::from_rgba_premultiplied(0x0e, 0x2f, 0x4a, 0x73);
/// White ink that sits on accent fills.
pub const ACCENT_INK: Color32 = Color32::WHITE;

/// Success green.
pub const OK: Color32 = Color32::from_rgb(0x3f, 0xb9, 0x50);
/// Translucent success fill (14% opacity) — success banner background.
///
/// Premultiplied: (0x3f, 0xb9, 0x50) × (0x24/255) → (0x09, 0x1a, 0x0b).
pub const OK_WEAK: Color32 = Color32::from_rgba_premultiplied(0x09, 0x1a, 0x0b, 0x24);
/// Warning amber (changed-field indicator).
pub const WARN: Color32 = Color32::from_rgb(0xd6, 0xa2, 0x32);
/// Error red.
pub const ERR: Color32 = Color32::from_rgb(0xf8, 0x51, 0x49);
/// Translucent error fill (13% opacity).
///
/// Premultiplied: (0xf8, 0x51, 0x49) × (0x21/255) → (0x20, 0x0a, 0x09).
pub const ERR_WEAK: Color32 = Color32::from_rgba_premultiplied(0x20, 0x0a, 0x09, 0x21);

// ─── spacing + radius ramps (--d=1, --rm=1.7 from the prototype) ──────────

/// 4 px gap.
pub const S1: f32 = 4.0;
/// 8 px gap.
pub const S2: f32 = 8.0;
/// 12 px gap.
pub const S3: f32 = 12.0;
/// 16 px gap.
pub const S4: f32 = 16.0;
/// 22 px gap.
pub const S5: f32 = 22.0;
/// 30 px gap.
pub const S6: f32 = 30.0;
/// 40 px gap.
pub const S7: f32 = 40.0;

/// Tight corner radius (chips inside groups).
pub const R1: f32 = 7.0;
/// Default button / input radius.
pub const R2: f32 = 14.0;
/// Card radius.
pub const R3: f32 = 20.0;
/// Outer panel radius.
pub const R4: f32 = 31.0;
/// Pill radius — large enough to fully round any normal-height element.
pub const RPILL: f32 = 999.0;

/// Build the dark visuals used by the entire app.
pub fn dark_visuals() -> Visuals {
    let mut v = Visuals::dark();
    apply_studio_visuals(&mut v);
    v
}

/// Customize an egui `Visuals` in place so default widgets pick up the
/// Studio palette without each call site needing to override colors.
pub fn apply_studio_visuals(v: &mut Visuals) {
    v.dark_mode = true;
    v.override_text_color = Some(TEXT);
    v.panel_fill = BG;
    v.window_fill = PANEL;
    v.window_stroke = Stroke::new(1.0, BORDER);
    v.faint_bg_color = PANEL_2;
    v.extreme_bg_color = INSET;
    v.code_bg_color = INSET;
    v.hyperlink_color = ACCENT;

    v.selection = Selection {
        bg_fill: ACCENT_WEAK,
        stroke: Stroke::new(1.0, ACCENT),
    };

    let rounding = Rounding::same(R2);
    v.widgets = Widgets {
        noninteractive: WidgetVisuals {
            bg_fill: PANEL,
            weak_bg_fill: PANEL,
            bg_stroke: Stroke::new(1.0, BORDER),
            fg_stroke: Stroke::new(1.0, TEXT_DIM),
            rounding,
            expansion: 0.0,
        },
        inactive: WidgetVisuals {
            bg_fill: PANEL_2,
            weak_bg_fill: PANEL_2,
            bg_stroke: Stroke::new(1.0, BORDER_STRONG),
            fg_stroke: Stroke::new(1.0, TEXT),
            rounding,
            expansion: 0.0,
        },
        hovered: WidgetVisuals {
            bg_fill: PANEL_2,
            weak_bg_fill: PANEL_2,
            bg_stroke: Stroke::new(1.0, ACCENT_LINE),
            fg_stroke: Stroke::new(1.0, TEXT),
            rounding,
            expansion: 0.0,
        },
        active: WidgetVisuals {
            bg_fill: PANEL_3,
            weak_bg_fill: PANEL_3,
            bg_stroke: Stroke::new(1.0, ACCENT),
            fg_stroke: Stroke::new(1.0, TEXT),
            rounding,
            expansion: 0.0,
        },
        open: WidgetVisuals {
            bg_fill: PANEL_3,
            weak_bg_fill: PANEL_3,
            bg_stroke: Stroke::new(1.0, ACCENT_LINE),
            fg_stroke: Stroke::new(1.0, TEXT),
            rounding,
            expansion: 0.0,
        },
    };
}

// ─── button-style helpers ─────────────────────────────────────────────────

/// Step / chip / progress state — drives color of step circles, partition
/// rows, and connection status chips.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum ProgressState {
    /// Not yet reached.
    Pending,
    /// Currently in progress / connection live.
    Active,
    /// Successfully completed.
    Done,
}

/// `egui::Frame::group`-equivalent helper that pre-applies the Studio
/// rounded-card look. Caller supplies the inner content.
pub fn card(ui: &mut Ui, add_contents: impl FnOnce(&mut Ui)) {
    egui::Frame::none()
        .fill(PANEL)
        .stroke(Stroke::new(1.0, BORDER))
        .rounding(Rounding::same(R3))
        .inner_margin(Margin::same(S4))
        .show(ui, add_contents);
}

/// A primary, accent-filled button. Designed for the "Connect" / "Flash"
/// call-to-action position.
pub fn primary_button(ui: &mut Ui, label: impl Into<RichText>) -> egui::Response {
    let text = label.into().color(ACCENT_INK).strong();
    let btn = egui::Button::new(text)
        .fill(ACCENT)
        .stroke(Stroke::new(1.0, ACCENT))
        .rounding(Rounding::same(R2));
    ui.add(btn)
}

/// Ghost button — transparent fill, default border. Used for secondary
/// actions like "Refresh ports" or "Reboot".
pub fn ghost_button(ui: &mut Ui, label: impl Into<RichText>) -> egui::Response {
    let text = label.into().color(TEXT);
    let btn = egui::Button::new(text)
        .fill(Color32::TRANSPARENT)
        .stroke(Stroke::new(1.0, BORDER))
        .rounding(Rounding::same(R2));
    ui.add(btn)
}

/// Danger button — red-tinted fill for "Cancel" / "Delete" / "Erase".
pub fn danger_button(ui: &mut Ui, label: impl Into<RichText>) -> egui::Response {
    let text = label.into().color(ERR);
    let btn = egui::Button::new(text)
        .fill(ERR_WEAK)
        .stroke(Stroke::new(1.0, ERR))
        .rounding(Rounding::same(R2));
    ui.add(btn)
}

/// Section heading — uppercase tracking + muted text — used above grouped
/// cards. Mirrors `<div class="font-700 letter-spacing-08em">` style from
/// the prototype.
pub fn section_heading(ui: &mut Ui, label: impl Into<String>) {
    ui.label(
        RichText::new(label.into().to_uppercase())
            .color(TEXT_MUTE)
            .strong()
            .size(11.0),
    );
}

/// FontId for the prototype's `.disp` class — used by page titles and
/// step headings. Falls back to the proportional family if Space Grotesk
/// isn't loaded.
pub fn display_font(size: f32) -> egui::FontId {
    egui::FontId::new(size, egui::FontFamily::Name("display".into()))
}
