//! Typed converter errors.

use thiserror::Error;

/// Errors that can occur during conversion.
#[derive(Debug, Error)]
pub enum ConvertError {
    /// Input image bytes couldn't be decoded as PNG/JPG/BMP.
    #[error("decode input image: {0}")]
    Decode(#[from] image::ImageError),

    /// Image dimensions exceed LVGL's 11-bit width/height fields (max 2047 px).
    #[error("dimensions {width}×{height} exceed LVGL limit of {max}×{max}")]
    DimensionTooLarge {
        /// Input width in pixels.
        width: u32,
        /// Input height in pixels.
        height: u32,
        /// LVGL maximum (2047).
        max: u32,
    },

    /// IO failure writing output bin / C file (when the converter writes to disk).
    #[error("io: {0}")]
    Io(#[from] std::io::Error),

    /// User clicked Cancel.
    #[error("conversion cancelled by user")]
    Cancelled,
}
