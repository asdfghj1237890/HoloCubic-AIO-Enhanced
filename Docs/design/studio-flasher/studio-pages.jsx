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
      {conn === "connecting" ? "連線中" : live ? (port ? "已連線 · " + port : "已連線") : "未連線"}
    </span>
  );
}

// ════════════════════════════════════════════════════════════════════════════
//  參數設定 — device parameters with Read All / Write Changes (diff count)
// ════════════════════════════════════════════════════════════════════════════
const PARAM_GROUPS = [
  { label: "WiFi 設定", icon: "wifi", fields: [
    { key: "ssid1", label: "SSID 1", type: "text", ph: "主要 WiFi 名稱" },
    { key: "pwd1", label: "密碼 1", type: "password", ph: "WiFi 密碼" },
    { key: "ssid2", label: "SSID 2", type: "text", ph: "備用 WiFi 名稱" },
    { key: "pwd2", label: "密碼 2", type: "password", ph: "備用 WiFi 密碼" },
  ] },
  { label: "系統設定", icon: "chip", fields: [
    { key: "backlight", label: "背光亮度", type: "slider", min: 0, max: 255 },
    { key: "rotation", label: "螢幕旋轉", type: "select", options: [["0", "0°"], ["1", "90°"], ["2", "180°"], ["3", "270°"]] },
    { key: "auto_mpu", label: "自動 MPU 翻轉", type: "toggle" },
    { key: "sleep", label: "待機息屏", type: "select", options: [["0", "永不"], ["30", "30 秒"], ["60", "1 分鐘"], ["300", "5 分鐘"]] },
    { key: "wake", label: "喚醒方式", type: "select", options: [["mpu", "翻轉喚醒"], ["touch", "觸控喚醒"], ["both", "翻轉 + 觸控"]] },
  ] },
  { label: "天氣設定", icon: "img", fields: [
    { key: "city", label: "城市名稱", type: "text", ph: "例如 Taipei" },
    { key: "weather_key", label: "天氣 API 金鑰", type: "password", ph: "心知天氣金鑰" },
    { key: "weather_interval", label: "更新間隔", type: "select", options: [["10", "10 分鐘"], ["30", "30 分鐘"], ["60", "60 分鐘"]] },
  ] },
  { label: "其他設定", icon: "braces", fields: [
    { key: "server_ip", label: "伺服器 IP", type: "text", ph: "192.168.0.100" },
    { key: "server_port", label: "連接埠", type: "text", ph: "6677" },
    { key: "timezone", label: "時區", type: "select", options: [["8", "UTC+8 台北/北京"], ["9", "UTC+9 東京"], ["0", "UTC+0 倫敦"]] },
  ] },
];
const DEVICE_VALUES = {
  ssid1: "Holo_Home_2.4G", pwd1: "home12345", ssid2: "Holo_Backup", pwd2: "",
  backlight: "180", rotation: "1", auto_mpu: "1", sleep: "60", wake: "both",
  city: "Taipei", weather_key: "SmiKQ3v9xPq7-Az0", weather_interval: "30",
  server_ip: "192.168.0.165", server_port: "6677", timezone: "8",
};
const EMPTY_VALUES = Object.fromEntries(Object.keys(DEVICE_VALUES).map((k) => [k, ""]));

function useSettings() {
  const { useState } = React;
  const [conn, setConn] = useState("disconnected");
  const [port, setPort] = useState(FLASH_PORTS[0].name);
  const [baud, setBaud] = useState("115200");
  const [vals, setVals] = useState(EMPTY_VALUES);
  const [base, setBase] = useState(EMPTY_VALUES);
  const [loaded, setLoaded] = useState(false);
  const [status, setStatus] = useState("連接裝置後即可讀取目前設定。");

  const connect = () => {
    setConn("connecting"); setStatus("開啟序列埠…");
    setTimeout(() => { setConn("connected"); setStatus("已連線，請按「讀取設定」載入裝置目前的參數。"); }, 800);
  };
  const disconnect = () => { setConn("disconnected"); setLoaded(false); setVals(EMPTY_VALUES); setBase(EMPTY_VALUES); setStatus("已中斷連線。"); };
  const readAll = () => {
    setVals({ ...DEVICE_VALUES }); setBase({ ...DEVICE_VALUES }); setLoaded(true);
    setStatus("已讀取 " + Object.keys(DEVICE_VALUES).length + " 項參數（Get ×" + Object.keys(DEVICE_VALUES).length + "）。");
  };
  const setField = (k, v) => setVals((s) => ({ ...s, [k]: v }));
  const changed = Object.keys(vals).filter((k) => vals[k] !== base[k]);
  const writeChanges = () => {
    if (!changed.length) return;
    setBase({ ...vals });
    setStatus("已寫入 " + changed.length + " 項修改（Set ×" + changed.length + "）：" + changed.join(", "));
  };
  return { conn, port, setPort, baud, setBaud, vals, base, loaded, status, changed,
           connect, disconnect, readAll, setField, writeChanges };
}

function ParamField({ f, value, changed, disabled, onChange }) {
  return (
    <div style={{ display: "grid", gridTemplateColumns: "150px 1fr", alignItems: "center", gap: "var(--s4)",
                  padding: "var(--s3) 0", borderTop: "1px solid var(--border)" }}>
      <div style={{ display: "flex", alignItems: "center", gap: "var(--s2)" }}>
        <span style={{ fontSize: 13, color: disabled ? "var(--text-mute)" : "var(--text-dim)", whiteSpace: "nowrap" }}>{f.label}</span>
        {changed && <span title="已修改，尚未寫入" style={{ width: 6, height: 6, borderRadius: "50%", background: "var(--warn)", flex: "none" }} />}
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
            {f.options.map(([v, l]) => <option key={v} value={v}>{l}</option>)}
          </select>
        ) : (
          <input className="fld" type={f.type} placeholder={f.ph} value={value} disabled={disabled}
            onChange={(e) => onChange(e.target.value)} style={{ maxWidth: 320 }} />
        )}
      </div>
    </div>
  );
}

function StudioParams() {
  const s = useSettings();
  const connected = s.conn === "connected";
  const enabled = connected && s.loaded;
  return (
    <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
      <PageHeader title={tr("參數設定")} sub={tr("讀取與修改 HoloCubic 的 WiFi、系統、天氣等參數")}
        right={<StatusChip conn={s.conn} port={connected ? s.port : null} />} />

      {/* toolbar */}
      <div style={{ display: "flex", alignItems: "center", gap: "var(--s2)", padding: "var(--s3) var(--s6)",
                    borderBottom: "1px solid var(--border)", flexWrap: "wrap", flex: "none" }}>
        <select className="fld mono" style={{ width: 140, height: 36 }} value={s.port} onChange={(e) => s.setPort(e.target.value)} disabled={connected}>
          {FLASH_PORTS.map((p) => <option key={p.name} value={p.name}>{p.name}</option>)}
        </select>
        <select className="fld mono" style={{ width: 104, height: 36 }} value={s.baud} onChange={(e) => s.setBaud(e.target.value)} disabled={connected}>
          {BAUD_RATES.map((b) => <option key={b} value={b}>{b}</option>)}
        </select>
        {connected
          ? <button className="btn" style={{ height: 36 }} onClick={s.disconnect}>中斷</button>
          : <button className="btn primary" style={{ height: 36 }} onClick={s.connect}>{s.conn === "connecting" ? "連線中…" : "連接"}</button>}
        <div style={{ flex: 1 }} />
        <button className="btn ghost" style={{ height: 36 }} disabled={!connected} onClick={s.readAll}><Icon d={ICON.download} size={15} />讀取設定</button>
        <button className="btn primary" style={{ height: 36 }} disabled={!enabled || !s.changed.length} onClick={s.writeChanges}>
          <Icon d={ICON.check} size={15} />寫入修改{s.changed.length ? ` (${s.changed.length})` : ""}
        </button>
      </div>

      <div className="scroll" style={{ overflow: "auto", padding: "var(--s5) var(--s6)" }}>
        <div style={{ maxWidth: 760 }}>
          <div style={{ display: "flex", alignItems: "center", gap: "var(--s2)", fontSize: 12.5, color: enabled ? "var(--text-dim)" : "var(--text-mute)", marginBottom: "var(--s4)" }}>
            <span className="dot" style={{ background: enabled ? "var(--accent)" : "var(--text-mute)" }} />{s.status}
          </div>
          <div style={{ display: "grid", gap: "var(--s5)", opacity: enabled ? 1 : 0.55, pointerEvents: enabled ? "auto" : "none" }}>
            {PARAM_GROUPS.map((g) => (
              <div key={g.label} style={{ background: "var(--panel)", border: "1px solid var(--border)", borderRadius: "var(--r4)", overflow: "hidden" }}>
                <div style={{ display: "flex", alignItems: "center", gap: "var(--s2)", padding: "var(--s3) var(--s4)", color: "var(--text-dim)" }}>
                  <Icon d={ICON[g.icon]} size={15} />
                  <span style={{ fontSize: 12, fontWeight: 700, letterSpacing: ".05em", whiteSpace: "nowrap" }}>{g.label}</span>
                </div>
                <div style={{ padding: "0 var(--s4) var(--s2)" }}>
                  {g.fields.map((f) => (
                    <ParamField key={f.key} f={f} value={s.vals[f.key]} changed={s.vals[f.key] !== s.base[f.key]} disabled={!enabled} onChange={(v) => s.setField(f.key, v)} />
                  ))}
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

function useFiles() {
  const { useState } = React;
  const [conn, setConn] = useState("disconnected");
  const [ip, setIp] = useState("192.168.0.165");
  const [port, setPort] = useState("6677");
  const [fs, setFs] = useState(() => JSON.parse(JSON.stringify(SD_FS)));
  const [path, setPath] = useState("/");
  const [sel, setSel] = useState(null);
  const [acts, setActs] = useState([]);

  const log = (text) => setActs((a) => [{ text, t: new Date().toLocaleTimeString("zh-TW", { hour12: false }) }, ...a].slice(0, 6));
  const connect = () => { setConn("connecting"); setTimeout(() => { setConn("connected"); setPath("/"); log(`已連線 ${ip}:${port}`); }, 800); };
  const disconnect = () => { setConn("disconnected"); setSel(null); log("已中斷連線"); };
  const entries = (fs[path] || []).slice().sort((a, b) => (a.type === b.type ? a.name.localeCompare(b.name) : a.type === "dir" ? -1 : 1));
  const enter = (name) => { setPath(path === "/" ? "/" + name : path + "/" + name); setSel(null); };
  const goPath = (p) => { setPath(p); setSel(null); };
  const remove = (e) => {
    setFs((F) => ({ ...F, [path]: F[path].filter((x) => x.name !== e.name) }));
    setSel(null); log(`已刪除 ${e.name}`);
  };
  const rename = (e, newName) => {
    if (!newName || newName === e.name) return;
    setFs((F) => ({ ...F, [path]: F[path].map((x) => (x.name === e.name ? { ...x, name: newName } : x)) }));
    setSel((s) => (s && s.name === e.name ? { ...s, name: newName } : s));
    log(`已重新命名 ${e.name} → ${newName}`);
  };
  const download = (e) => log(`下載 ${path === "/" ? "" : path}/${e.name} → 本機`);
  const newFolder = () => {
    const existing = new Set((fs[path] || []).map((x) => x.name));
    let name = "新增資料夾", i = 2;
    while (existing.has(name)) name = `新增資料夾 ${i++}`;
    const childPath = path === "/" ? "/" + name : path + "/" + name;
    setFs((F) => ({ ...F, [path]: [...F[path], { name, type: "dir" }], [childPath]: [] }));
    log(`已建立資料夾 ${name}`);
    return name;
  };
  const upload = () => {
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

  const crumbs = ["根目錄", ...f.path.split("/").filter(Boolean)];
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
        <input className="fld mono" style={{ width: 150, height: 36 }} value={f.ip} onChange={(e) => f.setIp(e.target.value)} disabled={connected} placeholder="IP 位址" />
        <span style={{ color: "var(--text-mute)" }}>:</span>
        <input className="fld mono" style={{ width: 80, height: 36 }} value={f.port} onChange={(e) => f.setPort(e.target.value)} disabled={connected} placeholder="埠" />
        {connected
          ? <button className="btn" style={{ height: 36 }} onClick={f.disconnect}>中斷</button>
          : <button className="btn primary" style={{ height: 36 }} onClick={f.connect}>{f.conn === "connecting" ? "連線中…" : "連線"}</button>}
      </div>

      {!connected ? (
        <div style={{ flex: 1, display: "grid", placeItems: "center" }}>
          <div style={{ textAlign: "center", color: "var(--text-mute)" }}>
            <Icon d={ICON.folderOpen} size={30} />
            <div style={{ fontSize: 13.5, marginTop: "var(--s3)" }}>輸入 HoloCubic 的 IP 與連接埠後按「連線」<br />即可瀏覽記憶卡檔案</div>
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
              <button className="btn ghost" style={{ height: 30, fontSize: 12 }} onClick={f.upload}><Icon d={ICON.upload} size={13} />上傳檔案</button>
              <button className="btn ghost" style={{ height: 30, fontSize: 12 }} onClick={f.newFolder}><Icon d={ICON.plus || ICON.folderOpen} size={13} />新增資料夾</button>
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
                    <span className="mono" style={{ fontSize: 11.5, color: "var(--text-mute)", flex: "none" }}>{isDir ? "資料夾" : fmtBytes(e.size)}</span>
                    {isDir && <Icon d={ICON.chevR} size={15} stroke={2} />}
                  </div>
                );
              })}
              {f.entries.length === 0 && <div style={{ textAlign: "center", color: "var(--text-mute)", fontSize: 13, padding: "var(--s6)" }}>空的資料夾</div>}
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
                  {[["類型", f.sel.type === "dir" ? "資料夾" : fileMeta(f.sel.name).kind],
                    ["大小", f.sel.type === "dir" ? "—" : fmtBytes(f.sel.size)],
                    ["路徑", (f.path === "/" ? "" : f.path) + "/" + f.sel.name],
                    ["修改時間", f.sel.mtime || "—"]].map(([k, v]) => (
                    <React.Fragment key={k}>
                      <span style={{ color: "var(--text-mute)" }}>{k}</span>
                      <span className="mono" style={{ color: "var(--text-dim)", textAlign: "right", wordBreak: "break-all" }}>{v}</span>
                    </React.Fragment>
                  ))}
                </div>
                <div style={{ display: "grid", gap: "var(--s2)" }}>
                  {f.sel.type === "dir"
                    ? <button className="btn primary" onClick={() => f.enter(f.sel.name)}><Icon d={ICON.folderOpen} size={15} />開啟</button>
                    : <button className="btn primary" onClick={() => f.download(f.sel)}><Icon d={ICON.download} size={15} />下載到本機</button>}
                  <button className="btn ghost" onClick={() => startRename(f.sel)}><Icon d={ICON.pencil} size={14} />重新命名</button>
                  <button className="btn danger" onClick={() => f.remove(f.sel)}><Icon d={ICON.trash} size={14} />刪除</button>
                </div>
              </div>
            ) : (
              <div style={{ flex: 1, display: "grid", placeItems: "center", color: "var(--text-mute)", fontSize: 13, padding: "var(--s5)", textAlign: "center" }}>
                點選檔案以檢視內容與操作
              </div>
            )}
            {/* recent actions */}
            <div style={{ borderTop: "1px solid var(--border)", padding: "var(--s4) var(--s5)" }}>
              <div style={{ fontSize: 11, fontWeight: 700, letterSpacing: ".08em", color: "var(--text-mute)", marginBottom: "var(--s2)" }}>最近操作</div>
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
            : [["下載", "download", () => { f.download(menu.entry); setMenu(null); }]]
          ).concat([
            ["重新命名", "pencil", () => startRename(menu.entry)],
            ["刪除", "trash", () => { f.remove(menu.entry); setMenu(null); }],
          ]).map(([label, icon, fn]) => (
            <button key={label} onClick={fn} style={{ display: "flex", alignItems: "center", gap: "var(--s3)", width: "100%", border: "none", background: "transparent",
              cursor: "pointer", padding: "8px 10px", borderRadius: "var(--r2)", color: label === "刪除" ? "var(--err)" : "var(--text-dim)", fontSize: 13, fontFamily: "var(--font)" }}
              onMouseEnter={(e) => e.currentTarget.style.background = "var(--panel-2)"} onMouseLeave={(e) => e.currentTarget.style.background = "transparent"}>
              <Icon d={ICON[icon]} size={15} />{label}
            </button>
          ))}
        </div>
      )}
    </div>
  );
}

Object.assign(window, { StudioParams, StudioFiles });
