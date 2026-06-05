//! User-facing converter API.
//!
//! [`Converter`] is the top-level dispatcher: it decodes the input image
//! once and then routes to the appropriate per-format encoder
//! (`encoders::rgb` / `encoders::alpha` / `encoders::indexed` for `.bin`,
//! `encoders::c_array` for `.c`). Progress events and cancellation flow
//! through the per-format encoders at row boundaries.

use std::sync::atomic::AtomicBool;
use std::sync::mpsc::Sender;
use std::sync::Arc;

use crate::encoders::{alpha, c_array, indexed, rgb};
use crate::error::ConvertError;
use crate::format::ColorFormat;
use crate::header::encode_lvgl_header;
use crate::image_input::{decode_image, Rgba8Image};
use crate::progress::ConvertEvent;

/// Top-level converter. Holds the decoded image + chosen format.
///
/// Construct with [`Converter::new`] (decodes the input once), then call
/// [`Converter::encode_bin`] for LVGL binary output or
/// [`Converter::encode_c_array`] for a `.c` source string. Both can be
/// called repeatedly on the same `Converter` without re-decoding.
pub struct Converter {
    img: Rgba8Image,
    fmt: ColorFormat,
    dither: bool,
}

impl Converter {
    /// Decode `image_bytes` and prepare to encode into `fmt`.
    ///
    /// If `dither` is true, Floyd–Steinberg is applied inside the RGB
    /// encoders (no effect on Alpha or Indexed formats — they ignore the
    /// flag).
    ///
    /// # Errors
    /// * `ConvertError::Decode` if `image_bytes` is not a valid PNG/JPG/BMP.
    /// * `ConvertError::DimensionTooLarge` if width or height exceeds
    ///   `LVGL_MAX_DIMENSION` (2047 px).
    pub fn new(image_bytes: &[u8], fmt: ColorFormat, dither: bool) -> Result<Self, ConvertError> {
        let img = decode_image(image_bytes)?;
        Ok(Self { img, fmt, dither })
    }

    /// Image width in pixels (read from decoded input).
    pub fn width(&self) -> u32 {
        self.img.width()
    }

    /// Image height in pixels.
    pub fn height(&self) -> u32 {
        self.img.height()
    }

    /// Active color format.
    pub fn format(&self) -> ColorFormat {
        self.fmt
    }

    /// Whether dithering is enabled.
    pub fn dither_enabled(&self) -> bool {
        self.dither
    }

    /// Encode to LVGL binary (header + pixel data).
    ///
    /// Returns the full byte stream that would normally be written to
    /// `<name>.bin`. Layout: 4-byte little-endian LVGL header followed by
    /// the per-format pixel data (which for indexed formats includes the
    /// palette before the pixel indices).
    ///
    /// Progress events fire via `progress_tx` if `Some`. Cancellation
    /// flows in via `cancel`; checked at row boundaries inside the
    /// per-format encoders. Cancelling before encoding starts (pre-set
    /// `AtomicBool`) also returns `ConvertError::Cancelled` immediately.
    ///
    /// # Errors
    /// * `ConvertError::Cancelled` if `cancel` is set before/during encoding.
    /// * `ConvertError::DimensionTooLarge` if the image dimensions can't
    ///   fit in the 11-bit LVGL header fields.
    /// * Encoder-specific errors from `encoders::{rgb,alpha,indexed}`.
    pub fn encode_bin(
        &self,
        progress_tx: Option<Sender<ConvertEvent>>,
        cancel: Option<Arc<AtomicBool>>,
    ) -> Result<Vec<u8>, ConvertError> {
        let header = encode_lvgl_header(self.fmt, self.img.width(), self.img.height())?;

        if let Some(tx) = &progress_tx {
            let _ = tx.send(ConvertEvent::Start {
                total_rows: self.img.height(),
            });
        }

        let pixels = match self.fmt {
            ColorFormat::Rgb332
            | ColorFormat::Rgb565
            | ColorFormat::Rgb565Swap
            | ColorFormat::Rgb888 => rgb::encode(
                &self.img,
                self.fmt,
                self.dither,
                progress_tx.as_ref(),
                cancel.as_ref(),
            )?,
            ColorFormat::Alpha1
            | ColorFormat::Alpha2
            | ColorFormat::Alpha4
            | ColorFormat::Alpha8 => {
                alpha::encode(&self.img, self.fmt, progress_tx.as_ref(), cancel.as_ref())?
            }
            ColorFormat::Indexed1
            | ColorFormat::Indexed2
            | ColorFormat::Indexed4
            | ColorFormat::Indexed8 => {
                indexed::encode(&self.img, self.fmt, progress_tx.as_ref(), cancel.as_ref())?
            }
        };

        if let Some(tx) = &progress_tx {
            let _ = tx.send(ConvertEvent::Done);
        }

        let mut out = Vec::with_capacity(4 + pixels.len());
        out.extend_from_slice(&header);
        out.extend_from_slice(&pixels);
        Ok(out)
    }

    /// Encode to a `.c` file string with `name` as the array identifier.
    ///
    /// `name` is used for the `{name}_map[]` array and the `lv_img_dsc_t
    /// {name}` descriptor. Caller is responsible for sanitizing it to a
    /// valid C identifier (alphanumeric + underscore).
    ///
    /// # Errors
    /// Propagates encoder errors from `encoders::c_array::encode`.
    pub fn encode_c_array(&self, name: &str) -> Result<String, ConvertError> {
        c_array::encode(&self.img, self.fmt, self.dither, name)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::ImageBuffer;
    use std::sync::atomic::AtomicBool;
    use std::sync::mpsc::channel;

    fn synthesize_png(width: u32, height: u32, color: image::Rgba<u8>) -> Vec<u8> {
        let img = ImageBuffer::from_fn(width, height, |_, _| color);
        let mut buf = std::io::Cursor::new(Vec::new());
        img.write_to(&mut buf, image::ImageFormat::Png).unwrap();
        buf.into_inner()
    }

    #[test]
    fn encode_bin_rgb565_size_is_header_plus_pixels() {
        let png = synthesize_png(16, 16, image::Rgba([255, 0, 0, 255]));
        let conv = Converter::new(&png, ColorFormat::Rgb565, false).unwrap();
        let out = conv.encode_bin(None, None).unwrap();
        // 4-byte header + 16 * 16 * 2 bytes/pixel = 4 + 512 = 516.
        assert_eq!(out.len(), 516);
    }

    #[test]
    fn encode_bin_rgb888_uses_4_bytes_per_pixel() {
        // RGB888 emits B, G, R, A unconditionally (4-byte fix from Group A).
        let png = synthesize_png(8, 8, image::Rgba([10, 20, 30, 200]));
        let conv = Converter::new(&png, ColorFormat::Rgb888, false).unwrap();
        let out = conv.encode_bin(None, None).unwrap();
        // 4-byte header + 8 * 8 * 4 = 4 + 256 = 260.
        assert_eq!(out.len(), 260);
        // First pixel after header: B=30, G=20, R=10, A=200.
        assert_eq!(&out[4..8], &[30, 20, 10, 200]);
    }

    #[test]
    fn encode_bin_progress_events_fire() {
        let png = synthesize_png(4, 3, image::Rgba([100, 100, 100, 255]));
        let (tx, rx) = channel();
        let conv = Converter::new(&png, ColorFormat::Rgb565, false).unwrap();
        conv.encode_bin(Some(tx), None).unwrap();
        let events: Vec<_> = rx.try_iter().collect();
        // Expect: Start, then 3 Progress (one per row), then Done.
        assert!(
            events.len() >= 2,
            "expected at least Start + Done, got {events:?}"
        );
        assert_eq!(events.first(), Some(&ConvertEvent::Start { total_rows: 3 }));
        assert_eq!(events.last(), Some(&ConvertEvent::Done));
    }

    #[test]
    fn encode_bin_pre_cancelled_returns_cancelled() {
        let png = synthesize_png(4, 4, image::Rgba([255, 0, 0, 255]));
        let cancel = Arc::new(AtomicBool::new(true)); // pre-set
        let conv = Converter::new(&png, ColorFormat::Rgb565, false).unwrap();
        let err = conv.encode_bin(None, Some(cancel)).unwrap_err();
        assert!(matches!(err, ConvertError::Cancelled));
    }

    #[test]
    fn encode_c_array_emits_string_with_descriptor() {
        let png = synthesize_png(2, 2, image::Rgba([255, 0, 0, 255]));
        let conv = Converter::new(&png, ColorFormat::Rgb565, false).unwrap();
        let c_text = conv.encode_c_array("test_img").unwrap();
        // Sanity: should contain the descriptor + the user-supplied name.
        assert!(c_text.contains("test_img"));
        assert!(c_text.contains("lv_img_dsc_t") || c_text.contains("LV_IMG_CF"));
    }

    #[test]
    fn accessors_reflect_constructor_args() {
        let png = synthesize_png(7, 5, image::Rgba([0, 0, 0, 255]));
        let conv = Converter::new(&png, ColorFormat::Rgb332, true).unwrap();
        assert_eq!(conv.width(), 7);
        assert_eq!(conv.height(), 5);
        assert_eq!(conv.format(), ColorFormat::Rgb332);
        assert!(conv.dither_enabled());
    }
}
