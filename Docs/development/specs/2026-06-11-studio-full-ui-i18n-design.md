# Full Studio UI internationalization

**Date:** 2026-06-11
**Status:** Approved design, pending implementation plan
**Depends on:** [2026-06-11-studio-i18n-canonical-json-design.md](./2026-06-11-studio-i18n-canonical-json-design.md) (the generated-dict + drift-check infrastructure this builds on)
**Scope:** `Docs/design/studio-flasher/*.jsx` + `index.html`, `AIO_Tool/i18n/*.json`, `AIO_Tool/studio` (test + generator)

## Problem

The Studio frontend was authored from a design-handoff prototype with **many hardcoded zh-TW string literals that never call `tr()`**. The just-landed i18n work wired `tr()` to the canonical JSON and proved every *wrapped* call resolves — but a visual pass (en + zh-CN) shows large parts of the UI stay Traditional Chinese in every language: the flasher step cards, most of the settings/params page, the file manager, the image/video converters, and the help page.

A discovery sweep of the four UI files found **~138 unwrapped CJK literal sites**:
- **~41** are static strings whose exact zh-TW already exists in the JSON → a plain `tr()` wrap resolves them.
- **~36** are interpolated — built from `${count}` / concatenation / split by inline `<Icon>`/`<strong>`/`<br>` — so a plain `tr()` is not a drop-in.
- **~88** have no dictionary entry → need a new canonical key (with authored en + zh-CN) or, rarely, a supplement entry.

Goal: every **production-visible** string in Studio renders in the active language, enforced so it can't regress.

## Decisions

- **Scope: production-visible only.** Translate everything that renders in the shipped Tauri app. **Exclude** browser-preview/mock-only messages (the `flash-sim` simulation paths, `(瀏覽器預覽模式) …`), dev/debug warning strings, and the concept-stage version footer — they never appear in production. **Exclude** the language-picker labels `繁體中文 / 简体中文 / English` (a language menu always shows each option in its own script).
- **Mechanism: keep `tr()`, add `trf()`** for placeholders (below). No new runtime dependency — the prototype stays bundler-free, loaded via CDN. *Rejected:* an ICU/i18n library (CDN dep, overkill) and migrating every site to semantic keys (would force re-doing all existing + new call sites; the repo deliberately keys `tr()` by the zh-TW string).
- **Storage: new strings go to the canonical JSON**, not the supplement. `AIO_Tool/i18n/*.json` is the source of truth (`build.rs` enforces 3-locale parity); the `I18N_SUPPLEMENT` stays small (its existing 11 entries + only genuinely Studio-only strings or zh-TW-value collisions the generator must skip).
- **Translations authored by Claude** from the existing zh-TW wording, following the glossary below for consistency with the JSON's existing terminology. The exact per-page tables live in the implementation plan and are reviewed there + verified visually per page.

## Component 1 — `trf()` placeholder helper (`i18n.jsx`)

```js
// trf(template, vars) — like tr() but fills {name} slots after the lookup.
function trf(template, vars) {
  return tr(template).replace(/\{(\w+)\}/g, (_, k) => (k in vars ? vars[k] : `{${k}}`));
}
window.trf = trf;
```

- The **template is the zh-TW string** with `{name}` placeholders (e.g. `"已讀取 {n} 項參數，跨 {c} 個分類。"`); it is the dictionary key, exactly as `tr()` keys are.
- Each locale's value carries the same `{name}` slots (`Read {n} parameters across {c} categories.`).
- Call-site conversion: `` `已讀取 ${flat.length} 項參數，跨 ${cats} 個分類。` `` → `trf("已讀取 {n} 項參數，跨 {c} 個分類。", { n: flat.length, c: cats })`.
- Error concatenations (`"✗ 讀取失敗：" + e`) become `trf("✗ 讀取失敗：{e}", { e })` (or keep the `+ e` and wrap only the prefix where the prefix is a standalone production string).

## Component 2 — strings split by inline elements

- **Icon-then-text** (the common Group-B case): the text is a sibling node → wrap it. `<Icon …/>讀取設定` → `<Icon …/>{tr("讀取設定")}`. The trailing count expression (e.g. `寫入修改` + `` ` (${n})` ``) stays a separate expression.
- **`<br>` two-liners**: wrap each line. `尚未連線<br/>接上 USB 後於步驟 1 連接` → `{tr("尚未連線")}<br/>{tr("接上 USB 後於步驟 1 連接")}`.
- **Sentence wrapping an inline `<strong>`/`<span>`** (help intro; converter footnote with `<span class=mono>RGB565_SWAP</span>`): the embedded token is a locale-invariant proper noun / code identifier. Render the full translated sentence and re-insert the token: either split into `{tr("… before …")}<strong>HoloCubic AIO</strong>{tr("… after …")}` where word order permits, or drop the inline styling and render the whole `tr()`'d sentence. These are a handful of named spots, each resolved explicitly in the plan.
- **Composite display lines** (`{count} 個分割區 · {size} · 推薦給多數使用者`): rebuild with one `trf` template (`"{count} 個分割區 · {size} · {rec}"`) or compose from existing keys; the plan specifies each.

## Component 3 — translation glossary (consistency with existing JSON)

zh-TW source term → en / zh-CN, matching the conventions already in `AIO_Tool/i18n/*.json`:

| zh-TW | en | zh-CN |
|---|---|---|
| 韌體 | firmware | 固件 |
| 燒錄 | flash | 烧录 |
| 分割區 | partition | 分区 |
| 連接埠 | port | 端口 |
| 裝置 | device | 设备 |
| 記憶卡 | SD card | 存储卡 |
| 螢幕 | screen | 屏幕 |
| 資料夾 | folder | 文件夹 |
| 檔案 | file | 文件 |
| 影片 | video | 视频 |
| 轉碼 | transcode | 转码 |
| 解析度 | resolution | 分辨率 |
| 影格 | frame | 帧 |
| 設定 | settings | 设置 |
| 連線 / 連接 | connect | 连接 |

(zh-CN values are simplified-character conversions of the zh-TW, applying these term swaps; en follows the JSON's existing phrasing for sibling keys.)

## Component 4 — naming & collision rules for new JSON keys

- New keys use the existing snake_case style, grouped by area (`param_*`, `files_*`, `image_*`, `video_*`, `help_*`).
- Each new key's **zh-TW value must be unique** across the JSON (the generator reverse-keys by zh-TW value and skips values that are ambiguous across keys). The plan checks each against existing values; a genuine collision is pinned in `I18N_SUPPLEMENT` (like `圖片`/`說明`) instead.
- `build.rs` parity means every new key is added to all three locale files at once.

## Component 5 — testing

- Extend `every_tr_literal_resolves` to also scan **`trf("…")`** first-arguments and require each template to resolve.
- New test **`trf_templates_have_consistent_placeholders`**: for every `trf` template, assert the set of `{name}` slots is identical across en/zh-CN/zh-TW (catches a translator dropping a `{n}`).
- Regenerate `i18n-generated.js` (the `UPDATE_I18N` snapshot) after JSON changes; the existing `generated_file_is_up_to_date` guards it.
- **Per-page visual verification** (en + zh-CN accessibility snapshots) — the authoritative proof, page by page.

## Rollout

One PR on `studio-i18n-canonical-json`, implemented **page by page**, each its own commit with its own resolve-check + visual pass, in this order (smallest/most-visible first):

1. **`trf` helper + test scaffolding** (the mechanism, before any page uses it).
2. **Flasher** (`dir-a-pro.jsx` `StudioFlasher`) — step cards, firmware card, not-connected panel, port hint, remote buttons, advanced toggle, success state.
3. **Tool Settings** (`dir-a-pro.jsx` `StudioSettings`) — appearance footnote, swatch names.
4. **Params** (`studio-pages.jsx` `StudioParams`) — field labels, category labels, read/write status templates.
5. **Files** (`studio-pages.jsx` `StudioFiles`) — toolbar, breadcrumbs, row labels, details panel, actions.
6. **Image + Video converters** (`studio-convert.jsx`) — toolbars, options, progress, ffmpeg status, footnotes.
7. **Help** (`studio-convert.jsx` `StudioHelp`) — intro paragraph, section + link titles, footer.

Each page's task in the plan lists its exact strings with authored en/zh-CN and the precise edits.

## Out of scope (noted)

- The `flash-sim.jsx` mock and `(瀏覽器預覽模式)` paths, dev warnings, version footer — preview-only.
- Language-picker option labels — shown in their own script by design.
- Switching the dictionary to semantic keys — the repo keys `tr()` by zh-TW string; unchanged.

## Size

Large: ~70–90 new JSON keys (~210–270 locale lines across the three files), the `trf` helper + two test additions, ~138 JSX edits across four files. Mitigated by the page-by-page rollout with a verification gate per page.
