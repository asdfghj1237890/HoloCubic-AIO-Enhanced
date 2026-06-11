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
