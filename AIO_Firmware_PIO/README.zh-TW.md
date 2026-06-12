# HoloCubic AIO 韌體

**Language / 语言 / 語言:** [English](README.md) | [简体中文](README.zh-CN.md) | [繁體中文](README.zh-TW.md)

這是 HoloCubic_AIO 的韌體專案，使用 PlatformIO 為 ESP32 (PICO-D4) 建構。

## 建構需求

- [PlatformIO Core](https://platformio.org/) 或 [PlatformIO IDE](https://platformio.org/platformio-ide)
- 平台：pioarduino `platform-espressif32 55.03.39`
- 框架：Arduino-ESP32 `3.3.9` on ESP-IDF `5.5.4`
- UI runtime：LVGL `9.5.0`
- JSON runtime：ArduinoJson `7.4.3`

## 目前韌體基線（2026-06）

韌體已經脫離舊的 PlatformIO espressif32 `~3.5.0` / LVGL 8 / ArduinoJson 6 基線。現在 release path 使用 pioarduino ESP32 platform，因為官方 PlatformIO espressif32 當前套件線尚未同時提供 Arduino-ESP32 3.x 與 ESP-IDF 5.5.x。

| 層級 | 目前版本 |
|---|---|
| 板卡 | ESP32-PICO-D4 (`pico32`) |
| PlatformIO platform | `pioarduino/platform-espressif32 55.03.39` |
| Arduino core | `framework-arduinoespressif32 3.3.9` |
| ESP-IDF libs | `5.5.4` |
| LVGL | `9.5.0` |
| ArduinoJson | `7.4.3` |

近期韌體加固包含 LVGL 9 開機 tick 修正、ESP32 core 3.x RGB 啟動修正、Stock 設定解析邊界檢查、Stock 長公司名 header 截斷、2048 輸入/渲染修正，以及 WebServer Glass UI 的 RWD CSS。

## 建構說明

### 使用命令列

```bash
# 進入韌體目錄
cd AIO_Firmware_PIO

# 建構發布版本（預設）
pio run
# 或不安裝全域 PlatformIO：
uvx platformio run

# 使用特定環境建構
pio run -e HoloCubic_AIO_Releases

# 建構除錯版本
pio run -e HoloCubic_AIO_Debug

# 清理建構檔案
pio run -t clean

# 建構並上傳到裝置
pio run -t upload
```

### 使用 VS Code 搭配 PlatformIO 擴充套件

1. 開啟 VS Code
2. 安裝 **PlatformIO IDE** 擴充套件
3. 開啟 `AIO_Firmware_PIO` 資料夾
4. 點擊側邊欄的 PlatformIO 圖示
5. 在 **Project Tasks** → **HoloCubic_AIO_Releases** → 點擊 **Build**

## 建構環境

專案有兩個建構設定：

### HoloCubic_AIO_Releases（預設）
- 針對正式使用最佳化
- 在 `platformio.ini` 中設定為 `[env:HoloCubic_AIO_Releases]`

### HoloCubic_AIO_Debug
- 啟用日誌的除錯建構
- 最佳化等級：-O0
- Arduino HAL 日誌等級：1

## 輸出檔案

編譯成功後，二進位檔案位於：

```
.pio/build/HoloCubic_AIO_Releases/
```

關鍵檔案：
- **firmware.bin** - 主應用程式韌體
- **bootloader.bin** - ESP32 開機引導程式
- **partitions.bin** - 分區表

## 燒錄到 ESP32

### 所需檔案和燒錄位址

| 檔案 | 燒錄位址 | 說明 |
|------|---------|------|
| `bootloader.bin` | 0x1000 | ESP32 開機引導程式 |
| `partitions.bin` | 0x8000 | 分區表 |
| `boot_app0.bin` | 0xe000 | 啟動應用程式選擇器 |
| `firmware.bin` | 0x10000 | 主韌體 |

### 使用 PlatformIO 燒錄

```bash
# 透過 PlatformIO 上傳（最簡單的方法）
pio run -t upload

# 指定自訂連接埠
pio run -t upload --upload-port COM5
```

### 使用 esptool 燒錄

```bash
# Windows (PowerShell, esptool v5)
$env:PYTHONIOENCODING='utf-8'
[Console]::OutputEncoding=[System.Text.Encoding]::UTF8
& "$env:USERPROFILE\.platformio\penv\Scripts\esptool.exe" `
  --chip esp32 --port COM5 --baud 115200 --connect-attempts 3 `
  --before default-reset --after hard-reset write-flash -z `
  --flash-mode dio --flash-freq 80m --flash-size detect `
  0x1000 .pio\build\HoloCubic_AIO_Releases\bootloader.bin `
  0x8000 .pio\build\HoloCubic_AIO_Releases\partitions.bin `
  0xe000 "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin" `
  0x10000 .pio\build\HoloCubic_AIO_Releases\firmware.bin

# Linux/macOS（依實際序列埠與 boot_app0 路徑調整）
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 115200 --connect-attempts 3 \
  --before default-reset --after hard-reset write-flash -z \
  --flash-mode dio --flash-freq 80m --flash-size detect \
  0x1000 .pio/build/HoloCubic_AIO_Releases/bootloader.bin \
  0x8000 .pio/build/HoloCubic_AIO_Releases/partitions.bin \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/HoloCubic_AIO_Releases/firmware.bin
```

**注意：** 將 `COM5` 更改為你實際的序列埠（例如 `COM3`、`/dev/ttyUSB0`、`/dev/cu.usbserial-*`）。Windows 使用 esptool v5 前請先設定 UTF-8 輸出，否則預設 CP950 主控台可能讓進度條輸出失敗。

### 清除快閃記憶體（如有需要）

```bash
esptool.py --chip esp32 --port COM5 erase-flash
```

## 序列埠監視器

檢視序列埠輸出：

```bash
# 使用 PlatformIO
pio device monitor

# 指定鮑率
pio device monitor -b 115200
```

Windows 長時間抓開機/崩潰 log 時，建議使用 `uvx --from pyserial python`，因為逾時的 PlatformIO monitor 有機會留下佔用 COM port 的子程序。健康的刷機後 smoke log 應該在 45-70 秒內看到 `AIO (All in one) version ...`、`Initialization MPU6050 success.`，且沒有反覆 `rst:0xc` / Guru Meditation 重啟循環。

預設監視器設定：
- 鮑率：115200
- 過濾器：esp32_exception_decoder

## 板卡設定

- **板卡：** ESP32 PICO-D4 (`pico32`)
- **CPU 頻率：** 240 MHz
- **快閃記憶體頻率：** 80 MHz
- **快閃記憶體模式：** PlatformIO 板卡設定為 QIO；上方保守手動 esptool 指令使用 DIO，已在 ESP32-PICO-D4 + CH9102 實機驗證
- **上傳速度：** 921600 鮑率
- **分區方案：** `partitions-no-ota.csv`（無 OTA 更新）

## 主機端測試

```bash
# 輕量韌體邏輯測試
pio test -e native_unit

# ESP32FtpServer 指令/登入/傳輸 harness
pio test -e native_ftp

# SDL2 + LVGL GUI regression harness
cd ../lv_simulater_platformio
pio run -e native_test
./.pio/build/native_test/program --scenario ../test/scenarios/stockmarket/long_company_name.scn --headless
```

共用同一個 `.pio/build` 的 PlatformIO 測試請順序執行；在 Windows/WSL 上並行跑多個 PlatformIO test/build 可能破壞 build cache。

## 專案結構

```
AIO_Firmware_PIO/
├── src/                    # 原始碼
│   ├── HoloCubic_AIO.cpp  # 主應用程式
│   ├── app/               # 應用模組
│   ├── driver/            # 硬體驅動程式
│   ├── sys/               # 系統工具
│   └── resource/          # 嵌入式資源
├── lib/                   # 專案函式庫
├── include/               # 標頭檔
├── platformio.ini         # PlatformIO 設定
├── partitions-no-ota.csv  # 分區表定義
└── README.md             # 說明檔案
```

## 建構旗標

通用建構旗標（在 `platformio.ini` 中定義）：
- `-fPIC` - 位置無關程式碼
- `-Wreturn-type` - 警告回傳型別問題
- `-Werror=return-type` - 將回傳型別警告視為錯誤

## 疑難排解

### 建構失敗
- 確保 PlatformIO 已正確安裝：`pio --version`
- 清理建構檔案：`pio run -t clean`
- 更新平台：`pio platform update espressif32`

### 上傳失敗
- 檢查裝置是否已連接：`pio device list`
- 驗證 `platformio.ini` 中的上傳連接埠（第 33 行）
- 嘗試在上傳期間按住 BOOT 按鈕
- 降低上傳速度：將 `upload_speed = 921600` 改為 `115200`

### 序列埠監視器顯示亂碼
- 驗證鮑率是否相符（115200）
- 檢查 ESP32 是否正常供電
- 嘗試更換 USB 線或連接埠

## 可用鮑率

支援的上傳/監視器鮑率：
- 115200
- 230400
- 460800
- 576000
- 921600
- 1152000

## 其他資源

- [PlatformIO 文件](https://docs.platformio.org/)
- [ESP32 Arduino 核心](https://github.com/espressif/arduino-esp32)
- [HoloCubic_AIO 主倉儲](https://github.com/ClimbSnail/HoloCubic_AIO)

## 授權

請參閱此倉儲根目錄中的 LICENSE 檔案。

