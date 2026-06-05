# i18n goldens

Each `<key>.<locale>.txt` is the byte-exact UTF-8 output of the legacy Python
tool's `I18n.t(key)` for the matching locale. Rust integration tests in
`../goldens.rs` assert Rust's `t()` produces an identical string.

Regenerate after legacy JSON edits:

    cd AIO_Tool && uv run python ../AIO_Tool_rs/scripts/dump_i18n_goldens.py
