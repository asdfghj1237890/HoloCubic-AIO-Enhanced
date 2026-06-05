//! LVGL 4-byte binary header encoding.

use byteorder::{ByteOrder, LittleEndian};

use crate::error::ConvertError;
use crate::format::ColorFormat;

/// LVGL's max width/height per the 11-bit header fields.
pub const LVGL_MAX_DIMENSION: u32 = 2047;

/// Encode the LVGL header u32 into 4 LE bytes.
///
/// Layout (per `convertor_core.py:389`):
/// ```text
/// header_u32 = lv_cf | (width << 10) | (height << 21)
/// ```
///
/// Returns `ConvertError::DimensionTooLarge` if either dimension exceeds
/// `LVGL_MAX_DIMENSION` — Python silently overflowed the height field
/// when width was too large; Rust rejects (Plan 5 D6).
pub fn encode_lvgl_header(
    fmt: ColorFormat,
    width: u32,
    height: u32,
) -> Result<[u8; 4], ConvertError> {
    if width > LVGL_MAX_DIMENSION || height > LVGL_MAX_DIMENSION {
        return Err(ConvertError::DimensionTooLarge {
            width,
            height,
            max: LVGL_MAX_DIMENSION,
        });
    }
    let header_u32 = fmt.lv_cf() | (width << 10) | (height << 21);
    let mut out = [0u8; 4];
    LittleEndian::write_u32(&mut out, header_u32);
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn encode_small_rgb565() {
        // lv_cf=4, width=32, height=32 → 4 | (32<<10) | (32<<21)
        //   = 4 | 32768 | 67108864 = 67141636 = 0x0400_8004
        let bytes = encode_lvgl_header(ColorFormat::Rgb565, 32, 32).unwrap();
        assert_eq!(bytes, [0x04, 0x80, 0x00, 0x04]);
    }

    #[test]
    fn encode_indexed4_64x64() {
        // lv_cf=9, width=64, height=64 → 9 | (64<<10) | (64<<21)
        let header_u32 = 9 | (64 << 10) | (64 << 21);
        let bytes = encode_lvgl_header(ColorFormat::Indexed4, 64, 64).unwrap();
        let mut expected = [0u8; 4];
        LittleEndian::write_u32(&mut expected, header_u32);
        assert_eq!(bytes, expected);
    }

    #[test]
    fn dimension_at_max_ok() {
        // 2047 × 2047 — the largest legal LVGL image.
        encode_lvgl_header(ColorFormat::Rgb565, 2047, 2047).unwrap();
    }

    #[test]
    fn width_too_large_rejected() {
        let err = encode_lvgl_header(ColorFormat::Rgb565, 2048, 100).unwrap_err();
        assert!(matches!(
            err,
            ConvertError::DimensionTooLarge {
                width: 2048,
                height: 100,
                max: 2047
            }
        ));
    }

    #[test]
    fn height_too_large_rejected() {
        let err = encode_lvgl_header(ColorFormat::Rgb565, 100, 2048).unwrap_err();
        assert!(matches!(err, ConvertError::DimensionTooLarge { .. }));
    }
}
