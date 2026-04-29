# 05 — AIO_Tool（Python 上位機）

[`AIO_Tool/`](../../AIO_Tool/) 是 Windows 桌面 GUI，給使用者燒韌體、檔案管理、轉圖、轉影片、遠端遙控 cube。CTk (CustomTkinter) + tkinter，包成 PyInstaller `.exe`。

## 1. 環境裝起來

```bash
cd AIO_Tool
pip install uv          # 第一次裝 uv
uv sync --all-groups    # 安裝所有依賴（dev + test + build）
```

`pyproject.toml` pin Python `>=3.11`。實際 dependency：customtkinter、pillow、requests、pyserial、esptool；dev tools 是 ruff、ty、pytest、pyinstaller。

啟動：
```bash
make run
# 或
uv run python CubicAIO_Tool.py
```

## 2. 倉庫結構

```
AIO_Tool/
├── CubicAIO_Tool.py     # 入口 + Engine (mainloop, tab manager)
├── CubicAIO_Tool.spec   # PyInstaller 設定
├── Makefile             # dev / lint / format / typecheck / test / build / run
├── pyproject.toml       # 依賴 + ruff/ty 設定
├── page/                # 7 個 tab 各一個 .py
│   ├── download_debug.py    # 燒錄+除錯+遙控（最大的）
│   ├── setting.py           # 裝置參數設定（透過 0x2323 frame protocol）
│   ├── filemanager.py       # FTP 檔案管理
│   ├── images_converter.py  # 轉換圖片格式
│   ├── videotool.py         # 轉換影片
│   ├── help.py              # 說明
│   └── tool_settings.py     # 工具自身設定（語言切換）
├── util/                # 共用模組
│   ├── i18n.py              # 翻譯
│   ├── massagehead.py       # M_ / A_ 訊息常數
│   ├── common.py            # TOOL_VERSION + helpers
│   ├── logger.py            # 日誌
│   ├── robotsocket.py       # 0x2323 frame 編碼器
│   └── widget_base.py       # 共用 widget (EntryWithPlaceholder)
├── i18n/                # 三個 JSON 翻譯檔
│   ├── en_US.json
│   ├── zh_CN.json
│   └── zh_TW.json
└── tests/               # pytest
    ├── conftest.py
    ├── test_i18n.py
    ├── test_logger.py
    ├── test_massagehead.py
    └── test_robotsocket.py
```

## 3. 入口 — `CubicAIO_Tool.py`

最尾巴的 `__main__` block：

```python
if __name__ == "__main__":
    _ensure_std_streams()    # 修 PyInstaller --noconsole 把 stdout 設成 None 的問題
    setup_logging()
    ctk.set_appearance_mode("Dark")
    tool_windows = ctk.CTk()
    tool_windows.title("HoloCubic_AIO Tools\t  " + TOOL_VERSION)
    tool_windows.geometry("1200x720+10+10")
    engine = Engine(tool_windows)
    tool_windows.mainloop()
```

`Engine.__init__` 開了 `ttk.Notebook`，每個 tab 是一個 page：

```python
self.m_tab_manager = ttk.Notebook(self.root)

self.m_debug_tab = ctk.CTkFrame(self.m_tab_manager, fg_color="transparent")
self.m_tab_manager.add(self.m_debug_tab, text=self.i18n.t("tab_download_debug"))
self.m_debug_tab_windows = DownloadDebug(self.m_debug_tab, self)

# ... Setting / FileManager / ImagesConverter / VideoTool / Helper / ToolSettings
```

每個 page class 接收 `(father, engine)` — `father` 是要 attach 的 frame，`engine` 是回傳到主程式的 reference。

## 4. 加一個按鈕到既有 page — 範例

假設我們要在 DownloadDebug 加一顆「打開 GitHub」的按鈕。

`page/download_debug.py` 找個合適的 `init_*` 方法，加：

```python
self.m_github_btn = ctk.CTkButton(
    btn_frame,
    text=self.i18n.t("open_github"),     # ← i18n key
    command=self._open_github_link,       # ← 點擊 callback
    width=110,
    height=32,
)
self.m_github_btn.pack(side=tk.LEFT, padx=4)

def _open_github_link(self) -> None:
    import webbrowser
    webbrowser.open("https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced")
```

加 i18n key 到 `i18n/en_US.json` / `zh_CN.json` / `zh_TW.json`：
```json
{
  "open_github": "GitHub" / "GitHub" / "GitHub"
}
```

完成。`make run` 看效果。

## 5. CTkButton 兩個容易踩的雷

從 PR #74 學到的：

### 5a. 不要用 `widget["text"]` 讀

```python
# ❌ 在 CTkButton 上會炸 TclError: unknown option "-text"
text = self.m_btn["text"]

# ✅ 用 cget()
text = self.m_btn.cget("text")
```

原因：`tkinter.Misc.__getitem__` 直接呼叫 `self.tk.call(self._w, 'cget', '-' + key)` 走純 Tk，不會走 CTkButton 的 cget override。

### 5b. 不要用 `widget["text"] = X` 寫

```python
# ❌ 看似 work 但其實 silent no-op
self.m_btn["text"] = "新文字"

# ✅ 用 configure()
self.m_btn.configure(text="新文字")
```

原因：`__setitem__` 呼叫 `self.configure({key: value})` 把 dict 當第一個 positional arg 丟進去，但 CTkButton.configure 簽名是 `configure(require_redraw=False, **kwargs)` — dict 被 absorb 進 `require_redraw` 然後 silently 丟掉，text 永遠不會更新。

**rule of thumb**：所有 customtkinter widget 一律 `widget.cget(...)` / `widget.configure(...)`。`ttk.Combobox` 這種真 Tk widget 用 `widget["..."]` 沒問題。

## 6. i18n — 加新翻譯

[`util/i18n.py`](../../AIO_Tool/util/i18n.py) 載入所有 JSON 檔到 singleton：

```python
class I18n:
    TRANSLATIONS = {}  # {"en_US": {...}, "zh_CN": {...}, "zh_TW": {...}}
    _current_language = "zh_CN"

    def t(self, key: str, default: str | None = None) -> str:
        lang_dict = self.TRANSLATIONS.get(self._current_language, {})
        return lang_dict.get(key, default if default is not None else key)
```

加新 key：直接編輯三個 JSON 檔，加同樣的 key。`get_i18n().t("your_key")` 就能取。

語言切換：使用者在 ToolSettings tab 改 → 寫到 `tool_config.json` → 重啟 .exe 生效（或當下廣播 `A_UPDATALANG` 給有 `api()` 的 page 即時翻譯）。

## 7. Inter-tab 訊息 — `massagehead`

[`util/massagehead.py`](../../AIO_Tool/util/massagehead.py) — 跨 tab 用的常數：

```python
M_ALL = "M_ALL"
M_ENGINE = "M_ENGINE"
M_DOWNLOAD_DEBUG = "M_DOWNLOAD_DEBUG"
M_SETTING = "M_SETTING"
# ... 等等
A_CLOSE_UART = "A_CLOSE_UART"
A_OPEN_UART  = "A_OPEN_UART"
A_UPDATALANG = "A_UPDATALANG"
```

從一個 tab 戳另一個 tab：
```python
self.engine.on_thread_message("M_SETTING", mh.M_DOWNLOAD_DEBUG, mh.A_CLOSE_UART)
```

`Engine.on_thread_message` ([`CubicAIO_Tool.py:118`](../../AIO_Tool/CubicAIO_Tool.py#L118)) 是 router：
```python
def on_thread_message(self, fromwho, towho, action, param=None):
    if towho == mh.M_DOWNLOAD_DEBUG:
        self.m_debug_tab_windows.api(action, param)
    elif towho == mh.M_SETTING:
        self.m_setting_tab_windows.api(action, param)
    elif towho == mh.M_ENGINE and action == mh.A_UPDATALANG:
        # 廣播給每個 page 的 api()
        for page in [...]:
            page.api(mh.A_UPDATALANG)
```

每個有 `api()` 的 page class 自己決定怎麼處理。

## 8. 序列埠通訊 — 兩種協定

### 8a. 純 byte 命令（簡單）— PR #77 的遙控

```python
self.ser.write(b"~U\n")  # 韌體 main loop 看到 ~U 就 inject UP IMU action
```

韌體側 (`HoloCubic_AIO.cpp`) 在 main loop 用 `Serial.read()` 收 byte，state machine 解 `~X` pattern。

### 8b. 0x2323 binary frame（complex）— Setting tab 用

[`util/robotsocket.py`](../../AIO_Tool/util/robotsocket.py) 包出來的 binary protocol：
```
| 0x23 0x23 (head) | length(2) | from(2) | to(2) | type(2) | payload | ... |
```

Setting tab 用這個讀寫 cube 上 NVS 的所有 prefs。韌體側在 `app/settings/settings.cpp:185` 解 frame。

新增按鈕用哪個？**簡單動作（按鈕觸發）用 ASCII byte**，**結構化資料（讀寫設定值）用 0x2323 frame**。

## 9. PyInstaller build — 包成 .exe

```bash
make build
# 等同於：uv run pyinstaller CubicAIO_Tool.spec
```

產物：`AIO_Tool/dist/CubicAIO_Tool.exe`（~25 MB）。

`CubicAIO_Tool.spec` 重點：
- `console=False` — windowed app，沒 console（**這就是 PR #74 stdout=None 問題的根源**）
- `datas=[...]` 把 `cubictool.json` / `image/` / `i18n/` / esptool data files 全部 bundle 進 .exe
- `collect_data_files('esptool')` — esptool 的 stub flasher JSON 不會被 PyInstaller 自動 bundle，要手動拉

每改一次原始碼要 rebuild。如果 `dist/CubicAIO_Tool.exe` 已經在跑，先關掉再 build（Windows 的檔案 lock）。

## 10. Test — pytest

```bash
make test
# 或 uv run pytest
```

範本在 [`tests/test_i18n.py`](../../AIO_Tool/tests/test_i18n.py)：

```python
import pytest
from util.i18n import I18n

@pytest.fixture(autouse=True)
def _reset_singleton(monkeypatch, tmp_path):
    """每個 test 隔離 i18n singleton state。"""
    I18n._instance = None
    I18n._current_language = I18n.LANG_ZH_CN
    monkeypatch.setattr(I18n, "_config_file", str(tmp_path / "tool_config.json"))

def test_default_language():
    i = I18n()
    assert i.get_language() == "zh_CN"

def test_set_language_persists_across_calls():
    i = I18n()
    assert i.set_language("en_US") is True
    assert i.get_language() == "en_US"
```

加新 page 的 test：
1. 建 `tests/test_my_page.py`
2. 用 `pytest` mark / fixture 隔離 GUI state
3. 跑 `make test` 確認 38+1 都過

## 11. Lint / Format / Typecheck

```bash
make format     # uv run ruff format .
make lint       # uv run ruff check .
make typecheck  # uv run ty check .
```

CI ([`.github/workflows/aio-tool.yml`](../../.github/workflows/aio-tool.yml)) 會跑這三個 + `pytest`。format / lint / 任一 fail 就擋 PR。`ty` 還在 alpha，不會擋。

## 下一步

- 寫測試 → [06 — 測試完整指南](./06-testing.md)
- 出新版 → [07 — CI + Release](./07-ci-and-release.md)
