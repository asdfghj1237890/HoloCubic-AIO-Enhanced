# 01 — 韌體入門

目標：把韌體 build 出來、燒進去、看到開機畫面。

## 0. 倉庫長相

```
HoloCubic_AIO/
├── AIO_Firmware_PIO/        # ← 韌體本體（你 90% 的時間在這）
│   ├── src/                 # C/C++ 原始碼
│   ├── platformio.ini       # 4 個 build env
│   └── test/                # Unity unit tests
├── AIO_Tool/                # Python 上位機 (PC GUI)
├── lv_simulater_platformio/ # host 端 SDL2 GUI 模擬（測試用）
├── test/                    # GUI scenario .scn 檔 + fixtures + harness
├── dist/                    # bootloader.bin / partitions.bin / boot_app0.bin
└── .github/workflows/       # CI
```

## 1. 安裝環境

```bash
pip install platformio
```

PlatformIO 會自己拉 ESP32 toolchain (~500MB，第一次 build 會有點久)。VS Code 用戶可以裝 PlatformIO 擴充，等同於 CLI + GUI。

驗證：
```bash
pio --version
# PlatformIO Core, version 6.x.x
```

## 2. 認識四個 build environment

`AIO_Firmware_PIO/platformio.ini` 有四個 env：

| env | platform | 用途 |
|---|---|---|
| `HoloCubic_AIO_Releases` | `espressif32` | **預設**。生產 build。`-O2`，無 debug log。Release 用。 |
| `HoloCubic_AIO_Debug` | `espressif32` | 除錯 build。`-O0`，`ARDUHAL_LOG_LEVEL=1`，serial print 一堆東西。 |
| `native_unit` | `native` | Host 端 Unity unit tests。**不需要 ESP32**。logic 測試用。 |
| `native_ftp` | `native` | Host 端 FTP server smoke tests。**不需要 ESP32**。 |

> 還有第五個 env 在 `lv_simulater_platformio/platformio.ini` 叫 `native_test`，是 host 端 SDL2 GUI 模擬，對應 GUI scenario 測試。詳見章節 06。

## 3. 第一次 build（不需要硬體）

```bash
cd AIO_Firmware_PIO
pio run -e HoloCubic_AIO_Releases
```

第一次 build 會花 5–10 分鐘下載 toolchain 和 vendored libraries。後續 build 約 1–2 分鐘。

成功後：
```
.pio/build/HoloCubic_AIO_Releases/firmware.bin    ← 4MB 的 ESP32 firmware
```

如果 build 失敗，最常見的原因是某個 lib download 504。retry：
```bash
pio run -e HoloCubic_AIO_Releases
```

## 4. 燒進真實 ESP32

兩條路：

### 4a. 用 AIO_Tool（推薦給沒命令列經驗的人）

下載 [latest release](https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced/releases) 裡的 `HoloCubic_AIO_Tool_vX.Y.Z.exe` + 四個 .bin（bootloader + partitions + boot_app0 + firmware），開 .exe → 選 COM port → 按「燒錄韌體」。詳見章節 05。

### 4b. 用 pio CLI（你剛改完原始碼，要燒自己 build 的那版）

```bash
cd AIO_Firmware_PIO
pio run -e HoloCubic_AIO_Releases -t upload --upload-port COM5
```

把 `COM5` 換成你的實際 port（Linux 是 `/dev/ttyUSB0` 之類）。

第一次連 ESP32 通常要按住板子上的 **BOOT** 鍵，再點 RESET 進 flash mode；esptool 進入後就可以放手。

## 5. 看 log

```bash
pio device monitor -p COM5 -b 115200
```

開機 log 大概像：
```
HoloCubic_AIO v2.6.8
mpu init OK
TFT init OK
WiFi AP started: HoloCubic_AIO @ 192.168.4.1
HTTP server started
```

> **Tip**：如果你要看 panic / stack trace，build 時用 `HoloCubic_AIO_Debug` env，monitor 會自動 decode addr2line。

## 6. 健康檢查清單

build 成功後，這些東西應該都正常：

- [ ] `pio run -e HoloCubic_AIO_Releases` 編譯通過
- [ ] `pio run -e HoloCubic_AIO_Debug` 也通過
- [ ] `pio test -e native_unit` — 6 個 unit test pass
- [ ] `pio test -e native_ftp` — 6 個 FTP test pass
- [ ] 從 `lv_simulater_platformio/` 跑 `pio run -e native_test` — host SDL2 binary build 出來

如果任何一項 fail 而你沒動程式，先 `git status` 看是不是工作樹有什麼東西。乾淨的 main 應該全部綠。

## 下一步

進到 [02 — 韌體架構](./02-firmware-architecture.md) 看 `APP_OBJ` 怎麼設計、AppController 怎麼跑你的 app。
