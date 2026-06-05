//! Image input decoding.
//!
//! Accepts PNG/JPG/BMP byte slices via the `image` crate and converts to a
//! uniform RGBA8 `ImageBuffer` so encoders downstream don't care about source
//! format.

use image::{ImageBuffer, Rgba};

use crate::error::ConvertError;
use crate::header::LVGL_MAX_DIMENSION;

/// Type alias for the canonical RGBA8 input.
pub type Rgba8Image = ImageBuffer<Rgba<u8>, Vec<u8>>;

/// Decode `bytes` (PNG/JPG/BMP) and convert to RGBA8.
///
/// Returns `ConvertError::Decode` on parse failure, or
/// `ConvertError::DimensionTooLarge` if width or height exceeds
/// `LVGL_MAX_DIMENSION` (2047 px).
pub fn decode_image(bytes: &[u8]) -> Result<Rgba8Image, ConvertError> {
    let dyn_img = image::load_from_memory(bytes)?;
    let (w, h) = (dyn_img.width(), dyn_img.height());
    if w > LVGL_MAX_DIMENSION || h > LVGL_MAX_DIMENSION {
        return Err(ConvertError::DimensionTooLarge {
            width: w,
            height: h,
            max: LVGL_MAX_DIMENSION,
        });
    }
    Ok(dyn_img.to_rgba8())
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::{ImageBuffer, Rgba};

    fn synthesize_png(width: u32, height: u32) -> Vec<u8> {
        let mut img = ImageBuffer::<Rgba<u8>, _>::new(width, height);
        for px in img.pixels_mut() {
            *px = Rgba([255, 0, 0, 255]);
        }
        let mut buf = std::io::Cursor::new(Vec::new());
        img.write_to(&mut buf, image::ImageFormat::Png).unwrap();
        buf.into_inner()
    }

    #[test]
    fn decode_synthesized_png_to_rgba() {
        let png = synthesize_png(16, 8);
        let rgba = decode_image(&png).unwrap();
        assert_eq!(rgba.width(), 16);
        assert_eq!(rgba.height(), 8);
        assert_eq!(rgba.get_pixel(0, 0).0, [255, 0, 0, 255]);
    }

    #[test]
    fn decode_oversized_rejected() {
        let png = synthesize_png(2048, 16);
        let err = decode_image(&png).unwrap_err();
        assert!(matches!(
            err,
            ConvertError::DimensionTooLarge { width: 2048, .. }
        ));
    }

    #[test]
    fn decode_garbage_returns_decode_error() {
        let err = decode_image(b"not a png").unwrap_err();
        assert!(matches!(err, ConvertError::Decode(_)));
    }
}
