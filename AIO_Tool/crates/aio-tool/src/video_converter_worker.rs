//! Background worker for the Video Converter tab.
//!
//! Wraps a two-step ffmpeg invocation (resize -> format-convert) using
//! `std::process::Command`. Streams stdout/stderr lines onto the bus.
//! Cancel is via `child.kill()` (no graceful shutdown — ffmpeg ignores
//! SIGINT on Windows anyway).
//!
//! Plan 7 polish carry-overs:
//! - `BufReader::read_line` paces the stdout pump; a separate watcher
//!   thread polls the cancel flag every 200ms and calls `child.kill()`
//!   when it flips (ffmpeg writes progress to stderr, so stdout stays
//!   quiet — checking cancel inline before each `read_line` would let
//!   Cancel hang until ffmpeg exited naturally).
//! - Cancel flag is **supplied by the caller** (matches the Image Converter
//!   post-fix shape, commit dcdfe66) so one Cancel button can cancel every
//!   in-flight job; the worker only reads it.
//! - `bus_tx.send` failure breaks the loop AND kills the child.

use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;

use crate::bus::{AppEvent, AppEventTx};

/// Resolution + encoder settings captured at click time.
pub struct Job {
    /// Source video path.
    pub src: PathBuf,
    /// Output directory (final out path = `{dst_dir}/{stem}_{w}x{h}.{rgb|mjpeg}`).
    pub dst_dir: PathBuf,
    /// Output width in px.
    pub width: u32,
    /// Output height in px.
    pub height: u32,
    /// Frames per second.
    pub fps: u32,
    /// ffmpeg `-q:v` value (1-9 — Python tool's range).
    pub quality: u32,
    /// Output codec.
    pub format: VideoFormat,
    /// Cache dir for the intermediate resize output.
    pub cache_dir: PathBuf,
}

/// Output codec choice.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VideoFormat {
    /// MJPEG -> `.mjpeg` output.
    Mjpeg,
    /// rawvideo rgb565be -> `.rgb` output.
    Rgb565be,
}

impl VideoFormat {
    /// File-extension suffix (with leading dot).
    pub fn ext(self) -> &'static str {
        match self {
            Self::Mjpeg => ".mjpeg",
            Self::Rgb565be => ".rgb",
        }
    }
}

/// Probe whether `ffmpeg -version` runs. Returns false if ffmpeg isn't on
/// PATH OR exits non-zero. **Does not spawn a worker** — called from the
/// UI thread on tab init / refresh.
pub fn ffmpeg_present() -> bool {
    Command::new("ffmpeg")
        .arg("-version")
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status()
        .map(|s| s.success())
        .unwrap_or(false)
}

/// Build the `ffmpeg -y -i <src> -vf scale=W:H <cache>` arg vector
/// (no shell quoting needed — we pass args directly).
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

/// Build the format-conversion args (Step 2).
///
/// Translates the Python tool's two cmd templates (cmd_to_rgb / cmd_to_mjpeg
/// at videotool.py:273-276) into argv form.
pub fn build_convert_args(
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

/// Spawn one ffmpeg conversion job.
///
/// `cancel` is owned by the caller (the UI keeps a clone for the Cancel
/// button); the worker only reads it. Matches the Image Converter shape
/// fixed in dcdfe66.
pub fn spawn(job: Job, bus_tx: AppEventTx, cancel: Arc<AtomicBool>) {
    thread::spawn(move || run(job, bus_tx, cancel));
}

fn run(job: Job, bus_tx: AppEventTx, cancel: Arc<AtomicBool>) {
    let _ = std::fs::create_dir_all(&job.cache_dir);

    let src_stem = job
        .src
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("video")
        .to_owned();
    let src_ext = job
        .src
        .extension()
        .and_then(|s| s.to_str())
        .unwrap_or("mp4")
        .to_owned();

    let cache = job.cache_dir.join(format!(
        "{src_stem}_{}x{}_cache.{}",
        job.width, job.height, src_ext
    ));
    let out = job.dst_dir.join(format!(
        "{src_stem}_{}x{}{}",
        job.width,
        job.height,
        job.format.ext()
    ));

    // Step 1: resize.
    let resize_args = build_resize_args(&job.src, job.width, job.height, &cache);
    let send_log = |line: String| -> bool { bus_tx.send(AppEvent::VideoConvertLog(line)).is_ok() };
    if !run_step("Step 1: Resize", &resize_args, &cancel, &send_log) {
        // Distinguish cancel vs failure for the final report.
        let result = if cancel.load(Ordering::Relaxed) {
            Err("cancelled".to_owned())
        } else {
            Err("resize step failed".to_owned())
        };
        let _ = bus_tx.send(AppEvent::VideoConvertFinished(result));
        return;
    }

    // Step 2: convert.
    let convert_args = build_convert_args(
        &cache,
        job.width,
        job.height,
        job.fps,
        job.quality,
        job.format,
        &out,
    );
    if !run_step("Step 2: Convert", &convert_args, &cancel, &send_log) {
        let result = if cancel.load(Ordering::Relaxed) {
            Err("cancelled".to_owned())
        } else {
            Err("convert step failed".to_owned())
        };
        let _ = bus_tx.send(AppEvent::VideoConvertFinished(result));
        return;
    }

    let _ = std::fs::remove_file(&cache);
    let _ = bus_tx.send(AppEvent::VideoConvertFinished(Ok(out)));
}

/// Returns false on cancel OR non-zero exit OR spawn failure.
fn run_step<F: Fn(String) -> bool>(
    description: &str,
    args: &[String],
    cancel: &Arc<AtomicBool>,
    send_log: &F,
) -> bool {
    let _ = send_log(format!("=== {description} ==="));
    let _ = send_log(format!("ffmpeg {}", args.join(" ")));
    let mut child = match Command::new("ffmpeg")
        .args(args)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
    {
        Ok(c) => c,
        Err(e) => {
            let _ = send_log(format!("\u{2717} spawn: {e}"));
            return false;
        }
    };

    // Take the pipes BEFORE moving the child into the shared mutex; we need
    // them on this thread (stdout) and the forwarder thread (stderr).
    let stdout = child.stdout.take();
    let stderr = child.stderr.take();
    let child = Arc::new(Mutex::new(child));

    // Cancel watcher: kills the child when the flag flips. Polls every 200ms
    // so cancel latency is bounded by the poll interval. Without this, the
    // stdout read_line below would block until ffmpeg writes to stdout (rare
    // — ffmpeg writes progress to stderr) or exits naturally, making Cancel
    // a silent no-op on long encodes. The watcher also self-exits via
    // try_wait so it doesn't leak when the child completes normally.
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
            // Detect natural exit so we don't keep polling forever.
            if let Ok(mut c) = child.lock() {
                if matches!(c.try_wait(), Ok(Some(_))) {
                    break;
                }
            }
            thread::sleep(std::time::Duration::from_millis(200));
        })
    };

    // Pump stderr (ffmpeg writes progress here) on a forwarder thread; we
    // own stdout pumping on this thread.
    let stderr_thread = stderr.map(|err| {
        // We can't borrow send_log across threads; use a channel instead.
        let (tx, rx) = std::sync::mpsc::channel::<String>();
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

    if let Some(stdout) = stdout {
        let mut reader = BufReader::new(stdout);
        let mut buf = String::new();
        loop {
            buf.clear();
            match reader.read_line(&mut buf) {
                Ok(0) => break,
                Ok(_) => {
                    if !send_log(buf.trim_end().to_owned()) {
                        // Bus dropped — kill the child so we don't leak it.
                        if let Ok(mut c) = child.lock() {
                            let _ = c.kill();
                        }
                        let _ = cancel_watcher.join();
                        return false;
                    }
                }
                Err(_) => break,
            }
        }
    }

    // Drain stderr forwarder.
    if let Some((t, rx)) = stderr_thread {
        for line in rx.try_iter() {
            let _ = send_log(line);
        }
        let _ = t.join();
    }

    // Join the watcher before waiting — once we exit either it has already
    // killed the child (cancel path) or try_wait will catch the natural exit
    // shortly. Either way it terminates quickly.
    let _ = cancel_watcher.join();

    let exit_status = match child.lock() {
        Ok(mut c) => c.wait(),
        Err(_) => {
            // Poisoned mutex — treat as failure.
            let _ = send_log("\u{2717} child mutex poisoned".to_owned());
            return false;
        }
    };
    match exit_status {
        Ok(status) if status.success() => true,
        Ok(status) => {
            let _ = send_log(format!("\u{2717} ffmpeg exited {}", status));
            false
        }
        Err(e) => {
            let _ = send_log(format!("\u{2717} wait: {e}"));
            false
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn resize_args_shape() {
        let args = build_resize_args(
            &PathBuf::from("/tmp/in.mp4"),
            240,
            240,
            &PathBuf::from("/tmp/cache.mp4"),
        );
        assert_eq!(args[0], "-y");
        assert_eq!(args[1], "-i");
        assert!(args[2].ends_with("in.mp4"));
        assert_eq!(args[3], "-vf");
        assert_eq!(args[4], "scale=240:240");
        assert!(args[5].ends_with("cache.mp4"));
    }

    #[test]
    fn convert_args_mjpeg_omits_rawvideo_flags() {
        let args = build_convert_args(
            &PathBuf::from("/tmp/cache.mp4"),
            240,
            240,
            20,
            5,
            VideoFormat::Mjpeg,
            &PathBuf::from("/tmp/out.mjpeg"),
        );
        assert!(!args.iter().any(|a| a == "rawvideo"));
        assert!(!args.iter().any(|a| a == "-pix_fmt"));
        // -q:v 5 present.
        let qv_idx = args.iter().position(|a| a == "-q:v").expect("-q:v present");
        assert_eq!(args[qv_idx + 1], "5");
    }

    #[test]
    fn convert_args_rgb565be_includes_rawvideo_and_pix_fmt() {
        let args = build_convert_args(
            &PathBuf::from("/tmp/cache.mp4"),
            240,
            240,
            20,
            5,
            VideoFormat::Rgb565be,
            &PathBuf::from("/tmp/out.rgb"),
        );
        assert!(args.iter().any(|a| a == "rawvideo"));
        let pix_idx = args.iter().position(|a| a == "-pix_fmt").expect("-pix_fmt");
        assert_eq!(args[pix_idx + 1], "rgb565be");
    }

    #[test]
    fn convert_args_vf_includes_fps_scale_crop() {
        let args = build_convert_args(
            &PathBuf::from("/in"),
            240,
            240,
            20,
            5,
            VideoFormat::Mjpeg,
            &PathBuf::from("/out"),
        );
        let vf = args.iter().position(|a| a == "-vf").expect("-vf");
        let value = &args[vf + 1];
        assert!(value.contains("fps=20"));
        assert!(value.contains("scale=-1:240:flags=lanczos"));
        assert!(value.contains("crop=240:in_h:(in_w-240)/2:0"));
    }

    #[test]
    fn video_format_extensions() {
        assert_eq!(VideoFormat::Mjpeg.ext(), ".mjpeg");
        assert_eq!(VideoFormat::Rgb565be.ext(), ".rgb");
    }
}
