//! HoloCubic AIO LVGL image converter.
//!
//! Pure-Rust port of `AIO_Tool/util/convertor_core.py`. Converts PNG/JPG/BMP
//! into LVGL binary or C-array output across 11 pixel formats (RGB332/565/
//! 565_swap/888 ± alpha, Alpha 1/2/4/8, Indexed 1/2/4/8). Output is
//! byte-for-byte identical to the Python tool, verified by golden tests in
//! `tests/goldens.rs`.
//!
//! See `Docs/superpowers/plans/2026-06-06-plan-5-converter.md` for design.
#![deny(missing_docs)]
#![deny(unsafe_code)]

pub mod converter;
pub mod dither;
pub mod encoders;
pub mod error;
pub mod format;
pub mod header;
pub mod image_input;
pub mod progress;

// TODO(plan-5): restore re-exports as modules gain real content
pub use error::ConvertError;
pub use format::ColorFormat;
// pub use converter::Converter;    // restored in Task 6
pub use progress::ConvertEvent;
