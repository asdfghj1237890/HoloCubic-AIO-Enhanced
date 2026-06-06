//! Alpha-only encoders — Alpha1/2/4/8.
//!
//! Algorithm port of `convertor_core.py:432-462` (`_conv_px` `CF_ALPHA_*` branches).
//!
//! Output is the pixel-data section only — no LVGL header / no palette
//! (alpha formats have no palette). The alpha source is the input's
//! RGBA `a` channel directly; the rest of the pixel is ignored.
//!
//! ## Bit-packing direction
//!
//! All multi-pixel-per-byte alpha formats pack **MSB-first within the byte**,
//! matching `convertor_core.py:432-462`:
//!
//! * **Alpha1**: 1 bit/pixel, threshold at `a > 0x80`. Bit position
//!   `7 - (x & 0x7)`. (i.e. pixel x=0 → bit 7, pixel x=7 → bit 0.)
//! * **Alpha2**: 2 bits/pixel, value = `a >> 6`. Bit position
//!   `6 - ((x & 0x3) * 2)`. (x=0 → bits 7..6, x=3 → bits 1..0.)
//! * **Alpha4**: 4 bits/pixel, value = `a >> 4`. Bit position
//!   `4 - ((x & 0x1) * 4)`. (x=0 → bits 7..4, x=1 → bits 3..0.)
//! * **Alpha8**: 1 byte/pixel, value = `a` directly.
//!
//! Row stride is `ceil(width / pixels_per_byte)` bytes. Partial trailing
//! bytes at the right edge keep their unused bit positions as zero.
//!
//! ## Source alpha
//!
//! Python derives alpha from `c[3] if len(c) == 4 else 0xFF`; since our
//! input is always `Rgba8Image` (a.k.a. RGBA8), `a = pixel[3]` always.
//! Sources with no alpha channel are already converted to RGBA8 by
//! `image_input::decode_image`, which fills `a = 0xFF`.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::Sender;
use std::sync::Arc;

use crate::error::ConvertError;
use crate::format::ColorFormat;
use crate::image_input::Rgba8Image;
use crate::progress::ConvertEvent;

/// Encode an image as one of Alpha1/Alpha2/Alpha4/Alpha8.
///
/// Output is the pixel-data section only — caller prepends the 4-byte LVGL
/// header via [`crate::header::encode_lvgl_header`] for `.bin` output.
///
/// `progress_tx` (if `Some`) fires `ConvertEvent::Progress { rows_processed }`
/// after each row. `cancel` (if `Some`) is checked at row boundaries and
/// aborts with `ConvertError::Cancelled`.
///
/// # Errors
/// * `ConvertError::Cancelled` if `cancel` flips to true between rows.
/// * `ConvertError::Decode` if called with a non-alpha `ColorFormat`.
pub fn encode(
    img: &Rgba8Image,
    fmt: ColorFormat,
    progress_tx: Option<&Sender<ConvertEvent>>,
    cancel: Option<&Arc<AtomicBool>>,
) -> Result<Vec<u8>, ConvertError> {
    let w = img.width();
    let h = img.height();

    // Pixels per byte (PPB) determines row stride and bit-packing layout.
    let ppb: u32 = match fmt {
        ColorFormat::Alpha1 => 8,
        ColorFormat::Alpha2 => 4,
        ColorFormat::Alpha4 => 2,
        ColorFormat::Alpha8 => 1,
        _ => {
            return Err(ConvertError::Decode(image::ImageError::Parameter(
                image::error::ParameterError::from_kind(image::error::ParameterErrorKind::Generic(
                    "encode_alpha called with non-alpha ColorFormat".to_owned(),
                )),
            )));
        }
    };

    // Row stride in bytes: ceil(w / ppb).
    let row_bytes = w.div_ceil(ppb) as usize;
    let mut out = vec![0u8; row_bytes * h as usize];

    for y in 0..h {
        if let Some(c) = cancel {
            if c.load(Ordering::Relaxed) {
                return Err(ConvertError::Cancelled);
            }
        }

        for x in 0..w {
            let a = img.get_pixel(x, y)[3];
            let row_off = y as usize * row_bytes;

            match fmt {
                ColorFormat::Alpha1 => {
                    // 1 bit/px MSB-first: bit `7 - (x & 0x7)`.
                    // Set iff a > 0x80 (matches Python).
                    if a > 0x80 {
                        let p = row_off + (x >> 3) as usize;
                        out[p] |= 1 << (7 - (x & 0x7));
                    }
                }
                ColorFormat::Alpha2 => {
                    // 2 bits/px MSB-first: value `a >> 6` at bit `6 - ((x & 0x3) * 2)`.
                    let p = row_off + (x >> 2) as usize;
                    let v = a >> 6;
                    let shift = 6 - ((x & 0x3) * 2);
                    out[p] |= v << shift;
                }
                ColorFormat::Alpha4 => {
                    // 4 bits/px MSB-first: value `a >> 4` at bit `4 - ((x & 0x1) * 4)`.
                    let p = row_off + (x >> 1) as usize;
                    let v = a >> 4;
                    let shift = 4 - ((x & 0x1) * 4);
                    out[p] |= v << shift;
                }
                ColorFormat::Alpha8 => {
                    let p = row_off + x as usize;
                    out[p] = a;
                }
                _ => unreachable!(),
            }
        }

        if let Some(tx) = progress_tx {
            let _ = tx.send(ConvertEvent::Progress {
                rows_processed: y + 1,
            });
        }
    }

    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::ImageBuffer;

    fn solid_alpha(w: u32, h: u32, a: u8) -> Rgba8Image {
        ImageBuffer::from_fn(w, h, |_, _| image::Rgba([0, 0, 0, a]))
    }

    #[test]
    fn alpha1_solid_opaque_8x1_one_byte_all_ones() {
        // 8 pixels @ a=255 (>0x80) → 1 byte = 0xFF.
        let img = solid_alpha(8, 1, 255);
        let out = encode(&img, ColorFormat::Alpha1, None, None).unwrap();
        assert_eq!(out, vec![0xFF]);
    }

    #[test]
    fn alpha1_solid_transparent_8x1_one_byte_all_zero() {
        let img = solid_alpha(8, 1, 0);
        let out = encode(&img, ColorFormat::Alpha1, None, None).unwrap();
        assert_eq!(out, vec![0x00]);
    }

    #[test]
    fn alpha1_threshold_at_0x80_strict_gt() {
        // a=0x80 (exactly) → NOT set per Python `if a > 0x80`.
        let img = solid_alpha(8, 1, 0x80);
        let out = encode(&img, ColorFormat::Alpha1, None, None).unwrap();
        assert_eq!(out, vec![0x00]);
        // a=0x81 → SET.
        let img = solid_alpha(8, 1, 0x81);
        let out = encode(&img, ColorFormat::Alpha1, None, None).unwrap();
        assert_eq!(out, vec![0xFF]);
    }

    #[test]
    fn alpha1_msb_first_bit_layout() {
        // 8-pixel row: x=0..7. Alpha = [0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0]
        // → bits at positions 7,5,3,1 set → 0b10101010 = 0xAA.
        let mut img = ImageBuffer::from_pixel(8, 1, image::Rgba([0, 0, 0, 0]));
        for x in (0..8).step_by(2) {
            img.put_pixel(x, 0, image::Rgba([0, 0, 0, 0xFF]));
        }
        let out = encode(&img, ColorFormat::Alpha1, None, None).unwrap();
        assert_eq!(out, vec![0xAA]);
    }

    #[test]
    fn alpha1_row_stride_ceils_for_partial_byte() {
        // 9-pixel-wide image needs 2 bytes per row (ceil(9/8)).
        // All opaque → first byte 0xFF, second byte's bit 7 set → 0x80.
        let img = solid_alpha(9, 1, 255);
        let out = encode(&img, ColorFormat::Alpha1, None, None).unwrap();
        assert_eq!(out, vec![0xFF, 0x80]);
        assert_eq!(out.len(), 2);
    }

    #[test]
    fn alpha2_solid_opaque() {
        // 4 pixels @ a=255: value = 255 >> 6 = 3 = 0b11.
        // Packed @ bits 7..6, 5..4, 3..2, 1..0 → 0b11_11_11_11 = 0xFF.
        let img = solid_alpha(4, 1, 255);
        let out = encode(&img, ColorFormat::Alpha2, None, None).unwrap();
        assert_eq!(out, vec![0xFF]);
    }

    #[test]
    fn alpha2_msb_first_per_pixel() {
        // x=0: value (a=255>>6)=3 at shift=6 → bits 7..6 = 11
        // x=1: a=0 at shift=4 → bits 5..4 = 00
        // x=2: a=255 at shift=2 → bits 3..2 = 11
        // x=3: a=0 at shift=0 → bits 1..0 = 00
        // → 0b11_00_11_00 = 0xCC.
        let mut img = ImageBuffer::from_pixel(4, 1, image::Rgba([0, 0, 0, 0]));
        img.put_pixel(0, 0, image::Rgba([0, 0, 0, 255]));
        img.put_pixel(2, 0, image::Rgba([0, 0, 0, 255]));
        let out = encode(&img, ColorFormat::Alpha2, None, None).unwrap();
        assert_eq!(out, vec![0xCC]);
    }

    #[test]
    fn alpha4_solid_opaque() {
        // 2 pixels @ a=255: value = 255>>4 = 15 = 0xF.
        // x=0 at shift=4 → top nibble; x=1 at shift=0 → bottom nibble → 0xFF.
        let img = solid_alpha(2, 1, 255);
        let out = encode(&img, ColorFormat::Alpha4, None, None).unwrap();
        assert_eq!(out, vec![0xFF]);
    }

    #[test]
    fn alpha4_high_nibble_first() {
        // 2 pixels: x=0 a=0xA0 → 0xA shifted to high nibble → 0xA0.
        //          x=1 a=0x50 → 0x5 shifted to low nibble → 0x05.
        // Combined byte = 0xA5.
        let mut img = ImageBuffer::from_pixel(2, 1, image::Rgba([0, 0, 0, 0]));
        img.put_pixel(0, 0, image::Rgba([0, 0, 0, 0xA0]));
        img.put_pixel(1, 0, image::Rgba([0, 0, 0, 0x50]));
        let out = encode(&img, ColorFormat::Alpha4, None, None).unwrap();
        assert_eq!(out, vec![0xA5]);
    }

    #[test]
    fn alpha8_solid_opaque_8x1() {
        let img = solid_alpha(8, 1, 0xFF);
        let out = encode(&img, ColorFormat::Alpha8, None, None).unwrap();
        assert_eq!(out, vec![0xFF; 8]);
    }

    #[test]
    fn alpha8_solid_transparent_8x1() {
        let img = solid_alpha(8, 1, 0);
        let out = encode(&img, ColorFormat::Alpha8, None, None).unwrap();
        assert_eq!(out, vec![0x00; 8]);
    }

    #[test]
    fn alpha8_per_pixel_passthrough() {
        let mut img = ImageBuffer::from_pixel(4, 1, image::Rgba([0, 0, 0, 0]));
        img.put_pixel(0, 0, image::Rgba([0, 0, 0, 0x10]));
        img.put_pixel(1, 0, image::Rgba([0, 0, 0, 0x40]));
        img.put_pixel(2, 0, image::Rgba([0, 0, 0, 0x80]));
        img.put_pixel(3, 0, image::Rgba([0, 0, 0, 0xC0]));
        let out = encode(&img, ColorFormat::Alpha8, None, None).unwrap();
        assert_eq!(out, vec![0x10, 0x40, 0x80, 0xC0]);
    }

    #[test]
    fn output_size_alpha1() {
        // 16×4 → 2 bytes/row × 4 rows = 8 bytes.
        let img = solid_alpha(16, 4, 255);
        let out = encode(&img, ColorFormat::Alpha1, None, None).unwrap();
        assert_eq!(out.len(), 8);
    }

    #[test]
    fn output_size_alpha4() {
        // 17×3 → ceil(17/2) = 9 bytes/row × 3 rows = 27 bytes.
        let img = solid_alpha(17, 3, 255);
        let out = encode(&img, ColorFormat::Alpha4, None, None).unwrap();
        assert_eq!(out.len(), 27);
    }

    #[test]
    fn non_alpha_format_returns_error() {
        let img = solid_alpha(2, 2, 255);
        let err = encode(&img, ColorFormat::Rgb565, None, None).unwrap_err();
        assert!(matches!(err, ConvertError::Decode(_)));
    }

    #[test]
    fn cancel_between_rows() {
        let img = solid_alpha(4, 100, 0xFF);
        let cancel = Arc::new(AtomicBool::new(true));
        let err = encode(&img, ColorFormat::Alpha8, None, Some(&cancel)).unwrap_err();
        assert!(matches!(err, ConvertError::Cancelled));
    }
}
