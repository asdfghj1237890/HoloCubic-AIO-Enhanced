//! Video-converter bridge for the Studio Tauri host.
//!
//! Pipes the two-step ffmpeg pipeline (resize → format-convert) from
//! the egui tool's `video_converter_worker.rs` into the Tauri
//! command surface, with one addition: ffmpeg's stderr `time=HH:MM:SS.MS`
//! progress lines are parsed against the `Duration:` header so the JS
//! progress bar shows real percent rather than the egui app's
//! line-by-line log scroll.
//!
//! Per-phase events ride `video:event`:
//!
//!     { kind: "phase",    phase: 1 | 2 }                   — step boundary
//!     { kind: "progress", phase, percent: 0..100 }         — time= ticks
//!     { kind: "log",      line: "...ffmpeg stderr/stdout" } — raw line
//!     { kind: "finished", ok, out_path, error }            — terminal
//!
//! Cancel comes via the shared `convert_cancel` AtomicBool — independent
//! from the image converter's so the JS sides can use the same "Cancel"
//! semantics without crosstalk.

use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::channel;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration as StdDuration;

use serde::{Deserialize, Serialize};
use tauri::{AppHandle, Emitter};

/// One picked source video, surfaced to JS via `video_pick_source`.
#[derive(Serialize, Clone)]
pub struct VideoSourceDto {
    /// Absolute path on disk.
    pub path: String,
    /// Basename.
    pub name: String,
    /// File size in bytes.
    pub size: u64,
}

/// Encoder settings passed by `video_run`.
#[derive(Deserialize, Clone)]
pub struct VideoJobDto {
    /// Source absolute path.
    pub src: String,
    /// Output absolute path. Caller picks (`video_pick_output`) so the
    /// extension matches `format`.
    pub out: String,
    /// Output width.
    pub w: u32,
    /// Output height.
    pub h: u32,
    /// Frames per second.
    pub fps: u32,
    /// ffmpeg `-q:v` (1–9 per the Python tool's slider).
    pub quality: u32,
    /// Output codec: `"MJPEG"` or `"rgb565be"`.
    pub format: String,
}

/// Stream payload — matches the documented kebab-case envelope.
#[derive(Serialize, Clone)]
#[serde(tag = "kind", rename_all = "kebab-case")]
pub enum VideoEventDto {
    /// 1 = resize step, 2 = encode step.
    Phase { phase: u8 },
    /// Per-phase percent computed from `time=` over the source duration.
    Progress { phase: u8, percent: u8 },
    /// Raw stderr/stdout line for the right-hand log column.
    Log { line: String },
    /// Terminal event. `out_path` is filled on success.
    Finished {
        ok: bool,
        out_path: String,
        error: String,
    },
}

/// Output codec selected on the JS side.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum VideoFormat {
    Mjpeg,
    Rgb565be,
}

impl VideoFormat {
    fn parse(raw: &str) -> Result<Self, String> {
        match raw {
            "MJPEG" => Ok(Self::Mjpeg),
            "rgb565be" => Ok(Self::Rgb565be),
            other => Err(format!("unknown video format `{other}`")),
        }
    }
}

/// True iff `ffmpeg -version` exits 0. Cheap; safe to call from the
/// Tauri command thread.
pub fn ffmpeg_present() -> bool {
    Command::new("ffmpeg")
        .arg("-version")
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status()
        .map(|s| s.success())
        .unwrap_or(false)
}

/// Open a native picker for the source video.
pub fn pick_source() -> Option<VideoSourceDto> {
    let picked = rfd::FileDialog::new()
        .add_filter("Videos", &["mp4", "mov", "avi", "mkv", "webm"])
        .pick_file()?;
    let meta = std::fs::metadata(&picked).ok()?;
    let name = picked
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or("(unknown)")
        .to_owned();
    Some(VideoSourceDto {
        path: picked.to_string_lossy().into_owned(),
        name,
        size: meta.len(),
    })
}

/// Open a save-as dialog with `default_name` (extension chosen by
/// caller to match the picked format).
pub fn pick_output(default_name: &str) -> Option<String> {
    let path = rfd::FileDialog::new()
        .set_file_name(default_name)
        .save_file()?;
    Some(path.to_string_lossy().into_owned())
}

/// Spawn the two-step ffmpeg pipeline. Returns immediately; events
/// arrive on `video:event`. The first failure (or cancel) ends the
/// job and emits `Finished { ok: false, ... }`.
pub fn spawn_job(job: VideoJobDto, cancel: Arc<AtomicBool>, app: AppHandle) -> Result<(), String> {
    let fmt = VideoFormat::parse(&job.format)?;
    let src = PathBuf::from(&job.src);
    let out = PathBuf::from(&job.out);
    let cache_dir = std::env::temp_dir().join("aio-studio-video-cache");

    cancel.store(false, Ordering::Relaxed);

    thread::spawn(move || run_job(src, out, fmt, job, cache_dir, cancel, app));
    Ok(())
}

fn run_job(
    src: PathBuf,
    out: PathBuf,
    fmt: VideoFormat,
    job: VideoJobDto,
    cache_dir: PathBuf,
    cancel: Arc<AtomicBool>,
    app: AppHandle,
) {
    let _ = std::fs::create_dir_all(&cache_dir);

    let src_stem = src
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("video")
        .to_owned();
    let src_ext = src
        .extension()
        .and_then(|s| s.to_str())
        .unwrap_or("mp4")
        .to_owned();
    let cache = cache_dir.join(format!(
        "{src_stem}_{}x{}_cache.{}",
        job.w, job.h, src_ext
    ));

    // Step 1 — resize.
    emit(&app, VideoEventDto::Phase { phase: 1 });
    let resize_args = build_resize_args(&src, job.w, job.h, &cache);
    if let Err(msg) = run_step(1, &resize_args, &cancel, &app) {
        let cancelled = cancel.load(Ordering::Relaxed);
        emit(
            &app,
            VideoEventDto::Finished {
                ok: false,
                out_path: String::new(),
                error: if cancelled { "cancelled".to_owned() } else { msg },
            },
        );
        return;
    }

    // Step 2 — convert.
    emit(&app, VideoEventDto::Phase { phase: 2 });
    let convert_args = build_convert_args(&cache, job.w, job.h, job.fps, job.quality, fmt, &out);
    if let Err(msg) = run_step(2, &convert_args, &cancel, &app) {
        let cancelled = cancel.load(Ordering::Relaxed);
        emit(
            &app,
            VideoEventDto::Finished {
                ok: false,
                out_path: String::new(),
                error: if cancelled { "cancelled".to_owned() } else { msg },
            },
        );
        let _ = std::fs::remove_file(&cache);
        return;
    }

    let _ = std::fs::remove_file(&cache);
    emit(
        &app,
        VideoEventDto::Finished {
            ok: true,
            out_path: out.to_string_lossy().into_owned(),
            error: String::new(),
        },
    );
}

/// Run one ffmpeg invocation. Streams stderr lines through the log bus
/// + extracts percent from `time=HH:MM:SS.MS` against the source
/// `Duration:` header. Returns `Err` on non-zero exit, spawn failure, or
/// killed-by-cancel.
fn run_step(
    phase: u8,
    args: &[String],
    cancel: &Arc<AtomicBool>,
    app: &AppHandle,
) -> Result<(), String> {
    emit_log(app, format!("=== Step {phase} ==="));
    emit_log(app, format!("ffmpeg {}", args.join(" ")));

    let mut child = Command::new("ffmpeg")
        .args(args)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| format!("spawn ffmpeg: {e}"))?;

    let stdout = child.stdout.take();
    let stderr = child.stderr.take();
    let child = Arc::new(Mutex::new(child));

    // Watcher thread: kills the child on cancel so the read loops below
    // return EOF and we can wait() promptly. Mirrors the egui worker —
    // ffmpeg ignores SIGINT on Windows so kill() is the only path.
    let cancel_watcher = {
        let cancel = Arc::clone(cancel);
        let child = Arc::clone(&child);
        thread::spawn(move || loop {
            if cancel.load(Ordering::Relaxed) {
                if let Ok(mut c) = child.lock() {
                    let _ = c.kill();
                }
                break;
            }
            if let Ok(mut c) = child.lock() {
                if matches!(c.try_wait(), Ok(Some(_))) {
                    break;
                }
            }
            thread::sleep(StdDuration::from_millis(200));
        })
    };

    // Forward stderr lines on a side thread; the main thread handles stdout
    // (which ffmpeg leaves quiet, but we still pump it so the pipe doesn't
    // fill and block the child).
    let stderr_thread = stderr.map(|err| {
        let (tx, rx) = channel::<String>();
        let t = thread::spawn(move || {
            let mut reader = BufReader::new(err);
            let mut buf = String::new();
            loop {
                buf.clear();
                match reader.read_line(&mut buf) {
                    Ok(0) | Err(_) => break,
                    Ok(_) => {
                        if tx.send(buf.trim_end().to_owned()).is_err() {
                            break;
                        }
                    }
                }
            }
        });
        (t, rx)
    });

    let mut total_secs: Option<f64> = None;
    let mut last_pct_emitted: u8 = 0;

    // Drain stderr live (so the log scrolls and progress moves in real time),
    // then optionally drain stdout once stderr is done. ffmpeg's stderr is
    // line-buffered when going to a pipe — perfect for line-at-a-time updates.
    if let Some((stderr_join, rx)) = stderr_thread {
        loop {
            match rx.recv() {
                Ok(line) => {
                    if total_secs.is_none() {
                        if let Some(d) = parse_duration_header(&line) {
                            total_secs = Some(d);
                        }
                    }
                    if let Some(total) = total_secs {
                        if let Some(t) = parse_time_field(&line) {
                            let pct = ((t / total) * 100.0).clamp(0.0, 100.0) as u8;
                            if pct != last_pct_emitted {
                                last_pct_emitted = pct;
                                emit(app, VideoEventDto::Progress { phase, percent: pct });
                            }
                        }
                    }
                    emit_log(app, line);
                }
                Err(_) => break,
            }
        }
        let _ = stderr_join.join();
    }

    // Drain stdout (rarely non-empty for ffmpeg, but pump anyway).
    if let Some(stdout) = stdout {
        let mut reader = BufReader::new(stdout);
        let mut buf = String::new();
        loop {
            buf.clear();
            match reader.read_line(&mut buf) {
                Ok(0) | Err(_) => break,
                Ok(_) => emit_log(app, buf.trim_end().to_owned()),
            }
        }
    }

    let _ = cancel_watcher.join();

    let exit_status = child
        .lock()
        .map_err(|_| "child mutex poisoned".to_owned())?
        .wait()
        .map_err(|e| format!("wait: {e}"))?;
    if !exit_status.success() {
        return Err(format!("ffmpeg exited {exit_status}"));
    }
    emit(app, VideoEventDto::Progress { phase, percent: 100 });
    Ok(())
}

fn emit(app: &AppHandle, evt: VideoEventDto) {
    let _ = app.emit("video:event", evt);
}

fn emit_log(app: &AppHandle, line: String) {
    emit(app, VideoEventDto::Log { line });
}

/// `ffmpeg -y -i <src> -vf scale=W:H <cache>`
pub fn build_resize_args(src: &Path, w: u32, h: u32, cache: &Path) -> Vec<String> {
    vec![
        "-y".to_owned(),
        "-i".to_owned(),
        src.display().to_string(),
        "-vf".to_owned(),
        format!("scale={w}:{h}"),
        cache.display().to_string(),
    ]
}

/// `ffmpeg -y -i <cache> -vf fps=N,scale=-1:H:flags=lanczos,crop=W:in_h:... [-c:v rawvideo -pix_fmt rgb565be] -q:v Q <out>`
fn build_convert_args(
    cache: &Path,
    w: u32,
    h: u32,
    fps: u32,
    quality: u32,
    format: VideoFormat,
    out: &Path,
) -> Vec<String> {
    let vf = format!("fps={fps},scale=-1:{h}:flags=lanczos,crop={w}:in_h:(in_w-{w})/2:0");
    let mut args = vec![
        "-y".to_owned(),
        "-i".to_owned(),
        cache.display().to_string(),
        "-vf".to_owned(),
        vf,
    ];
    if matches!(format, VideoFormat::Rgb565be) {
        args.extend([
            "-c:v".to_owned(),
            "rawvideo".to_owned(),
            "-pix_fmt".to_owned(),
            "rgb565be".to_owned(),
        ]);
    }
    args.extend([
        "-q:v".to_owned(),
        quality.to_string(),
        out.display().to_string(),
    ]);
    args
}

/// Suggested output filename for the save dialog, given a source path
/// and a format. `<stem>_<w>x<h>.<ext>` — mirrors the egui worker's
/// output naming convention.
pub fn default_output_name(src_path: &str, w: u32, h: u32, format: &str) -> String {
    let stem = Path::new(src_path)
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("video");
    let ext = match format {
        "MJPEG" => "mjpeg",
        "rgb565be" => "rgb",
        _ => "out",
    };
    format!("{stem}_{w}x{h}.{ext}")
}

/// Parse `Duration: 00:03:39.05, ...` (the field ffmpeg prints once per
/// file at the top of stderr) into seconds.
fn parse_duration_header(line: &str) -> Option<f64> {
    let pos = line.find("Duration:")?;
    let rest = line[pos + "Duration:".len()..].trim_start();
    let end = rest.find(',').unwrap_or(rest.len());
    parse_hms(&rest[..end])
}

/// Parse `time=00:01:23.45` (printed repeatedly during encoding) into seconds.
fn parse_time_field(line: &str) -> Option<f64> {
    let pos = line.find("time=")?;
    let rest = &line[pos + "time=".len()..];
    let end = rest.find(' ').unwrap_or(rest.len());
    parse_hms(rest[..end].trim())
}

/// `HH:MM:SS[.ms]` → seconds. Tolerates two-digit hour. Returns None on
/// any parse hiccup so callers fall through to "no progress".
fn parse_hms(s: &str) -> Option<f64> {
    let mut parts = s.split(':');
    let h: f64 = parts.next()?.parse().ok()?;
    let m: f64 = parts.next()?.parse().ok()?;
    let s: f64 = parts.next()?.parse().ok()?;
    Some(h * 3600.0 + m * 60.0 + s)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_duration_header() {
        let line = "  Duration: 00:03:39.05, start: 0.000000, bitrate: 1234 kb/s";
        assert_eq!(parse_duration_header(line), Some(219.05));
    }

    #[test]
    fn parses_time_field() {
        let line = "frame= 1024 fps=120 q=24.0 size=    512kB time=00:00:51.20 bitrate= 81.9kbits/s";
        assert_eq!(parse_time_field(line), Some(51.20));
    }

    #[test]
    fn rejects_garbage_hms() {
        assert!(parse_hms("not:a:time").is_none());
        assert!(parse_hms("99").is_none());
    }

    #[test]
    fn default_output_name_picks_extension() {
        assert_eq!(
            default_output_name("/tmp/Bad Apple.mp4", 240, 240, "MJPEG"),
            "Bad Apple_240x240.mjpeg"
        );
        assert_eq!(
            default_output_name("/x/clip.mkv", 320, 240, "rgb565be"),
            "clip_320x240.rgb"
        );
    }

    #[test]
    fn resize_args_layout() {
        let args = build_resize_args(
            Path::new("/tmp/in.mp4"),
            240,
            240,
            Path::new("/tmp/cache.mp4"),
        );
        assert_eq!(args[0], "-y");
        assert_eq!(args[3], "-vf");
        assert_eq!(args[4], "scale=240:240");
    }

    #[test]
    fn convert_args_mjpeg_skips_rawvideo() {
        let args = build_convert_args(
            Path::new("/tmp/c.mp4"),
            240,
            240,
            20,
            5,
            VideoFormat::Mjpeg,
            Path::new("/tmp/o.mjpeg"),
        );
        assert!(!args.iter().any(|a| a == "rawvideo"));
    }

    #[test]
    fn convert_args_rgb_includes_pix_fmt() {
        let args = build_convert_args(
            Path::new("/tmp/c.mp4"),
            240,
            240,
            20,
            5,
            VideoFormat::Rgb565be,
            Path::new("/tmp/o.rgb"),
        );
        let pix = args.iter().position(|a| a == "-pix_fmt").expect("pix_fmt");
        assert_eq!(args[pix + 1], "rgb565be");
    }
}
