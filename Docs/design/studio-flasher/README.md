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
├── i18n.jsx            tr() + dictionary for 繁中 / 简中 / English
├── dir-a-pro.jsx       <StudioFlasher>, <StudioSettings>, <StudioEmpty>
├── studio-pages.jsx    <StudioParams>, <StudioFiles>
└── studio-convert.jsx  <StudioImage>, <StudioVideo>, <StudioHelp>
```

Scripts use the legacy "attach to window" pattern (no ES module imports) so they
can be loaded as `<script type="text/babel" src=...>` without a bundler.

## Relationship to the real AIO_Tool

This is a **design reference**, not production code. The actual flasher is the
Rust + egui app in [`AIO_Tool/`](../../../AIO_Tool/). Use this prototype to
preview proposed UX changes before reworking egui widgets — flows like the
3-step guided connect→firmware→flash, per-partition progress checklist, and the
in-app appearance / language settings are the main candidates to port.

The connect/flash backend is mocked (`flash-sim.jsx`); the AIO_Tool's
`flasher_worker.rs` is the real implementation.

## Provenance

Bundle exported from a Claude Design session
(see [`.design-bundle/chats/chat1.md`](../../../.design-bundle/chats/chat1.md)
for the full conversation). The unused `tweaks-panel.jsx` floating-controls
widget from the bundle was dropped — its only consumer (`useTweaks`) is now a
tiny localStorage-backed inline hook in `index.html`.
