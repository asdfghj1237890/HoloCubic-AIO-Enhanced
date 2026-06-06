//! Indexed-colour encoders — Indexed1/2/4/8 (palette + bit-packed pixel indices).
//!
//! Algorithm port of `convertor_core.py:141-174` (palette emission) +
//! `convertor_core.py:464-496` (`_conv_px` `CF_INDEXED_*` branches).
//!
//! Output layout (caller prepends the 4-byte LVGL header for `.bin`):
//!
//! 1. **Palette**: `palette_size × 4` bytes. Each entry is `[R, G, B, 0xFF]`.
//!    Unused entries (when actual colour count < `palette_size`) are filled
//!    with `[0xFF, 0xFF, 0xFF, 0xFF]`.
//! 2. **Pixel data**: row-major, MSB-first bit-packed indices.
//!
//! Palette sizes per format:
//!
//! | Format    | Palette entries | Palette bytes | Bits/px |
//! |-----------|-----------------|---------------|---------|
//! | Indexed1  | 2               | 8             | 1       |
//! | Indexed2  | 4               | 16            | 2       |
//! | Indexed4  | 16              | 64            | 4       |
//! | Indexed8  | 256             | 1024          | 8       |
//!
//! Bit-pack direction mirrors alpha: MSB-first within a byte, identical
//! shift expressions to `convertor_core.py:464-496` (`(cx & mask) << (top - x*bpp)`).
//!
//! ## Palette quantization (PRESERVED-DIVERGENCE B10)
//!
//! Python uses `PIL.Image.convert(mode="P", colors=N)` which delegates to
//! PIL's median-cut quantizer. Rust would need a hand-rolled PIL-compatible
//! median-cut to match byte-for-byte. Per Plan 5 Task 5, we instead use a
//! standalone simple median-cut implementation (this module's
//! [`quantize_median_cut`]). Output pixel bytes for indexed formats will
//! differ from `convertor_core.py` for the same input, but visually the
//! image is equivalent. See `BUGS.md` B10.
//!
//! The palette's byte layout (R, G, B, 0xFF) and bit-packing direction
//! still match Python — only the *choice of colours* differs.
//!
//! ## Source colour
//!
//! Input is RGBA8 (`Rgba8Image`). Alpha is dropped — indexed formats only
//! quantize RGB. Python does the same: `Image.convert("P", colors=N)`
//! flattens RGBA → RGB internally before quantizing.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::Sender;
use std::sync::Arc;

use crate::error::ConvertError;
use crate::format::ColorFormat;
use crate::image_input::Rgba8Image;
use crate::progress::ConvertEvent;

/// Encode an image as one of Indexed1/2/4/8.
///
/// Returns the palette + pixel-data section concatenated. Caller prepends
/// the 4-byte LVGL header for `.bin` output.
///
/// `progress_tx` (if `Some`) fires `ConvertEvent::Progress { rows_processed }`
/// after each row of pixel encoding. `cancel` (if `Some`) is checked at
/// row boundaries.
///
/// # Errors
/// * `ConvertError::Cancelled` if `cancel` flips to true between rows.
/// * `ConvertError::Decode` if called with a non-indexed `ColorFormat`.
pub fn encode(
    img: &Rgba8Image,
    fmt: ColorFormat,
    progress_tx: Option<&Sender<ConvertEvent>>,
    cancel: Option<&Arc<AtomicBool>>,
) -> Result<Vec<u8>, ConvertError> {
    let w = img.width();
    let h = img.height();

    let (palette_size, ppb): (usize, u32) = match fmt {
        ColorFormat::Indexed1 => (2, 8),
        ColorFormat::Indexed2 => (4, 4),
        ColorFormat::Indexed4 => (16, 2),
        ColorFormat::Indexed8 => (256, 1),
        _ => {
            return Err(ConvertError::Decode(image::ImageError::Parameter(
                image::error::ParameterError::from_kind(image::error::ParameterErrorKind::Generic(
                    "encode_indexed called with non-indexed ColorFormat".to_owned(),
                )),
            )));
        }
    };

    // Quantize the image to a palette of `palette_size` colours and build the
    // per-pixel index map.
    let (palette, indices) = quantize_median_cut(img, palette_size);

    // 1. Palette: each entry `[R, G, B, 0xFF]`; pad with `[0xFF; 4]`.
    let mut out = Vec::with_capacity(palette_size * 4 + (w * h).div_ceil(ppb) as usize);
    for i in 0..palette_size {
        if let Some(&(r, g, b)) = palette.get(i) {
            out.push(r);
            out.push(g);
            out.push(b);
            out.push(0xFF);
        } else {
            out.extend_from_slice(&[0xFF, 0xFF, 0xFF, 0xFF]);
        }
    }

    // 2. Pixel data: row stride = ceil(w / ppb) bytes.
    let row_bytes = w.div_ceil(ppb) as usize;
    let pixel_data_start = out.len();
    out.resize(pixel_data_start + row_bytes * h as usize, 0);

    for y in 0..h {
        if let Some(c) = cancel {
            if c.load(Ordering::Relaxed) {
                return Err(ConvertError::Cancelled);
            }
        }

        for x in 0..w {
            let cx = indices[(y * w + x) as usize];
            let row_off = pixel_data_start + y as usize * row_bytes;

            match fmt {
                ColorFormat::Indexed1 => {
                    let p = row_off + (x >> 3) as usize;
                    out[p] |= (cx & 0x1) << (7 - (x & 0x7));
                }
                ColorFormat::Indexed2 => {
                    let p = row_off + (x >> 2) as usize;
                    let shift = 6 - ((x & 0x3) * 2);
                    out[p] |= (cx & 0x3) << shift;
                }
                ColorFormat::Indexed4 => {
                    let p = row_off + (x >> 1) as usize;
                    let shift = 4 - ((x & 0x1) * 4);
                    out[p] |= (cx & 0xF) << shift;
                }
                ColorFormat::Indexed8 => {
                    let p = row_off + x as usize;
                    out[p] = cx;
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

/// Quantize `img` (RGBA, alpha dropped) to at most `max_colors` colours via
/// simple median-cut.
///
/// Returns `(palette, indices)` where:
/// * `palette` is a `Vec<(R, G, B)>` of length ≤ `max_colors`. Entries are
///   the centroid of each bucket.
/// * `indices` is a `Vec<u8>` of length `img.width() * img.height()`, the
///   palette index for each pixel in row-major order.
///
/// Note: this does NOT match PIL's median-cut byte-for-byte; see B10 in
/// the crate's BUGS.md.
fn quantize_median_cut(img: &Rgba8Image, max_colors: usize) -> (Vec<(u8, u8, u8)>, Vec<u8>) {
    let pixels: Vec<(u8, u8, u8)> = img.pixels().map(|p| (p[0], p[1], p[2])).collect();

    // Special case: max_colors >= unique colours → 1 bucket per unique colour
    // (fast path AND ensures Indexed1 on a 1-colour image gets 1 entry, not 2).
    // We use the general median-cut path which handles this naturally when
    // bucket halving produces zero-volume buckets.

    // Initial bucket: all pixel indices.
    let initial_indices: Vec<usize> = (0..pixels.len()).collect();
    let mut buckets: Vec<Vec<usize>> = vec![initial_indices];

    // Split until we hit max_colors or no bucket can be split further
    // (range == 0 on all channels).
    while buckets.len() < max_colors {
        // Find the bucket with the largest range on any channel.
        let mut best_bucket: Option<usize> = None;
        let mut best_range: i32 = 0;
        let mut best_axis: usize = 0;

        for (bi, bucket) in buckets.iter().enumerate() {
            if bucket.len() < 2 {
                continue;
            }
            let (min_r, max_r, min_g, max_g, min_b, max_b) = channel_bounds(bucket, &pixels);
            let dr = max_r as i32 - min_r as i32;
            let dg = max_g as i32 - min_g as i32;
            let db = max_b as i32 - min_b as i32;
            let (range, axis) = if dr >= dg && dr >= db {
                (dr, 0)
            } else if dg >= db {
                (dg, 1)
            } else {
                (db, 2)
            };
            if range > best_range {
                best_range = range;
                best_bucket = Some(bi);
                best_axis = axis;
            }
        }

        let Some(bi) = best_bucket else {
            break; // no bucket can be split further
        };
        if best_range == 0 {
            break;
        }

        // Sort the chosen bucket by the chosen axis, split at the median.
        let mut bucket = std::mem::take(&mut buckets[bi]);
        bucket.sort_by_key(|&idx| match best_axis {
            0 => pixels[idx].0,
            1 => pixels[idx].1,
            _ => pixels[idx].2,
        });
        let mid = bucket.len() / 2;
        let right = bucket.split_off(mid);
        buckets[bi] = bucket;
        buckets.push(right);
    }

    // Compute centroid of each bucket = the palette entry. Skip empty buckets.
    let mut palette: Vec<(u8, u8, u8)> = Vec::with_capacity(buckets.len());
    let mut bucket_palette_idx: Vec<Option<u8>> = Vec::with_capacity(buckets.len());
    for bucket in &buckets {
        if bucket.is_empty() {
            bucket_palette_idx.push(None);
            continue;
        }
        let mut sr: u64 = 0;
        let mut sg: u64 = 0;
        let mut sb: u64 = 0;
        for &idx in bucket {
            sr += pixels[idx].0 as u64;
            sg += pixels[idx].1 as u64;
            sb += pixels[idx].2 as u64;
        }
        let n = bucket.len() as u64;
        let r = (sr / n) as u8;
        let g = (sg / n) as u8;
        let b = (sb / n) as u8;
        bucket_palette_idx.push(Some(palette.len() as u8));
        palette.push((r, g, b));
    }

    // Build per-pixel index map.
    let mut indices = vec![0u8; pixels.len()];
    for (bi, bucket) in buckets.iter().enumerate() {
        let Some(pi) = bucket_palette_idx[bi] else {
            continue;
        };
        for &px_idx in bucket {
            indices[px_idx] = pi;
        }
    }

    (palette, indices)
}

/// Min/max of R, G, B across the given subset of pixels.
/// Returns `(min_r, max_r, min_g, max_g, min_b, max_b)`.
fn channel_bounds(bucket: &[usize], pixels: &[(u8, u8, u8)]) -> (u8, u8, u8, u8, u8, u8) {
    let mut min_r: u8 = 255;
    let mut max_r: u8 = 0;
    let mut min_g: u8 = 255;
    let mut max_g: u8 = 0;
    let mut min_b: u8 = 255;
    let mut max_b: u8 = 0;
    for &i in bucket {
        let (r, g, b) = pixels[i];
        if r < min_r {
            min_r = r;
        }
        if r > max_r {
            max_r = r;
        }
        if g < min_g {
            min_g = g;
        }
        if g > max_g {
            max_g = g;
        }
        if b < min_b {
            min_b = b;
        }
        if b > max_b {
            max_b = b;
        }
    }
    (min_r, max_r, min_g, max_g, min_b, max_b)
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::ImageBuffer;

    fn solid_rgb(w: u32, h: u32, r: u8, g: u8, b: u8) -> Rgba8Image {
        ImageBuffer::from_fn(w, h, |_, _| image::Rgba([r, g, b, 255]))
    }

    #[test]
    fn indexed1_solid_color_palette_layout() {
        // Solid red (255, 0, 0). Indexed1 → 2-entry palette + pixel data.
        // Median-cut returns 1 bucket (range=0 on all axes after first iter).
        // Palette: entry 0 = (255, 0, 0, 0xFF); entry 1 = padding (0xFF*4).
        // Pixel data: all index 0 → all zero bits.
        let img = solid_rgb(8, 1, 255, 0, 0);
        let out = encode(&img, ColorFormat::Indexed1, None, None).unwrap();
        // Palette is 8 bytes (2 entries × 4).
        assert_eq!(out.len(), 8 + 1); // + 1 byte pixel data (8 pixels @ 1 bit)
        assert_eq!(&out[0..4], &[255, 0, 0, 0xFF]); // entry 0 = R,G,B,FF
        assert_eq!(&out[4..8], &[0xFF, 0xFF, 0xFF, 0xFF]); // entry 1 padded
        assert_eq!(out[8], 0x00); // pixel data all index 0
    }

    #[test]
    fn indexed1_palette_size_8_bytes() {
        let img = solid_rgb(4, 4, 100, 100, 100);
        let out = encode(&img, ColorFormat::Indexed1, None, None).unwrap();
        // Palette = 8 bytes, pixel data = 4*4=16 pixels, ceil(4/8)=1 byte/row × 4 rows = 4.
        assert_eq!(out.len(), 8 + 4);
    }

    #[test]
    fn indexed2_palette_size_16_bytes() {
        let img = solid_rgb(4, 4, 50, 50, 50);
        let out = encode(&img, ColorFormat::Indexed2, None, None).unwrap();
        // Palette = 4*4 = 16 bytes. Pixel data = ceil(4/4)=1 byte/row × 4 rows = 4.
        assert_eq!(out.len(), 16 + 4);
    }

    #[test]
    fn indexed4_palette_size_64_bytes() {
        let img = solid_rgb(2, 2, 50, 50, 50);
        let out = encode(&img, ColorFormat::Indexed4, None, None).unwrap();
        // Palette = 16*4 = 64 bytes. Pixel data = ceil(2/2)=1 byte/row × 2 = 2.
        assert_eq!(out.len(), 64 + 2);
    }

    #[test]
    fn indexed8_palette_size_1024_bytes() {
        let img = solid_rgb(2, 2, 50, 50, 50);
        let out = encode(&img, ColorFormat::Indexed8, None, None).unwrap();
        // Palette = 256*4 = 1024 bytes. Pixel data = 4 bytes (1 per pixel).
        assert_eq!(out.len(), 1024 + 4);
    }

    #[test]
    fn indexed1_checkerboard_two_distinct_colors() {
        // 2×2 checkerboard: alternating black & white.
        let mut img = ImageBuffer::from_pixel(2, 2, image::Rgba([0, 0, 0, 255]));
        img.put_pixel(1, 0, image::Rgba([255, 255, 255, 255]));
        img.put_pixel(0, 1, image::Rgba([255, 255, 255, 255]));
        let out = encode(&img, ColorFormat::Indexed1, None, None).unwrap();
        // Palette must contain two distinct entries.
        let entry0 = &out[0..3];
        let entry1 = &out[4..7];
        assert_ne!(entry0, entry1);
        // Both entries' 4th byte = 0xFF (RGBA alpha=full).
        assert_eq!(out[3], 0xFF);
        assert_eq!(out[7], 0xFF);
    }

    #[test]
    fn indexed1_pixel_data_indices_round_trip() {
        // For an 8-pixel-wide row of a solid colour, ALL bits must be the
        // same (single-bucket → all idx 0 → all 0 bits).
        let img = solid_rgb(8, 1, 200, 200, 200);
        let out = encode(&img, ColorFormat::Indexed1, None, None).unwrap();
        assert_eq!(out[8], 0x00); // 8 pixels of index 0 → 0b00000000
    }

    #[test]
    fn indexed8_index_byte_per_pixel() {
        let img = solid_rgb(4, 1, 128, 64, 200);
        let out = encode(&img, ColorFormat::Indexed8, None, None).unwrap();
        // Palette is 1024 bytes; pixel data starts at offset 1024.
        // All pixels solid → all index 0.
        assert_eq!(&out[1024..1028], &[0, 0, 0, 0]);
        // First palette entry is our colour.
        assert_eq!(&out[0..4], &[128, 64, 200, 0xFF]);
    }

    #[test]
    fn quantize_single_color_returns_one_entry() {
        let img = solid_rgb(4, 4, 100, 50, 25);
        let (palette, indices) = quantize_median_cut(&img, 16);
        assert_eq!(palette.len(), 1);
        assert_eq!(palette[0], (100, 50, 25));
        assert!(indices.iter().all(|&i| i == 0));
    }

    #[test]
    fn quantize_two_colors_two_buckets() {
        // 4×1 image: 2 black + 2 white pixels.
        let mut img = ImageBuffer::from_pixel(4, 1, image::Rgba([0, 0, 0, 255]));
        img.put_pixel(2, 0, image::Rgba([255, 255, 255, 255]));
        img.put_pixel(3, 0, image::Rgba([255, 255, 255, 255]));
        let (palette, _) = quantize_median_cut(&img, 4);
        assert!(palette.len() >= 2);
        // One entry must be black-ish, another white-ish.
        let has_black = palette.iter().any(|&(r, g, b)| r < 50 && g < 50 && b < 50);
        let has_white = palette
            .iter()
            .any(|&(r, g, b)| r > 200 && g > 200 && b > 200);
        assert!(has_black, "expected a black-ish palette entry");
        assert!(has_white, "expected a white-ish palette entry");
    }

    #[test]
    fn non_indexed_format_returns_error() {
        let img = solid_rgb(2, 2, 0, 0, 0);
        let err = encode(&img, ColorFormat::Rgb565, None, None).unwrap_err();
        assert!(matches!(err, ConvertError::Decode(_)));
    }

    #[test]
    fn cancel_between_rows() {
        let img = solid_rgb(4, 100, 0, 0, 0);
        let cancel = Arc::new(AtomicBool::new(true));
        let err = encode(&img, ColorFormat::Indexed1, None, Some(&cancel)).unwrap_err();
        assert!(matches!(err, ConvertError::Cancelled));
    }
}
