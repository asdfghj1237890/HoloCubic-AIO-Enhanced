# 07 — CI + Release

## 1. CI workflows 一覽

[`.github/workflows/`](../../.github/workflows/) 三個 workflow，trigger 條件不同：

| Workflow | 檔名 | 觸發 | 跑什麼 |
|---|---|---|---|
| **Regression** | `regression.yml` | push to main / PR / manual dispatch | 韌體所有測試（GUI scenario + unit + FTP + firmware build） |
| **AIO_Tool** | `aio-tool.yml` | 限定 `AIO_Tool/**` 改動 | Python pytest + ruff format + ruff check |
| **Release** | `release.yml` | tag push `v*.*.*` | Build .exe + 4 個 .bin → publish GitHub Release |

## 2. Regression workflow — 韌體 CI

[`.github/workflows/regression.yml`](../../.github/workflows/regression.yml) 的 4 個 job 並行跑：

```
┌─────────────────────────────────────────────────────────┐
│ gui-regression (lv_simulater_platformio/)                │
│   1. Install SDL2 dev headers                            │
│   2. pio run -e native_test                              │
│   3. Loop test/scenarios/*/*.scn 跑 --headless           │
│   4. Upload regression-results artifact                  │
│   ~ 2 min                                                │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│ unit-tests (AIO_Firmware_PIO/)                           │
│   1. pio test -e native_unit  (6 tests)                  │
│   2. pio test -e native_ftp   (6 tests, PR-3.0a)         │
│   ~ 30 sec                                               │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│ firmware-build (AIO_Firmware_PIO/)                       │
│   pio run -e HoloCubic_AIO_Releases                      │
│   ~ 2 min cached / 8 min cold                            │
│   PR-72: 抓 host-stub vs Arduino-core divergence        │
└─────────────────────────────────────────────────────────┘
```

每個 job 失敗就擋 PR。三個 job 互相獨立 — 一個爛不影響其他繼續跑。

### 2a. 為什麼有 firmware-build job

PR #72 之前 CI 只跑 host 端的 SDL2 build (`native_test`)，**從不編譯真正 ESP32 firmware**。所以 PR-3.3 (#69) 的 `ESP32FtpServer.cpp` 拆檔留下一個 missing `<WiFi.h>` 的 bug — host stub 把 `WiFiServer` 跟 `WiFiClient` 都塞 `<WiFiClient.h>`，不小心 mask 掉這個依賴。host build 過、release tag 後跑真實 ESP32 build 才炸。

PR #72 加了 `firmware-build` job，從此 host-stub 跟 Arduino-core 不一致這類 bug 在 PR-merge 時就會被抓到，不會等到 release time。

### 2b. Cache + retry

每個 job 有：
- **Cache**：`~/.platformio` 跟 project `.pio/` 都 cache，cache key 用 `platformio.ini` hash。改 `platformio.ini` 會 invalidate；改 `src/` 不會。
- **Retry**：3 次 with backoff 15s/30s。處理 transient lib download failures（GitHub-archive 偶爾 502）。

## 3. AIO_Tool workflow

[`.github/workflows/aio-tool.yml`](../../.github/workflows/aio-tool.yml)。**只在 `AIO_Tool/**` 或 workflow 自己改動時跑**。所以純韌體 PR 不會被 Python lint 拖慢。

四個步驟：
1. `uv sync --all-groups --frozen`
2. `uv run pytest`
3. `uv run ruff format --check .`
4. `uv run ruff check .`

任一 fail 擋 PR。`ty` typecheck 還在 alpha，**沒有**強制。

## 4. Release workflow — 觸發新版

[`.github/workflows/release.yml`](../../.github/workflows/release.yml) 在你 push 一個 `v*.*.*` 格式的 tag 時觸發：

```bash
git tag -a v2.6.9 -m "Release v2.6.9"
git push origin v2.6.9
```

接下來 GitHub Actions 自動：

```
┌─────────────────────────────────────────┐
│ build_firmware (Linux)                   │
│   1. uvx platformio run -e Releases      │
│   2. cp firmware.bin → release/          │
│   3. Upload firmware artifact            │
└─────────────────────────────────────────┘
┌─────────────────────────────────────────┐
│ build_tool (Windows)                     │
│   1. uv sync --all-groups --frozen       │
│   2. uv run pyinstaller CubicAIO_Tool.spec│
│   3. cp dist/CubicAIO_Tool.exe → release/│
│   4. Upload tool artifact                │
└─────────────────────────────────────────┘
        ↓ both succeed
┌─────────────────────────────────────────┐
│ publish_release (Linux)                  │
│   1. Download both artifacts             │
│   2. Stage 3 boot/partition .bin from    │
│      dist/ (committed in repo)           │
│   3. softprops/action-gh-release         │
│      → 5 assets in GitHub release page   │
└─────────────────────────────────────────┘
```

Release page 出現以下 5 個檔案：
- `HoloCubic_AIO_firmware_v2.6.9.bin` (0x10000)
- `HoloCubic_AIO_Tool_v2.6.9.exe`
- `bootloader_qio_80m.bin` (0x1000)
- `partitions.bin` (0x8000)
- `boot_app0.bin` (0xe000)

## 5. AIO_VERSION 怎麼跟 tag 對齊

`AIO_FIRMWARE_PIO/src/common.h` 有：
```cpp
#define AIO_VERSION "2.6.8"
```

**這個常數要跟 tag 同步升**。為什麼：
- `/api/stats` 回傳 `version: AIO_VERSION` → Glass UI 的 hero 顯示「Firmware 2.6.8」
- `/static/glass.{css,js}?v=AIO_VERSION` 的 cache-bust query 要新版本才會強迫瀏覽器重抓

升版流程：
```bash
# 1. 改 common.h
sed -i 's/AIO_VERSION "2.6.8"/AIO_VERSION "2.6.9"/' AIO_Firmware_PIO/src/common.h

# 2. commit + 開 PR + merge
git add AIO_Firmware_PIO/src/common.h
git commit -m "release: bump AIO_VERSION to 2.6.9"
git push origin HEAD
gh pr create --title "release: bump AIO_VERSION to 2.6.9" --body "..."
gh pr merge --squash

# 3. tag merged commit
git checkout main
git pull --ff-only
git tag -a v2.6.9 -m "Release v2.6.9"
git push origin v2.6.9
```

或者把 bump 跟某個 feature PR 綁在一起（最近幾版都這樣做）。

## 6. AIO_Tool TOOL_VERSION

[`AIO_Tool/util/common.py`](../../AIO_Tool/util/common.py) 有：
```python
TOOL_VERSION: str = "v2.5.0"
```

**這個目前不會自動跟 tag 對齊**，會穩定停在某個值（截至寫這份文件時是 `v2.5.0`）。release 流程用 `github.ref_name`（即 tag）作為檔名 + release title，但 release body 還是會 echo 出 `TOOL_VERSION`。

如果要升 TOOL_VERSION 跟 tag 對齊，編輯 `common.py` 那一行。

## 7. 萬一 release build 失敗怎麼辦

實際發生過（PR #69 後 v2.6.1 release）的 recovery 流程：

1. **Tag 已 push 但 release 未發布**（build 失敗就不會進到 publish_release job）→ 沒有公開 release artifact
2. 修 bug、commit、push 到 main
3. 把 tag 移到新 commit：
   ```bash
   git tag -d v2.6.1                      # 砍掉本地
   git push origin :refs/tags/v2.6.1      # 砍掉 remote
   git tag -a v2.6.1 -m "Release v2.6.1" # 重貼
   git push origin v2.6.1
   ```
4. release workflow 重跑

**注意**：force-update tag 是「destructive」操作，但前提是「沒人下載過那個 release」。一旦 release 已 publish，使用者可能已下載，這時要走「升版號」路線（v2.6.1 → v2.6.2）而不是 force-tag。

## 8. PR workflow（這個 repo 的習慣）

我們在這個 repo 的 PR 節奏：

```
1. git checkout -b feature-branch
2. 改程式碼
3. 本機測試（至少 build + 對應的 pio test）
4. git commit -m "..." (HEREDOC 格式，含 Co-Authored-By: Claude line)
5. git push -u origin feature-branch
6. gh pr create --title "..." --body "..."
7. 等 CI 全綠
8. gh pr merge <#> --squash --delete-branch
9. (Optional) tag 新版本觸發 release
```

**CI 通常 2-5 分鐘內全部跑完**。可以先寫下一個 commit 不用乾等。

`gh run watch <run-id>` 可以在 terminal 看 CI 即時輸出，比 GitHub UI 快。

## 9. 該不該為了 docs-only 改動 tag 新版本？

不需要。docs / README / 註解只改文字，沒影響韌體或 .exe 行為。merge 進 main 就好，不用 tag。

該 tag release 的場景：
- 韌體有新 feature / bugfix（影響使用者體驗）
- AIO_Tool 有新 feature / bugfix
- 安全性修補

該 **不** tag release：
- 純註解 / docs
- 重構但行為不變
- CI workflow 改動
- 開發環境 / .gitignore / .editorconfig 之類

## 10. 截至目前的 release 歷史

```
v2.6.0 - Phase 1 + 2 + 早期 Phase 3 累計
v2.6.1 - Phase 3 後續 + format-truncation 修正 + Node 24 CI 升級
v2.6.2 - AIO_Tool 三大修正（CTkButton dict-access、esptool stdout=None、stub_flasher 沒打包）
v2.6.3 - AIO_Tool 遠端方向控制（4 顆按鈕）
v2.6.4 - 加 ✓ OK 按鈕（GO_FORWORD action）
v2.6.5 - Uptime label 修正
v2.6.6 - IP pill 標示 + AP-mode IP 偵測
v2.6.7 - AIO_VERSION 升 + uptime 格式 + 靜態資源 cache-bust
v2.6.8 - 12 個 *_setting 表單完整 i18n
```

完整在 [GitHub Releases](https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced/releases)。

---

## 結語

這份開發者教學到此結束。

- 韌體入門 → [01](./01-firmware-getting-started.md)
- 架構 → [02](./02-firmware-architecture.md)
- 寫第一個 app → [03](./03-firmware-write-your-first-app.md)
- 工具函式 → [04](./04-firmware-utilities.md)
- AIO_Tool → [05](./05-aio-tool.md)
- 測試 → [06](./06-testing.md)
- CI / Release → 你正在看

如果有不準確、過時、或缺漏的地方，歡迎開 PR 修正。**這份文件本身的維護也走同樣的 PR 流程** — docs-only 不需要 tag release，merge 就生效。
