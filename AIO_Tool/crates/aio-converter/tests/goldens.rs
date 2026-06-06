//! Cross-language golden tests — Rust output must match Python byte-for-byte
//! for non-indexed formats. INDEXED_* skipped (BUGS.md B10).

use aio_converter::{ColorFormat, Converter};

fn check(fixture: &str, fmt: ColorFormat, dither: bool, golden_path: &str) {
    let bytes = std::fs::read(format!("tests/fixtures/{fixture}")).expect("read fixture");
    let conv = Converter::new(&bytes, fmt, dither).expect("convert");
    let actual = conv.encode_bin(None, None).expect("encode");
    let expected = std::fs::read(golden_path).expect("read golden");
    if actual != expected {
        // Helpful diff output: print first 64 bytes hex of each side.
        let n = actual.len().min(expected.len()).min(64);
        eprintln!("actual[..{n}]   = {:02x?}", &actual[..n]);
        eprintln!("expected[..{n}] = {:02x?}", &expected[..n]);
        eprintln!(
            "actual.len={}, expected.len={}",
            actual.len(),
            expected.len()
        );
    }
    assert_eq!(
        actual, expected,
        "fixture={fixture} fmt={fmt:?} dither={dither}"
    );
}

// --- RGB ---

#[test]
fn solid_red_rgb565() {
    check(
        "solid_red_32x32.png",
        ColorFormat::Rgb565,
        false,
        "tests/golden/solid_red_32x32.rgb565.bin",
    );
}

#[test]
fn solid_red_rgb888() {
    check(
        "solid_red_32x32.png",
        ColorFormat::Rgb888,
        false,
        "tests/golden/solid_red_32x32.rgb888.bin",
    );
}

#[test]
fn solid_red_rgb332() {
    check(
        "solid_red_32x32.png",
        ColorFormat::Rgb332,
        false,
        "tests/golden/solid_red_32x32.rgb332.bin",
    );
}

#[test]
fn gradient_rgb565_dither() {
    check(
        "gradient_16x16.png",
        ColorFormat::Rgb565,
        true,
        "tests/golden/gradient_16x16.rgb565.dither.bin",
    );
}

#[test]
fn gradient_rgb565_nodither() {
    check(
        "gradient_16x16.png",
        ColorFormat::Rgb565,
        false,
        "tests/golden/gradient_16x16.rgb565.nodither.bin",
    );
}

// JPEG decoder divergence (documented but not asserted):
//
// Rust's `image` crate (0.25.6) and PIL decode the same `photo_64x64.jpg`
// fixture with off-by-one differences at a handful of pixels — e.g. at
// (23, 0) PIL produces RGB (92, 1, 44) while `image` produces (93, 0, 44).
// That difference compounds through quantize + dither and shifts ~30%
// of the output bytes despite identical encoder logic. Inspecting the
// goldens visually they're clearly the same picture; byte parity just
// isn't reachable through the JPEG decode stage.
//
// We keep these tests in `#[ignore]` form so the goldens stay
// reproducible (rerun via the dumper) and the divergence is visible to
// anyone running `cargo test -- --ignored`. RGB / Alpha parity through
// the PNG inputs IS exact — see the eight active tests above.

#[test]
#[ignore = "JPEG decoder differs between PIL and image-rs 0.25.6 — see comment above"]
fn photo_rgb565_dither() {
    check(
        "photo_64x64.jpg",
        ColorFormat::Rgb565,
        true,
        "tests/golden/photo_64x64.rgb565.dither.bin",
    );
}

#[test]
#[ignore = "JPEG decoder differs between PIL and image-rs 0.25.6 — see comment above"]
fn photo_rgb565_swap() {
    check(
        "photo_64x64.jpg",
        ColorFormat::Rgb565Swap,
        false,
        "tests/golden/photo_64x64.rgb565_swap.bin",
    );
}

// --- Alpha ---

#[test]
fn alpha_gradient_alpha8() {
    check(
        "alpha_gradient_16x16.png",
        ColorFormat::Alpha8,
        false,
        "tests/golden/alpha_gradient_16x16.alpha8.bin",
    );
}

#[test]
fn alpha_gradient_alpha4() {
    check(
        "alpha_gradient_16x16.png",
        ColorFormat::Alpha4,
        false,
        "tests/golden/alpha_gradient_16x16.alpha4.bin",
    );
}

#[test]
fn transparent_circle_alpha1() {
    check(
        "transparent_circle_32x32.png",
        ColorFormat::Alpha1,
        false,
        "tests/golden/transparent_circle_32x32.alpha1.bin",
    );
}
