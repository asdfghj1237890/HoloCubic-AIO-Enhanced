# 11 — 測試框架抓出來的 bug

韌體（或 test stub）裡 latent 已久、隨著 regression suite 一步步建起來才被翻出來的真實 bug。每條都有 fix commit、症狀、root cause、以及未來 regression 該由哪個機制抓到。

> 這份不是「重構案例集」（那是 [chapter 08](./08-refactoring-case-studies.md)，講主動優化）。這份是「**被動發現**」— 寫測試時順便發現的東西，記下來避免下次又踩。

---

## 1. stockmarket — `lv_obj_del(stockmarket_gui)` 把 active screen 刪掉

**Severity**：app exit 時 crash
**Fix**：commit `ba4fb82` — 把 `stockmarket_gui_del` 裡的 `lv_obj_del` 改成 `lv_obj_clean`
**檔案**：`AIO_Firmware_PIO/src/app/stockmarket/stockmarket_gui.c:230`

### 症狀

stockmarket 的 smoke scenario 在跑到 RETURN action 半路就 reliably segfault。原本只有 `Serial.println` 可看時是**靜悄悄**的 — process exit code 0、CI 就以為「completed with 0 failure(s)」過了，因為當時 SIGSEGV handler 用 `_exit(0)`-style semantics 繞過了 harness exit code。

### Root cause

`stockmarket_gui_del` 是整個 codebase 唯一一個在自己的 top-level screen object 上呼叫 `lv_obj_del` 的 `_gui_del` — 其他每個 app（bilibili、anniversary、example、game_2048、game_snake、heartbeat）都用 `lv_obj_clean`。順序很關鍵：`AppController::app_exit` **先**呼叫 app 的 `exit_callback`，**再** `app_control_display_scr` 載下一個 screen。當 stockmarket 在那個 callback 裡刪掉 active screen，LVGL 就把 `disp->act_scr = NULL`（並且 print 出 `"the active screen was deleted"` 我們最後在 log 才看到），下一個 refresh tick 在 `lv_obj_update_layout` 裡 dereference NULL act_scr 就 segfault。

### 怎麼抓 regression

- **Track A** stockmarket smoke scenario 直接 segfault — `test/harness/main.cpp` 安裝的 SIGSEGV handler 把 backtrace print 到 stderr；CI workflow 跑 addr2line 把 `+0xN` offset decode 成 file:line。crash 經由 exit code 139 propagate 出來，workflow 現在正確地抓到了（這次 incident 也順便挖出 workflow 端 `if ! cmd; then rc=$?` 一個 bug 在吞 crash；同 series 一起修了）。

---

## 2. game_2048 — `judge()` off-by-one 讓「敗北」永不到達

**Severity**：邏輯 — game state 永遠不可能 report 敗北；如果 ship 出去使用者會看到「板子滿了之後遊戲永遠繼續」
**Fix**：commit `13c88b2` — `<= SCALE_SIZE * SCALE_SIZE` → `< SCALE_SIZE * SCALE_SIZE`，win-check 跟 empty-check 兩個 loop 都改
**檔案**：`AIO_Firmware_PIO/src/app/game_2048/game2048_contorller.cpp::GAME2048::judge()`

### 症狀

unit-test setup 把 4×4 板子塞滿不會 merge 的 2/4/2/4 交替 pattern（無零、無相鄰相等），預期 `judge()` 回 2（敗北）。它回 0（繼續）。

### Root cause

```cpp
for (int i = 0; i <= SCALE_SIZE * SCALE_SIZE; i++) {  // <= 16, off by one
    if (board[i / 4][i % 4] == 0) return 0;
}
```

`i <= 16` 讀到 `board[16/4][16%4] = board[4][0]` — 4×4 array 後面一格。class layout 把 `previous[0][0]` 接在後面，post-init 時值是 0，所以 empty-check loop 永遠在那第 17 次讀到 0 然後 return 0。win-check loop 也有同樣的 `<=` 但無害：`previous[0][0]` 不會 ≥ 2048，不會誤判勝。

### 怎麼抓 regression

- **Track B** `test_judge_returns_2_when_full_board_no_merges` 現在 assert 預期的「回 2」path。同一個 test 抓到了原本的 bug。

---

## 3. media_player — 含 `fs::File` 的 struct 用 `calloc` 是 UB

**Severity**：第一次解析到 SD 卡的 fetch 就 crash
**Fix**：commit `e31e70f` — `calloc` 後對 `File` member 做 placement-new
**檔案**：`AIO_Firmware_PIO/src/app/media_player/media_player.cpp::media_player_init`

### 症狀

SD fixture 補完之後，`tf.listDir("/movie")` 開始真的回 non-empty list，media_player 第一個 process tick 就在 `fs::File::operator=(fs::File&&)` → `String::operator=(String&&)` 裡 segfault。

### Root cause

```cpp
struct MediaAppRunData {
    ...
    File file;       // 內含 String fname / std::string s
};
run_data = (MediaAppRunData *)calloc(1, sizeof(MediaAppRunData));
...
run_data->file = tf.open(file_name);
```

`calloc` 把 raw memory 全部歸零但**從不跑 constructor**。`File` 內部的 `String` 變成 `std::string`，內部 SSO buffer pointer 被歸零而不是被正確 setup。`run_data->file = ...` 的 move-assign 接著 dereference 那些指標就 crash。在 ESP32 上能跑是因為 Arduino 的 `String` 對 zero-initialised state 比 `std::string` 寬容。

### 怎麼抓 regression

- **Track A** media smoke scenario 的 SIGSEGV，addr2line trace 指向 media_player.cpp line 110。修法是 2 行的 `new (&run_data->file) File()` placement-new，加 comment 交叉 reference 這個 failure mode。

---

## 4. FlashFS — `mkdir` 父目錄不存在，所有 write 靜悄悄失敗

**Severity**：infrastructure — 每個 app 的 `read_config` / `write_config` 在 host harness 上都是 no-op，被韌體「graceful fallback to defaults」行為遮掉了
**Fix**：commit `ae1058e` — 把 `FLASH_FIXTURE_DIR` 從 `"test/fixtures/flash"` 改成 `"../test/fixtures/flash"`
**檔案**：`test/stubs/stubs_runtime.cpp`

### 症狀

新加的 `flash_seed` scenario directive（為了 Sina stockmarket test）在 app_init 跑前寫了 `/stockmarket.cfg`，但 `read_config` 還是拿到預設的 `AAPL/US` config。seed 被 ignore — 走的是 `parse_yahoo_data` 而不是 `parse_sina_data`。

### Root cause

```cpp
static const char *FLASH_FIXTURE_DIR = "test/fixtures/flash";  // 錯
...
mkdir(FLASH_FIXTURE_DIR, 0755);
FILE *f = fopen(full.c_str(), "wb");
if (!f) return;
```

native_test binary 從 `lv_simulater_platformio/` 跑，所以 relative path 解到 `lv_simulater_platformio/test/fixtures/flash` — 一個父目錄（`lv_simulater_platformio/test/`）根本不存在的目錄。`mkdir` 回 `ENOENT`（我們沒檢查），`fopen` 回 NULL（靜悄悄 bail），每個 `writeFile` 都是 no-op。後續 `readFile` 回 0 byte，韌體的 `read_config` path 全部把 0 byte 解讀成「first boot — 寫 default」。

這個 bug 自從 FlashFS stub 第一天就 latent。沒被 regression 注意到是因為**每個會寫 config 的 scenario 也都會在 boot 上重新 derive 自己的資料**，所以「persistence missing」從外面看不到。

### 怎麼抓 regression

- **Track A** Sina stockmarket scenario 透過 screenshot diff assert parse 出來的資料是 `海得控制 / 11.65` 而不是 `AAPL / 175.50`。如果 seeded config 沒到 `read_config`，render 出來的 stock 就會錯。
- 更通用：**任何用 `flash_seed` 的 scenario 現在都是 FlashFS read/write pipeline 的 end-to-end 測試**。

---

## 偵測機制總結

這四個 bug 跨 4 種獨立機制 — 框架的價值就是**先有任何一種機制**就能在對應 code path 被走到時把 bug 翻出來：

| Bug | 偵測機制 |
|---|---|
| stockmarket active-screen del | Track A scenario + SIGSEGV+addr2line |
| game_2048 judge() | Track B Unity assertion |
| media_player calloc/String UB | Track A scenario + SD fixture path coverage |
| FlashFS mkdir | flash_seed end-to-end through screenshot diff |

## 跟其他章節的關係

- **這份**＝測試框架本身發現的 bug（被動發現）
- [chapter 08](./08-refactoring-case-studies.md) ＝ 主動 refactor 過的優化案例
- [chapter 09 §8](./09-test-architecture-decomposition.md) ＝ 還沒被框架 cover 的 bug class（long-run leak）

如果未來再發現類似 latent bug，新增到本章 — 包含 fix commit、檔案、症狀、root cause、未來偵測機制。
