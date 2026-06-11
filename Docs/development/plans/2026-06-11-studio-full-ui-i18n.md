# Full Studio UI Internationalization — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every production-visible string in the Studio UI renders in the active language (zh-TW / zh-CN / en), enforced so it can't regress.

**Architecture:** Keep the existing zh-TW-string-keyed `tr()`; add a tiny `trf(template, vars)` for `{name}`-placeholder strings. Wrap every hardcoded literal; route new strings into the canonical `AIO_Tool/i18n/*.json` (en + zh-CN authored from the existing zh-TW per the spec glossary). Roll out page by page, each gated by the studio-crate resolve-check + a placeholder-consistency check + an en/zh-CN visual snapshot.

**Tech Stack:** browser JSX (`@babel/standalone`, no bundler), Rust studio crate (`serde_json`, the `i18n_gen` module + `tests/i18n_sync.rs`), GitHub Actions `tool-studio.yml` (unchanged).

**Spec:** [Docs/development/specs/2026-06-11-studio-full-ui-i18n-design.md](../specs/2026-06-11-studio-full-ui-i18n-design.md). **Prior infra:** the generated-dict + drift-check from the dict-wiring feature (already merged on this branch).

---

## Per-page workflow (every page task — Tasks 2-7 — follows this)

For the page's component(s):
1. For each unwrapped literal in the task's inventory: decide `tr` (static) vs `trf` (has `{}` slots) vs restructure (split by inline element).
2. For literals whose exact zh-TW is **already** a `zh_TW.json` value → just wrap; they resolve via the generated dict.
3. For literals with **no** JSON value → add a new key to **all three** `AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json` (zh-TW = the literal; en + zh-CN authored per the spec §3 glossary). Keep `zh_TW` values **unique** (no collision with an existing value — if a collision is unavoidable, pin in `i18n.jsx` `I18N_SUPPLEMENT` instead). For `trf` templates, the value carries the same `{name}` slots in every locale.
4. Wrap the call sites (`{tr("…")}` / `trf("…", {…})`).
5. Regenerate the dict: `UPDATE_I18N=1 cargo test --manifest-path AIO_Tool/studio/Cargo.toml --test i18n_sync` and `git add` the updated `i18n-generated.js`.
6. Verify: `cargo test --manifest-path AIO_Tool/studio/Cargo.toml --test i18n_sync` (resolve + placeholder + nav all green), then `cargo fmt … --check` + `cargo clippy … --all-targets -- -D warnings`.
7. Visual: serve `Docs/design/studio-flasher` on :8765, set `localStorage.holocubic-studio-tweaks` to `{lang:"en"}` then `{lang:"cn"}`, snapshot the page, confirm no zh-TW remains (except own-script language labels). Reset to `{lang:"tw"}`.
8. Commit (`feat(studio): i18n <page>`).

`build.rs` rejects a key missing from any locale; the resolve-check rejects a wrapped literal with no dict entry; the placeholder-check rejects a dropped `{slot}`. These three gates make wrapping safe.

---

## Task 1: `trf()` helper + verification harness

**Files:**
- Modify: `Docs/design/studio-flasher/i18n.jsx`
- Modify: `AIO_Tool/studio/src/i18n_gen.rs`
- Modify: `AIO_Tool/studio/tests/i18n_sync.rs`

- [ ] **Step 1: Add `trf` to `i18n.jsx`**

After the `function tr(s) {…}` block and before `window.tr = tr;`, add:

```jsx
// trf(template, vars) — like tr() but fills {name} slots after the lookup.
// The template is the zh-TW string (the dict key); each locale value carries
// the same {name} slots. Unknown slots are left as literal "{name}".
function trf(template, vars) {
  return tr(template).replace(/\{(\w+)\}/g, (_, k) => (k in vars ? String(vars[k]) : `{${k}}`));
}
```

and add `window.trf = trf;` next to `window.tr = tr;`.

- [ ] **Step 2: Add `scan_trf_literals` + `generated_entries` to `i18n_gen.rs`**

After `scan_tr_literals`, add a sibling that finds `trf("…")` / `trf('…')` first-args (templates):

```rust
/// Find all static `trf("…")` / `trf('…')` template literals (first argument)
/// in a source string. Same rules as `scan_tr_literals` but for the `trf(` token.
pub fn scan_trf_literals(src: &str) -> BTreeSet<String> {
    let bytes = src.as_bytes();
    let mut found = BTreeSet::new();
    let mut i = 0usize;
    while let Some(rel) = src[i..].find("trf(") {
        let abs = i + rel; // byte index of 't' in "trf("
        let prev_is_ident = abs > 0 && {
            let p = bytes[abs - 1];
            p.is_ascii_alphanumeric() || p == b'_' || p == b'$' || p == b'.'
        };
        let mut j = abs + 4; // just past "trf("
        if !prev_is_ident {
            while j < bytes.len() && bytes[j].is_ascii_whitespace() {
                j += 1;
            }
            if j < bytes.len() && (bytes[j] == b'"' || bytes[j] == b'\'') {
                let quote = bytes[j] as char;
                let lit_start = j + 1;
                if let Some(qrel) = src[lit_start..].find(quote) {
                    found.insert(src[lit_start..lit_start + qrel].to_string());
                    i = lit_start + qrel + 1;
                    continue;
                }
            }
        }
        i = abs + 4;
    }
    found
}

/// The generated dict as full `zh_TW -> (cn, en)` entries (for placeholder checks).
pub fn generated_entries(dir: &Path) -> BTreeMap<String, (String, String)> {
    let en = load_locale(&dir.join("en_US.json"));
    let cn = load_locale(&dir.join("zh_CN.json"));
    let tw = load_locale(&dir.join("zh_TW.json"));
    build_entries(&en, &cn, &tw).0
}

/// The set of `{name}` placeholder slots in a template string.
pub fn placeholder_set(s: &str) -> BTreeSet<String> {
    let mut out = BTreeSet::new();
    let mut i = 0;
    while let Some(rel) = s[i..].find('{') {
        let start = i + rel + 1;
        if let Some(end_rel) = s[start..].find('}') {
            let name = &s[start..start + end_rel];
            if !name.is_empty() && name.bytes().all(|c| c.is_ascii_alphanumeric() || c == b'_') {
                out.insert(name.to_string());
            }
            i = start + end_rel + 1;
        } else {
            break;
        }
    }
    out
}
```

- [ ] **Step 3: Extend the resolve test + add the placeholder test in `tests/i18n_sync.rs`**

In `every_tr_literal_resolves`, replace the inner scan loop so it also covers `trf` templates. Change:

```rust
        for lit in i18n_gen::scan_tr_literals(&src) {
            if !resolvable.contains(&lit) {
                missing.push(format!("{name}: tr({lit:?})"));
            }
        }
```

to:

```rust
        for lit in i18n_gen::scan_tr_literals(&src) {
            if !resolvable.contains(&lit) {
                missing.push(format!("{name}: tr({lit:?})"));
            }
        }
        for lit in i18n_gen::scan_trf_literals(&src) {
            if !resolvable.contains(&lit) {
                missing.push(format!("{name}: trf({lit:?})"));
            }
        }
```

Then append a new test:

```rust
#[test]
fn trf_templates_have_consistent_placeholders() {
    let dir = i18n_gen::studio_flasher_dir();
    let entries = i18n_gen::generated_entries(&i18n_gen::i18n_dir());
    let mut problems = Vec::new();
    for path in consumer_files(&dir) {
        let src = std::fs::read_to_string(&path).expect("read consumer file");
        let name = path.file_name().unwrap().to_string_lossy().to_string();
        for tmpl in i18n_gen::scan_trf_literals(&src) {
            let tw = i18n_gen::placeholder_set(&tmpl);
            if let Some((cn, en)) = entries.get(&tmpl) {
                if i18n_gen::placeholder_set(cn) != tw || i18n_gen::placeholder_set(en) != tw {
                    problems.push(format!(
                        "{name}: trf({tmpl:?}) placeholders differ across locales \
                         (tw={tw:?} cn={:?} en={:?})",
                        i18n_gen::placeholder_set(cn),
                        i18n_gen::placeholder_set(en)
                    ));
                }
            }
            // templates resolved only via the JS supplement are not placeholder-checked
        }
    }
    problems.sort();
    assert!(problems.is_empty(), "trf placeholder mismatch:\n{}", problems.join("\n"));
}
```

- [ ] **Step 4: Verify the harness compiles and is green with no `trf` calls yet**

Run:

```
cargo fmt --manifest-path AIO_Tool/studio/Cargo.toml --all
cargo fmt --manifest-path AIO_Tool/studio/Cargo.toml --all -- --check
cargo clippy --manifest-path AIO_Tool/studio/Cargo.toml --all-targets -- -D warnings
cargo test --manifest-path AIO_Tool/studio/Cargo.toml --test i18n_sync
node -e "const fs=require('fs');new (require('vm').Script)(fs.readFileSync('Docs/design/studio-flasher/i18n.jsx','utf8'));console.log('JS_PARSE_OK')"
```

Expected: fmt clean; clippy clean; all `i18n_sync` tests pass (no `trf` calls exist yet, so `trf_templates_have_consistent_placeholders` finds nothing and passes); JS parses.

- [ ] **Step 5: Commit**

```
git add Docs/design/studio-flasher/i18n.jsx AIO_Tool/studio/src/i18n_gen.rs AIO_Tool/studio/tests/i18n_sync.rs
git commit -m "feat(studio): add trf() placeholder helper + i18n_sync coverage"
```

---

## Task 2: Flasher page (`dir-a-pro.jsx` `StudioFlasher` + `DeviceCard` + `RemotePad`)

**Files:** Modify `Docs/design/studio-flasher/dir-a-pro.jsx`; `AIO_Tool/i18n/*.json`; regenerate `i18n-generated.js`.

**Inventory** (from discovery; `[J]`=exact JSON value exists, wrap only; `[N]`=new key needed; `[F]`=trf; `[S]`=restructure/split):
- Remote buttons L62-64: `上`[J btn_up] `左`[J btn_left] `確認`[J btn_ok] `首頁`[J btn_home]; refresh tooltip L144 `重新整理連接埠`[N].
- Device chip labels L82: `晶片`[J device_chip] `版本`[J device_rev].
- DeviceCard placeholder L91: `尚未連線`[N] `接上 USB 後於步驟 1 連接`[N] (split by `<br/>`).
- Step 1 L139: `連接裝置`[J step1_title] `用 USB 接上 HoloCubic，選擇對應的連接埠`[J step1_sub]; port hint L157 `提示：找不到連接埠時請安裝 CH340 / CP210x 驅動，再按 <Icon/> 重新整理。`[S/N] (icon-split; author one key with the icon dropped or `{r}` slot).
- Step 2 L161: `選擇韌體`[J step2_title] `使用推薦版本，或展開進階自訂各分割區`[J step2_sub]; firmware card L168 `HoloCubic AIO 韌體`[N]; L169 composite `{count} 個分割區 · {size} · 推薦給多數使用者`[F] (use `推薦給多數使用者`[J recommended_firmware] in the template); version badges L172 `最新`[N] L174 `查詢中…`[N] L175 `離線`[N]; advanced toggle L178 `▾ 收合進階分割區`[N] / `▸ 進階：自訂 {n} 個分割區`[F].
- Step 3 L196: `開始燒錄`[J step3_title] `過程約 30–60 秒，請保持 USB 連接、勿關閉視窗`[J step3_sub]; partition file picker `選擇`[J select_button] L189.
- Success L241-242: `韌體燒錄成功`[J flash_succeeded] `裝置正在重新啟動，稍候即可使用。`[J flash_succeeded_sub].

- [ ] **Step 1: Add new JSON keys (all 3 locales).** Add to `AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json` the `[N]`/`[F]` keys above. Suggested key names + en (author zh-CN by glossary): `port_refresh_tip` (`Refresh ports`), `device_not_connected` (`尚未連線`→`Not connected`), `device_connect_hint` (`接上 USB 後於步驟 1 連接`→`Plug in USB, then connect in step 1`), `firmware_label` (`HoloCubic AIO firmware`), `partition_summary` template (`{count} partitions · {size} · {rec}`), `version_latest` (`Latest`), `version_checking` (`Checking…`), `version_offline` (`Offline`), `advanced_hide` (`▾ Hide advanced partitions`), `advanced_show` template (`▸ Advanced: customize {n} partitions`), plus a port-driver key matching the JSX wording. Keep each `zh_TW` value byte-equal to the JSX literal.

- [ ] **Step 2: Wrap the call sites** per the inventory (`tr`/`trf`/split), editing `dir-a-pro.jsx` only in `StudioFlasher`/`DeviceCard`/`RemotePad`/`Step`. Do NOT touch `StudioSettings` (Task 3) or other components.

- [ ] **Step 3: Regenerate + verify + visual** per the per-page workflow (steps 5-7 above). Expected: en/zh-CN snapshot of the Flash tab shows all step cards, firmware card, not-connected panel, remote tooltips translated.

- [ ] **Step 4: Commit** `git add` the edited `dir-a-pro.jsx`, the 3 JSON files, `i18n-generated.js`; `git commit -m "feat(studio): i18n flasher page"`.

---

## Task 3: Tool Settings (`dir-a-pro.jsx` `StudioSettings`)

**Inventory:** appearance footnote L353 `介面固定為深色主題。語言切換即時套用於導覽與各頁標題等主要介面文字。`[J appearance_footnote]; swatch names L289 `經典藍`/`青`/`紫`[N]. (Language-picker labels `繁體中文/简体中文/English` L324 — **leave unwrapped**, own-script.)

- [ ] **Step 1:** Add `[N]` accent-swatch keys to the 3 JSON (`swatch_classic_blue`→`Classic blue`, `swatch_teal`→`Teal`, `swatch_purple`→`Purple`; zh-CN `经典蓝`/`青`/`紫`).
- [ ] **Step 2:** Wrap the footnote + swatch names in `StudioSettings`; leave the LANGS labels as-is.
- [ ] **Step 3:** Regenerate + verify + visual (Tools tab, en/zh-CN).
- [ ] **Step 4:** Commit `feat(studio): i18n tool settings`.

---

## Task 4: Params page (`studio-pages.jsx` `StudioParams`)

**Inventory:** field labels + placeholders L71-99 (`SSID（自動連線）`, `預設 WiFi 名稱`, `密碼（自動連線）`, `預設 WiFi 密碼`, `備用 WiFi 名稱`, `密碼 1`[J password_1], `密碼 2`[J password_2], `背光亮度`[J backlight], `螢幕旋轉`[J rotation], `開機自啟 App`, `功耗模式`, `節能`, `效能`, `自動校準 MPU`, `操作方向`, `模式`, `Min 亮度`, `Max 亮度`, `亮度步進`, `動畫週期 (ms)` — all `[N]` unless marked `[J]`); category labels L109-110 `系統 / WiFi`, `IMU 校準`[N]; toolbar L292 `裝置 IP`[N] L296 `密碼`[J password]/placeholder `裝置螢幕顯示`[N]; read/write buttons L299 `讀取設定`[J read_settings] L303 `寫入修改`[J write_changes] (icon-split → wrap sibling); status chip L285 `處理中…`/`已載入`/`待讀取`[N]; read-only badge L318 `(唯讀)`[N], dirty tooltip L251 `已修改，尚未寫入`[N]; status templates L148/154/210 `從 {ip} 讀取中…`, `已讀取 {n} 項參數，跨 {c} 個分類。`, `已寫入 {n} 項修改，重新讀取以確認…`[F], errors L156/212 `✗ 讀取失敗：{e}`/`✗ 寫入失敗：{e}`[F], `✗ 請先輸入裝置 IP。`[N], `寫入中…`[N], `輸入裝置 IP 後按「讀取設定」開始。`[N]. **Skip** the `(瀏覽器預覽模式)` mock strings (L173/199/217) and dev-only `⚠` warning — preview-only.

- [ ] **Step 1:** Add the `[N]`/`[F]` keys (3 locales each). Group as `param_*` (e.g. `param_ssid_main`, `param_power_mode`, `param_status_reading` template, …).
- [ ] **Step 2:** Wrap sites in `StudioParams` (and its `CATEGORY_META`/field-config arrays — ensure the arrays are built at render time so `tr()` re-evaluates on language change; if a config array is module-level, move the `tr()` call to the render or convert stored value to a key resolved at render).
- [ ] **Step 3:** Regenerate + verify + visual (Params tab, en/zh-CN; check status line by triggering a read in preview-mock — its non-mock template text should be translated).
- [ ] **Step 4:** Commit `feat(studio): i18n params page`.

---

## Task 5: Files page (`studio-pages.jsx` `StudioFiles`)

**Inventory:** empty hint L598 `輸入 HoloCubic 的 IP 與連接埠後按「連線」`[N]/`即可瀏覽記憶卡檔案`[N] (`<br/>` split); connect controls L591 `連線中…`[J connecting]/`連線`[J connect]; toolbar `根目錄`[N] L565; breadcrumb; row type L638 `資料夾`[N]; empty-dir L643 `空的資料夾`[N]; file kinds L374-379 `影像`/`字型`/`設定檔`/`二進位`/`文字`[N] (and `影片` — pin/new key, distinct from the video-tab use); action buttons L669-672 `開啟`[J open]/`下載到本機`[N]/`重新命名`[J rename]/`刪除`[J delete] (icon-split); details panel L657-660 `類型`/`大小`/`路徑`/`修改時間`[N], L677 `點選檔案以檢視內容與操作`[N], L682 `最近操作`[N]; toolbar buttons L615/616 `上傳檔案`[J upload_file]/`新增資料夾`[J new_folder] (icon-split).

- [ ] **Step 1:** Add `[N]` keys (`files_*`, `filekind_*`). Keep `資料夾`/`影片` zh-TW values unique vs existing — if `影片` collides with `tab_video_converter_short`, pin a `filekind_video` via supplement or use a distinct label.
- [ ] **Step 2:** Wrap sites in `StudioFiles` + its file-meta helper (render-time evaluation as in Task 4).
- [ ] **Step 3:** Regenerate + verify + visual (Files tab, en/zh-CN).
- [ ] **Step 4:** Commit `feat(studio): i18n files page`.

---

## Task 6: Converters (`studio-convert.jsx` `StudioImage` + `StudioVideo`)

**Inventory (image):** header chip L178 `{n} 個檔案`[F]; toolbar L183 `輸出格式`[N]; checkboxes L188/191/195 `C 陣列`/`抖色`/`縮放至`[N]; drop-zone L213 `拖入或點此加入圖片（PNG / JPG / BMP）`[N]; empty L218 `尚未加入任何圖片`[N]; status row L244 `… · C 陣列`[F]; footnote L254 `轉換結果會輸出到…C 陣列。`[N/S] (inline `<span class=mono>`); buttons L202/203 `取消`[J cancel]/`開始轉換`[J start_convert].
**Inventory (video):** ffmpeg chips L451 `ffmpeg 已就緒`/`偵測中…`/`缺少 ffmpeg`[N]; banner L460 `未在 PATH 中找到 ffmpeg，無法轉碼。`[N]; VRows L469/477/478/492/499 `來源影片`[N]/`輸出路徑`[J output_path]/placeholder `輸出檔案路徑`[N]/`解析度`[N]/`影格率 FPS`[N]; default note L513 `預設：240×240 · 20fps · MJPEG · q80`[N]; buttons L518/519 `取消轉碼`/`開始轉碼`[N]; recheck L520 `重新偵測 ffmpeg`[N]; progress L529 `進度`[N], L532 `抽取影格`/`編碼封裝`[N], L541 `轉碼完成`[N], L544 `待命中`[N], L548 `記錄`[N].

- [ ] **Step 1:** Add `[N]`/`[F]` keys (`image_*`, `video_*`).
- [ ] **Step 2:** Wrap sites in `StudioImage` + `StudioVideo` (+ `VRow`/`Drop` helpers).
- [ ] **Step 3:** Regenerate + verify + visual (Image + Video tabs, en/zh-CN).
- [ ] **Step 4:** Commit `feat(studio): i18n converters`.

---

## Task 7: Help page (`studio-convert.jsx` `StudioHelp`)

**Inventory:** intro `<p>` L599 (full paragraph; JSON `help_intro` exists — render the tr()'d paragraph, re-inserting the `<strong>HoloCubic AIO</strong>` via split); section titles L603/608/613 `韌體與工具`[J help_section_firmware]/`AIO 韌體原始專案`[J help_section_original]/`硬體開源方案`[J help_section_hardware]; HelpLink titles L604/605/609/610/614 `HoloCubic AIO Enhanced（本工具增強版）`/`觀看示範影片`/`HoloCubic AIO 原始版本`/`HoloCubic AIO（Gitee 鏡像）`/`HoloCubic 硬體開源專案`[N]; footer L618 — **skip** (version/concept note, preview-only).

- [ ] **Step 1:** Add `[N]` help-link keys (`help_link_*`).
- [ ] **Step 2:** Wrap `StudioHelp` sites; handle the intro paragraph split (`{tr("…before…")}<strong>HoloCubic AIO</strong>{tr("…after…")}` using two new keys for the fragments, or render `help_intro` whole and drop the inline bold — pick whichever keeps word order correct in en).
- [ ] **Step 3:** Regenerate + verify + visual (Help tab, en/zh-CN).
- [ ] **Step 4:** Commit `feat(studio): i18n help page`.

---

## Task 8: Final sweep

- [ ] **Step 1:** Re-run the discovery grep for residual CJK literals not in `tr(`/`trf(`/comments/own-script labels across all four files; wrap any stragglers (or confirm each is intentionally excluded per spec).
- [ ] **Step 2:** Full gates: `cargo fmt … --check`, `cargo clippy … --all-targets -- -D warnings`, `cargo test --manifest-path AIO_Tool/studio/Cargo.toml` (all tests).
- [ ] **Step 3:** Full visual pass — every tab in en and zh-CN; confirm only own-script language labels remain non-translated.
- [ ] **Step 4:** Confirm `main` untouched (`git rev-list --left-right --count main...origin/main` = `0 0`); working tree clean. No commit (verification only).

---

## Self-review notes

- **`trf` template keys** are zh-TW strings with `{slots}`; they live in the JSON like any other key and resolve through the same generated dict. The placeholder-consistency test only covers JSON-sourced templates (supplement templates are hand-checked).
- **Render-time evaluation:** any `tr()`/`trf()` inside a module-level config array won't update on language change — Tasks 4/5 explicitly move those into render scope.
- **Collisions:** new zh-TW values must be unique or the generator skips them (resolve-check then fails) — pin in supplement if a real collision (`影片` filekind vs tab) arises.
- **Out of scope (unchanged):** `flash-sim.jsx` mock + `(瀏覽器預覽模式)` strings, dev warnings, version footer, language-picker labels.
