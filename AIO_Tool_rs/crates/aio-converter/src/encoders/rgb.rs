//! RGB encoders — RGB332, RGB565, RGB565_SWAP, RGB888.
//!
//! Algorithm port of `convertor_core.py:399-498` (`_conv_px` RGB branches).
//! Pixel data layout matches Python byte-for-byte:
//!
//! * **RGB332** — 1 byte/px: `r_act | (g_act >> 3) | (b_act >> 6)`. Since
//!   `r_act` is already 3-bit-quantized (top 3 bits set, rest zero), the
//!   formula is equivalent to `(r_act & 0xE0) | ((g_act >> 3) & 0x1C) | (b_act >> 6)`.
//! * **RGB565** — 2 bytes/px, **low byte first** (LE pixel data).
//! * **RGB565_SWAP** — 2 bytes/px, **high byte first** (BE pixel data).
//! * **RGB888** — 3 bytes/px in **B, G, R** order (NOT R, G, B; per
//!   `convertor_core.py:428-430`). The legacy Python emits a 4th `a` byte
//!   after BGR, but this group doesn't expose alpha-on-RGB (see plan).
//!
//! Floyd-Steinberg is applied when `dither` is true via [`crate::dither::Dither`].
//! Per-channel saturation clamps mirror Python's explicit
//! `if r_act > 0xE0: r_act = 0xE0`-style guards (RGB332: 0xE0/0xE0/0xC0,
//! RGB565: 0xF8/0xFC/0xF8, RGB888: 0xFF/0xFF/0xFF).

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::Sender;
use std::sync::Arc;

use crate::dither::{classify_pixel, Dither};
use crate::error::ConvertError;
use crate::format::ColorFormat;
use crate::image_input::Rgba8Image;
use crate::progress::ConvertEvent;

/// Encode an RGB image into one of RGB332/565/565_swap/888.
///
/// The output is the pixel-data section only — no LVGL header (use
/// `crate::header::encode_lvgl_header` for that and prepend separately).
///
/// `progress_tx` (if `Some`) fires `ConvertEvent::Progress { rows_processed }`
/// after each row. `cancel` (if `Some`) is checked at row boundaries and
/// aborts with `ConvertError::Cancelled` between rows.
///
/// # Errors
/// Returns `ConvertError::Cancelled` if `cancel` flips to true between rows.
///
/// # Panics
/// Panics in debug if called with a non-RGB ColorFormat (Indexed*/Alpha*).
/// Release builds return a `ConvertError::Decode` for the same case.
pub fn encode(
    img: &Rgba8Image,
    fmt: ColorFormat,
    dither: bool,
    progress_tx: Option<&Sender<ConvertEvent>>,
    cancel: Option<&Arc<AtomicBool>>,
) -> Result<Vec<u8>, ConvertError> {
    let w = img.width();
    let h = img.height();

    // Per-format bit depths + saturation caps. Matches convertor_core.py
    // lines 515-549.
    let (bits_r, bits_g, bits_b, max_rgb) = match fmt {
        ColorFormat::Rgb332 => (3, 3, 2, (0xE0u8, 0xE0u8, 0xC0u8)),
        ColorFormat::Rgb565 | ColorFormat::Rgb565Swap => (5, 6, 5, (0xF8u8, 0xFCu8, 0xF8u8)),
        ColorFormat::Rgb888 => (8, 8, 8, (0xFFu8, 0xFFu8, 0xFFu8)),
        _ => {
            return Err(ConvertError::Decode(image::ImageError::Parameter(
                image::error::ParameterError::from_kind(image::error::ParameterErrorKind::Generic(
                    "encode_rgb called with non-RGB ColorFormat".to_owned(),
                )),
            )));
        }
    };

    let bytes_per_pixel: u32 = match fmt {
        ColorFormat::Rgb332 => 1,
        ColorFormat::Rgb565 | ColorFormat::Rgb565Swap => 2,
        ColorFormat::Rgb888 => 3,
        _ => unreachable!(),
    };
    let mut out = Vec::with_capacity((w * h * bytes_per_pixel) as usize);

    let mut dither_state = if dither {
        Some(Dither::new(w, bits_r, bits_g, bits_b))
    } else {
        None
    };

    for y in 0..h {
        if let Some(c) = cancel {
            if c.load(Ordering::Relaxed) {
                return Err(ConvertError::Cancelled);
            }
        }

        if let Some(d) = &mut dither_state {
            d.reset_row();
        }

        for x in 0..w {
            let px = img.get_pixel(x, y);
            let (r, g, b) = (px[0], px[1], px[2]);

            let (r_act, g_act, b_act) = if let Some(d) = &mut dither_state {
                d.next(r, g, b, x, max_rgb)
            } else {
                // Non-dither path: classify + clamp per channel (matches
                // convertor_core.py:571-605).
                (
                    classify_pixel(r as i32, bits_r).min(max_rgb.0),
                    classify_pixel(g as i32, bits_g).min(max_rgb.1),
                    classify_pixel(b as i32, bits_b).min(max_rgb.2),
                )
            };

            match fmt {
                ColorFormat::Rgb332 => {
                    // c8 = r_act | (g_act >> 3) | (b_act >> 6)
                    // r_act top 3 bits = 0xE0 mask is already in place.
                    let c8 = r_act | (g_act >> 3) | (b_act >> 6);
                    out.push(c8);
                }
                ColorFormat::Rgb565 => {
                    let c16 = ((r_act as u16) << 8) | ((g_act as u16) << 3) | ((b_act as u16) >> 3);
                    out.push((c16 & 0xFF) as u8); // low byte first (LE pixel data)
                    out.push((c16 >> 8) as u8);
                }
                ColorFormat::Rgb565Swap => {
                    let c16 = ((r_act as u16) << 8) | ((g_act as u16) << 3) | ((b_act as u16) >> 3);
                    out.push((c16 >> 8) as u8); // high byte first (swapped)
                    out.push((c16 & 0xFF) as u8);
                }
                ColorFormat::Rgb888 => {
                    // B, G, R per convertor_core.py:428-430.
                    out.push(b_act);
                    out.push(g_act);
                    out.push(r_act);
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

    fn solid(w: u32, h: u32, r: u8, g: u8, b: u8) -> Rgba8Image {
        ImageBuffer::from_fn(w, h, |_, _| image::Rgba([r, g, b, 255]))
    }

    #[test]
    fn rgb332_solid_red() {
        // Red = (255, 0, 0). bits_r=3, bits_g=3, bits_b=2.
        // classify(255, 3) = ceil(255/32)*32 = 8*32 = 256 → clamped to 0xE0.
        // classify(0, 3) = 0; classify(0, 2) = 0.
        // c8 = 0xE0 | 0 | 0 = 0xE0.
        let img = solid(2, 2, 255, 0, 0);
        let out = encode(&img, ColorFormat::Rgb332, false, None, None).unwrap();
        assert_eq!(out, vec![0xE0, 0xE0, 0xE0, 0xE0]);
    }

    #[test]
    fn rgb565_solid_red_le_pixel_data() {
        // Red = (255, 0, 0). r_act after classify+clamp = 0xF8. g=0, b=0.
        // c16 = (0xF8 << 8) = 0xF800. LE byte order: [0x00, 0xF8].
        let img = solid(2, 1, 255, 0, 0);
        let out = encode(&img, ColorFormat::Rgb565, false, None, None).unwrap();
        assert_eq!(out, vec![0x00, 0xF8, 0x00, 0xF8]);
    }

    #[test]
    fn rgb565_swap_solid_red_high_byte_first() {
        let img = solid(1, 1, 255, 0, 0);
        let out = encode(&img, ColorFormat::Rgb565Swap, false, None, None).unwrap();
        assert_eq!(out, vec![0xF8, 0x00]);
    }

    #[test]
    fn rgb888_solid_color_emits_bgr_order() {
        // R=10, G=20, B=30. 8-bit classify is no-op; output is B, G, R.
        let img = solid(1, 1, 10, 20, 30);
        let out = encode(&img, ColorFormat::Rgb888, false, None, None).unwrap();
        assert_eq!(out, vec![30, 20, 10]);
    }

    #[test]
    fn rgb565_size_equals_2_wh() {
        let img = solid(8, 4, 100, 150, 200);
        let out = encode(&img, ColorFormat::Rgb565, false, None, None).unwrap();
        assert_eq!(out.len(), 8 * 4 * 2);
    }

    #[test]
    fn rgb_encode_with_dither_matches_non_dither_on_quantization_aligned_solid() {
        // For dither to match no-dither byte-for-byte on a solid color, each
        // channel must be exactly representable in its target depth. For
        // RGB565: R/B 5-bit (multiple of 8), G 6-bit (multiple of 4).
        // 200 = 0xC8 (÷8=25), 100 = 0x64 (÷4=25), 48 = 0x30 (÷8=6).
        let img = solid(4, 4, 200, 100, 48);
        let no_dith = encode(&img, ColorFormat::Rgb565, false, None, None).unwrap();
        let with_dith = encode(&img, ColorFormat::Rgb565, true, None, None).unwrap();
        assert_eq!(no_dith, with_dith);
    }

    #[test]
    fn rgb_encode_with_dither_produces_correct_size() {
        // Even on a non-aligned solid color, dither must produce the right
        // number of output bytes and never panic.
        let img = solid(8, 4, 199, 99, 51);
        let out = encode(&img, ColorFormat::Rgb565, true, None, None).unwrap();
        assert_eq!(out.len(), 8 * 4 * 2);
    }
}
