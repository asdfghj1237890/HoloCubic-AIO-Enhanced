//! Generates `Docs/design/studio-flasher/i18n-generated.js` from the canonical
//! trilingual locale files in `AIO_Tool/i18n/*.json`, plus the helpers the
//! `tests/i18n_sync.rs` drift check uses. See the design spec at
//! Docs/development/specs/2026-06-11-studio-i18n-canonical-json-design.md.

use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

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

/// `AIO_Tool/i18n/` — the canonical locale dir, resolved from this crate.
pub fn i18n_dir() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("..")
        .join("i18n")
}

/// `Docs/design/studio-flasher/` — the Studio frontend dir.
pub fn studio_flasher_dir() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("..")
        .join("..")
        .join("Docs")
        .join("design")
        .join("studio-flasher")
}

/// Path of the committed generated dict.
pub fn generated_js_path() -> PathBuf {
    studio_flasher_dir().join("i18n-generated.js")
}

/// Load one locale file as a flat key→string map (non-string values skipped).
pub fn load_locale(path: &Path) -> BTreeMap<String, String> {
    let text = std::fs::read_to_string(path)
        .unwrap_or_else(|e| panic!("i18n_gen: cannot read {}: {e}", path.display()));
    let value: serde_json::Value = serde_json::from_str(&text)
        .unwrap_or_else(|e| panic!("i18n_gen: {} is not valid JSON: {e}", path.display()));
    let obj = value
        .as_object()
        .unwrap_or_else(|| panic!("i18n_gen: {} is not a JSON object", path.display()));
    obj.iter()
        .filter_map(|(k, v)| v.as_str().map(|s| (k.clone(), s.to_string())))
        .collect()
}

/// Full generation from the canonical i18n dir. Logs skipped values to stderr.
pub fn generate_from_i18n_dir(dir: &Path) -> String {
    let en = load_locale(&dir.join("en_US.json"));
    let cn = load_locale(&dir.join("zh_CN.json"));
    let tw = load_locale(&dir.join("zh_TW.json"));
    let (entries, skipped) = build_entries(&en, &cn, &tw);
    if !skipped.is_empty() {
        eprintln!("i18n_gen: omitted {} zh-TW value(s):", skipped.len());
        for s in &skipped {
            eprintln!("  - {s}");
        }
    }
    render(&entries)
}

/// The key set the generated dict would expose (used by the resolve-check).
pub fn generated_keys(dir: &Path) -> BTreeSet<String> {
    let en = load_locale(&dir.join("en_US.json"));
    let cn = load_locale(&dir.join("zh_CN.json"));
    let tw = load_locale(&dir.join("zh_TW.json"));
    build_entries(&en, &cn, &tw).0.into_keys().collect()
}

/// Extract the `I18N_SUPPLEMENT` key set from i18n.jsx source, scanning between
/// the `I18N_SUPPLEMENT-START` / `-END` markers for lines beginning with a quote.
pub fn extract_supplement_keys(src: &str) -> BTreeSet<String> {
    let (Some(start), Some(end)) = (
        src.find("I18N_SUPPLEMENT-START"),
        src.find("I18N_SUPPLEMENT-END"),
    ) else {
        panic!("i18n_gen: I18N_SUPPLEMENT-START/-END markers not found in i18n.jsx");
    };
    let mut keys = BTreeSet::new();
    for line in src[start..end].lines() {
        if let Some(rest) = line.trim_start().strip_prefix('"') {
            if let Some(idx) = rest.find('"') {
                keys.insert(rest[..idx].to_string());
            }
        }
    }
    keys
}

/// Find all static `tr("…")` / `tr('…')` literal arguments in a source string.
/// Dynamic calls (e.g. `tr(it.label)`) and identifier-prefixed matches
/// (e.g. `attr(`) are ignored.
pub fn scan_tr_literals(src: &str) -> BTreeSet<String> {
    let bytes = src.as_bytes();
    let mut found = BTreeSet::new();
    let mut i = 0usize;
    while let Some(rel) = src[i..].find("tr(") {
        let abs = i + rel; // byte index of 't' in "tr("
        let prev_is_ident = abs > 0 && {
            let p = bytes[abs - 1];
            p.is_ascii_alphanumeric() || p == b'_' || p == b'$' || p == b'.'
        };
        let mut j = abs + 3; // just past "tr("
        if !prev_is_ident {
            while j < bytes.len() && bytes[j].is_ascii_whitespace() {
                j += 1;
            }
            if j < bytes.len() && (bytes[j] == b'"' || bytes[j] == b'\'') {
                let quote = bytes[j] as char;
                let lit_start = j + 1;
                if let Some(qrel) = src[lit_start..].find(quote) {
                    found.insert(src[lit_start..lit_start + qrel].to_string());
                    i = lit_start + qrel + 1;
                    continue;
                }
            }
        }
        i = abs + 3;
    }
    found
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

    #[test]
    fn extract_supplement_keys_reads_marker_block() {
        let src = "x\n// I18N_SUPPLEMENT-START\nconst I18N_SUPPLEMENT = {\n  \"連接\": { cn: \"连接\", en: \"Connect\" },\n  \"語言\": { cn: \"语言\", en: \"Language\" },\n};\n// I18N_SUPPLEMENT-END\nconst Z = { \"notthis\": 1 };\n";
        let keys = extract_supplement_keys(src);
        assert!(keys.contains("連接"));
        assert!(keys.contains("語言"));
        assert!(!keys.contains("notthis"));
        assert_eq!(keys.len(), 2);
    }

    #[test]
    fn scan_tr_literals_finds_static_calls_only() {
        let src = "<div>{tr(\"燒錄韌體\")}</div> {tr('語言')} {tr(it.label)} attr(\"x\")";
        let lits = scan_tr_literals(src);
        assert!(lits.contains("燒錄韌體"));
        assert!(lits.contains("語言"));
        assert_eq!(lits.len(), 2);
    }
}
