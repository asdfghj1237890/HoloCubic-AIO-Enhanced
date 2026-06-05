# Converter goldens

Each `*.bin` file is the byte-exact output of the legacy Python tool
(`convertor_core.py`) for the same input × format × dither combination.
Rust integration tests in `../goldens.rs` re-encode in Rust and assert
byte equality.

**Note**: `INDEXED_*` formats are NOT included — Rust uses a hand-rolled
median-cut quantizer (BUGS.md B10), so byte parity with PIL is not
expected. Indexed correctness is validated visually pre-release.

**Note**: The two `photo_64x64.*` goldens are present but their Rust
tests are `#[ignore]`d. The Rust `image` crate (0.25.6) JPEG decoder
differs from PIL by 1 LSB at a handful of pixels, which compounds
through the encoder. PNG-input cases (`solid_red_*`, `gradient_*`,
`alpha_*`, `transparent_circle_*`) match Python byte-for-byte.

Regenerate:

    cd AIO_Tool && uv run python ../AIO_Tool_rs/scripts/dump_converter_goldens.py

`.gitattributes` keeps `.bin` as binary (no CRLF translation on
Windows checkouts).
