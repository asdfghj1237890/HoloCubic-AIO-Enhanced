# Studio i18n consumes canonical trilingual JSON

**Date:** 2026-06-11
**Status:** Approved design, pending implementation plan
**Scope:** `AIO_Tool/` (studio crate) + `Docs/design/studio-flasher/` + docs

## Problem

The Studio frontend (`Docs/design/studio-flasher/`) translates its chrome through a
**hand-maintained 56-key JS dictionary** in `i18n.jsx`. The canonical trilingual
translation data lives in `AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json` (the `aio-i18n`
crate's source of truth; `aio-i18n/build.rs` enforces the three locales share an
identical key set at compile time). Since the legacy egui frontend was removed,
`aio-i18n` + those JSON files have **no shipping consumer**, and nothing checks the
Studio JS dict against them — they drift silently. `CLAUDE.md` records this as a
tracked follow-up ("keep them in sync by hand until the follow-up lands"). **This is
that follow-up.**

### The core complication: two key namespaces

The two sides are **not** keyed the same way:

| | Keyed by | Example |
|---|---|---|
| Studio JS dict (`i18n.jsx`) | the **zh-TW display string** | `tr("燒錄韌體")` → `{cn, en}` |
| Canonical JSON (`aio-i18n`) | **semantic identifiers** | `flasher_title` → per-locale string |

`tr(s)` takes a Traditional-Chinese *string* as the lookup key and returns the
active-language variant (`window.__lang ∈ {tw,cn,en}`, default `tw`; `tw` returns the
key verbatim). So "make the JS key set match the JSON key set" is not a straight swap.
Tracing **every** `tr("…")` call site (`index.html`, `dir-a-pro.jsx`,
`studio-pages.jsx`, `studio-convert.jsx`) against the JSON yields three categories:

1. **Match** — the zh-TW literal equals a JSON zh-TW *value* (most call sites).
2. **JS-only (7)** — strings absent from the JSON: the accent/font picker labels
   (`主色`, `字體`, `介面文字字型`, `按鈕、進度、強調元素的顏色` — Studio-only; the egui
   tool never had them), plus near-misses `連接` (JSON has `連線`), `連線中` (JSON has
   `連線中…`), and `語言` (JSON has `語言 / Language:`).
3. **Untranslated (1)** — `studio-pages.jsx:281` uses a leftover developer subtitle
   `tr("透過 /api/settings 讀取裝置設定,改完後 POST 到對應的 /save<Cat>Conf")` that is in
   *neither* dict, so it never translates today.

A naive "generate from JSON, delete `I18N_DICT`" would **regress** the 7 JS-only
strings and leave #3 broken — failing the acceptance criterion. Some reconciliation is
unavoidable.

## Decisions (resolved during brainstorming)

- **Keying model: keep zh-TW keys + supplement.** Generate a zh-TW-value-keyed dict
  from the JSON (so it is no longer hand-maintained), and keep a tiny hand-maintained
  supplement for the strings the JSON cannot provide. `tr()` and its call sites stay
  unchanged (one leftover dev string fixed). *Rejected alternative:* migrating all
  call sites to semantic keys (`tr("flasher_title")`) — more robust but a ~40-site,
  4-file change the task asked to avoid.
- **Generator + check: Rust, existing toolchain.** Hosted in the **studio crate**
  (stable, already depends on `serde_json`, already `cargo test`-ed by
  `tool-studio.yml` on all 3 OSes). Mirrors the spirit of `aio-i18n/build.rs` and adds
  **zero new CI toolchain**. *Rejected alternative:* a Node script (would add an
  `actions/setup-node` step). *Why studio and not `aio-i18n`:* the studio crate does
  **not** depend on `aio-i18n`, so a check placed in `aio-i18n` would not run in the
  `tool-studio.yml` job whose path triggers (`Docs/design/studio-flasher/**`) are the
  ones that matter here.

## Architecture / data flow

```
AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json     canonical source
        │   (Rust generator: reverse-key by zh-TW value)
        ▼
Docs/design/studio-flasher/i18n-generated.js   @generated, committed → window.__I18N_GENERATED
        │                         ┌─ i18n.jsx: I18N_SUPPLEMENT (10 hand-kept entries)
        ▼                         ▼
   I18N_DICT = Object.assign({}, window.__I18N_GENERATED, I18N_SUPPLEMENT)   tr() unchanged
```

Single source of truth = the JSON. The supplement holds only what the JSON
structurally cannot express (Studio-only strings + zh-CN disambiguations).

## Component 1 — generated file `i18n-generated.js`

- **Format:** plain JS (not JSX) with a `@generated … DO NOT EDIT` header, assigning a
  sorted object literal to `window.__I18N_GENERATED`. Each entry:
  `"<zh_TW value>": { "cn": "<zh_CN value>", "en": "<en_US value>" }`. (`tw` is not
  stored — `tr()` returns the key verbatim in `tw` mode.)
- **Construction:** for every JSON key `K`, map `zh_TW[K] → {cn: zh_CN[K], en: en_US[K]}`.
- **Skip rules** (each emitted as a `// skipped: …` comment for transparency):
  - **Multi-line values** (those containing `\n` — the `help_info` /
    `image_converter_info` blobs). Never UI labels; keeps the file to short chrome
    strings.
  - **Ambiguous collisions** — a zh-TW value that maps to differing `{cn,en}` across
    keys (e.g. `start_convert` vs `start_conversion`, both `開始轉換`). Skipped rather
    than guessed; if Studio ever needs one, the resolve-check (Component 4) fails
    loudly and forces an explicit supplement pin. Benign collisions (all keys agree,
    e.g. `cancel`/`cancel_button`) emit once.
- **Determinism:** entries sorted by zh-TW key (BTreeMap) so regeneration is
  byte-stable → the diff check is reliable.
- **Loading:** a plain `<script src="i18n-generated.js"></script>` in `index.html`,
  placed immediately **before** the `i18n.jsx` babel script. Plain scripts execute at
  parse time, before Babel evaluates any `type="text/babel"` module, so
  `window.__I18N_GENERATED` is set when `i18n.jsx` runs.

## Component 2 — supplement (inline in `i18n.jsx`)

A marker-delimited `I18N_SUPPLEMENT` object — **10 entries**, all copied verbatim from
today's dict (no new translation work), so behavior is preserved exactly.

**Group A — Studio-only strings the JSON lacks (7):**

| key (zh-TW) | cn | en |
|---|---|---|
| `連接` | `连接` | `Connect` |
| `連線中` | `连接中` | `Connecting` |
| `主色` | `主色` | `Accent color` |
| `字體` | `字体` | `Font` |
| `語言` | `语言` | `Language` |
| `按鈕、進度、強調元素的顏色` | `按钮、进度、强调元素的颜色` | `Color for buttons, progress and accents` |
| `介面文字字型` | `界面文字字型` | `Interface typeface` |

**Group B — disambiguation pins (3):** these strings exist in the JSON but under two
keys with *different* zh-CN, so the generator skips them as ambiguous. The supplement
pins each to the variant Studio shows today:

| key (zh-TW) | pinned cn | en | ambiguous because |
|---|---|---|---|
| `說明` | `说明` | `Help` | `tab_help` cn=`帮助` vs `tab_help_short` cn=`说明` |
| `燒錄韌體` | `烧录固件` | `Flash Firmware` | `flasher_title` cn=`烧录固件` vs `flash_firmware` cn=`刷写固件` |
| `參數設定` | `参数设定` | `Device Settings` | `params_title` cn=`参数设定` vs `tab_setting` cn=`参数设置` |

Merge is `Object.assign({}, generated, supplement)` — supplement wins, and Group A
keys are disjoint from `generated` anyway. `tr()`, `window.tr`, and `window.I18N_DICT`
are byte-identical to today.

## Component 3 — one call-site fix

`studio-pages.jsx:281` `sub`: replace the leftover dev note with the canonical
`params_subtitle` zh-TW value `讀取與修改 HoloCubic 的 WiFi、系統、天氣等參數`, so the
params page header resolves via the generated dict and translates in en/cn. This is
the only `tr()` literal in neither dict. (Pre-approved under the chosen option.)

## Behavior changes to review (canonicalization deltas)

Sourcing from the canonical JSON changes a handful of **non-`tw`** labels for
unique (non-ambiguous) Studio-used strings. All are reasonable; flagged here so any
can be vetoed (a veto is implemented by pinning the old value in the supplement, or by
editing the canonical JSON):

| zh-TW string | axis | today (JS dict) | after (canonical JSON) |
|---|---|---|---|
| `開始燒錄` (flash button) | en | `Flash` | `Start flashing` |
| `清空晶片` (erase button) | en | `Erase` | `Clear Chip` |
| `遙控` (section header) | en | `Remote` | `Remote Control` |
| `操作記錄` (log header) | en / cn | `Operation log` / `操作记录` | `Operation Log` / `操作日志` |
| `工具設定` (settings title) | cn | `工具设定` | `工具设置` |
| `中斷連線` (disconnect) | cn | `断开连接` | `断开` |
| `重新開機` (reboot) | cn | `重新启动` | `重启` |

Recommendation: **accept all** — the JSON is the more-maintained canonical copy.

## Component 4 — Rust generator + check (studio crate)

Generation logic in a small module (e.g. `src/i18n_gen.rs`), exercised by an
integration test `tests/i18n_sync.rs` that runs inside the existing `cargo test`,
using the **snapshot-update pattern** — no separate binary:

- **Default `cargo test`:**
  1. Regenerate the expected `i18n-generated.js` text in memory from the three JSON
     files.
  2. Read the committed file; compare **newline-normalized**; fail with a message
     telling the dev to re-run with the update env var if stale.
  3. **Resolve-check:** scan `index.html`, `dir-a-pro.jsx`, `studio-pages.jsx`,
     `studio-convert.jsx` for static `tr("…")` / `tr('…')` literals and assert each is
     in `keys(generated) ∪ keys(supplement)`.
- **Update mode:** `UPDATE_I18N=1 cargo test --test i18n_sync` writes the file instead
  of asserting (the only thing that regenerates it).
- **Paths:** built from `env!("CARGO_MANIFEST_DIR")` (= `AIO_Tool/studio`) →
  `../i18n/<locale>.json` and `../../Docs/design/studio-flasher/…`; cross-platform via
  `std::path` joins.
- **Supplement keys** are extracted from `i18n.jsx` by slicing between
  `I18N_SUPPLEMENT-START` / `-END` markers and matching `"<key>":` at line starts.

**Known limitation (documented):** dynamic `tr(it.label)` nav calls are not statically
scanned; their strings resolve via the generated `tab_*_short` values and are verified
manually. Any static checker has this gap.

## Component 5 — CI & cross-platform

- **No `tool-studio.yml` edit.** The check is a studio-crate test, so it rides the
  existing `cargo test` step (ubuntu / windows / macos).
- Add `Docs/design/studio-flasher/.gitattributes` with `i18n-generated.js text eol=lf`
  and normalize newlines in the comparison, so a Windows CRLF checkout cannot
  false-fail the diff.

## Drift coverage (acceptance mapping)

| Change | Caught by |
|---|---|
| New key in **one** locale | `aio-i18n/build.rs` (existing, via `tool-rust.yml`) |
| New key in **all three** locales | generated file goes stale → diff check fails until regenerated + committed |
| New `tr("新")` in **JSX only** | resolve-check fails |
| Edited zh-TW wording in JSON | generated file changes → diff check fails until regenerated |

Acceptance: non-English Studio chrome shows fully translated strings (every static
`tr()` literal resolves; line-281 fixed; nav verified), and adding a key to only one
side fails CI.

## Component 6 — docs to update

- `Docs/design/studio-flasher/README.md` — note `i18n-generated.js` (generated,
  committed) + the `UPDATE_I18N=1 cargo test --test i18n_sync` regen command; update
  the "i18n.jsx tr() + dictionary" line.
- `CLAUDE.md` — replace the "JS dict is not yet checked … keep them in sync by hand"
  notes (two places: the AIO_Tool overview bullet and the "AIO_Tool Rust (shared)"
  conventions bullet) with the landed mechanism.
- `AIO_Tool/crates/aio-i18n/README.md` — update the "no shipping consumer" statement
  (Studio now consumes it transitively via the generated file).

## Out of scope (noted, not changed)

- Hardcoded status text not wrapped in `tr()` (e.g. `studio-pages.jsx:285`
  `處理中…/已載入/待讀取`; convert-page action buttons) — pre-existing, unrelated.
- The residual value-keying imperfection where the Help **page title** (`tr("說明")`,
  `studio-convert.jsx:594`) shows the short zh-CN `说明` rather than `帮助` — matches
  today's behavior; fixing it would require a semantic key (the rejected approach B).
- Wiring `aio-i18n` (the Rust crate API) directly into anything — the JSON files are
  the contract; the crate stays as the parity enforcer.

## File-by-file change list

| File | Change |
|---|---|
| `Docs/design/studio-flasher/i18n-generated.js` | **new**, generated, committed |
| `Docs/design/studio-flasher/i18n.jsx` | replace inline `I18N_DICT` with `Object.assign(generated, I18N_SUPPLEMENT)`; add the 10-entry marker-delimited supplement; update header comment |
| `Docs/design/studio-flasher/index.html` | add `<script src="i18n-generated.js">` before the `i18n.jsx` script tag |
| `Docs/design/studio-flasher/studio-pages.jsx` | line 281 `sub` → canonical `params_subtitle` zh-TW |
| `Docs/design/studio-flasher/.gitattributes` | **new** — `i18n-generated.js text eol=lf` |
| `AIO_Tool/studio/src/i18n_gen.rs` | **new** — generation logic |
| `AIO_Tool/studio/src/lib.rs` | expose the `i18n_gen` module |
| `AIO_Tool/studio/tests/i18n_sync.rs` | **new** — snapshot diff + resolve-check |
| `Docs/design/studio-flasher/README.md`, `CLAUDE.md`, `AIO_Tool/crates/aio-i18n/README.md` | doc updates |

## Verification plan

1. `UPDATE_I18N=1 cargo test --test i18n_sync` to produce the committed file, then a
   clean `cargo test` to confirm the diff + resolve checks pass.
2. Negative checks: add a throwaway `tr("漏譯測試")` → expect resolve-check failure;
   add a dummy key to all three JSON → expect diff-check failure; revert both.
3. Serve the frontend (`npx http-server Docs/design/studio-flasher -p 8765` + preview
   MCP), set `window.__lang` to `en` then `cn`, and screenshot the chrome to confirm
   no Traditional-Chinese fallbacks remain.
