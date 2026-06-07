// studio-convert.jsx — 圖片轉換 (LVGL image encoder) + 影片轉碼 (ffmpeg pipeline).
// Exports: StudioImage, StudioVideo.

Object.assign(ICON, {
  plus: ["M12 5v14", "M5 12h14"],
  clock: ["M12 22a10 10 0 1 0 0-20 10 10 0 0 0 0 20z", "M12 7v5l3 2"],
  arrowR: ["M5 12h14", "M13 6l6 6-6 6"],
  upload: ["M12 16V4", "M7 9l5-5 5 5", "M5 20h14"],
  ext: ["M10 13a5 5 0 0 0 7 0l3-3a5 5 0 0 0-7-7l-1 1", "M14 11a5 5 0 0 0-7 0l-3 3a5 5 0 0 0 7 7l1-1"],
});

const PH_UNUSED = null;

// ════════════════════════════════════════════════════════════════════════════
//  圖片轉換
// ════════════════════════════════════════════════════════════════════════════
const IMG_FORMATS = ["RGB332", "RGB565", "RGB565_SWAP", "RGB888",
  "Alpha_1bit", "Alpha_2bit", "Alpha_4bit", "Alpha_8bit",
  "Indexed_1bit", "Indexed_2bit", "Indexed_4bit", "Indexed_8bit"];
const BPP = { RGB332: 8, RGB565: 16, RGB565_SWAP: 16, RGB888: 24, Alpha_1bit: 1, Alpha_2bit: 2, Alpha_4bit: 4, Alpha_8bit: 8, Indexed_1bit: 1, Indexed_2bit: 2, Indexed_4bit: 4, Indexed_8bit: 8 };
function outBytes(fmt, w, h) {
  let b = Math.ceil((w * h * BPP[fmt]) / 8) + 12;
  if (fmt.indexOf("Indexed") === 0) b += Math.pow(2, BPP[fmt]) * 4;
  return b;
}
const IMG_SAMPLES = [
  { name: "butterfly_1.jpg", w: 240, h: 240, size: 15068, ext: "jpg", hue: 28 },
  { name: "weather_sun.png", w: 64, h: 64, size: 2048, ext: "png", hue: 45 },
  { name: "wallpaper.bmp", w: 240, h: 320, size: 230400, ext: "bmp", hue: 210 },
  { name: "clock_face.png", w: 200, h: 200, size: 18000, ext: "png", hue: 280 },
  { name: "logo_mark.png", w: 128, h: 128, size: 6400, ext: "png", hue: 160 },
  { name: "gauge_bg.jpg", w: 240, h: 240, size: 21400, ext: "jpg", hue: 0 },
];

function useImageConv() {
  const { useState, useRef, useEffect } = React;
  const [files, setFiles] = useState([]);
  const [format, setFormat] = useState("RGB565");
  const [carray, setCarray] = useState(false);
  const [dither, setDither] = useState(false);
  const [resize, setResize] = useState(false);
  const [outW, setOutW] = useState("240");
  const [outH, setOutH] = useState("240");
  const [busy, setBusy] = useState(false);
  const timer = useRef(null);
  const addCount = useRef(0);

  const addSamples = () => {
    const s = IMG_SAMPLES[addCount.current % IMG_SAMPLES.length];
    addCount.current += 1;
    setFiles((F) => [...F, { ...s, id: Date.now() + Math.random(), status: "pending", out: 0 }]);
  };
  const remove = (id) => setFiles((F) => F.filter((f) => f.id !== id));
  const clear = () => setFiles([]);

  const convert = () => {
    if (busy || !files.length) return;
    setBusy(true);
    setFiles((F) => F.map((f) => ({ ...f, status: "pending", out: 0 })));
    let i = 0, pct = 0;
    timer.current = setInterval(() => {
      setFiles((F) => {
        if (i >= F.length) { clearInterval(timer.current); setBusy(false); return F; }
        pct = Math.min(100, pct + 18 + Math.random() * 22);
        const next = F.map((f, idx) => idx === i ? { ...f, status: pct >= 100 ? "done" : pct } : f);
        if (pct >= 100) {
          const f = F[i];
          const w = resize ? +outW || f.w : f.w, h = resize ? +outH || f.h : f.h;
          let bytes = outBytes(format, w, h);
          if (carray) bytes = Math.round(bytes * 4.6);
          next[i] = { ...next[i], status: "done", out: bytes, ow: w, oh: h };
          i += 1; pct = 0;
          if (i >= F.length) { clearInterval(timer.current); setBusy(false); }
        }
        return next;
      });
    }, 180);
  };
  const cancel = () => { if (timer.current) clearInterval(timer.current); setBusy(false); };
  useEffect(() => () => timer.current && clearInterval(timer.current), []);
  const doneCount = files.filter((f) => f.status === "done").length;
  return { files, format, setFormat, carray, setCarray, dither, setDither, resize, setResize,
           outW, setOutW, outH, setOutH, busy, addSamples, remove, clear, convert, cancel, doneCount };
}

function Thumb({ f, size = 44 }) {
  return (
    <div style={{ width: size, height: size, borderRadius: "var(--r2)", flex: "none", overflow: "hidden",
                  background: `linear-gradient(135deg, hsl(${f.hue} 55% 42%), hsl(${(f.hue + 40) % 360} 55% 30%))`,
                  display: "grid", placeItems: "center", color: "rgba(255,255,255,.85)", fontSize: 9, fontWeight: 700, letterSpacing: ".04em" }}>
      {f.ext.toUpperCase()}
    </div>
  );
}

function StudioImage() {
  const c = useImageConv();
  const ext = c.carray ? ".c" : (c.format.indexOf("Indexed") === 0 ? "" : "") + ".bin";
  return (
    <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
      <PageHeader title={tr("圖片轉換")} sub={tr("把任意圖片轉成 LVGL 影像格式（bin / C 陣列）")}
        right={<span className="chip" style={{ fontSize: 12.5 }}>{c.files.length} 個檔案{c.doneCount ? ` · ${c.doneCount} 完成` : ""}</span>} />

      {/* settings toolbar */}
      <div style={{ display: "flex", alignItems: "center", gap: "var(--s3)", padding: "var(--s3) var(--s6)",
                    borderBottom: "1px solid var(--border)", flexWrap: "wrap", flex: "none" }}>
        <span style={{ fontSize: 12, color: "var(--text-mute)" }}>輸出格式</span>
        <select className="fld mono" style={{ width: 158, height: 36 }} value={c.format} onChange={(e) => c.setFormat(e.target.value)} disabled={c.busy}>
          {IMG_FORMATS.map((f) => <option key={f} value={f}>{f}</option>)}
        </select>
        <label style={{ display: "flex", alignItems: "center", gap: "var(--s2)", fontSize: 12.5, color: "var(--text-dim)", cursor: "pointer" }}>
          <input type="checkbox" checked={c.carray} onChange={(e) => c.setCarray(e.target.checked)} disabled={c.busy} style={{ accentColor: "var(--accent)" }} />C 陣列
        </label>
        <label style={{ display: "flex", alignItems: "center", gap: "var(--s2)", fontSize: 12.5, color: "var(--text-dim)", cursor: "pointer" }}>
          <input type="checkbox" checked={c.dither} onChange={(e) => c.setDither(e.target.checked)} disabled={c.busy} style={{ accentColor: "var(--accent)" }} />抖色
        </label>
        <div style={{ width: 1, height: 22, background: "var(--border)" }} />
        <label style={{ display: "flex", alignItems: "center", gap: "var(--s2)", fontSize: 12.5, color: "var(--text-dim)", cursor: "pointer" }}>
          <input type="checkbox" checked={c.resize} onChange={(e) => c.setResize(e.target.checked)} disabled={c.busy} style={{ accentColor: "var(--accent)" }} />縮放至
        </label>
        <input className="fld mono" style={{ width: 56, height: 32 }} value={c.outW} onChange={(e) => c.setOutW(e.target.value)} disabled={c.busy || !c.resize} />
        <span style={{ color: "var(--text-mute)" }}>×</span>
        <input className="fld mono" style={{ width: 56, height: 32 }} value={c.outH} onChange={(e) => c.setOutH(e.target.value)} disabled={c.busy || !c.resize} />
        <div style={{ flex: 1 }} />
        {c.busy
          ? <button className="btn danger" style={{ height: 36 }} onClick={c.cancel}><Icon d={ICON.x} size={14} />取消</button>
          : <button className="btn primary" style={{ height: 36 }} disabled={!c.files.length} onClick={c.convert}><Icon d={ICON.ext} size={14} />開始轉換</button>}
      </div>

      <div className="scroll" style={{ overflow: "auto", padding: "var(--s5) var(--s6)" }}>
        <div style={{ maxWidth: 860 }}>
          {/* drop zone */}
          <div onClick={c.addSamples} style={{ border: "1.5px dashed var(--border-strong)", borderRadius: "var(--r4)",
                padding: "var(--s5)", display: "flex", alignItems: "center", justifyContent: "center", gap: "var(--s3)",
                cursor: "pointer", color: "var(--text-mute)", marginBottom: "var(--s4)", background: "var(--panel)" }}>
            <Icon d={ICON.upload} size={20} />
            <span style={{ fontSize: 13 }}>拖入或點此加入圖片（PNG / JPG / BMP）</span>
            <span className="chip" style={{ fontSize: 11 }}>+ 範例</span>
          </div>

          {c.files.length === 0 ? (
            <div style={{ textAlign: "center", color: "var(--text-mute)", fontSize: 13, padding: "var(--s6)" }}>尚未加入任何圖片</div>
          ) : (
            <div style={{ display: "grid", gap: "var(--s2)" }}>
              {c.files.map((f) => {
                const pct = typeof f.status === "number" ? f.status : f.status === "done" ? 100 : 0;
                const w = f.ow || (c.resize ? +c.outW || f.w : f.w), h = f.oh || (c.resize ? +c.outH || f.h : f.h);
                return (
                  <div key={f.id} style={{ display: "flex", alignItems: "center", gap: "var(--s4)", padding: "var(--s3)",
                        background: "var(--panel)", border: "1px solid var(--border)", borderRadius: "var(--r3)" }}>
                    <Thumb f={f} />
                    <div style={{ flex: 1, minWidth: 0 }}>
                      <div style={{ fontSize: 13, fontWeight: 600, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>{f.name}</div>
                      <div className="mono" style={{ fontSize: 11.5, color: "var(--text-mute)" }}>{f.w}×{f.h} · {fmtBytes(f.size)}</div>
                    </div>
                    <Icon d={ICON.arrowR} size={16} stroke={2} />
                    <div style={{ width: 200, flex: "none" }}>
                      {f.status === "done" ? (
                        <div style={{ display: "flex", alignItems: "center", gap: "var(--s2)" }}>
                          <Icon d={ICON.check} size={15} />
                          <span className="mono" style={{ fontSize: 11.5, color: "var(--text-dim)" }}>{f.name.replace(/\.\w+$/, "")}{c.carray ? ".c" : ".bin"} · {w}×{h} · {fmtBytes(f.out)}</span>
                        </div>
                      ) : typeof f.status === "number" ? (
                        <div style={{ height: 6, borderRadius: 999, background: "var(--panel-3)", overflow: "hidden" }}>
                          <div style={{ width: pct + "%", height: "100%", background: "var(--accent)", borderRadius: 999 }} />
                        </div>
                      ) : (
                        <span className="mono" style={{ fontSize: 11.5, color: "var(--text-mute)" }}>{c.format}{c.carray ? " · C 陣列" : ""}</span>
                      )}
                    </div>
                    <button className="btn ghost" style={{ width: 30, height: 30, padding: 0 }} onClick={() => c.remove(f.id)} disabled={c.busy}><Icon d={ICON.x} size={14} /></button>
                  </div>
                );
              })}
            </div>
          )}
          <div style={{ fontSize: 11.5, color: "var(--text-mute)", marginTop: "var(--s4)", lineHeight: 1.6 }}>
            轉換結果會輸出到本軟體同層的 OutFile 資料夾。記憶卡 bin 圖片建議用 <span className="mono">RGB565_SWAP</span>；韌體內嵌陣列用 <span className="mono">Indexed_4bit</span> + C 陣列。
          </div>
        </div>
      </div>
    </div>
  );
}

// ════════════════════════════════════════════════════════════════════════════
//  影片轉碼
// ════════════════════════════════════════════════════════════════════════════
const VID_SAMPLE = { name: "BadApple_1080p.mp4", dur: "3:39", size: 28400000 };
const VID_DEFAULT = { w: "240", h: "240", fps: "20", quality: "80", format: "MJPEG" };

function useVideoConv() {
  const { useState, useRef, useEffect } = React;
  const [src, setSrc] = useState(null);
  const [outPath, setOutPath] = useState("");
  const [mode, setMode] = useState("default");
  const [cfg, setCfg] = useState({ ...VID_DEFAULT });
  const [ffmpeg, setFfmpeg] = useState("ready"); // ready|checking|missing
  const [op, setOp] = useState("idle"); // idle|running|done
  const [progress, setProgress] = useState(null);
  const [log, setLog] = useState([{ level: "muted", text: "選擇來源影片後即可開始轉碼。" }]);
  const timer = useRef(null);
  const push = (text, level = "info") => setLog((L) => [...L.slice(-120), { level, text, t: new Date().toLocaleTimeString("zh-TW", { hour12: false }) }]);

  const pickSrc = () => { setSrc(VID_SAMPLE); setOutPath("C:/HoloCubic/movie/badapple.mjpeg"); push(`已選擇來源：${VID_SAMPLE.name}（${VID_SAMPLE.dur}, ${fmtBytes(VID_SAMPLE.size)}）`, "muted"); };
  const recheck = () => { setFfmpeg("checking"); push("偵測 ffmpeg…", "muted"); setTimeout(() => { setFfmpeg("ready"); push("ffmpeg 4.4.1 已就緒。", "ok"); }, 700); };
  const set = (k, v) => setCfg((s) => ({ ...s, [k]: v }));

  const run = () => {
    if (!src || ffmpeg !== "ready" || op === "running") return;
    const c = mode === "default" ? VID_DEFAULT : cfg;
    setOp("running");
    push(`開始轉碼 → ${c.w}×${c.h} @ ${c.fps}fps · ${c.format} · q${c.quality}`, "info");
    push("ffmpeg -i 來源 -vf scale,fps -f image2pipe …（第 1 步：抽取影格）", "muted");
    let phase = 1, pct = 0;
    setProgress({ phase: 1, percent: 0 });
    timer.current = setInterval(() => {
      pct += 6 + Math.random() * 7;
      if (pct >= 100) {
        if (phase === 1) {
          push("影格抽取完成，開始編碼 " + c.format + "…", "muted");
          push("ffmpeg 第 2 步：封裝 " + (c.format === "MJPEG" ? "mjpeg" : "rgb565be") + " 串流 …", "muted");
          phase = 2; pct = 0; setProgress({ phase: 2, percent: 0 });
        } else {
          clearInterval(timer.current); setProgress(null); setOp("done");
          push(`✓ 轉碼完成 → ${outPath}`, "ok");
        }
      } else setProgress({ phase, percent: pct });
    }, 150);
  };
  const cancel = () => { if (timer.current) clearInterval(timer.current); setProgress(null); setOp("idle"); push("已取消轉碼，子程序已結束。", "warn"); };
  useEffect(() => () => timer.current && clearInterval(timer.current), []);
  return { src, outPath, setOutPath, mode, setMode, cfg, ffmpeg, op, progress, log, pickSrc, recheck, set, run, cancel };
}

function VRow({ label, children }) {
  return (
    <div style={{ display: "grid", gridTemplateColumns: "120px 1fr", alignItems: "center", gap: "var(--s4)", padding: "var(--s3) 0", borderTop: "1px solid var(--border)" }}>
      <span style={{ fontSize: 13, color: "var(--text-dim)", whiteSpace: "nowrap" }}>{label}</span>
      <div>{children}</div>
    </div>
  );
}

function StudioVideo() {
  const v = useVideoConv();
  const custom = v.mode === "custom";
  const cfg = custom ? v.cfg : VID_DEFAULT;
  return (
    <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
      <PageHeader title={tr("影片轉碼")} sub={tr("把影片轉成 HoloCubic 可播放的 MJPEG / RGB565 串流")}
        right={
          v.ffmpeg === "ready"
            ? <span className="chip" style={{ color: "var(--ok)" }}><span className="dot live" />ffmpeg 已就緒</span>
            : <span className="chip" style={{ color: "var(--warn)" }}><span className="dot busy" />{v.ffmpeg === "checking" ? "偵測中…" : "缺少 ffmpeg"}</span>} />

      <div style={{ flex: 1, display: "grid", gridTemplateColumns: "1fr 380px", minHeight: 0 }}>
        {/* left config */}
        <div className="scroll" style={{ overflow: "auto", padding: "var(--s6)" }}>
          <div style={{ maxWidth: 560 }}>
            {v.ffmpeg === "missing" && (
              <div style={{ display: "flex", alignItems: "center", gap: "var(--s3)", padding: "var(--s4)", marginBottom: "var(--s4)",
                            background: "var(--err-weak)", border: "1px solid color-mix(in srgb, var(--err) 35%, transparent)", borderRadius: "var(--r3)" }}>
                <span style={{ color: "var(--err)", fontSize: 13, flex: 1 }}>未在 PATH 中找到 ffmpeg，無法轉碼。</span>
                <a href="https://ffmpeg.org/download.html" target="_blank" rel="noreferrer" style={{ color: "var(--accent)", fontSize: 13, fontWeight: 600 }}>安裝 ffmpeg</a>
                <button className="btn ghost" style={{ height: 30, fontSize: 12 }} onClick={v.recheck}>重新偵測</button>
              </div>
            )}

            {/* source */}
            <div style={{ fontSize: 11, fontWeight: 700, letterSpacing: ".08em", color: "var(--text-mute)", marginBottom: "var(--s2)" }}>來源與輸出</div>
            <div style={{ background: "var(--panel)", border: "1px solid var(--border)", borderRadius: "var(--r4)", padding: "0 var(--s4) var(--s2)" }}>
              <VRow label="來源影片">
                {v.src
                  ? <div style={{ display: "flex", alignItems: "center", gap: "var(--s2)" }}>
                      <span className="chip"><Icon d={ICON.film || ICON.doc} size={13} />{v.src.name}</span>
                      <span className="mono" style={{ fontSize: 11.5, color: "var(--text-mute)" }}>{v.src.dur} · {fmtBytes(v.src.size)}</span>
                    </div>
                  : <button className="btn ghost" style={{ height: 34 }} onClick={v.pickSrc}><Icon d={ICON.upload} size={14} />選擇影片</button>}
              </VRow>
              <VRow label="輸出路徑">
                <input className="fld mono" style={{ maxWidth: 360 }} value={v.outPath} onChange={(e) => v.setOutPath(e.target.value)} placeholder="輸出檔案路徑" />
              </VRow>
            </div>

            {/* output settings */}
            <div style={{ fontSize: 11, fontWeight: 700, letterSpacing: ".08em", color: "var(--text-mute)", margin: "var(--s5) 0 var(--s2)" }}>輸出設定</div>
            <div style={{ background: "var(--panel)", border: "1px solid var(--border)", borderRadius: "var(--r4)", padding: "var(--s4)" }}>
              <div style={{ display: "flex", gap: 2, background: "var(--panel-3)", borderRadius: "var(--rpill)", padding: 2, width: "fit-content", marginBottom: "var(--s2)" }}>
                {[["default", "預設"], ["custom", "自訂"]].map(([k, l]) => (
                  <button key={k} onClick={() => v.setMode(k)} style={{ border: "none", cursor: "pointer", fontSize: 12.5, padding: "5px 18px", borderRadius: "var(--rpill)",
                    background: v.mode === k ? "var(--accent)" : "transparent", color: v.mode === k ? "#fff" : "var(--text-mute)", fontWeight: 600 }}>{l}</button>
                ))}
              </div>
              <div style={{ padding: "0 var(--s1)", opacity: custom ? 1 : 0.55, pointerEvents: custom ? "auto" : "none" }}>
                <VRow label="解析度">
                  <div style={{ display: "flex", alignItems: "center", gap: "var(--s2)" }}>
                    <input className="fld mono" style={{ width: 64, height: 32 }} value={cfg.w} onChange={(e) => v.set("w", e.target.value)} />
                    <span style={{ color: "var(--text-mute)" }}>×</span>
                    <input className="fld mono" style={{ width: 64, height: 32 }} value={cfg.h} onChange={(e) => v.set("h", e.target.value)} />
                  </div>
                </VRow>
                <VRow label="影格率 FPS"><input className="fld mono" style={{ width: 80, height: 32 }} value={cfg.fps} onChange={(e) => v.set("fps", e.target.value)} /></VRow>
                <VRow label="品質">
                  <div style={{ display: "flex", alignItems: "center", gap: "var(--s3)", maxWidth: 320 }}>
                    <input type="range" min="1" max="100" value={cfg.quality} onChange={(e) => v.set("quality", e.target.value)} style={{ flex: 1, accentColor: "var(--accent)" }} />
                    <span className="mono" style={{ fontSize: 12, color: "var(--text-dim)", width: 28 }}>{cfg.quality}</span>
                  </div>
                </VRow>
                <VRow label="格式">
                  <select className="fld mono" style={{ width: 150 }} value={cfg.format} onChange={(e) => v.set("format", e.target.value)}>
                    <option value="MJPEG">MJPEG</option>
                    <option value="rgb565be">rgb565be</option>
                  </select>
                </VRow>
              </div>
              {!custom && <div style={{ fontSize: 11.5, color: "var(--text-mute)", marginTop: "var(--s2)" }}>預設：240×240 · 20fps · MJPEG · q80</div>}
            </div>

            <div style={{ display: "flex", gap: "var(--s2)", marginTop: "var(--s5)" }}>
              {v.op === "running"
                ? <button className="btn danger" style={{ height: 42, padding: "0 var(--s5)" }} onClick={v.cancel}><Icon d={ICON.x} size={15} />取消轉碼</button>
                : <button className="btn primary" style={{ height: 42, padding: "0 var(--s6)" }} disabled={!v.src || v.ffmpeg !== "ready"} onClick={v.run}><Icon d={ICON.ext} size={15} />開始轉碼</button>}
              {v.ffmpeg === "ready" && <button className="btn ghost" style={{ height: 42 }} onClick={v.recheck}>重新偵測 ffmpeg</button>}
            </div>
          </div>
        </div>

        {/* right: progress + log */}
        <div style={{ borderLeft: "1px solid var(--border)", background: "var(--panel)", display: "flex", flexDirection: "column", minHeight: 0 }}>
          <div style={{ padding: "var(--s5)", borderBottom: "1px solid var(--border)" }}>
            <div style={{ fontSize: 11, fontWeight: 700, letterSpacing: ".08em", color: "var(--text-mute)", marginBottom: "var(--s3)" }}>進度</div>
            {v.progress ? (
              <div>
                <div style={{ display: "flex", justifyContent: "space-between", fontSize: 12, marginBottom: 6 }}>
                  <span style={{ color: "var(--text-dim)", whiteSpace: "nowrap" }}>第 {v.progress.phase}/2 步 · {v.progress.phase === 1 ? "抽取影格" : "編碼封裝"}</span>
                  <span className="mono" style={{ color: "var(--accent)" }}>{v.progress.percent.toFixed(0)}%</span>
                </div>
                <div style={{ height: 8, borderRadius: 999, background: "var(--panel-3)", overflow: "hidden" }}>
                  <div style={{ width: v.progress.percent + "%", height: "100%", background: "var(--accent)", borderRadius: 999, transition: "width .12s" }} />
                </div>
              </div>
            ) : v.op === "done" ? (
              <div style={{ display: "flex", alignItems: "center", gap: "var(--s2)", color: "var(--ok)", fontSize: 13, fontWeight: 600 }}>
                <Icon d={ICON.check} size={16} />轉碼完成
              </div>
            ) : (
              <div style={{ fontSize: 12.5, color: "var(--text-mute)" }}>待命中</div>
            )}
          </div>
          <div style={{ flex: 1, display: "flex", flexDirection: "column", minHeight: 0, padding: "var(--s5)" }}>
            <div style={{ fontSize: 11, fontWeight: 700, letterSpacing: ".08em", color: "var(--text-mute)", marginBottom: "var(--s2)" }}>記錄</div>
            <LogView log={v.log} style={{ flex: 1, fontSize: 11.5, lineHeight: 1.7, color: "var(--text-dim)" }} />
          </div>
        </div>
      </div>
    </div>
  );
}

Object.assign(window, { StudioImage, StudioVideo, StudioHelp });

// ════════════════════════════════════════════════════════════════════════════
//  說明
// ════════════════════════════════════════════════════════════════════════════
function HelpLink({ icon, color, title, sub, url }) {
  return (
    <a href={url} target="_blank" rel="noreferrer"
      style={{ display: "flex", alignItems: "center", gap: "var(--s4)", padding: "var(--s3) var(--s4)",
               textDecoration: "none", borderTop: "1px solid var(--border)", transition: "background .12s" }}
      onMouseEnter={(e) => e.currentTarget.style.background = "var(--panel-2)"}
      onMouseLeave={(e) => e.currentTarget.style.background = "transparent"}>
      <span style={{ width: 36, height: 36, borderRadius: "var(--r2)", flex: "none", display: "grid", placeItems: "center",
                     background: "var(--panel-3)", color: color || "var(--accent)" }}><Icon d={icon} size={18} /></span>
      <div style={{ flex: 1, minWidth: 0 }}>
        <div style={{ fontSize: 13.5, fontWeight: 600, color: "var(--text)" }}>{title}</div>
        <div className="mono" style={{ fontSize: 11.5, color: "var(--text-mute)", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>{sub}</div>
      </div>
      <Icon d={ICON.ext} size={15} stroke={2} />
    </a>
  );
}

function HelpSection({ label, children }) {
  return (
    <div style={{ marginBottom: "var(--s5)" }}>
      <div style={{ fontSize: 11, fontWeight: 700, letterSpacing: ".08em", color: "var(--text-mute)", marginBottom: "var(--s2)" }}>{label}</div>
      <div style={{ background: "var(--panel)", border: "1px solid var(--border)", borderRadius: "var(--r4)", overflow: "hidden" }}>
        {children}
      </div>
    </div>
  );
}

function StudioHelp() {
  return (
    <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
      <PageHeader title={tr("說明")} sub={tr("關於本工具與 HoloCubic 的相關資源")}
        right={<span className="chip mono" style={{ fontSize: 12 }}>v3.0.0</span>} />
      <div className="scroll" style={{ overflow: "auto", padding: "var(--s6)" }}>
        <div style={{ maxWidth: 720 }}>
          <p style={{ fontSize: 13.5, lineHeight: 1.8, color: "var(--text-dim)", margin: "0 0 var(--s5)", textWrap: "pretty" }}>
            本電腦端工具專為 <strong style={{ color: "var(--text)" }}>HoloCubic AIO</strong> 韌體開發，韌體燒錄功能同時相容其他第三方韌體。
            透過上方分頁可進行韌體燒錄、參數設定、記憶卡檔案管理，以及圖片與影片的格式轉換。
          </p>

          <HelpSection label="韌體與工具">
            <HelpLink icon={ICON.bolt} title="HoloCubic AIO Enhanced（本工具增強版）" sub="github.com/asdfghj1237890/HoloCubic-AIO-Enhanced" url="https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced" />
            <HelpLink icon={ICON.film} color="#a78bfa" title="觀看示範影片" sub="bilibili.com/video/BV1wS4y1R7YF" url="https://www.bilibili.com/video/BV1wS4y1R7YF?p=1" />
          </HelpSection>

          <HelpSection label="AIO 韌體原始專案">
            <HelpLink icon={ICON.braces} color="var(--warn)" title="HoloCubic AIO 原始版本" sub="github.com/ClimbSnail/HoloCubic_AIO" url="https://github.com/ClimbSnail/HoloCubic_AIO" />
            <HelpLink icon={ICON.braces} color="var(--warn)" title="HoloCubic AIO（Gitee 鏡像）" sub="gitee.com/ClimbSnailQ/HoloCubic_AIO" url="https://gitee.com/ClimbSnailQ/HoloCubic_AIO" />
          </HelpSection>

          <HelpSection label="硬體開源方案">
            <HelpLink icon={ICON.chip} title="HoloCubic 硬體開源專案" sub="github.com/peng-zhihui/HoloCubic" url="https://github.com/peng-zhihui/HoloCubic" />
          </HelpSection>

          <div style={{ fontSize: 11.5, color: "var(--text-mute)", lineHeight: 1.7, paddingTop: "var(--s2)" }}>
            HoloCubic AIO Tool · v3.0.0 · 介面重新設計概念稿。功能流程為互動原型示意，連接、燒錄與轉換等結果均為模擬。
          </div>
        </div>
      </div>
    </div>
  );
}
