# Changelog

All notable changes to AIO Tool. Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [v2.5.0] — 2026-04-29

Major Modern-Python refactor + UI overhaul. Aligns the tool versioning scheme
with the firmware (both now on `2.5.x`).

### Added
- **CustomTkinter dark theme** + Windows 11 dark title bar
- **Responsive layout** in download_debug, setting, and filemanager tabs
  (resize the window or maximise; right-most + bottom panels track size)
- **i18n** — Simplified Chinese / Traditional Chinese / English driven
  by per-language JSON files in `i18n/` (134 translation keys each)
- **38-test pytest suite** covering protocol wire format (4 byte orders),
  i18n JSON loading, logger, and `RobotSocket` graceful shutdown
- **uv** + **ruff** + **ty** + **pyproject.toml** toolchain (replaces
  `requirements.txt`)
- **Centralised logging** (`util/logger.py`) — output to
  `OutFile/aio_tool.log`; replaced 107 `print()` calls
- **Online version check** against GitHub raw `pyproject.toml` and
  `common.h` (replaces unreachable `climbsnail.cn:5001`)
- **Cyber-tech logo** — isometric holo-cube wireframe with cyan glow,
  multi-resolution `.ico` (16/32/48/64/128/256), reproducible via
  `scripts/make_logo.py`
- **`Makefile`** with dev/lint/format/typecheck/test/build/run targets
- **`.github/workflows/aio-tool.yml`** — pytest + ruff CI on every PR

### Changed
- Tool version `v1.6.2` → `v2.5.0`
- Default window 1000×655 → 1200×720 (CTk widgets are slightly wider);
  resizable enabled with min 1000×600
- `MsgHead` / `SettingMsg` get type hints + explicit `_FIELD_ORDER`;
  the introspection-based `__dir__()` magic is preserved for subclass
  compatibility but documented
- Constants `ModuleType` / `ActionType` / `ValueType` → `IntEnum`
  (`MT`/`AT`/`VT` aliases preserved)
- All `tk.Frame` / `tk.Button` / `tk.Entry` / `tk.Label` / `tk.Checkbutton`
  / `tk.Radiobutton` / `tk.Scrollbar` migrated to CustomTkinter equivalents
- `EntryWithPlaceholder` rewritten as `ctk.CTkEntry` subclass using the
  built-in `placeholder_text` (with kwarg sanitiser for tk-only options)
- Engine notebook tabs `tk.Frame` → `ctk.CTkFrame`
- All identifiers normalised to PEP-8 (e.g.
  `OnThreadMessage` → `on_thread_message`,
  `byteOrder` → `byte_order`, `fileObj` → `file_obj`)
- `% formatting` → f-strings throughout
- `line-length` 100 → 120 (modern wide-monitor default)

### Fixed
- Duplicate `OnThreadMessage` definition (2nd silently shadowed 1st)
- `m_modelManager` AttributeError reference (never assigned)
- Duplicate `get_resource_path` and `_async_raise` definitions
- Synchronous version check froze startup for up to 3s — now in daemon thread
- `RobotSocket` `_async_raise`-based shutdown could leak FDs and locks —
  replaced with `threading.Event` + `socket.settimeout` cooperative loops
- Module-global `STRGLO`/`BOOL` serial reader state (race-prone) →
  per-instance `threading.Event`
- Buffer-overflow risk on Chinese city names (`cityname[10]` → `[32]`)
- Deprecated `Image.ANTIALIAS` → `Image.Resampling.LANCZOS` (Pillow 10+)
- Latent `serial.tools.list_ports` import-order bug
- 3 lint-level bug fixes (B015 pointless `==`, B023 late-binding lambda,
  B018 useless `self.tree.item` expression)

### Removed
- `requirements.txt` (replaced by `pyproject.toml` + `uv.lock`)
- Vendored dependency `./esptool_v41/` is no longer pip-installed
  (we depend on PyPI `esptool>=4.1,<5.0`); the directory remains in the
  repo as a legacy reference but is excluded from ruff
- 600+ lines of dead code: commented-out Amap weather API, NTP client,
  `MsgHead_TT` ctypes mirror, `getSendInfo`, `byteOrders`, `createConfig`,
  `init_modelBar`, old Amap weather→icon map (~75 lines), `windLevelAnalyse`,
  + 1872-char dead bytestring debug comment in filemanager
- All wildcard imports (`from x import *`) — now explicit
- QQ groups + 微雲 portal section from help text (replaced with link to
  the Enhanced fork repo)

### Lint state
- 0 ruff warnings (was ~700 before refactor)
- 0 ANN warnings (full type-hint coverage of `util/`, `page/`, main entry)
- 0 E711 None-comparison warnings
- 0 N* naming warnings (snake_case throughout)
- ruff format clean
- ruff check is now a **required** CI step
