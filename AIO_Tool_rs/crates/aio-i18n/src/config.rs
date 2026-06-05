//! Per-user config persistence: `config.json` in the platform-conventional
//! config dir (Windows: %APPDATA%, macOS: ~/Library/Application Support, Linux:
//! ~/.config). Tracks the user's chosen language plus any future settings
//! `aio-tool` decides to put there. Writes are round-trip-safe (D2): we read
//! the existing file, update only the keys we care about, and write back.

use std::fs;
use std::path::{Path, PathBuf};

use directories::ProjectDirs;
use serde::{Deserialize, Serialize};

use crate::error::{LoadError, SaveError};
use crate::lang::Lang;

/// Project identifier used to compute config / cache / data dirs across platforms.
const QUALIFIER: &str = "io";
const ORG: &str = "HoloCubic";
const APP: &str = "HoloCubic-AIO";

/// Top-level config schema. Currently only `language`. `extra` collects any
/// unrelated top-level keys so a future aio-tool can add settings without
/// invalidating users' existing files.
#[derive(Debug, Default, Serialize, Deserialize)]
pub struct ConfigDoc {
    /// Saved language code (Lang::code()). Absent on first launch.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub language: Option<String>,

    /// Any extra fields a future aio-tool version writes are preserved
    /// round-trip (D2 — Python's save_language_preference preserves siblings).
    #[serde(flatten)]
    pub extra: serde_json::Map<String, serde_json::Value>,
}

/// Resolve `<user-config-dir>/config.json`. Returns `None` if no per-user
/// config dir is available (rare; e.g. Linux without HOME and without XDG_CONFIG_HOME).
pub fn config_path() -> Option<PathBuf> {
    let dirs = ProjectDirs::from(QUALIFIER, ORG, APP)?;
    Some(dirs.config_dir().join("config.json"))
}

/// Load language from `config.json`. Returns `Lang::DEFAULT` if:
/// - File doesn't exist (first launch)
/// - File parses but has no `language` field
/// - `language` field isn't a known code
///
/// Surfaces typed errors via `LoadError` for io and JSON-syntax failures
/// (callers may choose to log and fall back). "No per-user config dir at
/// all" returns `Lang::DEFAULT` without error since the user simply hasn't
/// saved anything yet.
pub fn load_language() -> Result<Lang, LoadError> {
    let Some(path) = config_path() else {
        return Ok(Lang::DEFAULT);
    };
    load_language_from(&path)
}

/// Save the user's language to `config.json`, preserving any other top-level
/// keys (D2 — round-trip-safe write). Creates the parent directory if it
/// doesn't exist yet.
pub fn save_language(lang: Lang) -> Result<(), SaveError> {
    let path = config_path().ok_or(SaveError::NoConfigDir)?;
    save_language_to(&path, lang)
}

/// Path-injectable variant of [`load_language`] — tests target a tmp dir
/// without poking process-global env vars (which don't always redirect
/// `directories::ProjectDirs` on Windows because of SHGetKnownFolderPath).
fn load_language_from(path: &Path) -> Result<Lang, LoadError> {
    if !path.exists() {
        return Ok(Lang::DEFAULT);
    }
    let contents = fs::read_to_string(path)?;
    // Parse to Value first so non-object JSON (array, scalar) surfaces as
    // `NotAnObject` rather than a generic `serde_json::Error`. Matches
    // Python's `_load_translation` non-object guard (D5).
    let value: serde_json::Value = serde_json::from_str(&contents)?;
    if !value.is_object() {
        return Err(LoadError::NotAnObject);
    }
    let doc: ConfigDoc = serde_json::from_value(value)?;
    match doc.language.as_deref() {
        // Unknown lang code silently falls back to default (D3); we deliberately
        // do not propagate this as an error variant.
        Some(code) => Ok(code.parse().unwrap_or(Lang::DEFAULT)),
        None => Ok(Lang::DEFAULT),
    }
}

/// Path-injectable variant of [`save_language`] — see [`load_language_from`]
/// for the rationale.
fn save_language_to(path: &Path, lang: Lang) -> Result<(), SaveError> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }

    // Read-modify-write to preserve any extra fields.
    let mut doc: ConfigDoc = if path.exists() {
        let contents = fs::read_to_string(path)?;
        serde_json::from_str(&contents).unwrap_or_default()
    } else {
        ConfigDoc::default()
    };
    doc.language = Some(lang.code().to_owned());

    let json = serde_json::to_string_pretty(&doc)?;
    fs::write(path, json)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::env;
    use std::sync::atomic::{AtomicUsize, Ordering};

    /// Per-test counter so each test gets its own sandbox path within `temp_dir()`,
    /// avoiding collisions between parallel tests and between repeat runs.
    static COUNTER: AtomicUsize = AtomicUsize::new(0);

    /// Build a unique, freshly-cleaned tmp path for a test's `config.json`.
    /// We deliberately do NOT redirect via env vars — on Windows
    /// `directories::ProjectDirs` resolves through SHGetKnownFolderPath which
    /// ignores `%APPDATA%`, so env-var sandboxing leaks into the user's real
    /// AppData. The path-injectable `*_from` / `*_to` helpers let us hit a
    /// real tmp dir directly.
    fn fresh_tmp_config(name: &str) -> PathBuf {
        let pid = std::process::id();
        let n = COUNTER.fetch_add(1, Ordering::SeqCst);
        let mut dir = env::temp_dir();
        dir.push(format!("aio-i18n-test-{pid}-{n}-{name}"));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).expect("create tmp dir");
        dir.join("config.json")
    }

    #[test]
    fn load_language_defaults_when_no_config() {
        let path = fresh_tmp_config("no_config");
        // Path is in a freshly-created (empty) dir → file does not exist.
        let lang = load_language_from(&path).expect("load should succeed");
        assert_eq!(lang, Lang::DEFAULT);
        let _ = fs::remove_dir_all(path.parent().unwrap());
    }

    #[test]
    fn save_then_load_roundtrip() {
        let path = fresh_tmp_config("roundtrip");
        save_language_to(&path, Lang::EnUs).expect("save should succeed");
        let loaded = load_language_from(&path).expect("load should succeed");
        assert_eq!(loaded, Lang::EnUs);
        let _ = fs::remove_dir_all(path.parent().unwrap());
    }

    #[test]
    fn save_preserves_extra_fields() {
        let path = fresh_tmp_config("extras");
        fs::write(
            &path,
            r#"{"language":"zh_CN","window_width":1200,"window_height":720}"#,
        )
        .unwrap();

        save_language_to(&path, Lang::ZhTw).expect("save should succeed");

        let contents = fs::read_to_string(&path).unwrap();
        assert!(contents.contains("zh_TW"));
        assert!(contents.contains("window_width"));
        assert!(contents.contains("window_height"));
        assert!(contents.contains("1200"));
        assert!(contents.contains("720"));
        let _ = fs::remove_dir_all(path.parent().unwrap());
    }

    #[test]
    fn load_unknown_language_defaults() {
        let path = fresh_tmp_config("unknown");
        fs::write(&path, r#"{"language":"ja_JP"}"#).unwrap();

        let lang = load_language_from(&path).expect("load should succeed");
        assert_eq!(
            lang,
            Lang::DEFAULT,
            "unknown lang code must fall back to default"
        );
        let _ = fs::remove_dir_all(path.parent().unwrap());
    }

    #[test]
    fn load_non_object_json_surfaces_not_an_object() {
        // Array at top level — should produce `NotAnObject`, not a generic JSON error.
        let path = fresh_tmp_config("array");
        fs::write(&path, r#"["not", "a", "config"]"#).unwrap();

        match load_language_from(&path) {
            Err(LoadError::NotAnObject) => {}
            other => panic!("expected LoadError::NotAnObject, got {other:?}"),
        }
        let _ = fs::remove_dir_all(path.parent().unwrap());
    }

    #[test]
    fn save_creates_missing_parent_dir() {
        // Use a path whose parent's parent doesn't exist yet — save should mkdir -p.
        let base = fresh_tmp_config("nested_parent");
        let nested = base.parent().unwrap().join("a/b/c/config.json");
        // Verify the nested dirs really don't exist before the save.
        assert!(!nested.parent().unwrap().exists());

        save_language_to(&nested, Lang::EnUs).expect("save should mkdir -p");
        let loaded = load_language_from(&nested).expect("load should succeed");
        assert_eq!(loaded, Lang::EnUs);
        let _ = fs::remove_dir_all(base.parent().unwrap());
    }
}
