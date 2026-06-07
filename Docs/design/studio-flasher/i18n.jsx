// i18n.jsx — lightweight UI translation for the Studio app chrome.
// tr(zhTW) returns the active-language variant; unlisted strings fall back
// to Traditional Chinese. window.__lang ∈ {"tw","cn","en"} (default "tw").
window.__lang = window.__lang || "tw";

const I18N_DICT = {
  // ── nav ──
  "燒錄": { cn: "烧录", en: "Flash" },
  "參數": { cn: "参数", en: "Params" },
  "檔案": { cn: "文件", en: "Files" },
  "圖片": { cn: "图片", en: "Image" },
  "影片": { cn: "影片", en: "Video" },
  "說明": { cn: "说明", en: "Help" },
  "工具": { cn: "工具", en: "Tools" },

  // ── page headers ──
  "燒錄韌體": { cn: "烧录固件", en: "Flash Firmware" },
  "把 AIO 韌體寫入你的 HoloCubic 小電視": { cn: "把 AIO 固件写入你的 HoloCubic 小电视", en: "Write the AIO firmware to your HoloCubic" },
  "參數設定": { cn: "参数设定", en: "Device Settings" },
  "讀取與修改 HoloCubic 的 WiFi、系統、天氣等參數": { cn: "读取与修改 HoloCubic 的 WiFi、系统、天气等参数", en: "Read and edit WiFi, system and weather parameters" },
  "檔案管理": { cn: "文件管理", en: "File Manager" },
  "透過 WiFi 瀏覽 HoloCubic 記憶卡內的檔案": { cn: "通过 WiFi 浏览 HoloCubic 存储卡内的文件", en: "Browse the HoloCubic SD card over WiFi" },
  "圖片轉換": { cn: "图片转换", en: "Image Converter" },
  "把任意圖片轉成 LVGL 影像格式（bin / C 陣列）": { cn: "把任意图片转成 LVGL 图像格式（bin / C 数组）", en: "Convert images to LVGL formats (bin / C array)" },
  "影片轉碼": { cn: "影片转码", en: "Video Converter" },
  "把影片轉成 HoloCubic 可播放的 MJPEG / RGB565 串流": { cn: "把影片转成 HoloCubic 可播放的 MJPEG / RGB565 流", en: "Transcode video to MJPEG / RGB565 streams" },
  "工具設定": { cn: "工具设定", en: "Tool Settings" },
  "調整介面外觀，設定會自動保存": { cn: "调整界面外观，设定会自动保存", en: "Adjust the appearance; changes save automatically" },
  "關於本工具與 HoloCubic 的相關資源": { cn: "关于本工具与 HoloCubic 的相关资源", en: "About this tool and HoloCubic resources" },

  // ── flasher controls ──
  "連接": { cn: "连接", en: "Connect" },
  "連線中…": { cn: "连接中…", en: "Connecting…" },
  "中斷連線": { cn: "断开连接", en: "Disconnect" },
  "重新開機": { cn: "重新启动", en: "Reboot" },
  "開始燒錄": { cn: "开始烧录", en: "Flash" },
  "清空晶片": { cn: "清空芯片", en: "Erase" },
  "取消": { cn: "取消", en: "Cancel" },
  "未連線": { cn: "未连接", en: "Not connected" },
  "已連線": { cn: "已连接", en: "Connected" },
  "燒錄中": { cn: "烧录中", en: "Flashing" },
  "清空中": { cn: "清空中", en: "Erasing" },
  "連線中": { cn: "连接中", en: "Connecting" },
  "遙控": { cn: "遥控", en: "Remote" },
  "操作記錄": { cn: "操作记录", en: "Operation log" },
  "鍵盤方向鍵 / Enter 也可操作": { cn: "键盘方向键 / Enter 也可操作", en: "Arrow keys / Enter also work" },

  // ── settings page ──
  "外觀": { cn: "外观", en: "Appearance" },
  "主色": { cn: "主色", en: "Accent color" },
  "字體": { cn: "字体", en: "Font" },
  "語言": { cn: "语言", en: "Language" },
  "按鈕、進度、強調元素的顏色": { cn: "按钮、进度、强调元素的颜色", en: "Color for buttons, progress and accents" },
  "介面文字字型": { cn: "界面文字字型", en: "Interface typeface" },
  "工具介面的顯示語言": { cn: "工具界面的显示语言", en: "Display language of the tool" },
};

function tr(s) {
  if (window.__lang === "tw") return s;
  const e = I18N_DICT[s];
  return e && e[window.__lang] ? e[window.__lang] : s;
}

window.tr = tr;
window.I18N_DICT = I18N_DICT;
