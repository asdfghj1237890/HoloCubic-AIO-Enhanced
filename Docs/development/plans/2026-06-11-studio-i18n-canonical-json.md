# Studio i18n Consumes Canonical JSON — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Studio frontend's `tr()` dictionary derive from the canonical `AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json` instead of a hand-maintained 56-key JS dict, and add a Rust check so the two cannot silently drift.

**Architecture:** A small Rust module in the studio crate reverse-keys the three JSON locale files (by zh-TW value) into a committed `i18n-generated.js` (`window.__I18N_GENERATED`). `i18n.jsx` merges that generated base with a tiny hand-kept `I18N_SUPPLEMENT` (Studio-only strings + zh-CN disambiguations) — `tr()` is unchanged. A studio-crate integration test (riding the existing `cargo test` in `tool-studio.yml`) regenerates the file and fails on any diff, and asserts every static `tr("…")` literal resolves.

**Tech Stack:** Rust (stable, studio crate; `serde_json` already a dep — no new crates), browser JSX (loaded via `@babel/standalone`), GitHub Actions (`tool-studio.yml`, unchanged).

**Design spec:** [Docs/development/specs/2026-06-11-studio-i18n-canonical-json-design.md](../specs/2026-06-11-studio-i18n-canonical-json-design.md)

---

## File structure

| File | Responsibility |
|---|---|
| `AIO_Tool/studio/src/i18n_gen.rs` | **new** — pure generation logic + path helpers + the supplement-key and `tr()`-literal scanners |
| `AIO_Tool/studio/src/lib.rs` | add `pub mod i18n_gen;` |
| `AIO_Tool/studio/tests/i18n_sync.rs` | **new** — snapshot diff (with `UPDATE_I18N` regen) + resolve-check |
| `Docs/design/studio-flasher/i18n-generated.js` | **new, generated, committed** — `window.__I18N_GENERATED` |
| `Docs/design/studio-flasher/i18n.jsx` | replace inline `I18N_DICT` with `Object.assign(generated, I18N_SUPPLEMENT)` |
| `Docs/design/studio-flasher/index.html` | load `i18n-generated.js` before `i18n.jsx` |
| `Docs/design/studio-flasher/studio-pages.jsx` | fix the one untranslated `tr()` literal (line 281) |
| `Docs/design/studio-flasher/.gitattributes` | **new** — force LF on the generated file |
| `Docs/design/studio-flasher/README.md`, `CLAUDE.md`, `AIO_Tool/crates/aio-i18n/README.md` | doc updates |

**Note on build times:** the first `cargo test` against the studio crate compiles Tauri and friends (~5 min on a cold cache); subsequent runs are incremental (seconds). All `cargo` commands below are written for repo-root with `--manifest-path`; CI runs the equivalent from `AIO_Tool/studio`.

---

## Task 1: `i18n_gen` core transform (`build_entries` + `render`)

**Files:**
- Modify: `AIO_Tool/studio/src/lib.rs` (add module declaration)
- Create: `AIO_Tool/studio/src/i18n_gen.rs`

- [ ] **Step 1: Declare the module in `lib.rs`**

In `AIO_Tool/studio/src/lib.rs`, change the module block (currently lines 15-18):

```rust
mod commands;
mod fm;
mod img;
mod video;
```

to:

```rust
mod commands;
mod fm;
mod img;
pub mod i18n_gen;
mod video;
```

- [ ] **Step 2: Write `i18n_gen.rs` with the core transform + failing unit tests**

Create `AIO_Tool/studio/src/i18n_gen.rs`:

```rust
//! Generates `Docs/design/studio-flasher/i18n-generated.js` from the canonical
//! trilingual locale files in `AIO_Tool/i18n/*.json`, plus the helpers the
//! `tests/i18n_sync.rs` drift check uses. See the design spec at
//! Docs/development/specs/2026-06-11-studio-i18n-canonical-json-design.md.

use std::collections::{BTreeMap, BTreeSet};

/// Build the zh-TW-value-keyed dict, applying the skip rules. Returns
/// `(entries, skipped)`; `skipped` lists zh-TW values omitted and why.
///
/// Skip rules:
/// - multi-line values (the help/info blobs) — never UI labels;
/// - a zh-TW value that maps to differing `(cn, en)` across keys (ambiguous);
/// benign collisions (all keys agree) are emitted once.
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
        pairs.iter().map(|(k, v)| (k.to_string(), v.to_string())).collect()
    }

    #[test]
    fn build_entries_emits_benign_collision_once() {
        let en = map(&[("a", "Cancel"), ("b", "Cancel")]);
        let cn = map(&[("a", "取消"), ("b", "取消")]);
        let tw = map(&[("a", "取消"), ("b", "取消")]);
        let (e, skipped) = build_entries(&en, &cn, &tw);
        assert_eq!(e.get("取消"), Some(&("取消".to_string(), "Cancel".to_string())));
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
```

> The two `_…Task2` shims keep this step compiling on its own; Task 2 removes them when it adds the real IO code.

- [ ] **Step 3: Run the unit tests — expect PASS**

Run (repo root):

```bash
cargo test --manifest-path AIO_Tool/studio/Cargo.toml i18n_gen
```

Expected: the 4 `i18n_gen::tests::…` tests PASS. (First build is slow; see note above.)

- [ ] **Step 4: Commit**

```bash
git add AIO_Tool/studio/src/lib.rs AIO_Tool/studio/src/i18n_gen.rs
git commit -m "feat(studio): add i18n_gen core transform (zh-TW reverse-key + render)"
```

---

## Task 2: `i18n_gen` IO, paths, and scanners

**Files:**
- Modify: `AIO_Tool/studio/src/i18n_gen.rs`

- [ ] **Step 1: Add the IO + scanner code**

In `AIO_Tool/studio/src/i18n_gen.rs`, add a path import directly below the existing `use std::collections::…` line:

```rust
use std::path::{Path, PathBuf};
```

then add the following functions just above the `#[cfg(test)] mod tests` block:

```rust
/// `AIO_Tool/i18n/` — the canonical locale dir, resolved from this crate.
pub fn i18n_dir() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("..").join("i18n")
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
    let (Some(start), Some(end)) =
        (src.find("I18N_SUPPLEMENT-START"), src.find("I18N_SUPPLEMENT-END"))
    else {
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
```

- [ ] **Step 2: Add scanner unit tests**

Inside the existing `#[cfg(test)] mod tests`, append:

```rust
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
```

- [ ] **Step 3: Run the unit tests — expect PASS**

```bash
cargo test --manifest-path AIO_Tool/studio/Cargo.toml i18n_gen
```

Expected: all 6 `i18n_gen::tests::…` tests PASS.

- [ ] **Step 4: Commit**

```bash
git add AIO_Tool/studio/src/i18n_gen.rs
git commit -m "feat(studio): add i18n_gen IO, path helpers, and tr()/supplement scanners"
```

---

## Task 3: Rewrite `i18n.jsx` to merge generated + supplement

**Files:**
- Modify: `Docs/design/studio-flasher/i18n.jsx` (replace the whole file)

- [ ] **Step 1: Replace `i18n.jsx` contents**

Overwrite `Docs/design/studio-flasher/i18n.jsx` with:

```jsx
// i18n.jsx — lightweight UI translation for the Studio app chrome.
// The base dictionary is GENERATED from the canonical locale files
// AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json into i18n-generated.js
// (window.__I18N_GENERATED), loaded by index.html *before* this file.
// I18N_SUPPLEMENT below is the ONLY hand-maintained dict: Studio-only strings
// the JSON has no key for, plus a few zh-CN disambiguations. The studio crate's
// tests/i18n_sync.rs keeps both sides honest (regenerate + every-tr()-resolves).
// tr(zhTW) returns the active-language variant; unlisted strings fall back to
// Traditional Chinese. window.__lang ∈ {"tw","cn","en"} (default "tw").
window.__lang = window.__lang || "tw";

// I18N_SUPPLEMENT-START — hand-maintained; see AIO_Tool/studio/tests/i18n_sync.rs
const I18N_SUPPLEMENT = {
  // Studio-only strings absent from the AIO_Tool/i18n JSON files
  "連接": { cn: "连接", en: "Connect" },
  "連線中": { cn: "连接中", en: "Connecting" },
  "主色": { cn: "主色", en: "Accent color" },
  "字體": { cn: "字体", en: "Font" },
  "語言": { cn: "语言", en: "Language" },
  "按鈕、進度、強調元素的顏色": { cn: "按钮、进度、强调元素的颜色", en: "Color for buttons, progress and accents" },
  "介面文字字型": { cn: "界面文字字型", en: "Interface typeface" },
  // Disambiguation pins: these zh-TW strings map to two different zh-CN values
  // across JSON keys, so the generator omits them; pin the variant Studio shows.
  "說明": { cn: "说明", en: "Help" },
  "燒錄韌體": { cn: "烧录固件", en: "Flash Firmware" },
  "參數設定": { cn: "参数设定", en: "Device Settings" },
};
// I18N_SUPPLEMENT-END

const I18N_DICT = Object.assign({}, window.__I18N_GENERATED || {}, I18N_SUPPLEMENT);

function tr(s) {
  if (window.__lang === "tw") return s;
  const e = I18N_DICT[s];
  return e && e[window.__lang] ? e[window.__lang] : s;
}

window.tr = tr;
window.I18N_DICT = I18N_DICT;
```

- [ ] **Step 2: Commit**

```bash
git add Docs/design/studio-flasher/i18n.jsx
git commit -m "refactor(studio): source i18n.jsx dict from generated JSON + supplement"
```

> No automated check passes yet — the generated file and the integration test arrive in Task 4. (In a standalone browser load, `tr()` already works supplement-only via the `|| {}` fallback.)

---

## Task 4: Integration test, generated file, and the one call-site fix

**Files:**
- Create: `AIO_Tool/studio/tests/i18n_sync.rs`
- Create (generated): `Docs/design/studio-flasher/i18n-generated.js`
- Modify: `Docs/design/studio-flasher/index.html`
- Modify: `Docs/design/studio-flasher/studio-pages.jsx:281`

- [ ] **Step 1: Write the integration test**

Create `AIO_Tool/studio/tests/i18n_sync.rs`:

```rust
//! Drift check: keep `i18n-generated.js` in sync with the canonical JSON, and
//! keep every static `tr("…")` literal in the Studio frontend resolvable.
//! Runs inside the existing `cargo test` step of tool-studio.yml.

use aio_studio_lib::i18n_gen;
use std::path::{Path, PathBuf};

fn norm(s: &str) -> String {
    s.replace("\r\n", "\n")
}

/// All Studio UI source files (every *.jsx / *.html except i18n.jsx itself).
fn consumer_files(dir: &Path) -> Vec<PathBuf> {
    let mut files = Vec::new();
    for entry in std::fs::read_dir(dir).expect("read studio-flasher dir") {
        let path = entry.expect("dir entry").path();
        let name = path.file_name().unwrap().to_string_lossy().to_string();
        if name == "i18n.jsx" {
            continue;
        }
        if name.ends_with(".jsx") || name.ends_with(".html") {
            files.push(path);
        }
    }
    files
}

#[test]
fn generated_file_is_up_to_date() {
    let expected = i18n_gen::generate_from_i18n_dir(&i18n_gen::i18n_dir());
    let path = i18n_gen::generated_js_path();
    if std::env::var_os("UPDATE_I18N").is_some() {
        std::fs::write(&path, expected.as_bytes()).expect("write i18n-generated.js");
        eprintln!("UPDATE_I18N: wrote {}", path.display());
        return;
    }
    let actual = std::fs::read_to_string(&path).unwrap_or_else(|e| {
        panic!(
            "cannot read {} ({e}). Run: UPDATE_I18N=1 cargo test --manifest-path \
             AIO_Tool/studio/Cargo.toml --test i18n_sync",
            path.display()
        )
    });
    assert_eq!(
        norm(&actual),
        norm(&expected),
        "i18n-generated.js is stale. Regenerate: UPDATE_I18N=1 cargo test \
         --manifest-path AIO_Tool/studio/Cargo.toml --test i18n_sync"
    );
}

#[test]
fn every_tr_literal_resolves() {
    let dir = i18n_gen::studio_flasher_dir();
    let mut resolvable = i18n_gen::generated_keys(&i18n_gen::i18n_dir());
    let i18n_jsx = std::fs::read_to_string(dir.join("i18n.jsx")).expect("read i18n.jsx");
    resolvable.extend(i18n_gen::extract_supplement_keys(&i18n_jsx));

    let mut missing = Vec::new();
    for path in consumer_files(&dir) {
        let src = std::fs::read_to_string(&path).expect("read consumer file");
        let name = path.file_name().unwrap().to_string_lossy().to_string();
        for lit in i18n_gen::scan_tr_literals(&src) {
            if !resolvable.contains(&lit) {
                missing.push(format!("{name}: tr({lit:?})"));
            }
        }
    }
    missing.sort();
    assert!(
        missing.is_empty(),
        "unresolved tr() literals (add to the JSON or i18n.jsx supplement):\n{}",
        missing.join("\n")
    );
}
```

- [ ] **Step 2: Run the test — expect FAIL (no generated file yet)**

```bash
cargo test --manifest-path AIO_Tool/studio/Cargo.toml --test i18n_sync
```

Expected: `generated_file_is_up_to_date` FAILS ("cannot read … i18n-generated.js"); `every_tr_literal_resolves` FAILS listing `studio-pages.jsx: tr("透過 /api/settings …")`.

- [ ] **Step 3: Generate the committed file**

bash:

```bash
UPDATE_I18N=1 cargo test --manifest-path AIO_Tool/studio/Cargo.toml --test i18n_sync
```

PowerShell:

```powershell
$env:UPDATE_I18N=1; cargo test --manifest-path AIO_Tool/studio/Cargo.toml --test i18n_sync; Remove-Item Env:\UPDATE_I18N
```

Expected: writes `Docs/design/studio-flasher/i18n-generated.js`. Open it and confirm it starts with `// @generated`, defines `window.__I18N_GENERATED = {`, and contains entries like `"燒錄": { "cn": "烧录", "en": "Flash" },`.

- [ ] **Step 4: Load the generated file in `index.html`**

In `Docs/design/studio-flasher/index.html`, find (around line 50-51):

```html
<script type="text/babel" src="fl-shared.jsx"></script>
<script type="text/babel" src="i18n.jsx"></script>
```

Insert a plain (non-babel) script for the generated dict between them so it executes before `i18n.jsx`:

```html
<script type="text/babel" src="fl-shared.jsx"></script>
<script src="i18n-generated.js"></script>
<script type="text/babel" src="i18n.jsx"></script>
```

- [ ] **Step 5: Fix the one untranslated `tr()` literal**

In `Docs/design/studio-flasher/studio-pages.jsx` line 281, replace the leftover developer subtitle with the canonical `params_subtitle` zh-TW value.

From:

```jsx
      <PageHeader title={tr("參數設定")} sub={tr("透過 /api/settings 讀取裝置設定,改完後 POST 到對應的 /save<Cat>Conf")}
```

To:

```jsx
      <PageHeader title={tr("參數設定")} sub={tr("讀取與修改 HoloCubic 的 WiFi、系統、天氣等參數")}
```

- [ ] **Step 6: Run the test — expect PASS**

```bash
cargo test --manifest-path AIO_Tool/studio/Cargo.toml --test i18n_sync
```

Expected: both `generated_file_is_up_to_date` and `every_tr_literal_resolves` PASS.

- [ ] **Step 7: Commit**

```bash
git add AIO_Tool/studio/tests/i18n_sync.rs Docs/design/studio-flasher/i18n-generated.js Docs/design/studio-flasher/index.html Docs/design/studio-flasher/studio-pages.jsx
git commit -m "feat(studio): wire generated i18n dict + drift check; fix params subtitle"
```

---

## Task 5: `.gitattributes` + documentation

**Files:**
- Create: `Docs/design/studio-flasher/.gitattributes`
- Modify: `Docs/design/studio-flasher/README.md`
- Modify: `CLAUDE.md`
- Modify: `AIO_Tool/crates/aio-i18n/README.md`

- [ ] **Step 1: Force LF on the generated file**

Create `Docs/design/studio-flasher/.gitattributes`:

```gitattributes
# Keep the generated i18n dict LF so the studio crate's i18n_sync test
# (byte-compared after newline-normalization) stays stable across platforms.
i18n-generated.js text eol=lf
```

- [ ] **Step 2: Update the studio-flasher README file layout**

In `Docs/design/studio-flasher/README.md`, replace the single line (line 56):

```
├── i18n.jsx            tr() + dictionary for 繁中 / 简中 / English
```

with:

```
├── i18n-generated.js   GENERATED dict (window.__I18N_GENERATED) from AIO_Tool/i18n/*.json — do not edit
├── i18n.jsx            tr() + I18N_SUPPLEMENT (hand-kept Studio-only strings); merges the generated dict
```

- [ ] **Step 3: Update CLAUDE.md (two spots)**

In `CLAUDE.md`, first replace (the "What this is" overview bullet):

```
`aio-i18n` + `AIO_Tool/i18n/*.json` remain as the canonical translation source but currently have no shipping consumer (Studio uses its own JS dict; wiring the two is a tracked follow-up).
```

with:

```
`aio-i18n` + `AIO_Tool/i18n/*.json` are the canonical translation source. Studio consumes them: the studio crate generates `Docs/design/studio-flasher/i18n-generated.js` (committed) from the three JSON files, and `i18n.jsx` merges that with a small hand-kept `I18N_SUPPLEMENT`. The studio-crate test `tests/i18n_sync.rs` regenerates + diffs the file and asserts every `tr()` literal resolves, so the two cannot drift.
```

Then replace (the "AIO_Tool Rust (shared)" conventions bullet):

```
Every user-visible string in Studio MUST come from the JS-side i18n helper (`tr()` in `Docs/design/studio-flasher/i18n.jsx`). The `aio-i18n` Rust crate still enforces that `AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json` have identical key sets at compile time (`aio-i18n/build.rs` panics on divergence), but the JS dict is **not yet** checked against those JSON files — keep them in sync by hand until the follow-up lands.
```

with:

```
Every user-visible string in Studio MUST come from the JS-side i18n helper (`tr()` in `Docs/design/studio-flasher/i18n.jsx`). `tr()`'s base dict is generated from `AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json` into `i18n-generated.js`; only `i18n.jsx`'s `I18N_SUPPLEMENT` (Studio-only strings + zh-CN disambiguation pins) is hand-maintained. After editing any locale JSON, regenerate with `UPDATE_I18N=1 cargo test --manifest-path AIO_Tool/studio/Cargo.toml --test i18n_sync`; the same test (run by `tool-studio.yml`'s `cargo test`) fails CI if the committed file is stale or any `tr()` literal is unresolved. `aio-i18n/build.rs` still enforces the three JSON locales share an identical key set at compile time.
```

- [ ] **Step 4: Update aio-i18n "Adding a new key"**

In `AIO_Tool/crates/aio-i18n/README.md`, in the "Adding a new key" section, after item 2 (the "Values must be strings" item, which ends `Don't ship "key": null.`), insert a new item:

```
3. **Regenerate the Studio dict.** Studio's `tr()` reads a generated
   `Docs/design/studio-flasher/i18n-generated.js` built from these JSON files.
   Run `UPDATE_I18N=1 cargo test --manifest-path AIO_Tool/studio/Cargo.toml
   --test i18n_sync` and commit the result, or `tool-studio.yml` CI fails on the
   stale file.
```

and renumber the following "Optionally add a golden snapshot test…" item from `3.` to `4.`.

- [ ] **Step 5: Commit**

```bash
git add Docs/design/studio-flasher/.gitattributes Docs/design/studio-flasher/README.md CLAUDE.md AIO_Tool/crates/aio-i18n/README.md
git commit -m "docs(studio): document generated i18n dict + regeneration workflow"
```

---

## Task 6: Full verification

**Files:** none (verification only — no commit unless a fix is needed).

- [ ] **Step 1: Mirror the CI gates locally**

```bash
cargo fmt --manifest-path AIO_Tool/studio/Cargo.toml --all -- --check
cargo clippy --manifest-path AIO_Tool/studio/Cargo.toml --all-targets -- -D warnings
cargo test --manifest-path AIO_Tool/studio/Cargo.toml
```

Expected: fmt clean, clippy clean (the new test file is covered by `--all-targets`), all tests pass (the 6 `i18n_gen` unit tests + the 2 `i18n_sync` integration tests). Fix any issue and re-run; commit fixes with `style(studio): …` or `fix(studio): …` as appropriate.

- [ ] **Step 2: Negative check — unresolved literal fails**

Temporarily append `tr("漏譯測試")` to `Docs/design/studio-flasher/dir-a-pro.jsx` (anywhere in JSX), then:

```bash
cargo test --manifest-path AIO_Tool/studio/Cargo.toml --test i18n_sync every_tr_literal_resolves
```

Expected: FAIL listing `dir-a-pro.jsx: tr("漏譯測試")`. **Revert the edit** (`git checkout -- Docs/design/studio-flasher/dir-a-pro.jsx`).

- [ ] **Step 3: Negative check — stale generated file fails**

Temporarily add `"_drift_probe": "x"` to all three `AIO_Tool/i18n/*.json` (keeps build.rs parity), then:

```bash
cargo test --manifest-path AIO_Tool/studio/Cargo.toml --test i18n_sync generated_file_is_up_to_date
```

Expected: FAIL ("i18n-generated.js is stale…"). **Revert** all three JSON edits (`git checkout -- AIO_Tool/i18n`).

- [ ] **Step 4: Visual confirmation (en / cn)**

Serve the frontend and confirm no Traditional-Chinese fallbacks remain in non-`tw` modes:

```bash
npx --yes http-server Docs/design/studio-flasher -p 8765 -c-1 --cors
```

Open `http://localhost:8765`, then in the browser console set `localStorage.setItem("holocubic-studio-tweaks", JSON.stringify({lang:"en"}))` and reload; spot-check the rail labels, flasher header, and Tool Settings page. Repeat with `lang:"cn"`. (Or use the preview MCP: `preview_start`, `preview_eval` to set the lang, `preview_screenshot`.)

- [ ] **Step 5: Confirm `main` is untouched and the branch is clean**

```bash
git status
git log --oneline origin/main..HEAD
```

Expected: working tree clean; the log shows only this feature's commits on `studio-i18n-canonical-json`, none on `main`.

---

## Self-review notes (for the implementer)

- **`tr()` API unchanged:** signature, `tw` passthrough, and `window.tr` / `window.I18N_DICT` exports are byte-identical to the pre-change `i18n.jsx`. No call site changes except the single corrected literal in Task 4 Step 5.
- **Behavior deltas** adopting the canonical JSON (non-`tw` only): `Flash`→`Start flashing`, `Erase`→`Clear Chip`, `Remote`→`Remote Control`, `Operation log`/`操作记录`→`Operation Log`/`操作日志`, `工具设定`→`工具设置`, `断开连接`→`断开`, `重新启动`→`重启`. Intended (canonical wins). To pin an old value instead, add it to `I18N_SUPPLEMENT`.
- **Why studio crate, not aio-i18n:** `tool-studio.yml`'s `cargo test` builds the studio crate (and triggers on `Docs/design/studio-flasher/**`); the studio crate does not depend on `aio-i18n`, so a check there would not run for frontend-only edits.
- **No new dependencies:** `serde_json` is already in `AIO_Tool/studio/Cargo.toml`; scanners are hand-rolled (no `regex`).
