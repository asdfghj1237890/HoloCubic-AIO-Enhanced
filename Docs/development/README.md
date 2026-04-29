# HoloCubic AIO 開發者教學

這是給想動 HoloCubic AIO 程式碼的人看的入門教學。從零開始，一路講到測試、CI、發版。如果你是要燒 .bin 進去用，請看根目錄的 [README](../../README.md)；這裡是寫程式用的。

## 怎麼安排這份教學

| # | 章節 | 你會學到 | 預估時間 |
|---|---|---|---|
| 01 | [韌體入門](./01-firmware-getting-started.md) | 環境裝好、build 出來、燒進去 | 30 min |
| 02 | [韌體架構](./02-firmware-architecture.md) | `APP_OBJ` 是什麼、AppController 怎麼跑、訊息怎麼傳 | 30 min |
| 03 | [寫你的第一個 App](./03-firmware-write-your-first-app.md) | 從 `example/` 複製出來，加進主 loop，看到畫面 | 1 hr |
| 04 | [工具函式 + 常用模式](./04-firmware-utilities.md) | `http_util` / `json_util` / 設定持久化 / `Send_HTML` / 何時用 PROGMEM | 30 min |
| 05 | [AIO_Tool (Python 上位機)](./05-aio-tool.md) | tab + page 結構、加按鈕、i18n、build .exe | 45 min |
| 06 | [測試完整指南](./06-testing.md) | 三套測試環境（unit / ftp / GUI scenario）怎麼寫、怎麼跑、怎麼讀 fail；TDD walkthrough、harness 內部、stub 設計、goldens workflow、真實 CI fail 案例 | 1.5 hr |
| 07 | [CI + Release](./07-ci-and-release.md) | GitHub Actions 三個 workflow、tag 怎麼觸發 release、PR 流程 | 20 min |
| 08 | [重構與優化案例集](./08-refactoring-case-studies.md) | 10 個真實 PR 的「舊長什麼樣 → 為什麼不好 → 新的怎麼解 → 為什麼這樣解」，覆蓋 strcpy/JSON/MQTT callback/delay()/HTTPClient dedupe/CTkButton/PyInstaller/WiFi STA-vs-AP/cache busting/test design | 1 hr |

## 路徑慣例

文件裡看到 `AIO_Firmware_PIO/...` 都是相對於 repo 根目錄，例如：
- `AIO_Firmware_PIO/src/HoloCubic_AIO.cpp` ＝ 韌體 `setup()` / `loop()` 入口
- `AIO_Tool/CubicAIO_Tool.py` ＝ 上位機 GUI 入口
- `lv_simulater_platformio/` ＝ host 端 SDL2 GUI 模擬環境（測試用）
- `test/` ＝ scenario 檔 + fixture + harness 程式碼

## 你需要先有的東西

- **PlatformIO Core** (CLI)：`pip install platformio` 或裝 VS Code 的 PlatformIO 擴充
- **uv** (Python 依賴管理)：`pip install uv` 或從 [astral.sh](https://docs.astral.sh/uv/) 拿安裝包
- **Git**：有就好，沒特別版本要求
- (Optional) **SDL2 dev headers**：要在 host 跑 GUI scenario harness 時才需要 — Linux: `apt install libsdl2-dev` / macOS: `brew install sdl2` / Windows: PlatformIO 會自動拉

不需要實際 ESP32 板子也能做大多數開發 — 我們有完整的 host 端模擬。**真實硬體只有 driver 層 + WiFi + flash 行為才需要實機驗證**，其他都能在電腦上跑完。

## 如果只想看一件事

- 「怎麼加新 app」→ [章節 03](./03-firmware-write-your-first-app.md)
- 「怎麼跑測試」→ [章節 06](./06-testing.md)
- 「怎麼出新版」→ [章節 07](./07-ci-and-release.md) 最後一段
- 「怎麼動 web 設定頁」→ [章節 04](./04-firmware-utilities.md) 「Web Settings 怎麼加新欄位」
