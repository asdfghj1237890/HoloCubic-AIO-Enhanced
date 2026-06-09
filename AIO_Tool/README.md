# AIO_Tool (v3.1.1)

Cross-platform GUI for the HoloCubic AIO firmware — flashing, settings, file management, image/video conversion, remote control.

## Two parallel frontends

| Frontend | Path | Role | Stack |
|---|---|---|---|
| **Studio** | `AIO_Tool/studio/` | **Primary dev/UI target** — recent feature work (B15 Settings, single-session writes, latest-release fetch) lands here first. The intended release frontend. | Tauri 2 + JSX prototype in `Docs/design/studio-flasher/`, stable Rust (1.85+) |
| **egui binary** (`aio-tool`) | `AIO_Tool/crates/aio-tool/` | Legacy frontend — **still what `release.yml` packages** and uploads to GitHub Releases until the Studio bundle pipeline is wired in. | Rust 1.82 + egui 0.29 + eframe |

Both share the 5 backend crates (`aio-protocol` / `aio-i18n` / `aio-device` / `aio-flasher` / `aio-converter`).

When the user says "run the dev build" or "see the UI" without naming a frontend, **launch Studio** (see `CLAUDE.md` → Common commands for the procedure).

## Build + run

### Studio (Tauri — primary dev target)

```sh
# Step 1: start the JSX frontend on :8765 (Studio's tauri.conf.json points devUrl here)
npx --yes http-server Docs/design/studio-flasher -p 8765 -c-1 --cors

# Step 2: build + launch the Tauri shell — MUST pass --no-default-features in dev,
# otherwise Tauri's `custom-protocol` feature bundles the static assets and the
# binary won't read from the dev URL.
cargo run --manifest-path AIO_Tool/studio/Cargo.toml --no-default-features
```

First Tauri build is ~5 min. Tauri dev mode has no HMR — `Ctrl+R` / `F5` in the Studio window after editing JSX.

### egui binary (legacy)

```sh
cd AIO_Tool
cargo +1.82.0 run --bin aio-tool
```

First build pulls a substantial dep graph (~10 min cold cache; ~5 s incremental).

## Test

```sh
cd AIO_Tool
cargo +1.82.0 test --workspace
```

~199 tests across the 6 workspace crates (unit, integration, wire-format goldens, property tests via proptest, converter parity vs the legacy Python tool's byte output). Tests cover the backend crates that both frontends share. The Studio crate is excluded from this workspace (`exclude = ["studio"]`) because it uses the stable toolchain — its tests run via `tool-studio.yml` separately.

## Lint

```sh
cd AIO_Tool
cargo +1.82.0 clippy --all-targets --workspace -- -D warnings
cargo +1.82.0 fmt --all -- --check
```

## Linux build requirements

The `serialport` crate's `libudev` feature is enabled in `aio-tool` and `aio-studio` so the Flasher tab can enumerate COM ports.

```sh
# egui binary needs only libudev:
sudo apt install libudev-dev   # Debian / Ubuntu
sudo dnf install systemd-devel # Fedora

# Studio also needs Tauri 2's webkit + GTK deps:
sudo apt install libwebkit2gtk-4.1-dev libgtk-3-dev libayatana-appindicator3-dev librsvg2-dev libudev-dev
```

CI's `tool-rust.yml` (egui) and `tool-studio.yml` (Studio) install these in their Ubuntu runners.

## Workspace layout

| Crate | Purpose |
|-------|---------|
| `aio-protocol` | Wire format — byte-for-byte compatible with v2.6.x firmware |
| `aio-i18n` | Translation tables (compile-time key-parity check via build.rs); locale files in `AIO_Tool/i18n/*.json` |
| `aio-device` | Transport trait + Serial + TCP + Mock backends |
| `aio-flasher` | espflash 3.3.0 wrapper (erase, write_partitions, cancel) |
| `aio-converter` | LVGL image encoders (RGB332/565/565_SWAP/888, Alpha/Indexed 1-8 bit, C-array) |
| `aio-tool` | egui binary — 7 tabs (Flasher / Settings / File Manager / Image Converter / Video Converter / Tool Settings / Help) |
| `aio-studio` (separate workspace) | Tauri 2 shell — frontend in `Docs/design/studio-flasher/`; bridges to the 4 backend crates via `#[tauri::command]` + `Emitter::emit` events. See `AIO_Tool/studio/src/commands.rs`. |

## See also

- `crates/<name>/README.md` — per-crate API + tests + caveats
- `crates/<name>/BUGS.md` — preserved-from-Python wire-format bugs (B1, B2, B15)
- `Docs/design/studio-flasher/README.md` — Studio frontend prototype + file layout
- `Docs/superpowers/specs/2026-06-05-aio-tool-rust-rewrite-design.md` — original design rationale
- `Docs/superpowers/plans/2026-06-06-plan-*.md` — implementation plans 1-10
- Root `CLAUDE.md` → "Common commands" — single source of truth for which dev procedure to use when
