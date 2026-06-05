//! Tab declarations + the `Tab` enum used by `App` for dispatch.
//!
//! Order matches legacy `CubicAIO_Tool.py` (lines 78-114) minus the
//! dropped Screen Share placeholder, plus a dedicated Remote Control tab
//! split out of Download Debug (real content lands in Plan 7).

pub mod file_manager;
pub mod flasher;
pub mod help;
pub mod image_converter;
pub mod remote;
pub mod settings;
pub mod tool_settings;
pub mod video_converter;

/// All tabs the app exposes. Order = render order in the top bar.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Tab {
    /// Flash + serial monitor (legacy "Download & Debug").
    Flasher,
    /// Device settings web-UI relay.
    Settings,
    /// Remote control (cursor, keyboard, screencast).
    Remote,
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
    pub const ALL: [Tab; 8] = [
        Tab::Flasher,
        Tab::Settings,
        Tab::Remote,
        Tab::FileManager,
        Tab::ImageConverter,
        Tab::VideoConverter,
        Tab::ToolSettings,
        Tab::Help,
    ];

    /// i18n key for the tab label. Looked up via `aio_i18n::t()`.
    ///
    /// `Remote` uses `remote_control` because the legacy JSON never had a
    /// `tab_remote` key — remote control lived inside the Download Debug
    /// tab. Plan 7 splits it out as its own tab.
    pub fn i18n_key(self) -> &'static str {
        match self {
            Tab::Flasher => "tab_download_debug",
            Tab::Settings => "tab_setting",
            Tab::Remote => "remote_control",
            Tab::FileManager => "tab_file_manager",
            Tab::ImageConverter => "tab_image_converter",
            Tab::VideoConverter => "tab_video_converter",
            Tab::ToolSettings => "tab_tool_settings",
            Tab::Help => "tab_help",
        }
    }
}
