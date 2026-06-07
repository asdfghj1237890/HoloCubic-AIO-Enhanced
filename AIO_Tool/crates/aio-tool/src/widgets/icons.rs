//! Painter-drawn icons matching the prototype's SVG paths (Lucide-style).
//!
//! The prototype's `Icon` component renders strokes from `M…l…z` path
//! data inside a 24×24 viewBox. egui has no built-in vector-icon support,
//! so we translate each path into `Painter::line_segment` /
//! `add(Shape::closed_line(...))` calls scaled to the supplied rect.
//!
//! All icons use a 24-unit viewBox; the helper `vp` scales viewBox
//! coordinates into the rect we paint into.

use egui::epaint::{PathStroke, Shape};
use egui::{Color32, Painter, Pos2, Rect, Stroke};

/// `((from_x, from_y), (to_x, to_y))` viewBox segment — used by the
/// sliders/chip-leg icons to keep `let foo: &[Seg]`
/// out of clippy's `type_complexity` warning.
type Seg = ((f32, f32), (f32, f32));

/// Icons used by the Studio UI. Add a variant + a match arm in `paint`
/// to extend the set.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum Icon {
    /// Lightning bolt — `nav.flash`, recommended firmware card.
    Bolt,
    /// Three vertical sliders — `nav.params`.
    Sliders,
    /// Folder outline — `nav.files`.
    Folder,
    /// Image rectangle + sun + mountain — `nav.image`.
    Image,
    /// Right-pointing triangle — `nav.video`.
    Play,
    /// Circle question mark — `nav.help`.
    Help,
    /// Wrench — `nav.tool` (was the gear we wanted to avoid as "complex").
    Wrench,
    /// IC chip — brand mark at the top of the rail.
    Chip,
    /// Up arrow — D-pad up.
    Up,
    /// Left arrow — D-pad left.
    Left,
    /// Right arrow — D-pad right.
    Right,
    /// House — D-pad home.
    Home,
    /// Refresh / reload arrow.
    Refresh,
    /// Plug (USB plug shape).
    Plug,
}

/// Paint `icon` into `rect` with stroke color `color`. Stroke width
/// scales with the rect size (`8% of width`) so icons stay legible at
/// any size.
pub fn paint(painter: &Painter, rect: Rect, icon: Icon, color: Color32) {
    let stroke = Stroke::new((rect.width() * 0.085).max(1.2), color);
    match icon {
        Icon::Bolt => paint_bolt(painter, rect, color, stroke),
        Icon::Sliders => paint_sliders(painter, rect, stroke),
        Icon::Folder => paint_folder(painter, rect, stroke),
        Icon::Image => paint_image(painter, rect, stroke),
        Icon::Play => paint_play(painter, rect, color, stroke),
        Icon::Help => paint_help(painter, rect, stroke),
        Icon::Wrench => paint_wrench(painter, rect, stroke),
        Icon::Chip => paint_chip(painter, rect, stroke),
        Icon::Up => paint_arrow(painter, rect, stroke, ArrowDir::Up),
        Icon::Left => paint_arrow(painter, rect, stroke, ArrowDir::Left),
        Icon::Right => paint_arrow(painter, rect, stroke, ArrowDir::Right),
        Icon::Home => paint_home(painter, rect, stroke),
        Icon::Refresh => paint_refresh(painter, rect, stroke),
        Icon::Plug => paint_plug(painter, rect, stroke),
    }
}

/// Scale a 24x24 viewBox point to the destination rect.
fn vp(rect: Rect, x: f32, y: f32) -> Pos2 {
    Pos2::new(
        rect.left() + x / 24.0 * rect.width(),
        rect.top() + y / 24.0 * rect.height(),
    )
}

/// Lightning bolt — prototype path:
///   `M13 2 4 14h7l-1 8 9-12h-7l1-8z`
/// Points (closed): (13,2) (4,14) (11,14) (10,22) (19,10) (12,10).
fn paint_bolt(painter: &Painter, rect: Rect, color: Color32, stroke: Stroke) {
    let pts = vec![
        vp(rect, 13.0, 2.0),
        vp(rect, 4.0, 14.0),
        vp(rect, 11.0, 14.0),
        vp(rect, 10.0, 22.0),
        vp(rect, 19.0, 10.0),
        vp(rect, 12.0, 10.0),
    ];
    // We rely on stroke for the outline; if the caller wants a filled
    // version, the recommended-firmware card draws its own filled
    // background then this stroke sits on top.
    painter.add(Shape::closed_line(
        pts.clone(),
        PathStroke::new(stroke.width, color),
    ));
}

/// Three vertical sliders + horizontal knobs — prototype paths:
///   `M4 21v-7  M4 10V3  M12 21v-9  M12 8V3  M20 21v-5  M20 12V3
///    M1 14h6  M9 8h6  M17 16h6`
fn paint_sliders(painter: &Painter, rect: Rect, stroke: Stroke) {
    let line = |a, b| Shape::line_segment([a, b], stroke);
    let pts: &[Seg] = &[
        // Vertical bars (split at the knob).
        ((4.0, 21.0), (4.0, 14.0)),
        ((4.0, 10.0), (4.0, 3.0)),
        ((12.0, 21.0), (12.0, 12.0)),
        ((12.0, 8.0), (12.0, 3.0)),
        ((20.0, 21.0), (20.0, 16.0)),
        ((20.0, 12.0), (20.0, 3.0)),
        // Knob horizontals.
        ((1.0, 14.0), (7.0, 14.0)),
        ((9.0, 8.0), (15.0, 8.0)),
        ((17.0, 16.0), (23.0, 16.0)),
    ];
    for &((ax, ay), (bx, by)) in pts {
        painter.add(line(vp(rect, ax, ay), vp(rect, bx, by)));
    }
}

/// Folder outline — approximated as a tab-shaped polygon. The prototype
/// uses rounded corners via SVG arcs; we accept a straight-corner read.
fn paint_folder(painter: &Painter, rect: Rect, stroke: Stroke) {
    let pts = vec![
        vp(rect, 3.0, 8.0),
        vp(rect, 3.0, 6.0),
        vp(rect, 9.0, 6.0),
        vp(rect, 11.0, 8.0),
        vp(rect, 21.0, 8.0),
        vp(rect, 21.0, 19.0),
        vp(rect, 3.0, 19.0),
    ];
    painter.add(Shape::closed_line(pts, stroke));
}

/// Image rectangle + sun circle + mountain triangle line.
fn paint_image(painter: &Painter, rect: Rect, stroke: Stroke) {
    // Outer rectangle.
    let r = egui::Rect::from_min_max(vp(rect, 3.0, 5.0), vp(rect, 21.0, 19.0));
    painter.rect_stroke(r, egui::Rounding::same(rect.width() * 0.06), stroke);
    // Sun.
    painter.circle_stroke(vp(rect, 8.5, 9.5), rect.width() / 24.0 * 1.5, stroke);
    // Mountain line.
    painter.add(Shape::line(
        vec![
            vp(rect, 5.0, 19.0),
            vp(rect, 16.0, 11.0),
            vp(rect, 21.0, 16.0),
        ],
        stroke,
    ));
}

/// Right-pointing triangle — `nav.video` (filled in the prototype's
/// fill=true variant). We fill with the accent color so the play icon
/// reads at small sizes.
fn paint_play(painter: &Painter, rect: Rect, color: Color32, stroke: Stroke) {
    let pts = vec![
        vp(rect, 5.0, 4.5),
        vp(rect, 5.0, 19.5),
        vp(rect, 18.0, 12.0),
    ];
    painter.add(Shape::convex_polygon(pts.clone(), color, stroke));
}

/// Circle + question mark.
fn paint_help(painter: &Painter, rect: Rect, stroke: Stroke) {
    painter.circle_stroke(rect.center(), rect.width() * 0.42, stroke);
    // The question mark hook — approximated as a quarter-arc by chaining
    // three points.
    painter.add(Shape::line(
        vec![
            vp(rect, 9.6, 9.0),
            vp(rect, 12.0, 7.5),
            vp(rect, 14.0, 9.5),
            vp(rect, 12.0, 12.5),
            vp(rect, 12.0, 14.5),
        ],
        stroke,
    ));
    // Dot.
    painter.circle_filled(vp(rect, 12.0, 17.5), stroke.width * 0.7, stroke.color);
}

/// Wrench — a stylized open-ended wrench. We draw the handle as a single
/// thick segment, with a "C" shape for the head.
fn paint_wrench(painter: &Painter, rect: Rect, stroke: Stroke) {
    // Handle (diagonal).
    painter.add(Shape::line_segment(
        [vp(rect, 9.0, 15.0), vp(rect, 4.0, 20.0)],
        stroke,
    ));
    // Head — circle with a wedge cut out, approximated by an arc of points.
    let center = vp(rect, 14.0, 10.0);
    let r = rect.width() * 0.18;
    let mut arc = Vec::with_capacity(20);
    for i in 0..20 {
        let t = std::f32::consts::PI * 0.25 + (i as f32 / 19.0) * std::f32::consts::PI * 1.5;
        arc.push(Pos2::new(center.x + r * t.cos(), center.y + r * t.sin()));
    }
    painter.add(Shape::line(arc, stroke));
}

/// IC chip — brand mark. Inner square + 8 leg lines + outer square.
fn paint_chip(painter: &Painter, rect: Rect, stroke: Stroke) {
    // Outer square.
    let outer = egui::Rect::from_min_max(vp(rect, 6.0, 6.0), vp(rect, 18.0, 18.0));
    painter.rect_stroke(outer, egui::Rounding::same(rect.width() * 0.04), stroke);
    // Inner square.
    let inner = egui::Rect::from_min_max(vp(rect, 9.0, 9.0), vp(rect, 15.0, 15.0));
    painter.rect_stroke(inner, egui::Rounding::ZERO, stroke);
    // 8 legs (2 per side).
    let leg_pts: &[Seg] = &[
        ((4.0, 9.0), (6.0, 9.0)),
        ((4.0, 15.0), (6.0, 15.0)),
        ((18.0, 9.0), (20.0, 9.0)),
        ((18.0, 15.0), (20.0, 15.0)),
        ((9.0, 4.0), (9.0, 6.0)),
        ((15.0, 4.0), (15.0, 6.0)),
        ((9.0, 18.0), (9.0, 20.0)),
        ((15.0, 18.0), (15.0, 20.0)),
    ];
    for &((ax, ay), (bx, by)) in leg_pts {
        painter.add(Shape::line_segment(
            [vp(rect, ax, ay), vp(rect, bx, by)],
            stroke,
        ));
    }
}

/// Direction for the arrow icons.
#[derive(Copy, Clone, Debug)]
enum ArrowDir {
    Up,
    Left,
    Right,
}

/// Arrow with shaft + arrowhead — rotated by direction.
fn paint_arrow(painter: &Painter, rect: Rect, stroke: Stroke, dir: ArrowDir) {
    let (shaft, head_a, head_b, tip) = match dir {
        ArrowDir::Up => (
            (vp(rect, 12.0, 19.0), vp(rect, 12.0, 5.0)),
            vp(rect, 5.0, 12.0),
            vp(rect, 19.0, 12.0),
            vp(rect, 12.0, 5.0),
        ),
        ArrowDir::Left => (
            (vp(rect, 19.0, 12.0), vp(rect, 5.0, 12.0)),
            vp(rect, 12.0, 5.0),
            vp(rect, 12.0, 19.0),
            vp(rect, 5.0, 12.0),
        ),
        ArrowDir::Right => (
            (vp(rect, 5.0, 12.0), vp(rect, 19.0, 12.0)),
            vp(rect, 12.0, 5.0),
            vp(rect, 12.0, 19.0),
            vp(rect, 19.0, 12.0),
        ),
    };
    painter.add(Shape::line_segment([shaft.0, shaft.1], stroke));
    painter.add(Shape::line_segment([head_a, tip], stroke));
    painter.add(Shape::line_segment([head_b, tip], stroke));
}

/// House icon — D-pad home.
fn paint_home(painter: &Painter, rect: Rect, stroke: Stroke) {
    // Roof.
    painter.add(Shape::line(
        vec![
            vp(rect, 3.0, 11.0),
            vp(rect, 12.0, 3.0),
            vp(rect, 21.0, 11.0),
        ],
        stroke,
    ));
    // Walls.
    let walls = egui::Rect::from_min_max(vp(rect, 5.0, 10.0), vp(rect, 19.0, 20.0));
    painter.rect_stroke(walls, egui::Rounding::ZERO, stroke);
}

/// Refresh / reload arrow — partial circle + arrow tip.
fn paint_refresh(painter: &Painter, rect: Rect, stroke: Stroke) {
    let center = rect.center();
    let r = rect.width() * 0.38;
    let mut arc = Vec::with_capacity(30);
    for i in 0..30 {
        let t = std::f32::consts::PI * 0.1 + (i as f32 / 29.0) * std::f32::consts::PI * 1.7;
        arc.push(Pos2::new(center.x + r * t.cos(), center.y + r * t.sin()));
    }
    painter.add(Shape::line(arc, stroke));
    // Tiny chevron at the top-right indicating the rotation direction.
    painter.add(Shape::line(
        vec![
            vp(rect, 17.0, 4.0),
            vp(rect, 21.0, 4.0),
            vp(rect, 21.0, 8.0),
        ],
        stroke,
    ));
}

/// USB plug — two-prong plug + body.
fn paint_plug(painter: &Painter, rect: Rect, stroke: Stroke) {
    // Prongs.
    painter.add(Shape::line_segment(
        [vp(rect, 9.0, 2.0), vp(rect, 9.0, 8.0)],
        stroke,
    ));
    painter.add(Shape::line_segment(
        [vp(rect, 15.0, 2.0), vp(rect, 15.0, 8.0)],
        stroke,
    ));
    // Body (rounded rectangle).
    let body = egui::Rect::from_min_max(vp(rect, 6.0, 8.0), vp(rect, 18.0, 14.0));
    painter.rect_stroke(body, egui::Rounding::same(rect.width() * 0.08), stroke);
    // Cable.
    painter.add(Shape::line_segment(
        [vp(rect, 12.0, 14.0), vp(rect, 12.0, 22.0)],
        stroke,
    ));
}
