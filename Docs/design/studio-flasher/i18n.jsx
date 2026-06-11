// i18n.jsx — lightweight UI translation for the Studio app chrome.
// The base dictionary is GENERATED from the canonical locale files
// AIO_Tool/i18n/{en_US,zh_CN,zh_TW}.json into i18n-generated.js
// (window.__I18N_GENERATED), loaded by index.html *before* this file.
// I18N_SUPPLEMENT below is the ONLY hand-maintained dict: Studio-only strings
// the JSON has no key for, plus a few zh-CN disambiguations. The studio crate's
// tests/i18n_sync.rs keeps both sides honest (regenerate + every-tr()-resolves).
// tr(zhTW) returns the active-language variant; unlisted strings fall back to
// Traditional Chinese. window.__lang ∈ {"tw","cn","en"} (default "tw").
window.__lang = window.__lang || "tw";

// I18N_SUPPLEMENT-START — hand-maintained; see AIO_Tool/studio/tests/i18n_sync.rs
const I18N_SUPPLEMENT = {
  // Studio-only strings absent from the AIO_Tool/i18n JSON files
  "連接": { cn: "连接", en: "Connect" },
  "連線中": { cn: "连接中", en: "Connecting" },
  "主色": { cn: "主色", en: "Accent color" },
  "字體": { cn: "字体", en: "Font" },
  "語言": { cn: "语言", en: "Language" },
  "按鈕、進度、強調元素的顏色": { cn: "按钮、进度、强调元素的颜色", en: "Color for buttons, progress and accents" },
  "介面文字字型": { cn: "界面文字字型", en: "Interface typeface" },
  // Disambiguation pins: these zh-TW strings map to two different zh-CN values
  // across JSON keys, so the generator omits them; pin the variant Studio shows.
  // (圖片 is the Image nav tab, rendered via the dynamic tr(it.label).)
  "圖片": { cn: "图片", en: "Image" },
  "說明": { cn: "说明", en: "Help" },
  "燒錄韌體": { cn: "烧录固件", en: "Flash Firmware" },
  "參數設定": { cn: "参数设定", en: "Device Settings" },
};
// I18N_SUPPLEMENT-END

const I18N_DICT = Object.assign({}, window.__I18N_GENERATED || {}, I18N_SUPPLEMENT);

function tr(s) {
  if (window.__lang === "tw") return s;
  const e = I18N_DICT[s];
  return e && e[window.__lang] ? e[window.__lang] : s;
}

window.tr = tr;
window.I18N_DICT = I18N_DICT;
