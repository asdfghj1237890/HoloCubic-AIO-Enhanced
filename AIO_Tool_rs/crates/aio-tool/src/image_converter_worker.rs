//! One-shot background worker for the Image Converter tab.
//!
//! Spawn one thread per Convert click. Thread:
//! 1. Reads the input file from disk
//! 2. Constructs `aio_converter::Converter`
//! 3. Encodes (either `encode_bin` or `encode_c_array`)
//! 4. Sends `AppEvent::Convert(ConvertEvent)` for progress
//! 5. Sends `AppEvent::ConvertFinished(Ok(bytes) | Err(msg))` once
//!
//! No long-lived state; no Cmd enum. Cancel flag is **supplied by the
//! caller** so one Cancel button can cancel every in-flight encode of a
//! batch; the encoder checks it at row boundaries (Plan 5 Task 6).

use std::sync::atomic::AtomicBool;
use std::sync::mpsc::channel;
use std::sync::Arc;
use std::thread;

use aio_converter::{ColorFormat, ConvertError, Converter};

use crate::bus::{AppEvent, AppEventTx};

/// What the user wants emitted.
#[derive(Debug, Clone)]
pub enum Output {
    /// LVGL binary (4-byte header + pixel data).
    Bin,
    /// `.c` source with the given identifier as the array name.
    CArray {
        /// Sanitized C identifier used for the `{ident}_map[]` array and
        /// `lv_img_dsc_t {ident}` descriptor in the emitted `.c` file.
        ident: String,
    },
}

/// Parameters captured at the moment the user clicks Convert.
pub struct Job {
    /// Absolute path to read the input image from.
    pub input_path: std::path::PathBuf,
    /// Pixel format chosen via the UI dropdown.
    pub format: ColorFormat,
    /// Whether to apply Floyd-Steinberg dithering (RGB encoders only).
    pub dither: bool,
    /// What to emit.
    pub output: Output,
}

/// Spawn the worker. The `cancel` flag is owned by the caller and shared
/// across every job in a batch so a single Cancel button can stop all
/// in-flight encodes. The UI does NOT keep a `Sender` — there's no cmd
/// surface; the only control is "cancel".
pub fn spawn(job: Job, bus_tx: AppEventTx, cancel: Arc<AtomicBool>) {
    thread::spawn(move || {
        let result: Result<Vec<u8>, String> = (|| {
            let bytes = std::fs::read(&job.input_path)
                .map_err(|e| format!("read {}: {e}", job.input_path.display()))?;

            let conv = Converter::new(&bytes, job.format, job.dither)
                .map_err(|e: ConvertError| format!("decode: {e}"))?;

            match job.output {
                Output::Bin => {
                    let (tx, rx) = channel();
                    // Forwarder: pump ConvertEvent through the bus.
                    let bus_fwd = bus_tx.clone();
                    let fwd = thread::spawn(move || {
                        while let Ok(evt) = rx.recv() {
                            if bus_fwd.send(AppEvent::Convert(evt)).is_err() {
                                break;
                            }
                        }
                    });
                    let out = conv
                        .encode_bin(Some(tx), Some(cancel))
                        .map_err(|e| format!("encode: {e}"))?;
                    // Joining the forwarder is best-effort; drop it.
                    let _ = fwd.join();
                    Ok(out)
                }
                Output::CArray { ident } => {
                    // C-array encoder is synchronous (no progress / cancel
                    // hooks per Plan 5 D7); emit one Start + Done so the
                    // UI's progress affordance still updates.
                    let _ = bus_tx.send(AppEvent::Convert(aio_converter::ConvertEvent::Start {
                        total_rows: 0,
                    }));
                    let text = conv
                        .encode_c_array(&ident)
                        .map_err(|e| format!("encode c-array: {e}"))?;
                    let _ = bus_tx.send(AppEvent::Convert(aio_converter::ConvertEvent::Done));
                    Ok(text.into_bytes())
                }
            }
        })();

        let _ = bus_tx.send(AppEvent::ConvertFinished(result));
    });
}

/// Build a sanitized C identifier from a filesystem stem.
///
/// - Non-`[A-Za-z0-9_]` bytes become `_`.
/// - A leading digit is prefixed with `_`.
/// - Empty input becomes `_img`.
///
/// PRESERVED-DELTA vs Python: the legacy `convertor_core.py` does NOT
/// sanitize and silently emits invalid C files when input names contain
/// hyphens or unicode. We fix this. (Plan 9 D5.)
pub fn sanitize_c_identifier(stem: impl AsRef<str>) -> String {
    let stem = stem.as_ref();
    let mut out = String::with_capacity(stem.len() + 1);
    let mut chars = stem.chars();
    if let Some(first) = chars.next() {
        if first.is_ascii_digit() {
            out.push('_');
        }
        if first.is_ascii_alphanumeric() || first == '_' {
            out.push(first);
        } else {
            out.push('_');
        }
    } else {
        return "_img".to_owned();
    }
    for c in chars {
        if c.is_ascii_alphanumeric() || c == '_' {
            out.push(c);
        } else {
            out.push('_');
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sanitize_passes_already_valid() {
        assert_eq!(sanitize_c_identifier("logo"), "logo");
        assert_eq!(sanitize_c_identifier("logo_v2"), "logo_v2");
    }

    #[test]
    fn sanitize_replaces_hyphens_dots_spaces() {
        assert_eq!(
            sanitize_c_identifier("my-image.v2 final"),
            "my_image_v2_final"
        );
    }

    #[test]
    fn sanitize_prefixes_leading_digit() {
        assert_eq!(sanitize_c_identifier("2x2_pattern"), "_2x2_pattern");
    }

    #[test]
    fn sanitize_empty_returns_default() {
        assert_eq!(sanitize_c_identifier(""), "_img");
    }

    #[test]
    fn sanitize_unicode_becomes_underscores() {
        // legacy Python would emit "图标.c" → invalid; we sanitize.
        assert_eq!(sanitize_c_identifier("图标"), "__");
    }
}
