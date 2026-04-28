# AIO Tool — HoloCubic AIO 韌體上位機

中文文檔 | [English](README.md)

[HoloCubic AIO Enhanced fork](https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced) 的桌面端燒錄與設定工具。
原始上游：[ClimbSnail/HoloCubic_AIO](https://github.com/ClimbSnail/HoloCubic_AIO)。

<p align="center">
  <img src="image/holo_256.png" alt="AIO Tool icon" width="160">
</p>

---

## 功能

- **韌體燒錄** — 透過 [esptool](https://github.com/espressif/esptool)（4 個分區，每行：地址 + 路徑 + 勾選 + 選擇按鈕）
- **串口除錯** — 即時接收訊息視窗
- **圖片 / 影片 / 檔案轉換** — 為 HoloCubic SD 卡準備素材
- **深色主題** — 採用 [CustomTkinter](https://github.com/TomSchimansky/CustomTkinter)，連 Windows 11 標題列也是深色
- **響應式佈局** — 拖曳視窗或最大化，右側 / 下方面板會自動伸縮
- **多語系 UI** — 簡中 / 繁中 / 英文（`util/i18n` 從 `i18n/` 讀 JSON）
- **線上版本檢查** — 從 GitHub `main` 抓 [pyproject.toml](pyproject.toml) 與 [common.h](../AIO_Firmware_PIO/src/common.h) 比對

## 快速開始

### 環境需求
- **Python 3.11+**（CI 在 3.13 上跑）
- **[uv](https://github.com/astral-sh/uv)** — 取代 pip + venv 的快速套件管理工具

```powershell
# Windows
winget install astral-sh.uv
# 或:  powershell -c "irm https://astral.sh/uv/install.ps1 | iex"
```

```bash
# macOS / Linux
curl -LsSf https://astral.sh/uv/install.sh | sh
```

### 安裝 + 啟動

```bash
cd AIO_Tool
uv sync --all-groups          # 建立 .venv，安裝執行 + 開發依賴
uv run python CubicAIO_Tool.py
```

或用內附 PowerShell 腳本：

```powershell
.\setup.ps1
```

或使用 Makefile 捷徑：

```bash
make dev    # uv sync --all-groups
make run    # 啟動 GUI
make test   # pytest（38 個測試）
make lint   # ruff check
make build  # PyInstaller -> dist/CubicAIO_Tool.exe
```

## 預編譯版本下載

每次推送 `vX.Y.Z` git tag 會觸發 [`.github/workflows/release.yml`](../.github/workflows/release.yml)，自動編譯韌體 `.bin` + Windows `.exe` 並發佈到 [Releases page](https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced/releases)。

## ffmpeg（影片轉換需要，可選）

只用韌體燒錄功能可跳過此步。影片轉換頁會呼叫 `ffmpeg`：

```bash
# Windows (Chocolatey)
choco install ffmpeg -y

# macOS (Homebrew)
brew install ffmpeg

# Ubuntu / Debian
sudo apt install ffmpeg
```

或者直接把 `ffmpeg.exe` 放到 `CubicAIO_Tool.py` 旁邊（PyInstaller 打包後則放在 `CubicAIO_Tool.exe` 旁）。

## 專案結構

```
AIO_Tool/
├── CubicAIO_Tool.py        # 主程式，Engine 類組裝 7 個 tab
├── CubicAIO_Tool.spec      # PyInstaller 設定（打包 image/、i18n/、cubictool.json）
├── pyproject.toml          # 依賴 + ruff/ty/pytest 設定
├── uv.lock                 # 鎖定的依賴版本
├── Makefile                # dev/lint/format/typecheck/test/build/run 捷徑
├── setup.ps1               # Windows 一鍵安裝（呼叫 uv sync）
├── i18n/                   # 每個語系 134 個翻譯 key
├── image/                  # 視窗圖示（多解析度 ico）+ 預覽 png
├── page/                   # 每個 tab 一個檔案
│   ├── download_debug.py   # 韌體燒錄 + 串口除錯
│   ├── setting.py          # 裝置參數設定
│   ├── filemanager.py      # SD 卡檔案瀏覽
│   ├── images_converter.py # PNG/JPG -> LVGL 圖片格式
│   ├── videotool.py        # mp4 -> rgb565be / mjpeg
│   ├── tool_settings.py    # 語系切換
│   └── help.py             # 內建說明頁
├── util/                   # 共用工具
│   ├── common.py           # 常數 + get_resource_path
│   ├── logger.py           # 集中 logging -> OutFile/aio_tool.log
│   ├── i18n.py             # JSON 驅動翻譯（單例）
│   ├── massagehead.py      # 網路通訊協定（IntEnum + MsgHead）
│   ├── file_info.py        # 檔案操作訊息子類別
│   ├── robotsocket.py      # TCP server/client（threading.Event 優雅關閉）
│   ├── tkutils.py          # tkinter 輔助函數
│   ├── widget_base.py      # CTkEntry placeholder 包裝
│   └── convertor_core.py   # LVGL 圖片轉換核心
├── tests/                  # 38 個 pytest 測試
├── scripts/make_logo.py    # 重新產生 image/holo_256.{ico,png}
├── base_bin/               # 內建 bootloader / partitions / boot_app0
└── dist/                   # PyInstaller 輸出（gitignored）
```

## 開發流程

### 跑測試

```bash
uv run pytest -v
```

測試套件涵蓋：
- `MsgHead.encode/decode` 位元組格式（4 種 byte order）
- `IntEnum` 整數值（協定穩定性）
- `i18n` JSON 載入 + 語系切換
- `RobotSocket` 優雅關閉（server + client）
- `logger` 設定與檔案輸出

### Lint + format

```bash
uv run ruff check .          # 預期 0 warnings
uv run ruff format --check . # 程式風格一致性
uv run ty check .            # 型別檢查（informational）
```

CI（[`.github/workflows/aio-tool.yml`](../.github/workflows/aio-tool.yml)）每個動到 `AIO_Tool/` 的 PR 都會跑這三個。

### 編譯執行檔

```bash
uv run pyinstaller CubicAIO_Tool.spec --noconfirm --clean
# 輸出：dist/CubicAIO_Tool.exe（約 25 MB 單檔）
```

## 疑難排解

### 升級後出現「No module named X」
```bash
uv sync --all-groups --frozen   # 重新對齊 uv.lock
```

### 標題列顯示「[請到 GitHub 查看最新版本]」
代表版本檢查 URL 回應非 200。可能：
- `pyproject.toml` 還沒在 `main` 分支
- 網路被擋

merge 進 `main` 後，徽章會變成「[已是最新版本]」或「[推荐升级最新版本 vX.Y.Z]」。

### Build 失敗 esptool 找不到
我們依賴上游 PyPI 的 `esptool>=4.1,<5.0`，不是內附的 `esptool_v41/` 目錄（保留作為歷史參考）。

## 燒錄位址（參考）

| 檔案 | 位址 | 來源 |
|---|---|---|
| `bootloader_qio_80m.bin` | `0x1000` | `~/.platformio/.../tools/sdk/bin/` |
| `partitions.bin` | `0x8000` | `AIO_Firmware_PIO/.pio/build/<board>/` |
| `boot_app0.bin` | `0xe000` | `~/.platformio/.../tools/partitions/` |
| `firmware.bin` | `0x10000` | `AIO_Firmware_PIO/.pio/build/<board>/` |

四個檔案都已預先放在 `base_bin/` 下，GUI 燒錄頁預設指向那裡。

## 相關連結

- 韌體 repo（此 fork）: https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced
- 原始 AIO 韌體: https://github.com/ClimbSnail/HoloCubic_AIO
- 原始 HoloCubic 硬體: https://github.com/peng-zhihui/HoloCubic
- esptool: https://github.com/espressif/esptool
- ffmpeg: https://github.com/FFmpeg/FFmpeg
- LVGL 圖片轉換工具: https://github.com/W-Mai/lvgl_image_converter
