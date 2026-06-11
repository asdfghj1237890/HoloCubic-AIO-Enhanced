//! Generates `Docs/design/studio-flasher/i18n-generated.js` from the canonical
//! trilingual locale files in `AIO_Tool/i18n/*.json`, plus the helpers the
//! `tests/i18n_sync.rs` drift check uses. See the design spec at
//! Docs/development/specs/2026-06-11-studio-i18n-canonical-json-design.md.

use std::collections::{BTreeMap, BTreeSet};

/// Build the zh-TW-value-keyed dict, applying the skip rules. Returns
/// `(entries, skipped)`; `skipped` lists zh-TW values omitted and why.
///
/// Skip rules: drop multi-line values (the help/info blobs — never UI labels)
/// and ambiguous values (a zh-TW value mapping to differing `(cn, en)` pairs
/// across keys). Benign collisions (all keys agree) are emitted once.
pub fn build_entries(
    en: &BTreeMap<String, String>,
    cn: &BTreeMap<String, String>,
    tw: &BTreeMap<String, String>,
) -> (BTreeMap<String, (String, String)>, Vec<String>) {
    let mut grouped: BTreeMap<String, BTreeSet<(String, String)>> = BTreeMap::new();
    for (key, twv) in tw {
        let (Some(cnv), Some(env)) = (cn.get(key), en.get(key)) else {
            continue; // locale parity is enforced by aio-i18n/build.rs; be defensive
        };
        grouped
            .entry(twv.clone())
            .or_default()
            .insert((cnv.clone(), env.clone()));
    }
    let mut entries = BTreeMap::new();
    let mut skipped = Vec::new();
    for (twv, pairs) in grouped {
        // Keys are the zh-TW values; multi-line ones are the big help/info
        // blobs we don't want as keys. Any cn/en newlines are escaped by
        // js_str, so only the key axis needs this check.
        if twv.contains('\n') {
            skipped.push(format!("{twv:?} (multi-line)"));
            continue;
        }
        if pairs.len() > 1 {
            skipped.push(format!("{twv:?} (ambiguous zh-CN/en across keys)"));
            continue;
        }
        let (cnv, env) = pairs.into_iter().next().unwrap();
        entries.insert(twv, (cnv, env));
    }
    (entries, skipped)
}

const HEADER: &str = "\
// @generated from AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json — DO NOT EDIT BY HAND.
// Regenerate: UPDATE_I18N=1 cargo test --manifest-path AIO_Tool/studio/Cargo.toml --test i18n_sync
// The JSON is the source of truth; Studio-only strings live in i18n.jsx I18N_SUPPLEMENT.
// zh-TW values that are multi-line or ambiguous in zh-CN across keys are omitted
// (see AIO_Tool/studio/src/i18n_gen.rs and the design spec).
";

fn js_str(s: &str) -> String {
    serde_json::to_string(s).expect("a string is always JSON-serializable")
}

/// Render the committed `i18n-generated.js` text (header + sorted dict).
pub fn render(entries: &BTreeMap<String, (String, String)>) -> String {
    let mut out = String::from(HEADER);
    out.push_str("window.__I18N_GENERATED = {\n");
    for (twv, (cnv, env)) in entries {
        out.push_str(&format!(
            "  {}: {{ \"cn\": {}, \"en\": {} }},\n",
            js_str(twv),
            js_str(cnv),
            js_str(env)
        ));
    }
    out.push_str("};\n");
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn map(pairs: &[(&str, &str)]) -> BTreeMap<String, String> {
        pairs
            .iter()
            .map(|(k, v)| (k.to_string(), v.to_string()))
            .collect()
    }

    #[test]
    fn build_entries_emits_benign_collision_once() {
        let en = map(&[("a", "Cancel"), ("b", "Cancel")]);
        let cn = map(&[("a", "取消"), ("b", "取消")]);
        let tw = map(&[("a", "取消"), ("b", "取消")]);
        let (e, skipped) = build_entries(&en, &cn, &tw);
        assert_eq!(
            e.get("取消"),
            Some(&("取消".to_string(), "Cancel".to_string()))
        );
        assert!(skipped.is_empty());
    }

    #[test]
    fn build_entries_skips_ambiguous_cn() {
        let en = map(&[("full", "Help"), ("short", "Help")]);
        let cn = map(&[("full", "帮助"), ("short", "说明")]);
        let tw = map(&[("full", "說明"), ("short", "說明")]);
        let (e, skipped) = build_entries(&en, &cn, &tw);
        assert!(!e.contains_key("說明"));
        assert_eq!(skipped.len(), 1);
    }

    #[test]
    fn build_entries_skips_multiline() {
        let en = map(&[("info", "l1\nl2")]);
        let cn = map(&[("info", "行1\n行2")]);
        let tw = map(&[("info", "列1\n列2")]);
        let (e, skipped) = build_entries(&en, &cn, &tw);
        assert!(e.is_empty());
        assert_eq!(skipped.len(), 1);
    }

    #[test]
    fn render_is_sorted_and_formatted() {
        let mut e = BTreeMap::new();
        e.insert("b".to_string(), ("bc".to_string(), "B".to_string()));
        e.insert("a".to_string(), ("ac".to_string(), "A".to_string()));
        let out = render(&e);
        assert!(out.starts_with("// @generated"));
        assert!(out.contains("window.__I18N_GENERATED = {\n"));
        assert!(out.contains("  \"a\": { \"cn\": \"ac\", \"en\": \"A\" },\n"));
        let ia = out.find("\"a\":").unwrap();
        let ib = out.find("\"b\":").unwrap();
        assert!(ia < ib, "entries must be sorted by key");
    }
}
