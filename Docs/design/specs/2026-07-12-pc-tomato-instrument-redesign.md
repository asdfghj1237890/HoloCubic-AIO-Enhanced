# PC Monitor + Tomato — instrument-family screen redesign

- **Date**: 2026-07-12
- **Status**: approved by user (brainstorm session; style "A — instrument family", zero-font-work, English status words)
- **Scope class**: re-layout — same data sources, new visual composition; derived display elements allowed; app logic untouched

## 1. Context and goal

The README hero shows the stock screen golden (`test/golden/stockmarket/smoke/01_initial.png`). We want `pc_resource` and `tomato` screens redesigned so their goldens can sit next to it as one visual family, replacing the current five-color "hacker RGB" grid (pc_resource) and the fake frosted-glass rectangles (tomato).

The family look is defined by the stock screen: pure black, gold header, big mono numerals, hairline dividers, two-row footer with gold single-letter markers.

## 2. Scope / non-goals

**In scope**
- Rewrite `AIO_Firmware_PIO/src/app/pc_resource/pc_resource_gui.{c,h}` and `AIO_Firmware_PIO/src/app/tomato/tomato_gui.{c,h}` (visual layer only).
- One call-site signature change in `tomato.cpp` (see §6).
- Delete orphaned app-local font files (see §7).
- Regenerate 4 goldens; update README hero to a 3-up row (stock + pc + tomato).

**Non-goals**
- No change to `pc_resource.cpp` (AIDA64 parsing, data flow) or `struct PC_Resource`.
- No change to tomato countdown/gesture/RGB logic (existing `delay()` calls in `tomato_process` stay; out of scope).
- No change to stockmarket app files.
- No new fonts, no CJK glyph generation (zero-font-work constraint; user-approved English status words).
- No scenario (.scn) action changes.

## 3. Shared family tokens

Both new GUI files declare these as local `#define`s with a comment `// mirrors stockmarket_gui.c`:

| Token | Value | Used for |
|---|---|---|
| BG | `0x000000` | screen background |
| GOLD | `0xFFD000` | header text, footer letter markers, PC temperature values |
| GOLD_DIM | `0xC89030` | top divider |
| GRAY_DIM | `0x666666` | bottom divider, tomato gesture hint |
| GRAY_LABEL | `0x888888` | secondary labels/values |
| WHITE | `0xFFFFFF` | primary values, footer rail |
| TRACK | `0x222222` | bar backgrounds |
| GREEN | `0x00FF44` | up/OK/break accents (stock screen green, `stockmarket_color_rule.cpp:7`) |
| RED | `0xFF2020` | down/alert/focus accents (stock screen red, `stockmarket_color_rule.cpp:8`) |

Shared geometry (identical to stock): header at (12,8) in `ch_font20`; full-width 2px top divider at y=40; full-width 2px bottom divider at y=166; footer rows at y=172 and y=200 in `montserrat_24`; vertical white rail 2×24 at x=128 on footer rows.

## 4. pc_resource screen

All data fields of `struct PC_Resource` remain displayed. Element map (positions are `lv_obj_align(..., LV_ALIGN_TOP_LEFT/TOP_RIGHT, x, y)`):

| Element | Pos | Font | Color | Format / source |
|---|---|---|---|---|
| Header | (12,8) | ch_font20 | GOLD | `"PC MONITOR"` |
| CPU freq | TR(-12,10) | montserrat_20 | GRAY_LABEL | `"%dMHz"`, `cpu_freq` |
| Top divider | y=40 | — | GOLD_DIM | lv_line 240w 2px |
| Row name ×3 | (12, 52/92/132) | montserrat_20 | GRAY_LABEL | `"CPU"` / `"GPU"` / `"RAM"` |
| Row value ×3 | (72, 46/86/126) | ibmplex_bold_30 | WHITE | `"%d"`, `cpu_usage`/`gpu_usage`/`ram_usage` (0–100) |
| `%` suffix ×3 | align_to value, OUT_RIGHT_BOTTOM (+2,-2) | montserrat_20 | GRAY_LABEL | `"%"` |
| Row meta ×3 | TR(-12, 52/92/132) | montserrat_20 | GOLD / GOLD / GRAY_LABEL | `"%d.%d°C"` (`cpu_temp`,`gpu_temp`, value×10) ; `"%dMB"` (`ram_use`) |
| Usage bar ×3 | (12, 78/118/158) | lv_bar 216×4 | TRACK / GREEN | range 0–100, value = usage |
| Bottom divider | y=166 | — | GRAY_DIM | lv_line 240w 2px |
| Net up | (12,172) | montserrat_24 | recolor | `"#00ff44 " LV_SYMBOL_UPLOAD "# %s"`, white value |
| Rail 1 | (128,172) | — | WHITE | lv_line 2×24 |
| Net down | (140,172) | montserrat_24 | recolor | `"#ff2020 " LV_SYMBOL_DOWNLOAD "# %s"`, white value |
| CPU power | (12,200) | montserrat_24 | recolor | `"#ffd000 C# %d.%dW"`, `cpu_power` ×10 |
| Rail 2 | (128,200) | — | WHITE | lv_line 2×24 |
| GPU power | (140,200) | montserrat_24 | recolor | `"#ffd000 G# %d.%dW"`, `gpu_power` ×10 |

Net speed formatting (fields are KB/s ×10, fixed layout forbids the old scrolling labels): `kbps = val/10, frac = val%10`; if `kbps < 1000` → `"%d.%dK"`; else → `"%d.%dM"` with `mb = kbps/1000, mdec = (kbps%1000)/100`.

Removed: the 2×2 grid, cyan cell borders, 80px arcs, FontAwesome-bearing app fonts. `LV_SYMBOL_UPLOAD`/`LV_SYMBOL_DOWNLOAD` are the same codepoints (U+F093/U+F019) from LVGL's built-in Montserrat symbol set, so the up/down icons survive with zero font cost.

`pc_resource_gui.h`: drop `sensor_module` / extension-callback machinery; keep the three public entry points (`display_pc_resource_gui_init`, `display_pc_resource`, `pc_resource_gui_release`) with unchanged signatures.

## 5. tomato screen

| Element | Pos | Font | Color | Format / source |
|---|---|---|---|---|
| Header | (12,8) | ch_font20 | GOLD | `"TOMATO"` |
| Target | TR(-12,10) | montserrat_20 | GRAY_LABEL | `"%dmin"`, `t_start.minute` (live: reflects hold-to-add) |
| Top divider | y=40 | — | GOLD_DIM | lv_line 240w 2px |
| Minute | (32,56) | ibmplex_bold_64 | WHITE (RED when `t.minute==0`) | `"%02d"` |
| Colon dots ×2 | (116,76) and (116,100) | lv_obj 8×8 | same as digits | squares (ibmplex_bold_64 has no `:` — digits `0-9.` only) |
| Second | (132,56) | ibmplex_bold_64 | same as minute | `"%02d"` (132 balances the 8px dot gaps: content spans 32–208, 32px margins both sides) |
| Progress bar | (12,144) | lv_bar 216×6 | TRACK / RED (focus) or GREEN (break) | pct, see below |
| Bottom divider | y=166 | — | GRAY_DIM | lv_line 240w 2px |
| Status | (12,172) | montserrat_24 | RED / GREEN / WHITE | `"FOCUS %.*s"` / `"BREAK %.*s"` / `"TIME UP!"` (space before dots, as mocked) |
| Rail | (128,172) | — | WHITE | lv_line 2×24 |
| Next | (140,172) | montserrat_24 (base GRAY_LABEL) | recolor | `"#ffd000 N# %dmin"` |
| Gesture hint | (12,206) | montserrat_14 | GRAY_DIM | `"HOLD +1min \| TILT reset"` (ASCII only) |

Digit x-positions (32/116/140) assume IBM Plex Mono Bold 64 digit advance ≈38px; implementation may fine-tune ±4px with `lv_obj_align_to`, goldens lock the result.

**Derived values** (no app-logic change; all inputs already in `TomatoAppRunData`):
- Progress: `total = t_start.minute*60 + t_start.second`, `remain = t.minute*60 + t.second`, `pct = total ? clamp(100*(total-remain)/total, 0, 100) : 100`.
- Status: mode `0|1` → FOCUS, else BREAK; dots count keeps the existing formula `(60 - t.second - 1) % 5 + 1` (1–5 dots via `"%.*s"` against `"....."`).
- TIME UP: `t.minute==0 && t.second==0` → status `"TIME UP!"` white, bar full, digits RED. RGB flash logic untouched.
- Next-segment mapping (from `time_mode`): `0→5min`, `1→15min`, `-1→25min`, `2→45min`.

States to visually verify: focus counting, break counting, last-60s red digits, TIME UP.

## 6. Code changes

1. `pc_resource_gui.c` — full rewrite per §4 (`lv_style_t` set shrinks; keep the same init/release lifecycle pattern as stockmarket_gui.c, including the `lv_obj_clean`-not-`lv_obj_del` release rule).
2. `pc_resource_gui.h` — remove `sensor_module`; public API unchanged.
3. `tomato_gui.c` — full rewrite per §5. The `Frosted_Glass` block, uninitialized `btn_style`, and 200px font usage disappear with it.
4. `tomato_gui.h` — `void display_tomato(struct TimeStr t, struct TimeStr t_start, int mode);`
5. `tomato.cpp:363` — `display_tomato(run_data->t, run_data->t_start, run_data->time_mode);` (only call site; verify with grep).
6. Both files: family tokens as local `#define`s (§3). No shared header; stockmarket is untouched, so a 3-file dedupe header is deferred until the stock screen migrates too.

## 7. Font inventory (verified 2026-07-12)

**Reused (already linked into firmware):**
- `ch_font20` (`src/resource/font/ch_font20_tc.c`, NotoSansTC 20px, ASCII + 1438 CJK) — headers.
- `lv_font_ibmplex_bold_30` (stockmarket, glyphs `0-9.`) — PC row values.
- `lv_font_ibmplex_bold_64` (stockmarket, glyphs `0-9.`) — tomato digits. No colon → rectangle colon dots.
- Built-in `lv_font_montserrat_14/20/24` (lv_conf.h:56-61 enables 14/20/24/30/40/48; includes ASCII, `°` U+00B0, and LVGL symbols U+F093/U+F019).

**Deleted (orphaned by this change; each guarded by a repo-wide grep for its symbol before removal):**
- `pc_resource/lv_font_ibmplex_16.c`, `_18.c`, `_24.c`
- `tomato/lv_font_ibmplex_200.c` (largest single font in the app set)
- `tomato/tomato_chFont_20.c` (status words now English)

**Not touched:** `weather/lv_font_ibmplex_64.c` and `_115.c` (weather still owns them; note the tomato-reused `lv_font_ibmplex_bold_64` is the stockmarket one, distinct from weather's `lv_font_ibmplex_64`).

Known glyph gaps that shaped this design: `ch_font20_tc` lacks 專/注/時/到 (traditional), tomato's old font had only simplified 专注中/休息中/时间到 — hence English status words at zero font cost.

## 8. Tests, goldens, README, acceptance

- Scenario files unchanged: `test/scenarios/pc_resource/smoke.scn`, `test/scenarios/tomato/smoke.scn` (actions and stub data stay; goldens change).
- Regenerate exactly 4 goldens with `--update-golden` (locally: `./.pio/build/native_test/program --scenario ../test/scenarios/<app>/smoke.scn --update-golden --headless`; or CI `regression.yml` workflow_dispatch `mode=update-golden` and commit the artifact — per `Docs/development/06-testing.md`):
  - `test/golden/pc_resource/smoke/01_initial.png`, `02_with_data.png`
  - `test/golden/tomato/smoke/01_initial.png`, `02_after_go_forward.png`
- README hero: replace the single stock image at `README.md:7` with a 3-column table row: stock `01_initial` + pc_resource `02_with_data` + tomato `01_initial` (golden paths, so README keeps auto-tracking test baselines). Mirror in `README_zh-CN.md` if it has the same hero.

**Acceptance criteria (all must show real command output):**
1. `uvx platformio run -e HoloCubic_AIO_Releases` exits 0.
2. `pio run -e native_test` builds; both smoke scenarios pass against the 4 regenerated goldens; no other app's golden changes.
3. Repo-wide grep finds zero references to the 5 deleted font symbols.
4. Visual check of the 4 new goldens against §4/§5 (screenshots eyeballed, not assumed).
5. Release `.bin` size reported before/after (expected to shrink; the 200px font alone is large).
6. `git status` shows only intended files (the 4 pre-existing modified files at branch time — AGENTS.md, CLAUDE.md, AIO_Tool/Cargo.lock, AIO_Tool/studio/Cargo.toml — belong to other work and must not be committed here).

## 9. Decisions log

- Style A "instrument family" chosen over Glass-card and mono-terminal directions (mockups reviewed 2026-07-12); rationale: README family coherence next to the stock screen, cheapest LVGL composition, black blends with the device bezel.
- Zero-font-work constraint chosen; then status words switched from simplified-Chinese to English (`FOCUS`/`BREAK`/`TIME UP!`) per user revision — enables deleting `tomato_chFont_20`.
- Colon rendered as two 8×8 `lv_obj` squares rather than regenerating a 64px font with `:`.
- Spec lives under `Docs/design/specs/` (repo uses capital-D `Docs/`; the superpowers default `docs/superpowers/specs/` was overridden to match repo convention).
- Temperature/gold pairing: temps use GOLD rather than introducing a new amber token (fewer family colors).
