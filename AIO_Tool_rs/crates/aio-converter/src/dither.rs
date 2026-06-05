//! Floyd-Steinberg dithering for downsampling to lower bit depths.
//!
//! Algorithm port of `AIO_Tool/util/convertor_core.py:498-611`. Matches the
//! Python implementation's quirks exactly so golden tests stay byte-equal:
//!
//! * Per-channel single error array of size `width + 2` (single, not paired —
//!   Python keeps one `r_earr` that does double-duty: read+zeroed for current
//!   row's incoming carry, then immediately written for next row).
//! * Scalar `*_nerr` holds the 7/16 carry to the NEXT pixel in the SAME row.
//! * Error feedback uses `err = r_original - r_quantized` (NOT
//!   `(r + accumulated) - quantized`). This is unusual for Floyd-Steinberg
//!   but it's what Python does, so we replicate it.
//! * Diffusion uses `round((k * err) / 16)` with Python's banker's rounding
//!   (`round_ties_even`).
//! * `classify_pixel` uses ceiling rounding (`ceil(value / tmp) * tmp`), which
//!   can overshoot past 0xFF — the caller is expected to clamp per-channel
//!   (e.g. RGB565 R clamps to 0xF8, G to 0xFC, B to 0xF8).

/// Per-channel state for a Floyd-Steinberg pass over an image.
pub struct Dither {
    width: u32,
    bits_r: u32,
    bits_g: u32,
    bits_b: u32,
    /// Scalar 7/16 carry for the next pixel in the current row.
    r_nerr: i32,
    g_nerr: i32,
    b_nerr: i32,
    /// Error arrays — single array per channel, size `width + 2` so writes to
    /// `x` and `x + 2` (relative to current pixel) never overflow. Lives across
    /// rows: current pixel reads+zeroes its slot, then writes next-row carries.
    r_earr: Vec<i32>,
    g_earr: Vec<i32>,
    b_earr: Vec<i32>,
}

impl Dither {
    /// Create a Dither for `width` pixels per row, quantizing R/G/B to the
    /// given bit depths (e.g. 5/6/5 for RGB565, 3/3/2 for RGB332, 8/8/8 for
    /// RGB888 — last case is a no-op).
    pub fn new(width: u32, bits_r: u32, bits_g: u32, bits_b: u32) -> Self {
        let sz = width as usize + 2;
        Self {
            width,
            bits_r,
            bits_g,
            bits_b,
            r_nerr: 0,
            g_nerr: 0,
            b_nerr: 0,
            r_earr: vec![0; sz],
            g_earr: vec![0; sz],
            b_earr: vec![0; sz],
        }
    }

    /// Called between rows — clears the within-row 7/16 scalar carries.
    /// The `*_earr` arrays carry forward to the next row (that's their job).
    pub fn reset_row(&mut self) {
        self.r_nerr = 0;
        self.g_nerr = 0;
        self.b_nerr = 0;
    }

    /// Apply Floyd-Steinberg at column `x` (0-based). Returns the quantized
    /// `(r_act, g_act, b_act)` bytes (after clamping to the per-channel max
    /// for the target format).
    ///
    /// `max_rgb` is a `(max_r, max_g, max_b)` tuple of saturation caps that
    /// matches Python's explicit `if r_act > 0xE0: r_act = 0xE0`-style clamps
    /// right after `_classify_pixel`. Caller passes the cap for the chosen
    /// format (RGB332: `(0xE0, 0xE0, 0xC0)`, RGB565: `(0xF8, 0xFC, 0xF8)`,
    /// RGB888: `(0xFF, 0xFF, 0xFF)`).
    pub fn next(&mut self, r: u8, g: u8, b: u8, x: u32, max_rgb: (u8, u8, u8)) -> (u8, u8, u8) {
        let (max_r, max_g, max_b) = max_rgb;
        let xi = (x + 1) as usize;

        // 1. Read accumulated error (next-row carry from previous row) and
        //    add scalar within-row carry. Zero the array slot — it'll be
        //    rewritten below as a next-row carry for the column to the left.
        let r_val = r as i32 + self.r_nerr + self.r_earr[xi];
        let g_val = g as i32 + self.g_nerr + self.g_earr[xi];
        let b_val = b as i32 + self.b_nerr + self.b_earr[xi];
        self.r_earr[xi] = 0;
        self.g_earr[xi] = 0;
        self.b_earr[xi] = 0;

        // 2. Quantize via classify_pixel (ceiling) then clamp to per-channel
        //    max. Matches Python lines 515-549.
        let r_act = classify_pixel(r_val, self.bits_r).min(max_r);
        let g_act = classify_pixel(g_val, self.bits_g).min(max_g);
        let b_act = classify_pixel(b_val, self.bits_b).min(max_b);

        // 3. Compute residual — Python quirk: uses ORIGINAL r, not r_val
        //    (convertor_core.py:551-553).
        let r_err = r as i32 - r_act as i32;
        let g_err = g as i32 - g_act as i32;
        let b_err = b as i32 - b_act as i32;

        // 4. Diffuse. Python uses round((k * err) / 16) with banker's rounding.
        //    7/16 → next pixel right (scalar carry, same row).
        self.r_nerr = round_div16(7 * r_err);
        self.g_nerr = round_div16(7 * g_err);
        self.b_nerr = round_div16(7 * b_err);

        // 3/16 → row below, x-1 (which is r_earr[x] given our +1 offset)
        // 5/16 → row below, x   (r_earr[x+1])
        // 1/16 → row below, x+1 (r_earr[x+2])
        // Python uses raw `x` (no +1 offset) because its array is sized
        // `w + 2` but indexed [x..=x+2]. Same thing.
        let i = x as usize;
        self.r_earr[i] += round_div16(3 * r_err);
        self.g_earr[i] += round_div16(3 * g_err);
        self.b_earr[i] += round_div16(3 * b_err);

        self.r_earr[i + 1] += round_div16(5 * r_err);
        self.g_earr[i + 1] += round_div16(5 * g_err);
        self.b_earr[i + 1] += round_div16(5 * b_err);

        self.r_earr[i + 2] += round_div16(r_err);
        self.g_earr[i + 2] += round_div16(g_err);
        self.b_earr[i + 2] += round_div16(b_err);

        (r_act, g_act, b_act)
    }

    /// Image width this Dither was sized for (mostly for assertions / tests).
    pub fn width(&self) -> u32 {
        self.width
    }
}

/// Banker's rounding of `n / 16` (ties-to-even). Matches Python 3's `round()`
/// applied to `numerator / 16`. We do float math then round_ties_even because
/// the values are small (|n| typically < 2000) so f32 precision is exact.
fn round_div16(n: i32) -> i32 {
    (n as f32 / 16.0).round_ties_even() as i32
}

/// Quantize a value to `bits` bits and re-expand the top bits to a u8.
///
/// Matches `convertor_core.py:607-611` `_classify_pixel`:
/// ```text
///     tmp = 1 << (8 - bits)
///     val = ceil(value / tmp) * tmp
///     return val if val >= 0 else 0
/// ```
///
/// Note this can OVERSHOOT past 0xFF when the input is near the max for a
/// non-8-bit depth (e.g. value=255, bits=5 → 256). Caller is expected to
/// clamp via the explicit per-channel max (e.g. 0xF8 for RGB565 R).
///
/// Result is clamped to u8 here for ergonomics (256 → 255), but real
/// per-channel clamping (e.g. 248) MUST be applied by the caller.
pub fn classify_pixel(value: i32, bits: u32) -> u8 {
    if value <= 0 {
        return 0;
    }
    if bits >= 8 {
        return value.min(255) as u8;
    }
    let tmp = 1i32 << (8 - bits);
    // ceil(value / tmp) for positive ints = (value + tmp - 1) / tmp.
    let q = (value + tmp - 1) / tmp;
    let val = q * tmp;
    val.min(255) as u8
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn classify_8bit_passthrough() {
        assert_eq!(classify_pixel(0, 8), 0);
        assert_eq!(classify_pixel(127, 8), 127);
        assert_eq!(classify_pixel(255, 8), 255);
    }

    #[test]
    fn classify_5bit_ceiling_rounding() {
        // 5 bits → tmp = 8. 0x12 = 18, ceil(18/8)*8 = 3*8 = 24 = 0x18.
        // (NOT 0x10 — the Python algorithm rounds UP, not down.)
        assert_eq!(classify_pixel(0x12, 5), 0x18);
        // 0xFF = 255, ceil(255/8)*8 = 32*8 = 256 → clamped to u8 max 255.
        // Caller's explicit per-channel clamp would bring this to 0xF8.
        assert_eq!(classify_pixel(0xFF, 5), 255);
        // 0x80 = 128, ceil(128/8)*8 = 16*8 = 128 = 0x80 (exact divisor).
        assert_eq!(classify_pixel(0x80, 5), 0x80);
        // 0x00 = 0 → 0.
        assert_eq!(classify_pixel(0, 5), 0);
    }

    #[test]
    fn classify_clamps_negative_to_zero() {
        assert_eq!(classify_pixel(-5, 8), 0);
        assert_eq!(classify_pixel(-100, 5), 0);
        // Python returns 0 for value=0 (val=0, val>=0 so returns 0). Match.
        assert_eq!(classify_pixel(0, 5), 0);
    }

    #[test]
    fn dither_zero_input_zero_output() {
        let mut d = Dither::new(4, 8, 8, 8);
        for x in 0..4 {
            let (r, g, b) = d.next(0, 0, 0, x, (0xFF, 0xFF, 0xFF));
            assert_eq!((r, g, b), (0, 0, 0));
        }
    }

    #[test]
    fn dither_full_input_full_output_at_8bit() {
        // 8-bit quantize is a no-op → output equals input; err=0; no carry.
        let mut d = Dither::new(4, 8, 8, 8);
        for x in 0..4 {
            let (r, g, b) = d.next(200, 100, 50, x, (0xFF, 0xFF, 0xFF));
            assert_eq!((r, g, b), (200, 100, 50));
        }
    }

    #[test]
    fn round_div16_banker_ties_to_even() {
        // 8/16 = 0.5 → ties to even (0). 24/16 = 1.5 → ties to even (2).
        assert_eq!(round_div16(8), 0);
        assert_eq!(round_div16(24), 2);
        assert_eq!(round_div16(-8), 0);
        assert_eq!(round_div16(-24), -2);
        assert_eq!(round_div16(16), 1);
        assert_eq!(round_div16(0), 0);
    }
}
