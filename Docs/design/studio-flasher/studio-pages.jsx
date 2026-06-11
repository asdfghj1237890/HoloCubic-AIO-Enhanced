// studio-pages.jsx — full implementations of 參數設定 (device params) and
// 檔案管理 (SD-card file manager) in the Studio design system.
// Exports: StudioParams, StudioFiles.

Object.assign(ICON, {
  download: ["M12 3v12", "M7 10l5 5 5-5", "M5 21h14"],
  pencil: ["M12 20h9", "M16.5 3.5a2.12 2.12 0 0 1 3 3L7 19l-4 1 1-4z"],
  wifi: ["M2 8.8a16 16 0 0 1 20 0", "M5 12.2a11 11 0 0 1 14 0", "M8.5 15.5a6 6 0 0 1 7 0", "M12 19h.01"],
  img: ["M3 5h18v14H3z", "M8.5 11a1.5 1.5 0 1 0 0-3 1.5 1.5 0 0 0 0 3z", "M21 16l-5-5L6 20"],
  film: ["M4 3h16v18H4z", "M4 8h16M4 16h16M9 3v18M15 3v18"],
  type: ["M4 7V5h16v2", "M9 19h6", "M12 5v14"],
  braces: ["M8 4a2 2 0 0 0-2 2v3a2 2 0 0 1-2 2 2 2 0 0 1 2 2v3a2 2 0 0 0 2 2", "M16 4a2 2 0 0 1 2 2v3a2 2 0 0 0 2 2 2 2 0 0 0-2 2v3a2 2 0 0 1-2 2"],
  doc: ["M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z", "M14 2v6h6", "M9 14h6M9 17h4"],
  chevR: "M9 6l6 6-6 6",
  folderOpen: ["M3 8a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2H3z", "M3 10h19l-2.2 8.3a1 1 0 0 1-1 .7H4.5a1 1 0 0 1-1-.8z"],
  eye: ["M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7-10-7-10-7z", "M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6z"],
});

// ── tauri bridge detection ──────────────────────────────────────────────────
const IS_TAURI_PAGES = typeof window !== "undefined" && !!window.__TAURI__;
const invokePages = IS_TAURI_PAGES ? window.__TAURI__.core.invoke : null;
const listenPages = IS_TAURI_PAGES ? window.__TAURI__.event.listen : null;

// ── shared bits ─────────────────────────────────────────────────────────────
function Switch({ on, onToggle, disabled }) {
  return (
    <button onClick={() => !disabled && onToggle(!on)} disabled={disabled}
      style={{ width: 42, height: 24, borderRadius: 999, border: "none", cursor: disabled ? "not-allowed" : "pointer",
               background: on ? "var(--accent)" : "var(--panel-3)", position: "relative", transition: "background .16s", opacity: disabled ? 0.5 : 1 }}>
      <span style={{ position: "absolute", top: 3, left: on ? 21 : 3, width: 18, height: 18, borderRadius: "50%",
                     background: "#fff", transition: "left .16s", boxShadow: "0 1px 3px rgba(0,0,0,.3)" }} />
    </button>
  );
}

function PageHeader({ title, sub, right }) {
  return (
    <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between",
                  padding: "var(--s5) var(--s6)", borderBottom: "1px solid var(--border)", flex: "none" }}>
      <div>
        <div className="disp" style={{ fontSize: 21, fontWeight: 600, letterSpacing: "-.02em", whiteSpace: "nowrap" }}>{title}</div>
        <div style={{ fontSize: 13, color: "var(--text-mute)", whiteSpace: "nowrap" }}>{sub}</div>
      </div>
      {right}
    </div>
  );
}

function StatusChip({ conn, port }) {
  const live = conn === "connected";
  return (
    <span className="chip" style={{ fontSize: 12.5, padding: "6px 14px" }}>
      <span className={"dot " + (live ? "live" : conn === "connecting" ? "busy" : "")} />
      {conn === "connecting" ? tr("連線中") : live ? (port ? trf("已連線 · {port}", { port }) : tr("已連線")) : tr("未連線")}
    </span>
  );
}

// ════════════════════════════════════════════════════════════════════════════
//  參數設定 — device parameters via HTTP /api/settings + /save<Cat>Conf
//
//  Talks to the firmware's web flow (the same one the browser settings pages
//  use). Schema is firmware-driven: whatever categories /api/settings emits
//  get rendered. Replaces the broken serial SettingMsg protocol (B15).
// ════════════════════════════════════════════════════════════════════════════

// Friendly metadata per firmware-emitted field. Anything not listed falls
// back to plain text input with the raw key as label. Categories
// (sys/rgb/mpu/...) map to nice section labels + icons.
const FIRMWARE_FIELD_META = {
  ssid_0:               { label: "SSID（自動連線）", type: "text",     ph: "預設 WiFi 名稱" },
  password_0:           { label: "密碼（自動連線）", type: "password", ph: "預設 WiFi 密碼" },
  ssid_1:               { label: "SSID 1",           type: "text",     ph: "備用 WiFi 名稱" },
  password_1:           { label: "密碼 1",           type: "password" },
  ssid_2:               { label: "SSID 2",           type: "text",     ph: "備用 WiFi 名稱" },
  password_2:           { label: "密碼 2",           type: "password" },
  auto_start_app:       { label: "開機自啟 App",     type: "text",     ph: "Heartbeat / Weather / …" },
  power_mode:           { label: "功耗模式",          type: "select",
                          options: [["0","節能"],["1","效能"]] },
  backLight:            { label: "背光亮度",          type: "slider",   min: 1, max: 100 },
  rotation:             { label: "螢幕旋轉",          type: "select",
                          options: [["0","0°"],["1","90°"],["2","180°"],["3","270°"]] },
  auto_calibration_mpu: { label: "自動校準 MPU",     type: "toggle" },
  mpu_order:            { label: "操作方向",          type: "text",     ph: "0-7" },
  // RGB knobs — sliders so the live edit feels tangible
  mode:                 { label: "模式",             type: "text" },
  min_value_0:          { label: "Min R",            type: "slider", min: 0, max: 255 },
  min_value_1:          { label: "Min G",            type: "slider", min: 0, max: 255 },
  min_value_2:          { label: "Min B",            type: "slider", min: 0, max: 255 },
  max_value_0:          { label: "Max R",            type: "slider", min: 0, max: 255 },
  max_value_1:          { label: "Max G",            type: "slider", min: 0, max: 255 },
  max_value_2:          { label: "Max B",            type: "slider", min: 0, max: 255 },
  step_0:               { label: "Step R",           type: "text" },
  step_1:               { label: "Step G",           type: "text" },
  step_2:               { label: "Step B",           type: "text" },
  min_brightness:       { label: "Min 亮度",         type: "slider", min: 0, max: 1000 },
  max_brightness:       { label: "Max 亮度",         type: "slider", min: 0, max: 1000 },
  brightness_step:      { label: "亮度步進",         type: "slider", min: 0, max: 100 },
  time:                 { label: "動畫週期 (ms)",    type: "slider", min: 10, max: 1000 },
  // MPU offsets — informational sliders
  x_gyro_offset:        { label: "X gyro offset",    type: "text" },
  y_gyro_offset:        { label: "Y gyro offset",    type: "text" },
  z_gyro_offset:        { label: "Z gyro offset",    type: "text" },
  x_accel_offset:       { label: "X accel offset",   type: "text" },
  y_accel_offset:       { label: "Y accel offset",   type: "text" },
  z_accel_offset:       { label: "Z accel offset",   type: "text" },
};
const CATEGORY_META = {
  sys: { label: "系統 / WiFi", icon: "wifi", saveable: true },
  rgb: { label: "RGB LED",     icon: "img",  saveable: true },
  mpu: { label: "IMU 校準",    icon: "chip", saveable: false },  // no /saveMpuConf — read-only
};

// Default device host stored on window so re-entering the tab keeps the IP.
const DEFAULT_HOST = "192.168.4.1";

function flattenSnapshot(json) {
  // [{cat: "sys", key: "ssid_0", value: "...", meta}, ...]
  const out = [];
  if (!json || typeof json !== "object") return out;
  for (const [cat, fields] of Object.entries(json)) {
    if (!fields || typeof fields !== "object") continue;
    for (const [key, value] of Object.entries(fields)) {
      const meta = FIRMWARE_FIELD_META[key] || { label: key, type: "text" };
      out.push({ cat, key, value: String(value ?? ""), meta });
    }
  }
  return out;
}

function useSettings() {
  const { useState, useEffect, useCallback } = React;
  const [host, setHost]       = useState(window.__AIO_SETTINGS_HOST__ || DEFAULT_HOST);
  const [pw, setPw]           = useState(window.__AIO_SETTINGS_PW__ || "");   // device web password (shown on its screen)
  const [rows, setRows]       = useState([]);          // flattened from snapshot
  const [edits, setEdits]     = useState({});          // {"cat:key": newValue}
  const [status, setStatus]   = useState(() => tr("輸入裝置 IP 後按「讀取設定」開始。"));
  const [busy, setBusy]       = useState(false);
  const [loaded, setLoaded]   = useState(false);

  // Persist host across tab switches.
  useEffect(() => { window.__AIO_SETTINGS_HOST__ = host; }, [host]);
  useEffect(() => { window.__AIO_SETTINGS_PW__ = pw; }, [pw]);

  const fetchAll = useCallback(async () => {
    const ip = host.trim();
    if (!ip) { setStatus(tr("✗ 請先輸入裝置 IP。")); return; }
    setBusy(true); setStatus(trf("從 {ip} 讀取中…", { ip }));
    if (IS_TAURI_PAGES) {
      try {
        const { json } = await invokePages("fetch_settings_http", { host: ip, password: pw });
        const flat = flattenSnapshot(json);
        setRows(flat); setEdits({}); setLoaded(true);
        setStatus(trf("已讀取 {n} 項參數，跨 {c} 個分類。", { n: flat.length, c: Object.keys(json).length }));
      } catch (e) {
        setStatus(trf("✗ 讀取失敗：{e}", { e: e && e.toString ? e.toString() : e }));
      }
    } else {
      // Browser-preview mock — emits a sample shape matching the firmware.
      const mock = {
        sys: { ssid_0: "Holo_Home_2.4G", password_0: "home12345",
               ssid_1: "Holo_Backup", password_1: "", ssid_2: "", password_2: "",
               auto_start_app: "Heartbeat", power_mode: 1, backLight: 80,
               rotation: 1, auto_calibration_mpu: 1, mpu_order: 0 },
        rgb: { mode: 0, min_value_0: 0, min_value_1: 0, min_value_2: 0,
               max_value_0: 255, max_value_1: 255, max_value_2: 255,
               step_0: 1, step_1: 1, step_2: 1, min_brightness: 100,
               max_brightness: 800, brightness_step: 5, time: 50 },
        mpu: { x_gyro_offset: 0, y_gyro_offset: 0, z_gyro_offset: 0,
               x_accel_offset: 0, y_accel_offset: 0, z_accel_offset: 0 },
      };
      setRows(flattenSnapshot(mock)); setEdits({}); setLoaded(true);
      setStatus("(瀏覽器預覽模式) 已載入 mock 資料。");
    }
    setBusy(false);
  }, [host, pw]);

  const setField = (cat, key, value) =>
    setEdits((prev) => ({ ...prev, [cat + ":" + key]: value }));

  const dirtyKeys = Object.keys(edits);
  const dirtyByCategory = () => {
    const out = {};
    for (const k of dirtyKeys) {
      const ix = k.indexOf(":");
      const cat = k.slice(0, ix), field = k.slice(ix + 1);
      const meta = CATEGORY_META[cat];
      if (!meta || !meta.saveable) continue;   // skip mpu (no save handler)
      (out[cat] ||= []).push({ key: field, value: edits[k] });
    }
    return out;
  };

  const save = useCallback(async () => {
    if (!dirtyKeys.length) return;
    const ip = host.trim();
    const groups = dirtyByCategory();
    if (!Object.keys(groups).length) {
      setStatus("⚠ 已修改的欄位都在唯讀分類(例如 IMU);無 /save handler。");
      return;
    }
    setBusy(true); setStatus(tr("寫入中…"));
    if (IS_TAURI_PAGES) {
      try {
        let total = 0;
        for (const [cat, fields] of Object.entries(groups)) {
          await invokePages("save_settings_http", { host: ip, category: cat, fields, password: pw });
          total += fields.length;
        }
        setStatus(trf("已寫入 {n} 項修改，重新讀取以確認…", { n: total }));
        await fetchAll();
      } catch (e) {
        setStatus(trf("✗ 寫入失敗：{e}", { e: e && e.toString ? e.toString() : e }));
      }
    } else {
      const total = Object.values(groups).reduce((a, fs) => a + fs.length, 0);
      setStatus("(瀏覽器預覽模式) 模擬寫入 " + total + " 項。");
      setEdits({});
    }
    setBusy(false);
  }, [host, pw, edits, fetchAll, dirtyKeys.length]);

  // Group rows by category for rendering.
  const byCategory = () => {
    const out = [];
    const seen = new Map();
    for (const row of rows) {
      if (!seen.has(row.cat)) {
        const m = CATEGORY_META[row.cat] || { label: row.cat, icon: "braces", saveable: false };
        const group = { cat: row.cat, label: m.label, icon: m.icon, saveable: m.saveable, fields: [] };
        seen.set(row.cat, group); out.push(group);
      }
      seen.get(row.cat).fields.push(row);
    }
    return out;
  };

  return {
    host, setHost, pw, setPw, rows, edits, status, busy, loaded,
    dirtyCount: dirtyKeys.length, byCategory,
    fetchAll, setField, save,
  };
}

function ParamField({ f, value, changed, disabled, onChange }) {
  return (
    <div style={{ display: "grid", gridTemplateColumns: "150px 1fr", alignItems: "center", gap: "var(--s4)",
                  padding: "var(--s3) 0", borderTop: "1px solid var(--border)" }}>
      <div style={{ display: "flex", alignItems: "center", gap: "var(--s2)" }}>
        <span style={{ fontSize: 13, color: disabled ? "var(--text-mute)" : "var(--text-dim)", whiteSpace: "nowrap" }}>{tr(f.label)}</span>
        {changed && <span title={tr("已修改，尚未寫入")} style={{ width: 6, height: 6, borderRadius: "50%", background: "var(--warn)", flex: "none" }} />}
      </div>
      <div>
        {f.type === "slider" ? (
          <div style={{ display: "flex", alignItems: "center", gap: "var(--s3)" }}>
            <input type="range" min={f.min} max={f.max} value={value || 0} disabled={disabled}
              onChange={(e) => onChange(e.target.value)} style={{ flex: 1, accentColor: "var(--accent)" }} />
            <span className="mono" style={{ fontSize: 12, color: "var(--text-dim)", width: 34, textAlign: "right" }}>{value || 0}</span>
          </div>
        ) : f.type === "toggle" ? (
          <Switch on={value === "1"} disabled={disabled} onToggle={(v) => onChange(v ? "1" : "0")} />
        ) : f.type === "select" ? (
          <select className="fld" style={{ width: 160 }} value={value} disabled={disabled} onChange={(e) => onChange(e.target.value)}>
            {f.options.map(([v, l]) => <option key={v} value={v}>{tr(l)}</option>)}
          </select>
        ) : (
          <input className="fld" type={f.type} placeholder={f.ph ? tr(f.ph) : undefined} value={value} disabled={disabled}
            onChange={(e) => onChange(e.target.value)} style={{ maxWidth: 320 }} />
        )}
      </div>
    </div>
  );
}

function StudioParams() {
  const s = useSettings();
  const groups = s.byCategory();
  const enabled = s.loaded;
  return (
    <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
      <PageHeader title={tr("參數設定")} sub={tr("讀取與修改 HoloCubic 的 WiFi、系統、天氣等參數")}
        right={
          <span className="chip" style={{ fontSize: 12.5, padding: "6px 14px" }}>
            <span className={"dot " + (s.busy ? "busy" : s.loaded ? "live" : "")} />
            {s.busy ? tr("處理中…") : s.loaded ? tr("已載入") : tr("待讀取")}
          </span>
        } />

      {/* toolbar — IP input + read + write */}
      <div style={{ display: "flex", alignItems: "center", gap: "var(--s2)", padding: "var(--s3) var(--s6)",
                    borderBottom: "1px solid var(--border)", flexWrap: "wrap", flex: "none" }}>
        <span style={{ fontSize: 13, color: "var(--text-mute)" }}>{tr("裝置 IP")}</span>
        <input className="fld mono" style={{ width: 180, height: 36 }} placeholder="192.168.x.x"
          value={s.host} onChange={(e) => s.setHost(e.target.value)} disabled={s.busy} />
        <span style={{ fontSize: 13, color: "var(--text-mute)" }}>{tr("密碼")}</span>
        <input className="fld mono" style={{ width: 120, height: 36 }} type="password" placeholder={tr("裝置螢幕顯示")}
          value={s.pw} onChange={(e) => s.setPw(e.target.value)} disabled={s.busy} />
        <button className="btn ghost" style={{ height: 36 }} disabled={s.busy} onClick={s.fetchAll}>
          <Icon d={ICON.download} size={15} />{tr("讀取設定")}
        </button>
        <div style={{ flex: 1 }} />
        <button className="btn primary" style={{ height: 36 }} disabled={s.busy || !s.dirtyCount} onClick={s.save}>
          <Icon d={ICON.check} size={15} />{tr("寫入修改")}{s.dirtyCount ? ` (${s.dirtyCount})` : ""}
        </button>
      </div>

      <div className="scroll" style={{ overflow: "auto", padding: "var(--s5) var(--s6)" }}>
        <div style={{ maxWidth: 760 }}>
          <div style={{ display: "flex", alignItems: "center", gap: "var(--s2)", fontSize: 12.5, color: enabled ? "var(--text-dim)" : "var(--text-mute)", marginBottom: "var(--s4)" }}>
            <span className="dot" style={{ background: enabled ? "var(--accent)" : "var(--text-mute)" }} />{s.status}
          </div>
          <div style={{ display: "grid", gap: "var(--s5)" }}>
            {groups.map((g) => (
              <div key={g.cat} style={{ background: "var(--panel)", border: "1px solid var(--border)", borderRadius: "var(--r4)", overflow: "hidden" }}>
                <div style={{ display: "flex", alignItems: "center", gap: "var(--s2)", padding: "var(--s3) var(--s4)", color: "var(--text-dim)" }}>
                  <Icon d={ICON[g.icon] || ICON.braces} size={15} />
                  <span style={{ fontSize: 12, fontWeight: 700, letterSpacing: ".05em", whiteSpace: "nowrap" }}>{tr(g.label)}</span>
                  {!g.saveable && <span style={{ fontSize: 11, color: "var(--text-mute)" }}>{tr("(唯讀)")}</span>}
                </div>
                <div style={{ padding: "0 var(--s4) var(--s2)" }}>
                  {g.fields.map((row) => {
                    const eKey = row.cat + ":" + row.key;
                    const current = eKey in s.edits ? s.edits[eKey] : row.value;
                    const changed = eKey in s.edits;
                    return (
                      <ParamField key={eKey} f={row.meta} value={current}
                        changed={changed} disabled={!g.saveable}
                        onChange={(v) => s.setField(row.cat, row.key, v)} />
                    );
                  })}
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}

// ════════════════════════════════════════════════════════════════════════════
//  檔案管理 — SD-card browser over WiFi (TCP), with file actions
// ════════════════════════════════════════════════════════════════════════════
const SD_FS = {
  "/": [
    { name: "image", type: "dir" },
    { name: "movie", type: "dir" },
    { name: "font", type: "dir" },
    { name: "config", type: "dir" },
    { name: "setting.cfg", type: "file", size: 256, mtime: "2024-11-02 14:21" },
    { name: "README.txt", type: "file", size: 1180, mtime: "2024-09-18 09:03" },
  ],
  "/image": [
    { name: "butterfly_1.jpg", type: "file", size: 15068, mtime: "2024-10-21 20:11" },
    { name: "butterfly_2.jpg", type: "file", size: 12514, mtime: "2024-10-21 20:11" },
    { name: "wallpaper.bin", type: "file", size: 115200, mtime: "2024-10-22 11:47" },
  ],
  "/movie": [
    { name: "BadApple.mjpeg", type: "file", size: 4404019, mtime: "2024-08-30 22:40" },
    { name: "demo_565.rgb", type: "file", size: 1887436, mtime: "2024-08-30 22:55" },
  ],
  "/font": [
    { name: "ageo_18.vlw", type: "file", size: 98304, mtime: "2024-07-12 08:00" },
    { name: "ageo_28.vlw", type: "file", size: 184320, mtime: "2024-07-12 08:00" },
  ],
  "/config": [
    { name: "wifi.json", type: "file", size: 128, mtime: "2024-11-02 14:20" },
    { name: "weather.json", type: "file", size: 96, mtime: "2024-11-02 14:20" },
  ],
};
function fileMeta(name) {
  const ext = (name.split(".").pop() || "").toLowerCase();
  if (["jpg", "jpeg", "png", "bmp"].includes(ext)) return { icon: "img", color: "var(--ok)", kind: "影像" };
  if (["mjpeg", "rgb", "mp4", "avi"].includes(ext)) return { icon: "film", color: "#a78bfa", kind: "影片" };
  if (ext === "vlw") return { icon: "type", color: "var(--accent-2)", kind: "字型" };
  if (["json", "cfg"].includes(ext)) return { icon: "braces", color: "var(--warn)", kind: "設定檔" };
  if (ext === "bin") return { icon: "chip", color: "var(--text-dim)", kind: "二進位" };
  return { icon: "doc", color: "var(--text-mute)", kind: "文字" };
}
// NOTE: fileMeta().kind returns raw zh-TW strings; call tr(kind) at render time.

// Absolute path of a child given the current path. Avoids the
// `path === "/"` edge case repeating throughout the hook.
const joinPath = (cur, name) => (cur === "/" ? "/" + name : cur + "/" + name);

function useFiles() {
  const { useState, useEffect } = React;
  const [conn, setConn] = useState("disconnected");
  const [ip, setIp] = useState("192.168.0.165");
  const [port, setPort] = useState("6677");
  const [fs, setFs] = useState(() => JSON.parse(JSON.stringify(SD_FS)));
  const [path, setPath] = useState("/");
  const [sel, setSel] = useState(null);
  const [acts, setActs] = useState([]);

  const log = (text) => setActs((a) => [{ text, t: new Date().toLocaleTimeString("zh-TW", { hour12: false }) }, ...a].slice(0, 6));

  // In Tauri mode, all device state arrives via `fm:event`. Subscribe
  // once and dispatch by `kind`.
  useEffect(() => {
    if (!IS_TAURI_PAGES || !listenPages) return;
    let unlisten = null;
    listenPages("fm:event", ({ payload }) => {
      if (!payload || !payload.kind) return;
      switch (payload.kind) {
        case "connected":
          setConn("connected"); log(`已連線 ${ip}:${port}`);
          break;
        case "dir-listed": {
          const names = payload.entries || [];
          // Firmware returns a flat name list; infer file vs dir by the
          // same "dot in name" heuristic the egui tool uses (FsNode).
          const items = names.map((n) => ({
            name: n,
            type: n.includes(".") ? "file" : "dir",
          }));
          setFs((F) => ({ ...F, [payload.path]: items }));
          break;
        }
        case "file-bytes": {
          const b64 = payload.bytes_b64 || "";
          try {
            const bin = atob(b64);
            const arr = new Uint8Array(bin.length);
            for (let i = 0; i < bin.length; i++) arr[i] = bin.charCodeAt(i);
            const filename = (payload.path || "file.bin").split("/").pop();
            const blob = new Blob([arr], { type: "application/octet-stream" });
            const url = URL.createObjectURL(blob);
            const a = document.createElement("a");
            a.href = url; a.download = filename;
            document.body.appendChild(a); a.click();
            setTimeout(() => { URL.revokeObjectURL(url); a.remove(); }, 200);
            log(`已下載 ${filename}（${arr.length} bytes）`);
          } catch (e) { log("✗ 下載失敗: " + e); }
          break;
        }
        case "properties":
          log(`屬性 ${payload.path}: ${(payload.raw_b64 || "").length} bytes (b64)`);
          break;
        case "log":
          if (payload.message) log(payload.message);
          break;
        case "warning":
          if (payload.message) log("⚠ " + payload.message);
          break;
        case "finished":
          setConn("disconnected");
          log(payload.ok ? "已中斷連線" : "✗ " + (payload.error || "連線錯誤"));
          break;
      }
    }).then((fn) => { unlisten = fn; });
    return () => { if (unlisten) unlisten(); };
  }, [ip, port]);

  const connect = async () => {
    setConn("connecting");
    if (IS_TAURI_PAGES) {
      try {
        await invokePages("fm_connect", { ip, port });
        await invokePages("fm_list_dir", { path: "/" });
        setPath("/");
      } catch (e) {
        setConn("disconnected");
        log("✗ " + (e && e.toString ? e.toString() : e));
      }
      return;
    }
    setTimeout(() => { setConn("connected"); setPath("/"); log(`已連線 ${ip}:${port}`); }, 800);
  };

  const disconnect = async () => {
    if (IS_TAURI_PAGES) { try { await invokePages("fm_disconnect"); } catch (_) {} }
    setConn("disconnected"); setSel(null); log("已中斷連線");
  };

  const entries = (fs[path] || []).slice().sort((a, b) => (a.type === b.type ? a.name.localeCompare(b.name) : a.type === "dir" ? -1 : 1));

  const enter = (name) => {
    const newPath = joinPath(path, name);
    setPath(newPath); setSel(null);
    if (IS_TAURI_PAGES) { invokePages("fm_list_dir", { path: newPath }).catch(() => {}); }
  };
  const goPath = (p) => {
    setPath(p); setSel(null);
    if (IS_TAURI_PAGES) { invokePages("fm_list_dir", { path: p }).catch(() => {}); }
  };

  const remove = (e) => {
    if (IS_TAURI_PAGES) {
      const abs = joinPath(path, e.name);
      invokePages("fm_remove", { name: abs }).catch((err) => log("✗ " + err));
      setFs((F) => ({ ...F, [path]: (F[path] || []).filter((x) => x.name !== e.name) }));
      setSel(null); log(`已刪除 ${e.name}`);
      // Firmware sends no response — re-request the listing after a
      // short delay so we surface any divergence (e.g. delete denied).
      setTimeout(() => invokePages("fm_list_dir", { path }).catch(() => {}), 400);
      return;
    }
    setFs((F) => ({ ...F, [path]: F[path].filter((x) => x.name !== e.name) }));
    setSel(null); log(`已刪除 ${e.name}`);
  };

  const rename = (e, newName) => {
    if (!newName || newName === e.name) return;
    if (IS_TAURI_PAGES) {
      // Preserved-bug B1: the wire frame carries old=new=name, so the
      // firmware can't actually rename. Send the call for parity with
      // the legacy tool and surface the limitation in the log.
      const abs = joinPath(path, e.name);
      invokePages("fm_rename", { name: abs }).catch((err) => log("✗ " + err));
      log(`⚠ 重新命名 ${e.name} → ${newName}（韌體 B1 未實作）`);
      return;
    }
    setFs((F) => ({ ...F, [path]: F[path].map((x) => (x.name === e.name ? { ...x, name: newName } : x)) }));
    setSel((s) => (s && s.name === e.name ? { ...s, name: newName } : s));
    log(`已重新命名 ${e.name} → ${newName}`);
  };

  const download = (e) => {
    if (IS_TAURI_PAGES) {
      const abs = joinPath(path, e.name);
      log(`下載 ${abs}…`);
      invokePages("fm_read_file", { path: abs }).catch((err) => log("✗ " + err));
      return;
    }
    log(`下載 ${path === "/" ? "" : path}/${e.name} → 本機`);
  };

  const newFolder = () => {
    if (IS_TAURI_PAGES) {
      log("⚠ 新增資料夾尚未支援（DirCreate 待補）");
      return "";
    }
    const existing = new Set((fs[path] || []).map((x) => x.name));
    let name = "新增資料夾", i = 2;
    while (existing.has(name)) name = `新增資料夾 ${i++}`;
    const childPath = joinPath(path, name);
    setFs((F) => ({ ...F, [path]: [...F[path], { name, type: "dir" }], [childPath]: [] }));
    log(`已建立資料夾 ${name}`);
    return name;
  };

  const upload = () => {
    if (IS_TAURI_PAGES) {
      log("⚠ 上傳尚未支援（FileWrite 待補）");
      return;
    }
    const n = (fs[path] || []).filter((x) => /upload_/.test(x.name)).length + 1;
    const name = `upload_${String(n).padStart(2, "0")}.bin`;
    const ent = { name, type: "file", size: 4096 + Math.floor(Math.random() * 120000), mtime: new Date().toLocaleString("zh-TW", { hour12: false }).slice(0, 16) };
    setFs((F) => ({ ...F, [path]: [...F[path], ent] }));
    setSel(ent); log(`已上傳 ${name}（${fmtBytes(ent.size)}）`);
  };

  return { conn, ip, setIp, port, setPort, path, entries, sel, setSel, acts, connect, disconnect, enter, goPath, remove, rename, download, newFolder, upload };
}

function StudioFiles() {
  const { useState } = React;
  const f = useFiles();
  const connected = f.conn === "connected";
  const [editing, setEditing] = useState(null);
  const [draft, setDraft] = useState("");
  const [menu, setMenu] = useState(null); // {x,y,entry}

  const crumbs = [tr("根目錄"), ...f.path.split("/").filter(Boolean)];
  const crumbPath = (i) => "/" + f.path.split("/").filter(Boolean).slice(0, i).join("/");

  const startRename = (e) => { setEditing(e.name); setDraft(e.name); setMenu(null); };
  const commitRename = (e) => { f.rename(e, draft.trim()); setEditing(null); };

  React.useEffect(() => {
    const close = () => setMenu(null);
    window.addEventListener("click", close);
    return () => window.removeEventListener("click", close);
  }, []);

  return (
    <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }} onClick={() => setMenu(null)}>
      <PageHeader title={tr("檔案管理")} sub={tr("透過 WiFi 瀏覽 HoloCubic 記憶卡內的檔案")}
        right={<StatusChip conn={f.conn} port={connected ? f.ip + ":" + f.port : null} />} />

      {/* connection bar */}
      <div style={{ display: "flex", alignItems: "center", gap: "var(--s2)", padding: "var(--s3) var(--s6)",
                    borderBottom: "1px solid var(--border)", flex: "none" }}>
        <Icon d={ICON.wifi} size={16} />
        <input className="fld mono" style={{ width: 150, height: 36 }} value={f.ip} onChange={(e) => f.setIp(e.target.value)} disabled={connected} placeholder={tr("IP 位址")} />
        <span style={{ color: "var(--text-mute)" }}>:</span>
        <input className="fld mono" style={{ width: 80, height: 36 }} value={f.port} onChange={(e) => f.setPort(e.target.value)} disabled={connected} placeholder={tr("埠")} />
        {connected
          ? <button className="btn" style={{ height: 36 }} onClick={f.disconnect}>{tr("中斷")}</button>
          : <button className="btn primary" style={{ height: 36 }} onClick={f.connect}>{f.conn === "connecting" ? tr("連線中…") : tr("連線")}</button>}
      </div>

      {!connected ? (
        <div style={{ flex: 1, display: "grid", placeItems: "center" }}>
          <div style={{ textAlign: "center", color: "var(--text-mute)" }}>
            <Icon d={ICON.folderOpen} size={30} />
            <div style={{ fontSize: 13.5, marginTop: "var(--s3)" }}>{tr("輸入 HoloCubic 的 IP 與連接埠後按「連線」")}<br />{tr("即可瀏覽記憶卡檔案")}</div>
          </div>
        </div>
      ) : (
        <div style={{ flex: 1, display: "grid", gridTemplateColumns: "1fr 320px", minHeight: 0 }}>
          {/* file list */}
          <div style={{ display: "flex", flexDirection: "column", minHeight: 0 }}>
            {/* breadcrumb + folder ops */}
            <div style={{ display: "flex", alignItems: "center", gap: 4, padding: "var(--s3) var(--s6)", borderBottom: "1px solid var(--border)", fontSize: 13, flexWrap: "wrap" }}>
              {crumbs.map((c, i) => (
                <React.Fragment key={i}>
                  {i > 0 && <Icon d={ICON.chevR} size={13} stroke={2} />}
                  <button onClick={() => f.goPath(i === 0 ? "/" : crumbPath(i))} style={{ border: "none", background: "transparent", cursor: "pointer",
                    color: i === crumbs.length - 1 ? "var(--text)" : "var(--text-mute)", fontWeight: i === crumbs.length - 1 ? 600 : 500, fontSize: 13, padding: "2px 4px" }}>{c}</button>
                </React.Fragment>
              ))}
              <div style={{ flex: 1 }} />
              <button className="btn ghost" style={{ height: 30, fontSize: 12 }} onClick={f.upload}><Icon d={ICON.upload} size={13} />{tr("上傳檔案")}</button>
              <button className="btn ghost" style={{ height: 30, fontSize: 12 }} onClick={f.newFolder}><Icon d={ICON.plus || ICON.folderOpen} size={13} />{tr("新增資料夾")}</button>
            </div>
            <div className="scroll" style={{ overflow: "auto", padding: "var(--s3) var(--s4)" }}>
              {f.entries.map((e) => {
                const isDir = e.type === "dir";
                const meta = isDir ? { icon: "folder", color: "var(--accent)" } : fileMeta(e.name);
                const on = f.sel && f.sel.name === e.name;
                return (
                  <div key={e.name} onClick={() => f.setSel(e)} onDoubleClick={() => isDir && f.enter(e.name)}
                    onContextMenu={(ev) => { ev.preventDefault(); f.setSel(e); setMenu({ x: ev.clientX, y: ev.clientY, entry: e }); }}
                    style={{ display: "flex", alignItems: "center", gap: "var(--s3)", padding: "8px var(--s3)", borderRadius: "var(--r2)",
                             cursor: "pointer", background: on ? "var(--accent-weak)" : "transparent", transition: "background .1s" }}
                    onMouseEnter={(ev) => { if (!on) ev.currentTarget.style.background = "var(--panel-2)"; }}
                    onMouseLeave={(ev) => { if (!on) ev.currentTarget.style.background = "transparent"; }}>
                    <span style={{ color: meta.color, flex: "none", display: "grid", placeItems: "center" }}><Icon d={ICON[meta.icon]} size={18} fill={false} /></span>
                    {editing === e.name ? (
                      <input className="fld mono" autoFocus value={draft} onChange={(ev) => setDraft(ev.target.value)} onClick={(ev) => ev.stopPropagation()}
                        onKeyDown={(ev) => { if (ev.key === "Enter") commitRename(e); if (ev.key === "Escape") setEditing(null); }}
                        onBlur={() => commitRename(e)} style={{ flex: 1, height: 28, padding: "2px 8px" }} />
                    ) : (
                      <span style={{ flex: 1, fontSize: 13, color: "var(--text)", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>{e.name}</span>
                    )}
                    <span className="mono" style={{ fontSize: 11.5, color: "var(--text-mute)", flex: "none" }}>{isDir ? tr("資料夾") : fmtBytes(e.size)}</span>
                    {isDir && <Icon d={ICON.chevR} size={15} stroke={2} />}
                  </div>
                );
              })}
              {f.entries.length === 0 && <div style={{ textAlign: "center", color: "var(--text-mute)", fontSize: 13, padding: "var(--s6)" }}>{tr("空的資料夾")}</div>}
            </div>
          </div>

          {/* details panel */}
          <div style={{ borderLeft: "1px solid var(--border)", background: "var(--panel)", display: "flex", flexDirection: "column", minHeight: 0 }}>
            {f.sel ? (
              <div style={{ padding: "var(--s5)", flex: 1, overflow: "auto" }} className="scroll">
                <div style={{ display: "grid", placeItems: "center", marginBottom: "var(--s4)" }}>
                  {(() => { const m = f.sel.type === "dir" ? { icon: "folderOpen", color: "var(--accent)" } : fileMeta(f.sel.name);
                    return <div style={{ width: 64, height: 64, borderRadius: "var(--r3)", background: "var(--panel-2)", border: "1px solid var(--border)", display: "grid", placeItems: "center", color: m.color }}><Icon d={ICON[m.icon]} size={28} /></div>; })()}
                </div>
                <div style={{ textAlign: "center", fontWeight: 600, fontSize: 14, wordBreak: "break-all", marginBottom: "var(--s4)" }}>{f.sel.name}</div>
                <div style={{ display: "grid", gridTemplateColumns: "auto 1fr", gap: "6px var(--s3)", fontSize: 12.5, marginBottom: "var(--s5)" }}>
                  {[[tr("類型"), f.sel.type === "dir" ? tr("資料夾") : tr(fileMeta(f.sel.name).kind)],
                    [tr("大小"), f.sel.type === "dir" ? "—" : fmtBytes(f.sel.size)],
                    [tr("路徑"), (f.path === "/" ? "" : f.path) + "/" + f.sel.name],
                    [tr("修改時間"), f.sel.mtime || "—"]].map(([k, v]) => (
                    <React.Fragment key={k}>
                      <span style={{ color: "var(--text-mute)" }}>{k}</span>
                      <span className="mono" style={{ color: "var(--text-dim)", textAlign: "right", wordBreak: "break-all" }}>{v}</span>
                    </React.Fragment>
                  ))}
                </div>
                <div style={{ display: "grid", gap: "var(--s2)" }}>
                  {f.sel.type === "dir"
                    ? <button className="btn primary" onClick={() => f.enter(f.sel.name)}><Icon d={ICON.folderOpen} size={15} />{tr("開啟")}</button>
                    : <button className="btn primary" onClick={() => f.download(f.sel)}><Icon d={ICON.download} size={15} />{tr("下載到本機")}</button>}
                  <button className="btn ghost" onClick={() => startRename(f.sel)}><Icon d={ICON.pencil} size={14} />{tr("重新命名")}</button>
                  <button className="btn danger" onClick={() => f.remove(f.sel)}><Icon d={ICON.trash} size={14} />{tr("刪除")}</button>
                </div>
              </div>
            ) : (
              <div style={{ flex: 1, display: "grid", placeItems: "center", color: "var(--text-mute)", fontSize: 13, padding: "var(--s5)", textAlign: "center" }}>
                {tr("點選檔案以檢視內容與操作")}
              </div>
            )}
            {/* recent actions */}
            <div style={{ borderTop: "1px solid var(--border)", padding: "var(--s4) var(--s5)" }}>
              <div style={{ fontSize: 11, fontWeight: 700, letterSpacing: ".08em", color: "var(--text-mute)", marginBottom: "var(--s2)" }}>{tr("最近操作")}</div>
              {f.acts.length ? f.acts.slice(0, 4).map((a, i) => (
                <div key={i} className="mono" style={{ fontSize: 11, color: "var(--text-mute)", display: "flex", gap: 8, marginBottom: 2 }}>
                  <span style={{ opacity: .7 }}>{a.t}</span><span style={{ color: "var(--text-dim)" }}>{a.text}</span>
                </div>
              )) : <div style={{ fontSize: 11.5, color: "var(--text-mute)" }}>—</div>}
            </div>
          </div>
        </div>
      )}

      {/* right-click context menu */}
      {menu && (
        <div style={{ position: "fixed", left: menu.x, top: menu.y, zIndex: 50, background: "var(--panel)", border: "1px solid var(--border-strong)",
                      borderRadius: "var(--r3)", boxShadow: "var(--shadow)", padding: 4, minWidth: 150 }} onClick={(e) => e.stopPropagation()}>
          {(menu.entry.type === "dir"
            ? [["開啟", "folderOpen", () => { f.enter(menu.entry.name); setMenu(null); }]]
            : [["下載到本機", "download", () => { f.download(menu.entry); setMenu(null); }]]
          ).concat([
            ["重新命名", "pencil", () => startRename(menu.entry)],
            ["刪除", "trash", () => { f.remove(menu.entry); setMenu(null); }],
          ]).map(([label, icon, fn]) => (
            <button key={label} onClick={fn} style={{ display: "flex", alignItems: "center", gap: "var(--s3)", width: "100%", border: "none", background: "transparent",
              cursor: "pointer", padding: "8px 10px", borderRadius: "var(--r2)", color: label === "刪除" ? "var(--err)" : "var(--text-dim)", fontSize: 13, fontFamily: "var(--font)" }}
              onMouseEnter={(e) => e.currentTarget.style.background = "var(--panel-2)"} onMouseLeave={(e) => e.currentTarget.style.background = "transparent"}>
              <Icon d={ICON[icon]} size={15} />{tr(label)}
            </button>
          ))}
        </div>
      )}
    </div>
  );
}

Object.assign(window, { StudioParams, StudioFiles });
