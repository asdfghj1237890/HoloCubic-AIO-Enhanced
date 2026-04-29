# 10 — 測試框架設計（end-state record）

> 這份是「測試框架本身」的設計記錄 — 它怎麼長成現在這樣、機制是什麼、邊界劃在哪。
>
> 想知道**怎麼跑**測試 → [`test/README.md`](../../test/README.md)
> 想知道**怎麼寫**測試 → [chapter 06](./06-testing.md)
> 想看**架構視角** → [chapter 09](./09-test-architecture-decomposition.md)
> 想看**框架在 build 過程踩過的 bug** → [chapter 11](./11-bugs-surfaced-by-tests.md)

## 目標

在開發機上抓 UI regression 跟純邏輯 regression，**不用燒實機**。每個 PR 的 CI 兩分鐘內跑完整套，並 upload candidate screenshot / Unity test result 當 artifact。

框架**不能取代**裝置上的 smoke test — TFT SPI/DMA timing、WiFi reconnect、MPU6050 校正漂移、OTA partition 等等都要實機。期待是「PR push 後 5 分鐘知道 UI 還是 logic 有沒有壞」，不是「不燒板子直接 ship」。

## 雙軌架構

兩個獨立 CI job 並行跑。

### Track A — GUI regression（SDL2 host harness）

把真實的韌體 app GUI 程式碼編成 desktop binary，透過 PlatformIO `env:native_test` 在 [`lv_simulater_platformio/platformio.ini`](../../lv_simulater_platformio/platformio.ini) 裡定義。LVGL render 到 SDL2 surface；CI 上設 `SDL_VIDEODRIVER=dummy` 跑 headless。

關鍵元件：

- **Stubs**（[`test/stubs/`](../../test/stubs/)）— header shim，優先順序高於韌體 driver。Cover Arduino、WiFi/HTTP/SD/FastLED/MPU6050 等等的 surface，讓韌體程式碼可以 link + 跑在 desktop target。`stubs_runtime.cpp` 提供 singleton instance + 一個輕量的 `AppController` 替身。
- **Harness**（[`test/harness/`](../../test/harness/)）— `main.cpp` 啟 LVGL+SDL，`scenario_runner.cpp` parse + 跑 `.scn` 檔，`screenshot.cpp` snapshot `lv_scr_act()` 成 PNG 然後跟 commit 過的 baseline 做 diff。
- **Scenarios**（`test/scenarios/<app>/<case>.scn`）— line-based script。每個 `.scn` 宣告一個受測 app 加一連串 `wait_ms` / `action` / `screenshot` / `assert_no_crash` 步驟。可選 directive：`init_only`（跳過第二個 main_process tick；給那種 main_process 是內部 `while(1)` 永不返回的 app 用）跟 `flash_seed <path> <content>`（預先寫一個 config 檔到 FlashFS，讓 app_init 的 read_config 拿到自訂 state）。
- **Goldens**（`test/golden/<app>/<case>/<step>.png`）— commit 過的 baseline。CI 在 > 0.5% pixel diff 時 fail；`actions/upload-artifact` 把 candidate + diff PNG upload 給 review。

### Track B — Unity unit test（native）

純邏輯韌體模組的 unit test，**不**用 LVGL 或 SDL2。位於 [`AIO_Firmware_PIO/test/native/`](../../AIO_Firmware_PIO/test/native/)，build env `[env:native_unit]` 在 [`AIO_Firmware_PIO/platformio.ini`](../../AIO_Firmware_PIO/platformio.ini)。Stubs 獨立放（[`AIO_Firmware_PIO/test/stubs_unit/`](../../AIO_Firmware_PIO/test/stubs_unit/)）讓 unit-test binary 保持小巧 — 沒 LVGL、沒 SDL2、沒完整 Arduino String。

涵蓋的模組：

| Test | 對應韌體程式碼 | cover 什麼 |
|---|---|---|
| `test_imu_action` | `src/driver/imu.cpp::IMU::getAction` | v_ax / v_ay 閾值表、3 連發 sample 的長按提升 |
| `test_config` | `src/driver/analyse_param.cpp` | 每個 config parser 共用的 line splitter；basic / 空 line / partial-argc 行為 |
| `test_app_controller` | `src/sys/send_to_dispatch.cpp` | event-queue path（cap、push）、dispatch path（handler invocation、NULL handler、missing toApp） |
| `test_game_2048` | `src/app/game_2048/game2048_contorller.cpp::GAME2048` | init 歸零、4 個移動方向（slide + 一次 merge）、`judge()` 0/1/2 回傳 |

為了讓上述模組可測試，做過 3 個小型 refactor，**沒改行為**：
- `analyseParam` 從 `flash_fs.cpp` 抽到自己的 TU
- `send_to_dispatch` 從 `app_controller.cpp::send_to` 抽出來
- `game_2048::judge()` off-by-one boundary 修正（這是真的 bug — 見 chapter 11 第 2 條）

## Coverage snapshot

- **19 / 19 個韌體 app** 在 Track A 至少有一支 smoke scenario
- **18 / 19 app** 有 commit 的視覺 golden（總共 30 張截圖）。三個明確的 opt-out：
  - `weather` page 0 — 時鐘 label 每次 render 都會跳，本質非確定
  - `settings` — 版本 label 用 auto-scrolling marquee，offset 取決於 wall-clock tick 排程
  - `idea` — scenario 只 assert no-crash；沒 screenshot step
- Track B 4 個模組共 **30 個 unit test case**

## Fixture 機制

每種 fixture 有一條 resolution rule，找不到 fixture 就 fallback 到「無 fixture = 舊行為」，所以加 fixture 是 per-app opt-in。

### HTTP fixture（`test/fixtures/http/<host>/<path>.json`）

`HTTPClient::begin(url)` 記下 URL；`GET()` 把 query string 拿掉，去 `test/fixtures/http/<host><path>.json` 找 fixture 檔。檔案有 → 回 200 + 填 buffer。沒有 → 回 -1（舊的「always offline」sentinel）。被 bilibili（Bilibili stat endpoint）、weather（3 步 AccuWeather chain）、stockmarket（Yahoo US 跟 Sina CN parser 都用）使用。

### Socket fixture（`test/fixtures/socket/<host>.txt`）

給走 raw `WiFiClient::connect`（不是 HTTPClient）的 app 用。`connect(host, port)` 把 `test/fixtures/socket/<host>.txt` 載進 client 的 read buffer；`find()` / `readStringUntil()` / `read()` 走它代替真實 socket。目前 pc_resource 用（HTTP-style SSE reply）。

`screen_share` 不需要 — 它的可見狀態都是 "Connect succ"（已經由 WIFI_CONN routing cover），而且 JPEG decoder + `tft->pushImageDMA` 被 stub 成 no-op，所以 fake 一段 MJPEG byte stream 不會增加視覺 coverage。

### SD fixture（`test/fixtures/sd/<dir>/...`）

`SdCard::listDir(dirname)` 掃 host filesystem 上的 `test/fixtures/sd/<dirname>/` 然後 build 一個 `File_Info` linked list mirror 韌體的 circular doubly-linked layout。被 picture（`/image/`）跟 media（`/movie/`）使用。

### Flash fixture（`flash_seed` scenario directive + `test/fixtures/flash/`）

輕量 FlashFS 實作把 `g_flashCfg.writeFile` 呼叫持久化到 `../test/fixtures/flash/<path>`（相對 `lv_simulater_platformio/`，所以實際落到 repo root commit 的目錄）。每個 scenario 開頭會清掉那個目錄，避免 per-scenario state 在 suite 內互相污染。

Scenario 可以用 directive 預先 seed config：

```
flash_seed /stockmarket.cfg "603019\nCN\n10000\n"
```

被 `test/scenarios/stockmarket/cn_smoke.scn` 用，把 CN-market config 塞進去讓 app 走 Sina parser 而不是預設的 Yahoo path。`\n` / `\t` / `\\` / `\"` 會被 decode；包圍的雙引號會被脫掉。

### WIFI_CONN routing（沒 fixture，是 harness 行為）

真實韌體把 WIFI_CONN 訊息排進 queue，`req_event_deal` 在 `wifi_event()` 成功後才呼叫 sender 的 callback。Harness short-circuit：當 `send_to(from_app, "AppCtrl", WIFI_CONN/AP, ...)` 被呼叫，輕量 `AppController::send_to` **同步**呼叫 `from_app->message_handle("AppCtrl", from_app_name, type, message, NULL)`。靠這個 callback 觸發 fetch 的 app（bilibili / weather / stockmarket / pc_resource / file_manager / screen_share）在 scenario 跑時都會走到 fetch path。

### SIGSEGV handler + addr2line decode

[`test/harness/main.cpp`](../../test/harness/main.cpp) 安裝 glibc backtrace handler，攔 SIGSEGV/SIGABRT/SIGBUS/SIGFPE。CI workflow 加一個 addr2line post-pass 把 raw `+0xN` offset decode 成 function name + file:line，crash trace 直接在 log 裡看，不用下載 binary。

## 怎麼加新的 ⋯

### 既有 app 的新 scenario

1. 在 `test/scenarios/<app_name>/<case>.scn` 放 `.scn`，照既有 pattern（看 bilibili / stockmarket 的「fetch + render after action UP」流程）。
2. push、讓 CI 存 candidate、review artifact PNG、覺得對就 copy 到 `test/golden/<app_name>/<case>/<step>.png`。

### 還沒被測過的 app

1. 在 `lv_simulater_platformio/platformio.ini` 的 `build_src_filter` 加 `+<../../AIO_Firmware_PIO/src/app/<name>>`。
2. 在 `test/harness/main.cpp` 加 `#include` + `kRegisteredApps` entry。
3. linker 抱怨什麼 unresolved symbol 就漸進地擴 `test/stubs/`。
4. 照上面寫 scenario。

### 新 endpoint 的 HTTP fixture

1. 把預錄的 reply 放到 `test/fixtures/http/<host>/<path-with-slashes-preserved>.json`。Query string 在 lookup 前會被剝掉，所以同 endpoint 不同 query param 共用 fixture。
2. 更新對應 scenario，要嘛預先觸發 fetch、要嘛 assert post-fetch 的 render 結果。

### 新純邏輯模組的 Track B test

1. 如果目標 function 還卡在某個依賴重的 class 裡，先抽出來自己的 TU（範例：`analyseParam` / `send_to_dispatch`）。
2. 開 `AIO_Firmware_PIO/test/native/test_<name>/test_main.cpp`，照既有 `Unity` pattern。
3. 在 `AIO_Firmware_PIO/platformio.ini` 的 `[env:native_unit]` 把 source 加進 `build_src_filter`。

## 已知限制

- Goldens 是 240×240 PNG，0.5% pixel-diff tolerance。任何取決於絕對 wall-clock 時間的東西（時鐘 label、marquee 動畫、scroll-state-dependent render）都從 golden suite 排除 — 見上面三個 opt-out。
- `test/stubs/stubs_runtime.cpp` 裡的輕量 host AppController **不**模擬真實韌體的 event queue + `req_event_deal`。WIFI_CONN 的同步 callback 捷徑對 fetch-on-WiFi-up 流程算 reasonable 近似，但任何依賴 retry timing、多個 queued event、controller 的 screen-load 動畫做 app transition 的東西，Track A 都覆蓋不夠。
- Track B 的 `stubs_unit/` 是刻意 minimal。任何新 unit-test 模組如果拉進去的韌體程式碼碰到 FreeRTOS timer、LVGL render、driver global，就要 refactor（推薦）或繼續加 stub。

## Reference

- 原始計畫（hybrid 雙軌架構、四階段 rollout、scope-out 清單）：`~/.claude/plans/full-regression-wise-axolotl.md`
- 框架抓出來的 bug：[chapter 11](./11-bugs-surfaced-by-tests.md)
- 完整測試教學：[chapter 06](./06-testing.md)
- 架構視角：[chapter 09](./09-test-architecture-decomposition.md)
