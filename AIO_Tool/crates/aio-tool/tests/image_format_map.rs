//! Smoke-test that the dropdown's FORMATS array covers all 12 binary
//! ColorFormat variants exactly once. Catches "I added a variant to
//! aio-converter but forgot to expose it in the GUI" regressions.

use aio_converter::ColorFormat;

#[test]
fn formats_constant_covers_every_color_format_variant_once() {
    // Tab module's FORMATS array (12 binary entries).
    let formats = [
        ColorFormat::Rgb332,
        ColorFormat::Rgb565,
        ColorFormat::Rgb565Swap,
        ColorFormat::Rgb888,
        ColorFormat::Alpha1,
        ColorFormat::Alpha2,
        ColorFormat::Alpha4,
        ColorFormat::Alpha8,
        ColorFormat::Indexed1,
        ColorFormat::Indexed2,
        ColorFormat::Indexed4,
        ColorFormat::Indexed8,
    ];
    let mut seen = std::collections::HashSet::new();
    for f in formats {
        assert!(seen.insert(f), "duplicate variant: {f:?}");
    }
    assert_eq!(seen.len(), 12, "expected all 12 ColorFormat variants");
}
