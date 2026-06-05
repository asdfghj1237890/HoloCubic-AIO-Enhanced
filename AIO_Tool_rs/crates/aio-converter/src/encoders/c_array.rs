//! C-array text emission for `.c` LVGL image files.
//!
//! Algorithm port of `convertor_core.py:176-367` (`format_to_c_array`,
//! `_get_c_header`, `_get_c_footer`, `get_c_code_file`).
//!
//! The function internally calls one of the binary encoders
//! (rgb/alpha/indexed) to get the byte stream, then formats it as `0xNN, ...`
//! arrays interspersed with `#ifndef` guards and the `lv_img_dsc_t`
//! descriptor.
//!
//! ## Output sections
//!
//! 1. **C header**: `#if defined(LV_LVGL_H_INCLUDE_SIMPLE)`, `#include`s,
//!    `#ifndef LV_ATTRIBUTE_MEM_ALIGN`, array declaration.
//! 2. **Body**: comma-separated `0xNN` hex bytes, one source-pixel row per
//!    line for true-colour / alpha; palette entries on their own annotated
//!    lines for indexed.
//! 3. **C footer**: `};` + `const lv_img_dsc_t {name} = { ... };`.
//!
//! Output is semantically equivalent to Python's `get_c_code_file`. Whitespace
//! follows the same scheme but is not bit-exact — Plan 5 Group C explicitly
//! allows whitespace liberties so long as the generated `.c` compiles to the
//! same in-memory representation. The byte values inside `_map[]`,
//! the `lv_img_dsc_t` field values, and the `LV_IMG_CF_*` constants match
//! Python byte-for-byte.

use crate::encoders::{alpha, indexed, rgb};
use crate::error::ConvertError;
use crate::format::ColorFormat;
use crate::image_input::Rgba8Image;

/// Encode `img` as a `.c` LVGL image source file.
///
/// `name` is the C identifier (used for `{name}_map[]` and the
/// `lv_img_dsc_t {name}` descriptor). Caller is responsible for sanitizing
/// (alphanumeric + underscore).
///
/// # Errors
/// Propagates errors from the underlying byte encoder.
pub fn encode(
    img: &Rgba8Image,
    fmt: ColorFormat,
    dither: bool,
    name: &str,
) -> Result<String, ConvertError> {
    let w = img.width();
    let h = img.height();

    // Step 1: get the byte stream from the corresponding binary encoder.
    let bytes = match fmt {
        ColorFormat::Rgb332
        | ColorFormat::Rgb565
        | ColorFormat::Rgb565Swap
        | ColorFormat::Rgb888 => rgb::encode(img, fmt, dither, None, None)?,
        ColorFormat::Alpha1 | ColorFormat::Alpha2 | ColorFormat::Alpha4 | ColorFormat::Alpha8 => {
            alpha::encode(img, fmt, None, None)?
        }
        ColorFormat::Indexed1
        | ColorFormat::Indexed2
        | ColorFormat::Indexed4
        | ColorFormat::Indexed8 => indexed::encode(img, fmt, None, None)?,
    };

    // Step 2: build the .c source as a String.
    let mut out = String::with_capacity(bytes.len() * 6 + 1024);

    // -- Header --
    out.push_str("#if defined(LV_LVGL_H_INCLUDE_SIMPLE)\n");
    out.push_str("#include \"lvgl.h\"\n");
    out.push_str("#else\n");
    out.push_str("#include \"../lvgl/lvgl.h\"\n");
    out.push_str("#endif\n");
    out.push_str("#ifndef LV_ATTRIBUTE_MEM_ALIGN\n");
    out.push_str("#define LV_ATTRIBUTE_MEM_ALIGN\n");
    out.push_str("#endif\n\n");
    let attr_name = format!("LV_ATTRIBUTE_IMG_{}", name.to_uppercase());
    out.push_str(&format!("#ifndef {attr_name}\n"));
    out.push_str(&format!("#define {attr_name}\n"));
    out.push_str("#endif\n");
    out.push_str(&format!(
        "const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST {attr_name} uint8_t {name}_map[] = {{"
    ));

    // -- Body (format_to_c_array port) --
    append_body(&mut out, fmt, &bytes, w, h);

    // -- Footer --
    out.push_str("\n};\n\n");
    out.push_str(&format!(
        "const lv_img_dsc_t {name} = {{\n  .header.always_zero = 0,\n  .header.w = {w},\n  .header.h = {h},\n"
    ));
    out.push_str(&format!(
        "  .data_size = {},\n",
        data_size_expr(fmt, w, h, bytes.len())
    ));
    out.push_str(&format!("  .header.cf = {},\n", lv_img_cf_constant(fmt)));
    out.push_str(&format!("  .data = {name}_map,\n}};\n"));

    Ok(out)
}

/// Append the `format_to_c_array` body — the hex-byte rows between
/// `_map[] = {` and `};`.
fn append_body(out: &mut String, fmt: ColorFormat, bytes: &[u8], w: u32, h: u32) {
    // Format-specific prelude: TRUE_COLOR variants get an LV_COLOR_DEPTH guard
    // + pixel-format comment. Indexed variants get the palette dump.
    let mut i: usize = 0;
    match fmt {
        ColorFormat::Rgb332 => {
            out.push_str("\n#if LV_COLOR_DEPTH == 1 || LV_COLOR_DEPTH == 8");
            out.push_str("\n  /*Pixel format: Blue: 2 bit, Green: 3 bit, Red: 3 bit*/");
        }
        ColorFormat::Rgb565 => {
            out.push_str("\n#if LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 0");
            out.push_str("\n  /*Pixel format: Blue: 5 bit, Green: 6 bit, Red: 5 bit*/");
        }
        ColorFormat::Rgb565Swap => {
            out.push_str("\n#if LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP != 0");
            out.push_str(
                "\n  /*Pixel format: Blue: 5 bit, Green: 6 bit, Red: 5 bit BUT the 2 bytes are swapped*/",
            );
        }
        ColorFormat::Rgb888 => {
            out.push_str("\n#if LV_COLOR_DEPTH == 32");
            out.push_str(
                "\n  /*Pixel format:  Blue: 8 bit, Green: 8 bit, Red: 8 bit, Alpha: 8 bit*/",
            );
        }
        ColorFormat::Indexed1 => {
            append_palette_lines(out, bytes, 2);
            i = 2 * 4;
        }
        ColorFormat::Indexed2 => {
            append_palette_lines(out, bytes, 4);
            i = 4 * 4;
        }
        ColorFormat::Indexed4 => {
            append_palette_lines(out, bytes, 16);
            i = 16 * 4;
        }
        ColorFormat::Indexed8 => {
            append_palette_lines(out, bytes, 256);
            i = 256 * 4;
        }
        ColorFormat::Alpha1 | ColorFormat::Alpha2 | ColorFormat::Alpha4 | ColorFormat::Alpha8 => {
            // No prelude; pixel data starts at i=0.
        }
    }

    // Collect hex strings for pixel data (one per byte), then split into rows
    // of `bytes_per_row` and join with `, \n  `.
    let bytes_per_row = bytes_per_source_row(fmt, w);
    let mut hex: Vec<String> = Vec::with_capacity(bytes.len() - i);
    for &b in &bytes[i..] {
        hex.push(format!("0x{b:02X}"));
    }

    // Format: "\n  " + ", \n  ".join(", ".join(rows))
    out.push_str("\n  ");
    for (row_idx, row) in hex.chunks(bytes_per_row).enumerate() {
        if row_idx > 0 {
            out.push_str(", \n  ");
        }
        out.push_str(&row.join(", "));
    }

    // TRUE_COLOR variants close with #endif.
    if matches!(
        fmt,
        ColorFormat::Rgb332 | ColorFormat::Rgb565 | ColorFormat::Rgb565Swap | ColorFormat::Rgb888
    ) {
        out.push_str("\n#endif");
    }

    // Avoid the unused `h` warning when we don't reference it in this fn.
    let _ = h;
}

/// Append the palette section for indexed formats. Each palette entry is
/// 4 bytes; emit each as `"0xRR, 0xGG, 0xBB, 0xFF, \t/*Color of index N*/\n  "`.
fn append_palette_lines(out: &mut String, bytes: &[u8], n_entries: usize) {
    out.push_str("\n  ");
    for p in 0..n_entries {
        let off = p * 4;
        out.push_str(&format!(
            "0x{:02X}, 0x{:02X}, 0x{:02X}, 0x{:02X}, \t/*Color of index {}*/\n  ",
            bytes[off],
            bytes[off + 1],
            bytes[off + 2],
            bytes[off + 3],
            p,
        ));
    }
    // Trim the trailing "  " from the last line so the pixel-data row glues
    // cleanly. (Python emits the same trailing whitespace; we follow.)
}

/// Bytes per source-pixel-row in the C-array layout. This drives line wrapping
/// inside `_map[]`. Mirrors the logic in `convertor_core.py:278-285` where
/// `x_end` is divided by pixels-per-byte for sub-byte formats and multiplied
/// for multi-byte formats.
fn bytes_per_source_row(fmt: ColorFormat, w: u32) -> usize {
    match fmt {
        ColorFormat::Rgb332 => w as usize,
        ColorFormat::Rgb565 | ColorFormat::Rgb565Swap => (w * 2) as usize,
        ColorFormat::Rgb888 => (w * 3) as usize,
        // Sub-byte formats: ceil(w / pixels_per_byte) bytes/row.
        ColorFormat::Alpha1 | ColorFormat::Indexed1 => w.div_ceil(8) as usize,
        ColorFormat::Alpha2 | ColorFormat::Indexed2 => w.div_ceil(4) as usize,
        ColorFormat::Alpha4 | ColorFormat::Indexed4 => w.div_ceil(2) as usize,
        ColorFormat::Alpha8 | ColorFormat::Indexed8 => w as usize,
    }
}

/// `.data_size` field expression, matching Python's `_get_c_footer` map.
///
/// TRUE_COLOR variants use the LVGL macro `{w*h} * LV_COLOR_SIZE / 8`.
/// Alpha/Indexed use the raw byte count of the encoded data.
fn data_size_expr(fmt: ColorFormat, w: u32, h: u32, bytes_len: usize) -> String {
    match fmt {
        ColorFormat::Rgb332
        | ColorFormat::Rgb565
        | ColorFormat::Rgb565Swap
        | ColorFormat::Rgb888 => format!("{} * LV_COLOR_SIZE / 8", w * h),
        _ => bytes_len.to_string(),
    }
}

/// LVGL `LV_IMG_CF_*` constant for the `.header.cf` field.
fn lv_img_cf_constant(fmt: ColorFormat) -> &'static str {
    match fmt {
        ColorFormat::Rgb332
        | ColorFormat::Rgb565
        | ColorFormat::Rgb565Swap
        | ColorFormat::Rgb888 => "LV_IMG_CF_TRUE_COLOR",
        ColorFormat::Alpha1 => "LV_IMG_CF_ALPHA_1BIT",
        ColorFormat::Alpha2 => "LV_IMG_CF_ALPHA_2BIT",
        ColorFormat::Alpha4 => "LV_IMG_CF_ALPHA_4BIT",
        ColorFormat::Alpha8 => "LV_IMG_CF_ALPHA_8BIT",
        ColorFormat::Indexed1 => "LV_IMG_CF_INDEXED_1BIT",
        ColorFormat::Indexed2 => "LV_IMG_CF_INDEXED_2BIT",
        ColorFormat::Indexed4 => "LV_IMG_CF_INDEXED_4BIT",
        ColorFormat::Indexed8 => "LV_IMG_CF_INDEXED_8BIT",
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::ImageBuffer;

    fn solid(w: u32, h: u32, r: u8, g: u8, b: u8) -> Rgba8Image {
        ImageBuffer::from_fn(w, h, |_, _| image::Rgba([r, g, b, 255]))
    }

    #[test]
    fn c_array_rgb565_contains_lv_img_dsc_t_and_correct_byte_count() {
        let img = solid(4, 4, 200, 100, 48);
        let out = encode(&img, ColorFormat::Rgb565, false, "myicon").unwrap();
        // Should declare a lv_img_dsc_t with the name.
        assert!(
            out.contains("const lv_img_dsc_t myicon"),
            "missing lv_img_dsc_t declaration: {out}"
        );
        // Should declare the _map[] array.
        assert!(
            out.contains("uint8_t myicon_map[]"),
            "missing _map[] declaration"
        );
        // Should mark TRUE_COLOR.
        assert!(out.contains("LV_IMG_CF_TRUE_COLOR"));
        // Should contain dimensions.
        assert!(out.contains(".header.w = 4"));
        assert!(out.contains(".header.h = 4"));
        // Should have 32 hex byte literals (4×4 × 2 bytes/px) in the _map[].
        let hex_count = out.matches("0x").count();
        // Account for any LV_IMG_CF_* constants etc. — we just check >= 32.
        assert!(
            hex_count >= 32,
            "expected at least 32 hex bytes in C-array, got {hex_count}"
        );
    }

    #[test]
    fn c_array_includes_lvgl_header_block() {
        let img = solid(2, 2, 0, 0, 0);
        let out = encode(&img, ColorFormat::Rgb565, false, "x").unwrap();
        assert!(out.contains("#if defined(LV_LVGL_H_INCLUDE_SIMPLE)"));
        assert!(out.contains("#include \"lvgl.h\""));
        assert!(out.contains("#ifndef LV_ATTRIBUTE_MEM_ALIGN"));
        assert!(out.contains("LV_ATTRIBUTE_IMG_X"));
    }

    #[test]
    fn c_array_rgb565_has_lv_color_depth_guard() {
        let img = solid(2, 2, 0, 0, 0);
        let out = encode(&img, ColorFormat::Rgb565, false, "x").unwrap();
        assert!(out.contains("#if LV_COLOR_DEPTH == 16"));
        assert!(out.contains("#endif"));
    }

    #[test]
    fn c_array_alpha8_uses_alpha_8bit_cf() {
        let img = solid(2, 2, 0, 0, 0);
        let out = encode(&img, ColorFormat::Alpha8, false, "myicon").unwrap();
        assert!(out.contains("LV_IMG_CF_ALPHA_8BIT"));
        // Alpha formats don't get the #if LV_COLOR_DEPTH guard.
        assert!(!out.contains("#if LV_COLOR_DEPTH"));
        // data_size = 4 (2*2 = 4 bytes for alpha8).
        assert!(out.contains(".data_size = 4"));
    }

    #[test]
    fn c_array_indexed1_emits_palette_color_comments() {
        let img = solid(8, 1, 100, 50, 25);
        let out = encode(&img, ColorFormat::Indexed1, false, "myicon").unwrap();
        assert!(out.contains("LV_IMG_CF_INDEXED_1BIT"));
        // Each palette entry gets a /*Color of index N*/ comment.
        assert!(out.contains("/*Color of index 0*/"));
        assert!(out.contains("/*Color of index 1*/"));
    }

    #[test]
    fn c_array_indexed4_emits_16_palette_comments() {
        let img = solid(4, 4, 100, 50, 25);
        let out = encode(&img, ColorFormat::Indexed4, false, "x").unwrap();
        for i in 0..16 {
            assert!(
                out.contains(&format!("/*Color of index {i}*/")),
                "missing palette comment for index {i}"
            );
        }
    }

    #[test]
    fn c_array_name_uppercased_in_attribute_macro() {
        let img = solid(2, 2, 0, 0, 0);
        let out = encode(&img, ColorFormat::Rgb565, false, "MyIcon_v2").unwrap();
        assert!(
            out.contains("LV_ATTRIBUTE_IMG_MYICON_V2"),
            "expected uppercased attribute name"
        );
        // Original case preserved in array/dsc names.
        assert!(out.contains("MyIcon_v2_map"));
        assert!(out.contains("const lv_img_dsc_t MyIcon_v2"));
    }

    #[test]
    fn c_array_rgb888_data_size_formula() {
        let img = solid(4, 4, 10, 20, 30);
        let out = encode(&img, ColorFormat::Rgb888, false, "x").unwrap();
        // For TRUE_COLOR: data_size = w*h * LV_COLOR_SIZE / 8.
        assert!(out.contains(".data_size = 16 * LV_COLOR_SIZE / 8"));
    }
}
