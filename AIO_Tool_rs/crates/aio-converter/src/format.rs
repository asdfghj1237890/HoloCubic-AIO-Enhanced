//! Color format enum + LVGL header value mapping.

/// One of the 11 supported LVGL pixel formats.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum ColorFormat {
    /// 8-bit RGB332 — 3 R + 3 G + 2 B bits packed in 1 byte.
    Rgb332,
    /// 16-bit RGB565 little-endian.
    Rgb565,
    /// 16-bit RGB565 with byte-swapped pixel data (big-endian per pixel).
    Rgb565Swap,
    /// 24-bit RGB888 — 3 bytes per pixel.
    Rgb888,
    /// 1-bit indexed — 2-entry palette + packed bits.
    Indexed1,
    /// 2-bit indexed — 4-entry palette + packed bits.
    Indexed2,
    /// 4-bit indexed — 16-entry palette + packed nibbles.
    Indexed4,
    /// 8-bit indexed — 256-entry palette + 1 byte per pixel.
    Indexed8,
    /// 1-bit alpha-only.
    Alpha1,
    /// 2-bit alpha-only.
    Alpha2,
    /// 4-bit alpha-only.
    Alpha4,
    /// 8-bit alpha-only.
    Alpha8,
}

impl ColorFormat {
    /// The `lv_cf` value placed in the 4-byte LVGL header.
    ///
    /// PRESERVED-QUIRK (B7): All four TRUE_COLOR_* variants (332/565/565_swap/888)
    /// return 4. The pixel encoding is implied by file size, not by lv_cf.
    pub fn lv_cf(self) -> u32 {
        match self {
            Self::Rgb332 | Self::Rgb565 | Self::Rgb565Swap | Self::Rgb888 => 4,
            Self::Indexed1 => 7,
            Self::Indexed2 => 8,
            Self::Indexed4 => 9,
            Self::Indexed8 => 10,
            Self::Alpha1 => 11,
            Self::Alpha2 => 12,
            Self::Alpha4 => 13,
            Self::Alpha8 => 14,
        }
    }

    /// Bits per pixel for the pixel-data section.
    pub fn bits_per_pixel(self) -> u32 {
        match self {
            Self::Rgb332 => 8,
            Self::Rgb565 | Self::Rgb565Swap => 16,
            Self::Rgb888 => 24,
            Self::Indexed1 | Self::Alpha1 => 1,
            Self::Indexed2 | Self::Alpha2 => 2,
            Self::Indexed4 | Self::Alpha4 => 4,
            Self::Indexed8 | Self::Alpha8 => 8,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lv_cf_pinned() {
        // SAFETY: must match the LVGL constants in convertor_core.py:375-387.
        assert_eq!(ColorFormat::Rgb332.lv_cf(), 4);
        assert_eq!(ColorFormat::Rgb565.lv_cf(), 4);
        assert_eq!(ColorFormat::Rgb565Swap.lv_cf(), 4);
        assert_eq!(ColorFormat::Rgb888.lv_cf(), 4);
        assert_eq!(ColorFormat::Indexed1.lv_cf(), 7);
        assert_eq!(ColorFormat::Indexed4.lv_cf(), 9);
        assert_eq!(ColorFormat::Alpha1.lv_cf(), 11);
        assert_eq!(ColorFormat::Alpha8.lv_cf(), 14);
    }

    #[test]
    fn bits_per_pixel_correct() {
        assert_eq!(ColorFormat::Rgb332.bits_per_pixel(), 8);
        assert_eq!(ColorFormat::Rgb565.bits_per_pixel(), 16);
        assert_eq!(ColorFormat::Rgb888.bits_per_pixel(), 24);
        assert_eq!(ColorFormat::Indexed1.bits_per_pixel(), 1);
        assert_eq!(ColorFormat::Alpha8.bits_per_pixel(), 8);
    }
}
