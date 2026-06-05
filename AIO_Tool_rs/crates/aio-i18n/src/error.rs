//! Typed error variants for `aio-i18n`.

use std::io;
use thiserror::Error;

/// Errors that can happen reading `config.json`.
#[derive(Debug, Error)]
pub enum LoadError {
    /// OS reported an I/O failure (permissions, disk, etc.).
    #[error("io error reading config: {0}")]
    Io(#[from] io::Error),

    /// File parsed but wasn't a JSON object (matches Python's `_load_translation`
    /// non-object guard, Plan 2 D5).
    #[error("config file is not a JSON object")]
    NotAnObject,

    /// The `language` field was present but wasn't a known locale code (Plan 2 D3).
    #[error("unknown language code in config: {0}")]
    UnknownLang(String),

    /// JSON syntax error.
    #[error("json parse error: {0}")]
    Json(#[from] serde_json::Error),
}

/// Errors that can happen writing `config.json`.
#[derive(Debug, Error)]
pub enum SaveError {
    /// OS reported an I/O failure (permissions, disk full, etc.).
    #[error("io error writing config: {0}")]
    Io(#[from] io::Error),

    /// Couldn't find a per-user config dir (no $HOME, broken Windows shell folders, etc.).
    #[error("could not determine per-user config directory")]
    NoConfigDir,

    /// JSON serialize error (effectively unreachable but threaded for clean typing).
    #[error("json serialize error: {0}")]
    Json(#[from] serde_json::Error),
}
