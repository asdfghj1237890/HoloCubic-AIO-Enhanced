//! Window + visuals configuration.

use egui::Visuals;

/// Initial window dimensions matching the legacy tool.
pub const INITIAL_WIDTH: f32 = 1200.0;
/// Initial window height.
pub const INITIAL_HEIGHT: f32 = 720.0;
/// Minimum window dimensions allowing the layout to remain usable.
pub const MIN_WIDTH: f32 = 1000.0;
/// Minimum window height.
pub const MIN_HEIGHT: f32 = 600.0;

/// Build the dark visuals used by the entire app.
pub fn dark_visuals() -> Visuals {
    Visuals::dark()
}
