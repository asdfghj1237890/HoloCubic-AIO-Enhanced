//! Translation lookup with embedded JSON tables.
//!
//! Translation tables are baked into the binary via `include_str!` so the
//! tool ships as a single executable with no external resource dependency.
//! At runtime we parse the JSON once into a `HashMap<String, String>` per
//! locale (lazy via `OnceLock`) and look up keys by string.
//!
//! Behavior matches `AIO_Tool/util/i18n.py` per Plan 2 D3 / D4 / D5.

use std::collections::HashMap;
use std::sync::{Mutex, OnceLock};

use crate::lang::Lang;

// Embedded JSON sources. Paths are relative to this source file.
// At v3 cutover (Plan 10) the AIO_Tool_rs/AIO_Tool layout collapses; paths
// shrink to `../../../i18n/<locale>.json` once renamed.
const EN_US_RAW: &str = include_str!("../../../../AIO_Tool/i18n/en_US.json");
const ZH_CN_RAW: &str = include_str!("../../../../AIO_Tool/i18n/zh_CN.json");
const ZH_TW_RAW: &str = include_str!("../../../../AIO_Tool/i18n/zh_TW.json");

fn parse(raw: &str) -> HashMap<String, String> {
    // build.rs already validated structure; this can only fail if someone
    // bypasses cargo build (e.g. ships pre-built artifacts and edits the JSON).
    // We fall back to an empty map so the app still launches (D5).
    serde_json::from_str(raw).unwrap_or_default()
}

fn tables() -> &'static HashMap<Lang, HashMap<String, String>> {
    static TABLES: OnceLock<HashMap<Lang, HashMap<String, String>>> = OnceLock::new();
    TABLES.get_or_init(|| {
        let mut m = HashMap::with_capacity(3);
        m.insert(Lang::EnUs, parse(EN_US_RAW));
        m.insert(Lang::ZhCn, parse(ZH_CN_RAW));
        m.insert(Lang::ZhTw, parse(ZH_TW_RAW));
        m
    })
}

/// Global i18n state — current language only. Translations are static.
pub struct I18n {
    current: Mutex<Lang>,
}

impl I18n {
    /// Build a fresh I18n state, defaulting to `Lang::DEFAULT`.
    fn new() -> Self {
        Self {
            current: Mutex::new(Lang::DEFAULT),
        }
    }

    /// Active language.
    pub fn get_language(&self) -> Lang {
        *self.current.lock().expect("i18n lock poisoned")
    }

    /// Set the active language. Always succeeds (the type system enforces validity).
    pub fn set_language(&self, lang: Lang) {
        *self.current.lock().expect("i18n lock poisoned") = lang;
    }

    /// Look up `key`. Falls back to `default` if provided, otherwise to `key`.
    /// Matches Python's `t(key, default=None)` (Plan 2 D4).
    pub fn t(&self, key: &str, default: Option<&str>) -> String {
        let lang = self.get_language();
        if let Some(translations) = tables().get(&lang) {
            if let Some(v) = translations.get(key) {
                return v.clone();
            }
        }
        default.unwrap_or(key).to_owned()
    }
}

/// Global singleton accessor.
pub fn get_i18n() -> &'static I18n {
    static INSTANCE: OnceLock<I18n> = OnceLock::new();
    INSTANCE.get_or_init(I18n::new)
}

/// Convenience: translate `key` via the global singleton, falling back to
/// `default` (or `key`).
pub fn t(key: &str, default: Option<&str>) -> String {
    get_i18n().t(key, default)
}

#[cfg(test)]
mod tests {
    use super::*;

    // Tests share a global singleton. Serialize so language switches don't race
    // when cargo test runs multiple tests in parallel.
    use std::sync::Mutex as StdMutex;
    static TEST_LOCK: StdMutex<()> = StdMutex::new(());

    fn with_language<F: FnOnce()>(lang: Lang, f: F) {
        let _g = TEST_LOCK.lock().expect("test lock poisoned");
        let i = get_i18n();
        let prev = i.get_language();
        i.set_language(lang);
        f();
        i.set_language(prev);
    }

    #[test]
    fn t_returns_translation_for_current_language() {
        with_language(Lang::EnUs, || {
            assert_eq!(t("tab_help", None), "Help");
        });
    }

    #[test]
    fn t_differs_between_languages() {
        let _g = TEST_LOCK.lock().expect("test lock poisoned");
        let i = get_i18n();
        let prev = i.get_language();
        i.set_language(Lang::ZhCn);
        let zh = t("tab_help", None);
        i.set_language(Lang::EnUs);
        let en = t("tab_help", None);
        i.set_language(prev);
        assert_ne!(zh, en, "tab_help should differ between zh_CN and en_US");
    }

    #[test]
    fn missing_key_falls_back_to_key_itself_when_no_default() {
        with_language(Lang::EnUs, || {
            assert_eq!(t("nonexistent_key_xyz", None), "nonexistent_key_xyz");
        });
    }

    #[test]
    fn missing_key_returns_provided_default() {
        with_language(Lang::EnUs, || {
            assert_eq!(
                t("nonexistent_key_xyz", Some("custom_fallback")),
                "custom_fallback"
            );
        });
    }

    #[test]
    fn default_language_is_zh_cn_on_fresh_instance() {
        // Use a fresh I18n to avoid leakage from earlier tests.
        let i = I18n::new();
        assert_eq!(i.get_language(), Lang::ZhCn);
    }

    #[test]
    fn set_language_persists_on_singleton() {
        with_language(Lang::ZhTw, || {
            assert_eq!(get_i18n().get_language(), Lang::ZhTw);
        });
    }
}
