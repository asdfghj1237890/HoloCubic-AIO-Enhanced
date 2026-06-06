//! Integration sanity check that the worker's pure-function argv builders
//! produce the same shape the legacy Python tool used (videotool.py:271-275).

use aio_tool::video_converter_worker::{build_convert_args, build_resize_args, VideoFormat};
use std::path::PathBuf;

#[test]
fn resize_step_matches_python_template_shape() {
    // Python: ffmpeg -y -i "{src}" -vf scale={w}:{h} "{cache}"
    let args = build_resize_args(
        &PathBuf::from("src.mp4"),
        240,
        240,
        &PathBuf::from("cache.mp4"),
    );
    let joined = args.join(" ");
    assert!(joined.contains("-y"));
    assert!(joined.contains("-i src.mp4"));
    assert!(joined.contains("-vf scale=240:240"));
    assert!(joined.ends_with("cache.mp4"));
}

#[test]
fn mjpeg_step_template_matches_python() {
    let args = build_convert_args(
        &PathBuf::from("cache.mp4"),
        240,
        240,
        20,
        5,
        VideoFormat::Mjpeg,
        &PathBuf::from("out.mjpeg"),
    );
    let joined = args.join(" ");
    // Python: -vf "fps={fps},scale=-1:{h}:flags=lanczos,crop={w}:in_h:(in_w-{w})/2:0"
    assert!(joined.contains("fps=20"));
    assert!(joined.contains("scale=-1:240:flags=lanczos"));
    assert!(joined.contains("crop=240:in_h:(in_w-240)/2:0"));
    // No rawvideo/pix_fmt for MJPEG.
    assert!(!joined.contains("rawvideo"));
}
