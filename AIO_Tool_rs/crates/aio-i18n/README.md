# aio-i18n

HoloCubic AIO tool internationalization — translation lookup + config
persistence for the three supported locales: 简体中文, 繁體中文, English.

## Design notes

- **Compile-time key parity**: `build.rs` validates that all three locale
  JSON files expose the same key set. Drift fails the build (`cargo build`
  panics with a list of missing / extra keys per locale) instead of being
  caught by a runtime test. Plan 2 D8.
- **Embedded translations**: JSON tables are baked into the binary via
  `include_str!`. No external resource dependency at runtime; the tool
  ships as a single executable.
- **Missing-key behavior**: `t(key, default=None)` returns the provided
  default if any, else the key itself (visible in UI). Does NOT silently
  fall back to English. Plan 2 D4.
- **Singleton + per-OS config**: `get_i18n()` returns a `OnceLock`-backed
  global. `save_language(Lang)` and `load_language()` persist to
  `<config-dir>/HoloCubic-AIO/config.json` (per `directories` crate) and
  preserve any unrelated top-level keys round-trip. Public no-arg API
  delegates to private path-injectable helpers so tests can target a tmp
  directory directly (the `directories` crate ignores `%APPDATA%` on
  Windows, making env-var-based test isolation unreliable).

## Testing

```sh
cargo test -p aio-i18n
```

Three layers:

- Unit tests in `src/{lang,i18n,config}.rs` cover the enum, lookup, and
  config round-trip behaviors.
- `build.rs` enforces locale key parity at compile time — anything that
  passes `cargo build` already has parity.
- Golden snapshot tests in `tests/golden/*.txt` are byte-exact captures
  of the legacy Python tool's `I18n.t()` output. Regenerate with:

  ```sh
  cd AIO_Tool && uv run python ../AIO_Tool_rs/scripts/dump_i18n_goldens.py
  ```

  `tests/golden/.gitattributes` keeps `.txt` files as `binary` so git
  preserves embedded LF byte-for-byte regardless of host `core.autocrlf`.

## Adding a new key

1. Edit all three `i18n/<locale>.json` files. **Add the key to all three at
   once** — `cargo build` will reject anything else.
2. Optionally add a golden snapshot test for the new key if it has
   structurally interesting shape (multi-line, format characters, etc.).

## Stability

The `Lang` enum's `code()` outputs are part of the on-disk `config.json`
format and must NOT change. Renaming a Rust variant is fine; changing the
`"zh_CN"` / `"zh_TW"` / `"en_US"` codes would orphan every user's saved
preference.
