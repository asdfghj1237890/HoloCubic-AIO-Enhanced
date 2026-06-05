//! Compile-time-ish (cargo test) i18n key validation.
//!
//! Grep `src/` for `t("<key>", ...)` literals and assert every key is
//! present in `../../AIO_Tool/i18n/en_US.json`. Catches the Plan 6
//! Group B regression class (button rendered literal "erase_flash"
//! because that key didn't exist in the locale JSONs).

use std::collections::HashSet;
use std::fs;
use std::path::Path;

#[test]
fn every_t_call_references_an_existing_i18n_key() {
    // 1. Load en_US.json keys. The path is relative to this crate's
    //    manifest directory (where cargo runs the test binary from).
    let manifest_dir = env!("CARGO_MANIFEST_DIR");
    let json_path = Path::new(manifest_dir)
        .join("..")
        .join("..")
        .join("..")
        .join("AIO_Tool")
        .join("i18n")
        .join("en_US.json");
    let raw = fs::read_to_string(&json_path)
        .unwrap_or_else(|e| panic!("read {}: {e}", json_path.display()));
    let parsed: serde_json::Value = serde_json::from_str(&raw).expect("parse en_US.json");
    let valid_keys: HashSet<String> = parsed
        .as_object()
        .expect("en_US.json must be a JSON object")
        .keys()
        .cloned()
        .collect();

    // 2. Walk src/ for *.rs files, grep for `t("...", ` literals.
    let src_root = Path::new(manifest_dir).join("src");
    let mut found: HashSet<String> = HashSet::new();
    walk_rs(&src_root, &mut |_path, contents| {
        let re = regex::Regex::new(r#"\bt\("([a-zA-Z0-9_]+)""#).unwrap();
        for cap in re.captures_iter(contents) {
            found.insert(cap[1].to_owned());
        }
    });

    // 3. Assert every found key is in en_US.json.
    let mut missing: Vec<&String> = found.iter().filter(|k| !valid_keys.contains(*k)).collect();
    missing.sort();
    assert!(
        missing.is_empty(),
        "Found t(\"...\") calls referencing keys NOT in en_US.json: {:?}",
        missing
    );
}

fn walk_rs<F: FnMut(&Path, &str)>(dir: &Path, f: &mut F) {
    for entry in fs::read_dir(dir).expect("read_dir") {
        let entry = entry.expect("entry");
        let path = entry.path();
        if path.is_dir() {
            walk_rs(&path, f);
        } else if path.extension().and_then(|s| s.to_str()) == Some("rs") {
            let contents = fs::read_to_string(&path).expect("read_to_string");
            f(&path, &contents);
        }
    }
}
