//! Compile-time check: all three locales must expose the same key set.
//!
//! Catches the class of bug "developer added a new key to en_US but forgot
//! zh_CN/zh_TW" before it reaches users. Failure is a compile error — no
//! amount of `cargo test` skipping can hide it.
//!
//! Path note: this reads from `../../i18n/<locale>.json` because i18n/ lives at
//! the AIO_Tool workspace root and this build.rs is at crates/aio-i18n/build.rs
//! (two parents up gets us to the workspace root).

use std::collections::BTreeSet;
use std::path::Path;

const LOCALES: &[&str] = &["en_US", "zh_CN", "zh_TW"];

fn main() {
    let i18n_dir = Path::new("../../i18n");
    let mut key_sets: Vec<(&'static str, BTreeSet<String>)> = Vec::new();
    for locale in LOCALES {
        let path = i18n_dir.join(format!("{locale}.json"));
        println!("cargo:rerun-if-changed={}", path.display());
        let contents = std::fs::read_to_string(&path).unwrap_or_else(|e| {
            panic!("aio-i18n build: cannot read {}: {e}", path.display());
        });
        let value: serde_json::Value = serde_json::from_str(&contents).unwrap_or_else(|e| {
            panic!("aio-i18n build: {} is not valid JSON: {e}", path.display());
        });
        let obj = value.as_object().unwrap_or_else(|| {
            panic!("aio-i18n build: {} is not a JSON object", path.display());
        });
        let keys: BTreeSet<String> = obj.keys().cloned().collect();
        key_sets.push((locale, keys));
    }

    let (first_name, first_keys) = &key_sets[0];
    for (name, keys) in &key_sets[1..] {
        let missing: Vec<_> = first_keys.difference(keys).collect();
        let extra: Vec<_> = keys.difference(first_keys).collect();
        if !missing.is_empty() || !extra.is_empty() {
            let mut msg =
                format!("aio-i18n build: locale `{name}` key set differs from `{first_name}`:\n");
            if !missing.is_empty() {
                msg.push_str(&format!("  missing in {name}: {missing:?}\n"));
            }
            if !extra.is_empty() {
                msg.push_str(&format!("  extra in {name}: {extra:?}\n"));
            }
            panic!("{}", msg);
        }
    }

    println!(
        "cargo:warning=aio-i18n: all {} locales share {} keys",
        LOCALES.len(),
        first_keys.len()
    );
}
