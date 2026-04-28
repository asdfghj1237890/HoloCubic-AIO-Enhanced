# AIO Tool — PC companion for HoloCubic AIO firmware

[中文文檔](README_zh-CN.md) | English

A modernised desktop tool for flashing and configuring [HoloCubic AIO firmware](https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced).
This is the **Enhanced fork** — the original upstream is at [ClimbSnail/HoloCubic_AIO](https://github.com/ClimbSnail/HoloCubic_AIO).

<p align="center">
  <img src="image/holo_256.png" alt="AIO Tool icon" width="160">
</p>

---

## Features

- **Firmware flashing** via [esptool](https://github.com/espressif/esptool) (4 partitions × 4-row form, address + path + checkbox + select)
- **Serial debugging** with live receive console
- **Image / video / file conversion** for the HoloCubic SD card
- **Dark theme** powered by [CustomTkinter](https://github.com/TomSchimansky/CustomTkinter) — Windows 11 dark title bar included
- **Responsive layout** — drag the window or maximise; the right and bottom panels resize live
- **Multi-language UI** — Simplified Chinese / Traditional Chinese / English (`util/i18n` reads JSON files in `i18n/`)
- **Online version check** — compares against [pyproject.toml](pyproject.toml) and [common.h](../AIO_Firmware_PIO/src/common.h) on `main`

## Quick start

### Prerequisites
- **Python 3.11+** (3.13 tested in CI)
- **[uv](https://github.com/astral-sh/uv)** — fast Python package manager (replaces pip + venv + virtualenv)

```powershell
# Windows
winget install astral-sh.uv
# or:  powershell -c "irm https://astral.sh/uv/install.ps1 | iex"
```

```bash
# macOS / Linux
curl -LsSf https://astral.sh/uv/install.sh | sh
```

### Install + run

```bash
cd AIO_Tool
uv sync --all-groups          # creates .venv, installs runtime + dev tools
uv run python CubicAIO_Tool.py
```

Or use the bundled PowerShell helper:

```powershell
.\setup.ps1
```

Or via Makefile:

```bash
make dev    # uv sync --all-groups
make run    # launch the GUI
make test   # pytest (38 tests)
make lint   # ruff check
make build  # PyInstaller -> dist/CubicAIO_Tool.exe
```

## Pre-built binaries

Each `vX.Y.Z` git tag triggers [`.github/workflows/release.yml`](../.github/workflows/release.yml) which builds the firmware `.bin` + the Windows tool `.exe` and publishes them to the [Releases page](https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced/releases).

## ffmpeg (optional, for video conversion)

The video conversion tab shells out to `ffmpeg`. If you only flash firmware, you can skip this step.

```bash
# Windows (Chocolatey)
choco install ffmpeg -y

# macOS (Homebrew)
brew install ffmpeg

# Ubuntu / Debian
sudo apt install ffmpeg
```

Or drop `ffmpeg.exe` next to `CubicAIO_Tool.py` (or `CubicAIO_Tool.exe` after PyInstaller build).

## Project layout

```
AIO_Tool/
├── CubicAIO_Tool.py        # main entry; Engine class wires up the 7 tabs
├── CubicAIO_Tool.spec      # PyInstaller spec (bundles image/, i18n/, cubictool.json)
├── pyproject.toml          # deps + ruff/ty/pytest config
├── uv.lock                 # locked dependency versions
├── Makefile                # dev/lint/format/typecheck/test/build/run targets
├── setup.ps1               # one-line Windows installer wrapper around uv sync
├── i18n/
│   ├── zh_CN.json          # 134 translation keys per language
│   ├── zh_TW.json
│   └── en_US.json
├── image/
│   ├── holo_256.ico        # window icon (multi-res 16/32/48/64/128/256)
│   └── holo_256.png        # README preview
├── page/                   # one module per tab
│   ├── download_debug.py   # firmware flash + serial debug
│   ├── setting.py          # device parameter setting
│   ├── filemanager.py      # SD card file browser
│   ├── images_converter.py # PNG/JPG -> LVGL image format
│   ├── videotool.py        # mp4 -> rgb565be / mjpeg
│   ├── tool_settings.py    # language switcher
│   └── help.py             # in-app docs
├── util/                   # shared helpers
│   ├── common.py           # constants + get_resource_path
│   ├── logger.py           # centralised logging -> OutFile/aio_tool.log
│   ├── i18n.py             # JSON-driven translator (singleton)
│   ├── massagehead.py      # network message protocol (IntEnum + MsgHead)
│   ├── file_info.py        # file-op message subclasses
│   ├── robotsocket.py      # TCP server/client with cooperative shutdown
│   ├── tkutils.py          # tkinter helpers
│   ├── widget_base.py      # CTkEntry placeholder wrapper
│   └── convertor_core.py   # LVGL image converter
├── tests/                  # 38 pytest tests
│   ├── test_massagehead.py # protocol wire format + IntEnum stability
│   ├── test_i18n.py        # JSON loading + language switching + fallbacks
│   ├── test_logger.py      # logger setup + file output
│   └── test_robotsocket.py # graceful shutdown via threading.Event
├── scripts/
│   └── make_logo.py        # regenerates image/holo_256.{ico,png}
├── base_bin/               # bundled bootloader / partitions / boot_app0
├── partitions/             # custom partition tables
└── dist/                   # PyInstaller output (gitignored)
    └── CubicAIO_Tool.exe   # ~25 MB single-file
```

## Development workflow

### Running tests

```bash
uv run pytest -v
```

The test suite locks down:
- `MsgHead.encode/decode` byte-format (4 byte orders)
- `IntEnum` integer values (protocol stability)
- `i18n` JSON loading + language switching
- `RobotSocket` graceful shutdown (server + client)
- `logger` setup and file output

### Linting + formatting

```bash
uv run ruff check .         # 0 warnings expected
uv run ruff format --check . # style consistency
uv run ty check .           # type checker (informational)
```

CI ([`.github/workflows/aio-tool.yml`](../.github/workflows/aio-tool.yml)) runs all three on every PR that touches `AIO_Tool/`.

### Building the executable

```bash
uv run pyinstaller CubicAIO_Tool.spec --noconfirm --clean
# Output: dist/CubicAIO_Tool.exe (~25 MB single-file)
```

The `.spec` file bundles:
- `cubictool.json` (settings schema)
- `image/` (icon + UI graphics)
- `i18n/` (translation JSON files)

## Troubleshooting

### "No module named X" after upgrading
```bash
uv sync --all-groups --frozen   # re-pin to uv.lock
```

### Title bar shows "[請到 GitHub 查看最新版本]"
Means the version-check URL `https://raw.githubusercontent.com/asdfghj1237890/HoloCubic-AIO-Enhanced/main/AIO_Tool/pyproject.toml` returned non-200. Either:
- The file isn't on `main` yet (check the [latest commit](https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced/commits/main))
- Network is blocked

After `pyproject.toml` lands on `main`, the badge will become `[已是最新版本]` or `[推荐升级最新版本 vX.Y.Z]`.

### Build fails with esptool import error
We depend on the upstream PyPI `esptool>=4.1,<5.0`, not the vendored `esptool_v41/` directory (which remains in the repo as legacy reference). If `import esptool` fails, run `uv sync --all-groups --frozen`.

## Flash addresses (reference)

| File | Address | Source |
|---|---|---|
| `bootloader_qio_80m.bin` | `0x1000` | `~/.platformio/packages/framework-arduinoespressif32/tools/sdk/bin/` |
| `partitions.bin` | `0x8000` | `AIO_Firmware_PIO/.pio/build/<board>/` |
| `boot_app0.bin` | `0xe000` | `~/.platformio/packages/framework-arduinoespressif32/tools/partitions/` |
| `firmware.bin` | `0x10000` | `AIO_Firmware_PIO/.pio/build/<board>/` |

The four files are pre-bundled under `base_bin/`; the GUI flash tab points at them by default.

## Related

- Firmware repo (this fork): https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced
- Original AIO firmware: https://github.com/ClimbSnail/HoloCubic_AIO
- Original HoloCubic hardware: https://github.com/peng-zhihui/HoloCubic
- esptool: https://github.com/espressif/esptool
- ffmpeg: https://github.com/FFmpeg/FFmpeg
- LVGL image converter: https://github.com/W-Mai/lvgl_image_converter
