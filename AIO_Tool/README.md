# AIO_Tool (v3.0.0)

Cross-platform GUI tool for the HoloCubic AIO firmware. Rust 1.82 + egui 0.29 + eframe.

## Build + run

```sh
cd AIO_Tool
cargo +1.82.0 run --bin aio-tool
```

First build pulls a substantial dep graph (~10 min on cold cache; ~5 s incremental). See `crates/aio-tool/README.md` for the per-tab feature list and the bus + worker pattern docs.

## Test

```sh
cd AIO_Tool
cargo +1.82.0 test --workspace
```

~199 tests across 6 crates (unit, integration, wire-format goldens, property tests via proptest, converter parity vs the legacy Python tool's byte output).

## Lint

```sh
cd AIO_Tool
cargo +1.82.0 clippy --all-targets --workspace -- -D warnings
cargo +1.82.0 fmt --all -- --check
```

## Linux build requirements

The `serialport` crate's `libudev` feature is enabled in the aio-tool crate so the Flasher tab can enumerate COM ports. Linux runners need:

```sh
sudo apt install libudev-dev   # Debian / Ubuntu
sudo dnf install systemd-devel # Fedora
```

CI's tool-rust workflow installs this in its Ubuntu runner.

## Workspace layout

| Crate | Purpose |
|-------|---------|
| `aio-protocol` | Wire format — byte-for-byte compatible with v2.6.x firmware |
| `aio-i18n` | Translation tables (compile-time key-parity check via build.rs) |
| `aio-device` | Transport trait + Serial + TCP + Mock backends |
| `aio-flasher` | espflash 3.3.0 wrapper (erase, write_partitions, cancel) |
| `aio-converter` | LVGL image encoders (RGB332/565/565_SWAP/888, Alpha/Indexed 1-8 bit, C-array) |
| `aio-tool` | egui binary — 7 tabs (Flasher / Settings / File Manager / Image Converter / Video Converter / Tool Settings / Help) |

## See also

- `crates/<name>/README.md` — per-crate API + tests + caveats
- `crates/<name>/BUGS.md` — preserved-from-Python wire-format bugs (B1, B2, B15)
- `Docs/superpowers/specs/2026-06-05-aio-tool-rust-rewrite-design.md` — original design rationale
- `Docs/superpowers/plans/2026-06-06-plan-*.md` — implementation plans 1-10
