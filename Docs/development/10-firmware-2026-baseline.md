# 10 — 2026 韌體技術基線

截至 2026-06，production firmware 已完成第一輪 2026 技術基線升級：Arduino-ESP32 / ESP-IDF、LVGL、ArduinoJson 都已移到目前可在 Arduino framework 下實際落地的新版線。這仍不是「純 ESP-IDF 6.0」專案；它是以 Arduino framework 為前提的現代化 ESP32 韌體基線。

| Layer | Current | 2026 target | 狀態 |
|---|---:|---:|---|
| PlatformIO platform | `pioarduino/platform-espressif32 55.03.39` | Arduino-ESP32 3.x / ESP-IDF 5.5.x | 已升級並通過 COM5 實機 smoke |
| UI runtime | LVGL 9.5.0 via `lib_deps` | LVGL 9.5 | 已升級；保留 LVGL 8 compatibility shim |
| JSON runtime | vendored ArduinoJson 7.4.3 | ArduinoJson 7 | 已升級 |

參考來源：
- PlatformIO espressif32 releases: <https://github.com/platformio/platform-espressif32/releases>
- pioarduino espressif32 releases: <https://github.com/pioarduino/platform-espressif32/releases>
- LVGL changelog: <https://lvgl.io/docs/open/CHANGELOG>
- ESP-IDF v6.0: <https://www.espressif.com/en/news/ESP_IDF_6.0>
- ArduinoJson v6 to v7 guide: <https://arduinojson.org/v7/how-to/upgrade-from-v6/>

## 為什麼不能一行升版

這個 firmware 用的是 Arduino framework，不是純 ESP-IDF 專案。ESP-IDF 6.0 的存在不等於現有 `framework = arduino` 可以無痛切過去；要看 Arduino-ESP32 core 與 PlatformIO package 是否已經提供相容封裝。

官方 PlatformIO `platform-espressif32` 在 6.13.0 提供 ESP-IDF 5.5.3，但 Arduino framework 仍是 Arduino-ESP32 2.0.17。要同時取得 Arduino-ESP32 3.x 與 ESP-IDF 5.5.x，目前採用 pioarduino fork 的 `55.03.39`，對應 Arduino-ESP32 3.3.9 / ESP-IDF 5.5.4。

LVGL 8 → 9 已完成第一輪 migration。因為現有 app 大量使用 LVGL 8 的 object / style / font / image descriptor pattern，專案保留 `aio_lvgl_compat.h` 兼容層，讓舊 app code 能在 LVGL 9.5 下編譯與運作。後續若要寫新 UI，應優先使用 LVGL 9 原生 API，而不是再擴大兼容層。

ArduinoJson 6 → 7 改了記憶體模型。這份 firmware 已改用 `JsonDocument`，但 v7 的 document 會在 heap 上彈性成長；後續仍要用實機觀察天氣、股票等大 payload 的 heap 峰值與碎片化。

## 2026 upgrade contract

任何「現代基線」PR 必須同時滿足：

1. `pio test -e native_unit` 通過。
2. `pio test -e native_ftp` 通過。
3. `pio run -e HoloCubic_AIO_Releases` 能編譯出 firmware。
4. 實機驗證啟動、launcher 切換、WiFi connect / disconnect、File Manager FTP、至少一個 LVGL 重繪密集 app。
5. 文件把版本表從 legacy 改成 upgraded，不能只改 `platformio.ini`。

## 建議拆法

第一階段已完成：切到 pioarduino `55.03.39`，目標是先拿到較新的 toolchain、WiFi stack 和 compiler diagnostics。

第二階段已完成：ArduinoJson 升到 7.4.3，建立 JSON parser/serializer 的 host-side smoke test，並把 firmware call sites 從 `DynamicJsonDocument` / `containsKey()` 遷到 v7 idiom。

第三階段已完成：LVGL 升到 9.5.0，先讓 simulator / host GUI harness 能編 LVGL 9，再用 compatibility shim 承接 firmware app。後續重構應逐步把新 code 收斂到 LVGL 9 原生 API。

第四階段建議：把實機 smoke 變成更固定的 release checklist，尤其是 boot、Stock/WebServer、File Manager、2048、長時間 heap/fragmentation 觀察。ArduinoJson 7 的 document 會在 heap 上彈性成長，天氣、股票等大 payload 仍需要實機長時間觀察。
