# HoloCubic 燒錄工具 — Studio 設計原型

Interactive web prototype of a redesigned HoloCubic flasher GUI. Sourced from a
Claude Design (claude.ai/design) handoff bundle and deployed here as a runnable
reference for the next iteration of `AIO_Tool/`.

## What it is

A dark-themed, guided-flow redesign of the AIO flasher with seven fully working
tabs. Every action — connect, flash, read settings, browse files, convert
images, transcode video — runs against a simulated backend so the prototype
demonstrates the UX end-to-end without touching a real device.

| Tab | Status | Notes |
| --- | --- | --- |
| 燒錄 (Flash) | working | 3-step guided flow · per-partition progress · D-pad remote (keyboard arrows/Enter) · `重新開機` |
| 參數設定 (Params) | working | 15 fields · diff counter · `寫入修改 (N)` |
| 檔案管理 (Files) | working | WiFi connect · breadcrumbs · upload / new folder / rename / delete · right-click menu |
| 圖片轉換 (Image) | working | 12 LVGL formats · C-array · dither · resize |
| 影片轉碼 (Video) | working | ffmpeg pipeline · 2-step progress · MJPEG / rgb565be |
| 工具設定 (Tool) | working | accent color · font · language (繁中 / 简中 / English) · saves to localStorage |
| 說明 (Help) | working | About text · upstream project links |

Accent color defaults to the original egui steel-blue (`#1f6aa5`) sampled from
the existing Rust tool.

## Running it

The prototype transpiles JSX in-browser via `@babel/standalone`, which requires
an HTTP origin (CORS blocks `file://` fetches of the `.jsx` scripts). Serve the
directory with any static server:

```bash
# from this directory
python -m http.server 8000
# or
npx serve .
# or
php -S localhost:8000
```

Then open <http://localhost:8000>. Opening `index.html` directly via `file://`
shows a notice with the same instructions instead of a blank page.

A working internet connection is required for the React, ReactDOM, Babel and
Google Fonts CDNs.

## File layout

```
studio-flasher/
├── index.html          App shell, rail navigation, useTweaks (localStorage-backed)
├── theme.css           Design tokens (dark theme, radius/spacing ramps, .btn / .chip primitives)
├── flash-sim.jsx       useFlasher() hook — port list, connect, flash queue, espflash-style log
├── fl-shared.jsx       <LogView>, <Icon>, ICON path dictionary
├── i18n-generated.js   GENERATED dict (window.__I18N_GENERATED) from AIO_Tool/i18n/*.json — do not edit
├── i18n.jsx            tr() + I18N_SUPPLEMENT (hand-kept Studio-only strings); merges the generated dict
├── dir-a-pro.jsx       <StudioFlasher>, <StudioSettings>, <StudioEmpty>
├── studio-pages.jsx    <StudioParams>, <StudioFiles>
└── studio-convert.jsx  <StudioImage>, <StudioVideo>, <StudioHelp>
```

Scripts use the legacy "attach to window" pattern (no ES module imports) so they
can be loaded as `<script type="text/babel" src=...>` without a bundler.

## Relationship to AIO_Tool

This directory IS the **Studio frontend** — the same JSX is served verbatim
into the Tauri 2 webview that the [`AIO_Tool/studio/`](../../../AIO_Tool/studio/)
binary opens. The Tauri shell registers `tauri.conf.json::devUrl` =
`http://localhost:8765`; in dev you serve this directory with any HTTP server
on :8765 and `cargo run --no-default-features` against the Studio crate. In a
release build, Studio's `custom-protocol` feature (default-on outside dev)
bundles these files into the binary itself.

Studio is the **shipping UI** for AIO_Tool — `release.yml` packages this
directory + the Tauri shell as NSIS (Windows), DMG (macOS), and AppImage
(Linux) bundles for every tag. The legacy
[`AIO_Tool/crates/aio-tool/`](../../../AIO_Tool/crates/aio-tool/) egui binary
stays in-tree for backend-crate cross-validation and as a fallback dev surface
but is no longer in release artefacts. See the root
[`CLAUDE.md`](../../../CLAUDE.md) → "Common commands" for the canonical dev
procedure and the relationship between the two frontends.

Backend bridging:

- **Connect / flash / file ops**: real implementations live in the workspace
  backend crates (`aio-flasher`, `aio-device`, etc.) and are exposed to the
  webview via Tauri commands in
  [`AIO_Tool/studio/src/commands.rs`](../../../AIO_Tool/studio/src/commands.rs);
  events flow back via `Emitter::emit`.
- **Local-only simulation**: `flash-sim.jsx` is the in-browser mock used when
  this directory is served standalone (no Tauri shell) — useful for UX
  iteration without the device round-trip. The Studio binary doesn't read
  this — its commands.rs path takes over instead.

## Provenance

Bundle exported from a Claude Design session
(see [`.design-bundle/chats/chat1.md`](../../../.design-bundle/chats/chat1.md)
for the full conversation). The unused `tweaks-panel.jsx` floating-controls
widget from the bundle was dropped — its only consumer (`useTweaks`) is now a
tiny localStorage-backed inline hook in `index.html`.
