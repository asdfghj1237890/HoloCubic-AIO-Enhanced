# aio-studio — hardware verification checklist

End-to-end test plan for the Tauri Studio build with a real HoloCubic
attached. Run before each release tag, and any time a Tauri command
in `AIO_Tool/studio/src/commands.rs` changes.

The browser preview (mock mode) is verified automatically; this doc
covers the parts the preview *can't* exercise — real serial I/O, real
ESP32 chip metadata, real TCP/SD card traffic, real ffmpeg output.

## Setup

1. Install the build:
   - Windows: `target/release/bundle/nsis/HoloCubic AIO Studio_<ver>_x64-setup.exe`
   - Linux: `target/release/bundle/{deb,appimage}/*`
2. Plug a HoloCubic into USB.
3. (For the FM / Settings TCP path) make sure the device is on the
   same WiFi as the host and you know its IP / port.
4. Optional: install ffmpeg on PATH if you want the Video tab green.

If any step below fails, copy the relevant `Logs` panel text into the
PR or issue. Reference each step by its `[ID]` for easy correlation.

---

## Flasher tab

- [ ] **[F-1] Port list** — opening the tab shows the HoloCubic's COM
      port in the dropdown. Re-plugging the device + clicking "重新整理
      連接埠" refreshes the list within ~1 s.
- [ ] **[F-2] Connect** — clicking 連接 transitions the status chip to
      `已連線 · <port>` within ~2 s. Chip card on the right shows
      "ESP32" (placeholder — real chip metadata isn't wired yet; this is
      a known limitation, not a test failure).
- [ ] **[F-3] Flash recommended firmware** — pick the bundled
      `HoloCubic_AIO_firmware_v*.bin`, click 開始燒錄. Progress bar
      advances continuously; log lines stream in. Completion log reads
      `✓ 燒錄完成` and the device reboots into the new firmware.
- [ ] **[F-4] Custom 4-partition flash** — expand "進階：自訂 4 個分割
      區", load four .bin files at addresses 0x1000 / 0x8000 / 0xE000 /
      0x10000, flash. Each partition's progress bar fills independently.
- [ ] **[F-5] Erase chip** — click 清空晶片. Spinner runs; completion
      reads `✓ 清除完成`. Re-flashing afterwards still works (regression
      check for any state-leak between cancel and re-spawn).
- [ ] **[F-6] Cancel mid-flash** — start a flash, click cancel before
      progress reaches 100 %. UI returns to non-busy state within ~1 s;
      log shows a cancelled marker.
- [ ] **[F-7] Reboot** — click 重啟. Device LCD blanks then comes back.
      (Currently this just writes `~B`; CH340/CP210x RTS/DTR pulse
      fallback is a future polish.)
- [ ] **[F-8] D-pad remote** — Up / Left / OK / Right / Home each move
      the on-device UI cursor or select the highlighted item.

## Settings tab (device parameters)

- [ ] **[S-1] Schema renders** — every key from `cubictool.json`
      (15 rows across sys / zhixin / tianqi / other) shows as a row.
      Friendly labels apply to the keys listed in `FIRMWARE_FIELD_META`;
      unknown keys fall through to raw-key labels.
- [ ] **[S-2] Connect → Read** — connect to the device's COM port, click
      讀取設定. Status chip flips to "已連線"; the inputs populate.
      Note: against current firmware (B15), many or all rows may show
      "(undecodable N bytes — firmware bug B15)" — that's expected
      until the firmware-side parser is fixed.
- [ ] **[S-3] Write changes** — edit a known-safe field (e.g. `cityname`
      → "Taipei"), click 寫入修改 (1). Confirm device picks up the
      change on next read.
- [ ] **[S-4] Connect-disconnect-reconnect** — cycle 中斷 → 連接 a few
      times. No hung worker thread, no state cross-talk into Flasher tab.

## File Manager tab (TCP/WiFi)

- [ ] **[FM-1] Connect** — enter IP / port, click 連線. Status chip
      flips to "已連線 · <ip:port>" within ~2 s. Root listing populates
      with `image / movie / font / config` (or whatever's on the SD).
- [ ] **[FM-2] Navigate into a folder** — double-click `image`.
      Breadcrumb advances; subfolder contents render.
- [ ] **[FM-3] Download a file** — right-click → 下載 (or click 下載
      from the right-hand details panel). Native save dialog opens
      automatically; saving completes and the file is the right size.
- [ ] **[FM-4] Delete a file** — pick a disposable file, click 刪除.
      File disappears optimistically; ~400 ms later the listing is
      re-requested and it stays gone.
- [ ] **[FM-5] Rename — log only** — rename a file; log says
      `⚠ 重新命名 ... （韌體 B1 未實作）`. The on-device file does NOT
      actually rename. Expected: B1 firmware bug.
- [ ] **[FM-6] Properties — log only** — right-click → properties; the
      log shows `屬性 ...: N bytes (b64)`. No nice metadata yet — B2
      firmware response format isn't reverse-engineered.
- [ ] **[FM-7] Upload / New folder — not wired** — clicking either
      should log `⚠ 尚未支援`. Confirm no crash.
- [ ] **[FM-8] Disconnect** — click 中斷. Status flips back; clicking
      連線 again works (no worker leak).

## Image Converter tab

- [ ] **[I-1] Pick PNG/JPG/BMP** — click the drop zone, pick 2-3 mixed
      format images via the native dialog. Each row shows source size,
      thumb chip, and pending state.
- [ ] **[I-2] Convert to RGB565 (.bin)** — default format, click 開始
      轉換. Per-row progress bars advance smoothly; on completion each
      row shows `<stem>.bin · WxH · <size>`. Files exist at
      `<source-dir>/OutFile/<stem>.bin`.
- [ ] **[I-3] Convert to .c array** — toggle "C 陣列", convert. Output
      lands at `<source-dir>/OutFile/<stem>.c`. File contains
      `lv_img_dsc_t <name>` descriptor and a sane pixel array.
- [ ] **[I-4] Dither toggle (RGB only)** — toggle 抖色 on, re-convert a
      photo to RGB565. Compare visually against the no-dither version
      — banding should be visibly reduced.
- [ ] **[I-5] Indexed / Alpha formats** — convert one image through
      `Indexed_4bit` and `Alpha_8bit`. Output file sizes match
      `outBytes` formula in `studio-convert.jsx`.
- [ ] **[I-6] Cancel mid-batch** — queue 5 files, hit cancel during the
      second. First file lands as `.bin`, second is partially written
      and overwritten on next run.
- [ ] **[I-7] Resize toggle — known gap** — toggle "縮放至 W×H".
      Currently the Rust command ignores this (output uses source
      dimensions). Confirm the gap until it's wired.

## Video Converter tab (ffmpeg)

> Requires ffmpeg on PATH. If absent, the status chip stays red /
> "未在 PATH 中找到 ffmpeg" — skip this whole section.

- [ ] **[V-1] ffmpeg probe** — opening the tab triggers the check; chip
      reads "ffmpeg 已就緒" within ~1 s. Clicking 重新偵測 ffmpeg
      re-runs the probe.
- [ ] **[V-2] Source pick → output pick chained** — click 選擇影片,
      pick an MP4. Native save dialog opens immediately with the
      suggested default name (`<stem>_240x240.mjpeg`). Accept it.
- [ ] **[V-3] Default convert (MJPEG @ 20fps)** — leave mode = 預設,
      click 開始轉碼. Progress bar advances based on `time=` parsed
      from ffmpeg stderr; phase chip flips from "第 1/2 步" to
      "第 2/2 步". On completion the log reads `✓ 轉碼完成 → <path>`.
      Output file plays back on the HoloCubic.
- [ ] **[V-4] Custom config** — toggle 自訂, set 320×240 @ 15fps,
      format rgb565be, quality 60. Re-run. Output extension is `.rgb`;
      file size ≈ 320 × 240 × 2 × 15 × duration bytes.
- [ ] **[V-5] Cancel mid-encode** — start a long video conversion, hit
      取消轉碼 during step 2. ffmpeg child dies within ~1 s; log shows
      `已取消轉碼，子程序已結束。`; UI returns to idle.

## Cross-cutting

- [ ] **[X-1] No worker thread leaks** — open Task Manager (Windows) or
      `top` (Linux). After running through all five tabs and
      disconnecting, the `aio-studio.exe` process holds steady-state
      thread count (no monotonic growth).
- [ ] **[X-2] Window resize** — drag the window from 1100×700 to
      1920×1200 and back. All tab layouts reflow cleanly; no overlap
      or clipping.
- [ ] **[X-3] Tab switching during ops** — start a video encode, switch
      to other tabs and back. The encode keeps running; the progress
      bar updates the moment you return.
- [ ] **[X-4] No console errors** — open WebView devtools (right-click
      → Inspect on Windows). Run through every tab. The JS console
      should stay empty of red.

## Reporting back

For each failure: paste the `[ID]`, the last few log lines from the
right-hand panel, and (if a Rust panic) the contents of any crash
dialog. Open an issue against the PR or branch.
