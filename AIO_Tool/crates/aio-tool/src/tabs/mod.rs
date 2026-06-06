//! Tab declarations + the `Tab` enum used by `App` for dispatch.
//!
//! Order matches legacy `CubicAIO_Tool.py` (lines 78-114) minus the
//! dropped Screen Share placeholder. Remote control folds into the
//! Flasher (Download & Debug) tab in Plan 7 (Task 7) — there is no
//! dedicated Remote tab.

pub mod file_manager;
pub mod flasher;
pub mod fs_node;
pub mod help;
pub mod image_converter;
pub mod settings;
pub mod settings_schema;
pub mod tool_settings;
pub mod video_converter;

/// All tabs the app exposes. Order = render order in the top bar.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Tab {
    /// Flash + serial monitor (legacy "Download & Debug").
    Flasher,
    /// Device settings web-UI relay.
    Settings,
    /// FTP file manager.
    FileManager,
    /// Image → MJPEG / RGB565 converter.
    ImageConverter,
    /// Video → MJPEG / RGB565 converter.
    VideoConverter,
    /// Tool-level preferences (language, theme).
    ToolSettings,
    /// About / help.
    Help,
}

impl Tab {
    /// Render order. Used by `App::update` to draw the tab bar.
    pub const ALL: [Tab; 7] = [
        Tab::Flasher,
        Tab::Settings,
        Tab::FileManager,
        Tab::ImageConverter,
        Tab::VideoConverter,
        Tab::ToolSettings,
        Tab::Help,
    ];

    /// i18n key for the tab label. Looked up via `aio_i18n::t()`.
    pub fn i18n_key(self) -> &'static str {
        match self {
            Tab::Flasher => "tab_download_debug",
            Tab::Settings => "tab_setting",
            Tab::FileManager => "tab_file_manager",
            Tab::ImageConverter => "tab_image_converter",
            Tab::VideoConverter => "tab_video_converter",
            Tab::ToolSettings => "tab_tool_settings",
            Tab::Help => "tab_help",
        }
    }
}
