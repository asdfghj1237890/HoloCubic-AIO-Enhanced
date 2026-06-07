//! Image-converter bridge for the Studio Tauri host.
//!
//! Wraps `aio-converter::Converter` with a Tauri-friendly batch
//! command surface plus a Tauri-event progress stream. The egui
//! tool's `image_converter_worker.rs` runs one job per spawn; here we
//! batch the worker so the UI hook can fire a single `invoke()` for
//! the entire queue and let the event bus drive per-file UI updates.
//!
//! Output convention: `<source-dir>/OutFile/<basename>.{bin,c}`, the
//! same layout the legacy Python tool used (matches the JS prototype's
//! tooltip at studio-convert.jsx:178).

use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::channel;
use std::sync::Arc;

use serde::{Deserialize, Serialize};
use tauri::{AppHandle, Emitter};

use aio_converter::{ColorFormat, ConvertEvent, Converter};

/// One picked image file, surfaced to JS via `convert_pick_images`.
#[derive(Serialize, Clone)]
pub struct PickedImageDto {
    /// Absolute path on disk — round-tripped back to `convert_image_batch`
    /// so the bridge doesn't need to track file IDs.
    pub path: String,
    /// Basename (no directory).
    pub name: String,
    /// Lowercased extension without the dot — `"png"`, `"jpg"`, `"bmp"`.
    pub ext: String,
    /// File size in bytes (from `fs::metadata`).
    pub size: u64,
    /// Pixel width — `image::image_dimensions` reads only the header,
    /// so this is cheap even for huge files.
    pub w: u32,
    /// Pixel height.
    pub h: u32,
}

/// One item in a batch the JS side hands to `convert_image_batch`.
#[derive(Deserialize)]
pub struct ConvertItem {
    /// Absolute source path.
    pub path: String,
    /// Display name — echoed back in progress events for label rendering.
    pub name: String,
}

/// Progress / completion events streamed on the `convert:event` bus.
/// `index` is the 0-based offset into the original batch so JS can
/// update the matching row without juggling IDs.
#[derive(Serialize, Clone)]
#[serde(tag = "kind", rename_all = "kebab-case")]
pub enum ConvertEventDto {
    /// One file's encoder kicked off — `total_rows` lets JS pre-size the
    /// per-row progress denominator. (For `.c` output we synthesize a
    /// single "all done" tick since the encoder doesn't stream rows.)
    Start {
        /// Batch index of the file.
        index: usize,
        /// Source file name (for label).
        name: String,
        /// Total rows the encoder will process (image height).
        total_rows: u32,
    },
    /// Row-level progress for `Start { index, .. }` above.
    Progress {
        /// Batch index of the file.
        index: usize,
        /// Rows processed so far (0..total_rows).
        rows_processed: u32,
    },
    /// One file finished — `out_path` is where the bytes landed (or empty
    /// if `error` is set).
    FileDone {
        /// Batch index of the file.
        index: usize,
        /// Source file name.
        name: String,
        /// Absolute output path, or empty on error.
        out_path: String,
        /// Output size in bytes (0 on error).
        out_bytes: u64,
        /// Output dimensions, copied from the source.
        out_w: u32,
        /// Output dimensions, copied from the source.
        out_h: u32,
        /// Populated on error; empty on success.
        error: String,
    },
    /// Batch is complete — JS resets the busy flag and reports counts.
    Finished {
        /// Number of files that produced output.
        ok_count: usize,
        /// Number of files that failed (file-level error, not batch abort).
        err_count: usize,
        /// True if the user cancelled mid-batch.
        cancelled: bool,
    },
}

/// Open a native file picker and return metadata for the selected files.
///
/// Runs on the calling Tauri command thread — `rfd` is synchronous and
/// the command pool already gives us a worker, so blocking is fine.
/// Returns an empty vec on cancel.
pub fn pick_images() -> Vec<PickedImageDto> {
    let picked = rfd::FileDialog::new()
        .add_filter("Images", &["png", "jpg", "jpeg", "bmp"])
        .pick_files();
    let Some(paths) = picked else {
        return Vec::new();
    };
    paths
        .into_iter()
        .filter_map(|p| describe_one(&p).ok())
        .collect()
}

fn describe_one(path: &Path) -> Result<PickedImageDto, String> {
    let meta = std::fs::metadata(path).map_err(|e| e.to_string())?;
    let (w, h) = image::image_dimensions(path).map_err(|e| e.to_string())?;
    let name = path
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or("(unknown)")
        .to_owned();
    let ext = path
        .extension()
        .and_then(|e| e.to_str())
        .unwrap_or("")
        .to_lowercase();
    Ok(PickedImageDto {
        path: path.to_string_lossy().into_owned(),
        name,
        ext,
        size: meta.len(),
        w,
        h,
    })
}

/// Map the JS-side format string to the `ColorFormat` enum.
/// `JS keys are taken verbatim from studio-convert.jsx::IMG_FORMATS.
pub fn parse_format(raw: &str) -> Result<ColorFormat, String> {
    Ok(match raw {
        "RGB332" => ColorFormat::Rgb332,
        "RGB565" => ColorFormat::Rgb565,
        "RGB565_SWAP" => ColorFormat::Rgb565Swap,
        "RGB888" => ColorFormat::Rgb888,
        "Alpha_1bit" => ColorFormat::Alpha1,
        "Alpha_2bit" => ColorFormat::Alpha2,
        "Alpha_4bit" => ColorFormat::Alpha4,
        "Alpha_8bit" => ColorFormat::Alpha8,
        "Indexed_1bit" => ColorFormat::Indexed1,
        "Indexed_2bit" => ColorFormat::Indexed2,
        "Indexed_4bit" => ColorFormat::Indexed4,
        "Indexed_8bit" => ColorFormat::Indexed8,
        other => return Err(format!("unknown image format `{other}`")),
    })
}

/// Spawn a background thread that runs `items` through the converter
/// in order, emitting per-file progress on the `convert:event` bus.
///
/// The thread takes ownership of everything it needs; the only caller
/// state it borrows is the shared `cancel` flag, which `convert_image_cancel`
/// flips. On batch completion (success, file-level errors, or cancel)
/// the thread emits a `Finished` event with counts.
pub fn spawn_batch(
    items: Vec<ConvertItem>,
    format: ColorFormat,
    dither: bool,
    c_array: bool,
    cancel: Arc<AtomicBool>,
    app: AppHandle,
) {
    std::thread::spawn(move || run_batch(items, format, dither, c_array, cancel, app));
}

fn run_batch(
    items: Vec<ConvertItem>,
    format: ColorFormat,
    dither: bool,
    c_array: bool,
    cancel: Arc<AtomicBool>,
    app: AppHandle,
) {
    cancel.store(false, Ordering::Relaxed);
    let mut ok_count = 0usize;
    let mut err_count = 0usize;
    let mut cancelled = false;

    for (i, item) in items.iter().enumerate() {
        if cancel.load(Ordering::Relaxed) {
            cancelled = true;
            break;
        }
        match encode_one(item, format, dither, c_array, i, &cancel, &app) {
            Ok((out_path, out_bytes, w, h)) => {
                emit(
                    &app,
                    ConvertEventDto::FileDone {
                        index: i,
                        name: item.name.clone(),
                        out_path,
                        out_bytes,
                        out_w: w,
                        out_h: h,
                        error: String::new(),
                    },
                );
                ok_count += 1;
            }
            Err(e) => {
                let cancelled_now = e.to_lowercase().contains("cancel");
                if cancelled_now {
                    cancelled = true;
                    break;
                }
                emit(
                    &app,
                    ConvertEventDto::FileDone {
                        index: i,
                        name: item.name.clone(),
                        out_path: String::new(),
                        out_bytes: 0,
                        out_w: 0,
                        out_h: 0,
                        error: e,
                    },
                );
                err_count += 1;
            }
        }
    }

    emit(
        &app,
        ConvertEventDto::Finished {
            ok_count,
            err_count,
            cancelled,
        },
    );
}

fn encode_one(
    item: &ConvertItem,
    format: ColorFormat,
    dither: bool,
    c_array: bool,
    index: usize,
    cancel: &Arc<AtomicBool>,
    app: &AppHandle,
) -> Result<(String, u64, u32, u32), String> {
    let src = PathBuf::from(&item.path);
    let bytes = std::fs::read(&src).map_err(|e| format!("read {}: {e}", item.path))?;
    let conv = Converter::new(&bytes, format, dither).map_err(|e| format!("decode: {e}"))?;
    let w = conv.width();
    let h = conv.height();

    // Inform the UI before encoding starts so progress bar can size itself.
    emit(
        app,
        ConvertEventDto::Start {
            index,
            name: item.name.clone(),
            total_rows: h,
        },
    );

    let out_dir = match src.parent() {
        Some(parent) => parent.join("OutFile"),
        None => PathBuf::from("OutFile"),
    };
    std::fs::create_dir_all(&out_dir).map_err(|e| format!("create {}: {e}", out_dir.display()))?;

    let stem = src.file_stem().and_then(|s| s.to_str()).unwrap_or("output");
    let (out_path, out_bytes_len) = if c_array {
        // `.c` output has no row-level streaming; synthesize a 100% tick
        // when the string is ready.
        let text = conv
            .encode_c_array(&sanitize_c_ident(stem))
            .map_err(|e| format!("encode .c: {e}"))?;
        emit(
            app,
            ConvertEventDto::Progress {
                index,
                rows_processed: h,
            },
        );
        let out_path = out_dir.join(format!("{stem}.c"));
        std::fs::write(&out_path, text.as_bytes())
            .map_err(|e| format!("write {}: {e}", out_path.display()))?;
        let n = std::fs::metadata(&out_path).map(|m| m.len()).unwrap_or(0);
        (out_path, n)
    } else {
        // Spin a forwarder thread that pumps the converter's mpsc channel
        // into the JS event bus. Cancellation is the converter's
        // responsibility — it polls the AtomicBool at row boundaries.
        let (tx, rx) = channel::<ConvertEvent>();
        let app_for_fwd = app.clone();
        let fwd_index = index;
        let fwd = std::thread::spawn(move || {
            while let Ok(evt) = rx.recv() {
                if let ConvertEvent::Progress { rows_processed } = evt {
                    let _ = app_for_fwd.emit(
                        "convert:event",
                        ConvertEventDto::Progress {
                            index: fwd_index,
                            rows_processed,
                        },
                    );
                }
            }
        });
        let bin = conv
            .encode_bin(Some(tx), Some(cancel.clone()))
            .map_err(|e| format!("encode .bin: {e}"))?;
        let _ = fwd.join();
        let out_path = out_dir.join(format!("{stem}.bin"));
        std::fs::write(&out_path, &bin)
            .map_err(|e| format!("write {}: {e}", out_path.display()))?;
        (out_path, bin.len() as u64)
    };

    Ok((out_path.to_string_lossy().into_owned(), out_bytes_len, w, h))
}

fn emit(app: &AppHandle, evt: ConvertEventDto) {
    let _ = app.emit("convert:event", evt);
}

/// Replace anything not `[A-Za-z0-9_]` with `_` and prefix a `_` if the
/// first char is a digit. Mirrors the Python tool's loose sanitisation.
fn sanitize_c_ident(name: &str) -> String {
    let mut out = String::with_capacity(name.len());
    for (i, c) in name.chars().enumerate() {
        let ok = c.is_ascii_alphanumeric() || c == '_';
        if i == 0 && c.is_ascii_digit() {
            out.push('_');
        }
        out.push(if ok { c } else { '_' });
    }
    if out.is_empty() {
        out.push_str("img");
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sanitize_replaces_punctuation() {
        assert_eq!(sanitize_c_ident("foo.bar"), "foo_bar");
        assert_eq!(sanitize_c_ident("a-b-c"), "a_b_c");
        assert_eq!(sanitize_c_ident(""), "img");
        assert_eq!(sanitize_c_ident("123abc"), "_123abc");
    }

    #[test]
    fn parse_format_known_keys() {
        assert!(matches!(
            parse_format("RGB565").unwrap(),
            ColorFormat::Rgb565
        ));
        assert!(matches!(
            parse_format("RGB565_SWAP").unwrap(),
            ColorFormat::Rgb565Swap
        ));
        assert!(matches!(
            parse_format("Indexed_4bit").unwrap(),
            ColorFormat::Indexed4
        ));
        assert!(matches!(
            parse_format("Alpha_8bit").unwrap(),
            ColorFormat::Alpha8
        ));
    }

    #[test]
    fn parse_format_rejects_unknown() {
        assert!(parse_format("RGB128").is_err());
    }
}
