# 05 — AIO_Tool（Rust 上位機）

> **Status: under revision.** 這一章原本是從 Python customtkinter 時代寫的，整個工具在 v3.0.0 已經改寫成 Rust，前端又在 v3.1.x 分裂成兩個（Studio Tauri / egui legacy）。下面是當前狀態的最短入口，完整教學（tab 結構 / widget walkthrough / build artefact / packaging）等下次重寫。

## 兩個前端

| 前端 | 路徑 | 角色 |
|---|---|---|
| **Studio** | [`AIO_Tool/studio/`](../../AIO_Tool/studio/) | Tauri 2 + JSX 原型在 [`Docs/design/studio-flasher/`](../design/studio-flasher/) — 目前主要 dev / 新功能落腳處。預定的 release frontend。 |
| **egui** | [`AIO_Tool/crates/aio-tool/`](../../AIO_Tool/crates/aio-tool/) | Rust 1.82 + egui 0.29 — Legacy 前端，但 `release.yml` 目前還在 ship 這個 binary。 |

兩邊共用 5 個 backend crate：`aio-protocol` / `aio-i18n` / `aio-device` / `aio-flasher` / `aio-converter`。

## 想動程式碼前看哪裡

| 想做的事 | 對應檔案 / 文件 |
|---|---|
| 怎麼 dev build + run | 根目錄 [`CLAUDE.md`](../../CLAUDE.md) 的 "Common commands" 章節 — 兩個前端的指令都列在那 |
| 兩個前端怎麼分工 | [`AIO_Tool/README.md`](../../AIO_Tool/README.md) 的 "Two parallel frontends" 表格 |
| Studio JSX 怎麼擺 | [`Docs/design/studio-flasher/README.md`](../design/studio-flasher/README.md) |
| 加新 i18n key | 三個 locale 檔 (`AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json`) 同步加；`aio-i18n/build.rs` compile-time 強制 parity |
| Bus + worker pattern（egui 的長時間 op） | [`CLAUDE.md`](../../CLAUDE.md) "Code conventions" 的 "AIO_Tool Rust — egui frontend only" 區塊 |
| Tauri command + event（Studio 的長時間 op） | [`AIO_Tool/studio/src/commands.rs`](../../AIO_Tool/studio/src/commands.rs) — `#[tauri::command]` + `Emitter::emit` |
| 跑測試 | `cargo +1.82.0 test --workspace` ：~199 個測試（unit、integration、wire-format golden、proptest property、converter parity） |
| Lint | `cargo +1.82.0 fmt --all -- --check` + `cargo +1.82.0 clippy --all-targets --workspace -- -D warnings` |
| CI 怎麼跑 | 章節 [07](./07-ci-and-release.md) |

## 重要慣例（NON-OBVIOUS，會被 enforce）

- **每個使用者可見的字串都要走 i18n**：egui 透過 `aio_i18n::t("key", None)`，Studio 透過 JSX 端的 `tr("key")`。**新 key 要同步加到三個 locale 檔**，少了就 `aio-i18n/build.rs` panic。
- **保留的 Python 時代 wire-format bug**（B1 FileRename、B2 FileGetInfo）在 `aio-protocol` 有 `// PRESERVED-BUG` 註解；不要「修」它，會跟韌體對不上。
- **不要把長時間 op 直接寫在 UI thread**：egui 用 bus + worker（`flasher_worker.rs` 等）+ `Arc<AtomicBool>` cancel flag；Studio 用 Tauri command + `std::thread::spawn` + `Emitter::emit`。兩種模式不混。

如果這一章的內容比 [`AIO_Tool/README.md`](../../AIO_Tool/README.md) 還詳細，那就是 README 該更新；以 README 為準。
