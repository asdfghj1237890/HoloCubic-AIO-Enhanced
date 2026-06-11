// dir-a-pro.jsx — deepened "Studio" flasher: full-app, guided, high-polish.
// Exports: StudioFlasher, StudioEmpty. Reuses useFlasher / LogView / Icon.
const { useState: useSt, useEffect: useEf, useRef: useRf } = React;

// ── Step scaffold ──────────────────────────────────────────────────────────
function Step({ n, title, sub, state, children }) {
  // state: 'pending' | 'active' | 'done'
  const done = state === "done", active = state === "active";
  return (
    <div style={{ display: "flex", gap: "var(--s4)", opacity: state === "pending" ? 0.5 : 1, transition: "opacity .25s" }}>
      <div style={{ display: "flex", flexDirection: "column", alignItems: "center", flex: "none" }}>
        <div style={{ width: 34, height: 34, borderRadius: "50%", display: "grid", placeItems: "center",
              fontWeight: 700, fontSize: 14, flex: "none", transition: "all .25s",
              background: done ? "var(--ok)" : active ? "var(--accent)" : "var(--panel-3)",
              color: done || active ? "var(--accent-ink)" : "var(--text-mute)",
              boxShadow: active ? "0 0 0 4px var(--accent-weak)" : "none" }}>
          {done ? <Icon d={ICON.check} size={17} /> : n}
        </div>
        <div style={{ flex: 1, width: 2, background: done ? "var(--ok)" : "var(--border)", marginTop: 6, borderRadius: 2, transition: "background .25s" }} />
      </div>
      <div style={{ flex: 1, paddingBottom: "var(--s6)", minWidth: 0 }}>
        <div style={{ display: "flex", alignItems: "baseline", gap: "var(--s2)" }}>
          <span className="disp" style={{ fontSize: 17, fontWeight: 600, letterSpacing: "-.01em" }}>{title}</span>
          {done && <span style={{ fontSize: 11.5, color: "var(--ok)", fontWeight: 600 }}>{tr("已完成")}</span>}
        </div>
        <div style={{ fontSize: 12.5, color: "var(--text-mute)", marginTop: 2, marginBottom: "var(--s3)" }}>{sub}</div>
        {children}
      </div>
    </div>
  );
}

// ── Remote D-pad (rounded cluster) + keyboard ───────────────────────────────
function Dpad({ fl }) {
  const on = fl.conn === "connected";
  const [hot, setHot] = useSt(null);
  const fire = (dir) => { if (!on) return; setHot(dir); fl.sendRemote(dir); setTimeout(() => setHot((h) => (h === dir ? null : h)), 160); };
  useEf(() => {
    const onKey = (e) => {
      const tag = (e.target.tagName || "").toLowerCase();
      if (tag === "input" || tag === "select" || tag === "textarea") return;
      const map = { ArrowUp: "up", ArrowLeft: "left", ArrowRight: "right", Enter: "ok", Home: "home", " ": "ok" };
      if (map[e.key]) { e.preventDefault(); fire(map[e.key]); }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [on]);
  const Key = ({ dir, icon, label, big }) => (
    <button onClick={() => fire(dir)} disabled={!on} title={label}
      style={{ width: big ? 54 : 48, height: big ? 54 : 48, padding: 0, cursor: on ? "pointer" : "not-allowed",
               borderRadius: "var(--r3)", display: "grid", placeItems: "center", transition: "all .12s",
               border: "1px solid " + (big ? "var(--accent-line)" : "var(--border)"),
               background: hot === dir ? "var(--accent)" : big ? "var(--accent-weak)" : "var(--panel-2)",
               color: hot === dir ? "#fff" : big ? "var(--accent)" : "var(--text-dim)",
               transform: hot === dir ? "scale(.92)" : "scale(1)", opacity: on ? 1 : 0.4 }}>
      {icon === "ok" ? <span style={{ fontWeight: 700, fontSize: 13 }}>OK</span> : <Icon d={ICON[icon]} size={21} />}
    </button>
  );
  return (
    <div>
      <div style={{ display: "grid", gridTemplateColumns: "repeat(3,54px)", gap: "var(--s2)", justifyContent: "center", justifyItems: "center", alignItems: "center" }}>
        <span /><Key dir="up" icon="up" label={tr("上")} /><span />
        <Key dir="left" icon="left" label={tr("左")} /><Key dir="ok" icon="ok" label={tr("確認")} big /><Key dir="right" icon="right" label={tr("右")} />
        <span /><Key dir="home" icon="home" label={tr("首頁")} /><span />
      </div>
      <div style={{ textAlign: "center", marginTop: "var(--s3)", fontSize: 11, color: "var(--text-mute)" }}>
        {tr("鍵盤方向鍵 / Enter 也可操作")}
      </div>
    </div>
  );
}

// ── Device card with state-reactive glow + spec readout ─────────────────────
function DeviceCard({ fl }) {
  const connected = fl.conn === "connected";
  const flashing = fl.op === "flashing" || fl.op === "erasing";
  const glow = flashing ? "var(--accent)" : connected ? "var(--ok)" : "transparent";
  return (
    <div style={{ padding: "var(--s5)", borderBottom: "1px solid var(--border)" }}>
      {connected ? (
        <div style={{ display: "grid", gridTemplateColumns: "auto 1fr", gap: "4px var(--s3)", fontSize: 12 }}>
          {[[tr("晶片"), fl.chip.model], [tr("版本"), fl.chip.rev], ["Flash", fl.chip.flash], ["MAC", fl.chip.mac]].map(([k, v]) => (
            <React.Fragment key={k}>
              <span style={{ color: "var(--text-mute)" }}>{k}</span>
              <span className="mono" style={{ color: "var(--text-dim)", textAlign: "right" }}>{v}</span>
            </React.Fragment>
          ))}
        </div>
      ) : (
        <div style={{ textAlign: "center", fontSize: 12, color: "var(--text-mute)", lineHeight: 1.6 }}>
          {tr("尚未連線")}<br />{tr("接上 USB 後於步驟 1 連接")}
        </div>
      )}
    </div>
  );
}

// ── The flasher screen ──────────────────────────────────────────────────────
function StudioFlasher() {
  const fl = useFlasher();
  const [adv, setAdv] = useSt(false);
  const connected = fl.conn === "connected";
  const connecting = fl.conn === "connecting";
  const flashing = fl.op === "flashing";
  const erasing = fl.op === "erasing";
  const success = fl.op === "done";

  // partition queue mapping for the checklist
  const queue = fl.parts.map((p, i) => ({ ...p, i })).filter((p) => p.enabled);
  const partState = (qi) => {
    if (success && !fl.progress) return "done";
    if (!flashing || !fl.progress) return "pending";
    if (qi < fl.progress.idx) return "done";
    if (qi === fl.progress.idx) return "active";
    return "pending";
  };

  const step2State = connected ? (success ? "done" : "active") : "pending";
  const step3State = !connected ? "pending" : (flashing || erasing) ? "active" : success ? "done" : "active";

  return (
    <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
      {/* header */}
      <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between",
                    padding: "var(--s5) var(--s6)", borderBottom: "1px solid var(--border)" }}>
        <div>
          <div className="disp" style={{ fontSize: 21, fontWeight: 600, letterSpacing: "-.02em", whiteSpace: "nowrap" }}>{tr("燒錄韌體")}</div>
          <div style={{ fontSize: 13, color: "var(--text-mute)", whiteSpace: "nowrap" }}>{tr("把 AIO 韌體寫入你的 HoloCubic 小電視")}</div>
        </div>
        <span className="chip" style={{ fontSize: 12.5, padding: "6px 14px" }}>
          <span className={"dot " + (flashing || erasing ? "busy" : connected ? "live" : "")} />
          {flashing ? tr("燒錄中") : erasing ? tr("清空中") : connecting ? tr("連線中") : connected ? `${tr("已連線")} · ${fl.port}` : tr("未連線")}
        </span>
      </div>

      <div style={{ flex: 1, display: "grid", gridTemplateColumns: "1fr 360px", minHeight: 0 }}>
        {/* steps */}
        <div className="scroll" style={{ overflow: "auto", padding: "var(--s6)", maxWidth: 720 }}>
          <Step n="1" title={tr("連接裝置")} sub={tr("用 USB 接上 HoloCubic，選擇對應的連接埠")} state={connected ? "done" : "active"}>
            <div style={{ display: "flex", gap: "var(--s2)", alignItems: "center", flexWrap: "wrap" }}>
              <select className="fld mono" style={{ width: 156, height: 38 }} value={fl.port} onChange={(e) => fl.setPort(e.target.value)} disabled={connected}>
                {fl.ports.map((p) => <option key={p.name} value={p.name}>{p.name}</option>)}
              </select>
              <button className="btn ghost" style={{ width: 38, height: 38, padding: 0 }} onClick={fl.refreshPorts} title={tr("重新整理連接埠")}><Icon d={ICON.refresh} size={16} /></button>
              <select className="fld mono" style={{ width: 112, height: 38 }} value={fl.baud} onChange={(e) => fl.setBaud(e.target.value)} disabled={connected}>
                {BAUD_RATES.map((b) => <option key={b} value={b}>{b}</option>)}
              </select>
              {connected
                ? <button className="btn" style={{ height: 38 }} onClick={fl.disconnect}>{tr("中斷連線")}</button>
                : <button className="btn primary" style={{ height: 38, minWidth: 92 }} disabled={connecting} onClick={fl.connect}>
                    {connecting ? <><Icon d={ICON.refresh} size={15} /><span className="spin" style={{ display: "none" }} /></> : <Icon d={ICON.plug} size={15} />}
                    {connecting ? tr("連線中…") : tr("連接")}
                  </button>}
              {connected && <button className="btn ghost" style={{ height: 38 }} onClick={fl.reboot} title={tr("重新啟動裝置")}><Icon d={ICON.refresh} size={15} />{tr("重新開機")}</button>}
            </div>
            <div style={{ fontSize: 11.5, color: "var(--text-mute)", marginTop: "var(--s2)" }}>
              {tr("提示：找不到連接埠時請安裝 CH340 / CP210x 驅動，再按")} <Icon d={ICON.refresh} size={11} /> {tr("重新整理。")}
            </div>
          </Step>

          <Step n="2" title={tr("選擇韌體")} sub={tr("使用推薦版本，或展開進階自訂各分割區")} state={step2State}>
            <div style={{ display: "flex", alignItems: "center", gap: "var(--s3)", padding: "var(--s4)",
                          border: "1px solid var(--accent-line)", background: "var(--accent-weak)", borderRadius: "var(--r3)" }}>
              <div style={{ width: 42, height: 42, borderRadius: "var(--r2)", background: "var(--accent)", color: "#fff", display: "grid", placeItems: "center", flex: "none" }}>
                <Icon d={ICON.bolt} size={20} fill />
              </div>
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{ fontWeight: 600, fontSize: 14 }}>{tr("HoloCubic AIO 韌體")} {fl.latestRelease.status === "ok" && <span className="mono" style={{ color: "var(--accent)" }}>v{fl.latestRelease.version}</span>}</div>
                <div style={{ fontSize: 12, color: "var(--text-mute)" }}>{trf("{count} 個分割區 · {size} · 推薦給多數使用者", { count: fl.enabledCount, size: fmtBytes(fl.totalBytes) })}</div>
              </div>
              {fl.latestRelease.status === "ok"
                ? <span className="chip" style={{ background: "var(--panel)", color: "var(--ok)" }}><span className="dot live" />{tr("最新")}</span>
                : fl.latestRelease.status === "loading"
                  ? <span className="chip" style={{ background: "var(--panel)" }}><span className="dot busy" />{tr("查詢中…")}</span>
                  : <span className="chip" style={{ background: "var(--panel)" }} title={`無法查詢 GitHub (${fl.latestRelease.reason})`}><span className="dot" />{tr("離線")}</span>}
            </div>
            <button onClick={() => setAdv(!adv)} className="btn ghost" style={{ marginTop: "var(--s3)", fontSize: 12, height: 32 }}>
              {adv ? "▾ " + tr("收合進階分割區") : "▸ " + trf("進階：自訂 {n} 個分割區", { n: fl.parts.length })}
            </button>
            {adv && (
              <div style={{ marginTop: "var(--s2)", border: "1px solid var(--border)", borderRadius: "var(--r3)", overflow: "hidden", animation: "riseIn .2s" }}>
                {fl.parts.map((p, i) => (
                  <div key={i} style={{ display: "flex", alignItems: "center", gap: "var(--s3)", padding: "var(--s2) var(--s3)",
                        borderTop: i ? "1px solid var(--border)" : "none", fontSize: 12 }}>
                    <input type="checkbox" checked={p.enabled} onChange={() => fl.togglePart(i)} style={{ accentColor: "var(--accent)", width: 15, height: 15 }} />
                    <span className="mono" style={{ color: "var(--accent)", width: 62 }}>{hexAddr(p.addr)}</span>
                    <span style={{ flex: 1, color: "var(--text-dim)", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }} title={p.file}>{p.displayFile || p.file}</span>
                    <span className="mono" style={{ color: "var(--text-mute)", width: 64, textAlign: "right" }}>{fmtBytes(p.bytes)}</span>
                    <button className="btn ghost" style={{ height: 27, padding: "0 var(--s2)", fontSize: 11 }} onClick={() => fl.pickFile(i)}>{tr("選擇")}</button>
                  </div>
                ))}
              </div>
            )}
          </Step>

          <Step n="3" title={tr("開始燒錄")} sub={tr("過程約 30–60 秒，請保持 USB 連接、勿關閉視窗")} state={step3State}>
            <div style={{ display: "flex", gap: "var(--s2)", alignItems: "center" }}>
              {flashing || erasing
                ? <button className="btn danger" style={{ height: 42, padding: "0 var(--s5)" }} onClick={fl.cancel}><Icon d={ICON.x} size={15} />{tr("取消")}</button>
                : <button className="btn primary" style={{ height: 42, padding: "0 var(--s6)", fontSize: 14 }} disabled={!connected} onClick={fl.flash}><Icon d={ICON.bolt} size={17} fill />{tr("開始燒錄")}</button>}
              <button className="btn ghost" style={{ height: 42 }} disabled={!connected || fl.busy} onClick={fl.erase}><Icon d={ICON.trash} size={14} />{tr("清空晶片")}</button>
            </div>

            {/* per-partition checklist appears once connected */}
            {connected && (flashing || success || fl.progress) && (
              <div style={{ marginTop: "var(--s4)", border: "1px solid var(--border)", borderRadius: "var(--r3)", overflow: "hidden" }}>
                {queue.map((p, qi) => {
                  const st = partState(qi);
                  const pct = st === "active" && fl.progress ? fl.progress.percent : st === "done" ? 100 : 0;
                  return (
                    <div key={p.i} style={{ padding: "var(--s2) var(--s4)", borderTop: qi ? "1px solid var(--border)" : "none",
                          display: "flex", alignItems: "center", gap: "var(--s3)", fontSize: 12,
                          background: st === "active" ? "var(--accent-weak)" : "transparent" }}>
                      <span style={{ width: 18, height: 18, borderRadius: "50%", display: "grid", placeItems: "center", flex: "none",
                            background: st === "done" ? "var(--ok)" : st === "active" ? "var(--accent)" : "var(--panel-3)",
                            color: st === "pending" ? "var(--text-mute)" : "#fff" }}>
                        {st === "done" ? <Icon d={ICON.check} size={11} /> : st === "active" ? <Icon d={ICON.refresh} size={11} className="spin" /> : "·"}
                      </span>
                      <span className="mono" style={{ color: "var(--accent)", width: 60 }}>{hexAddr(p.addr)}</span>
                      <span style={{ flex: 1, color: "var(--text-dim)", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }} title={p.file}>{p.displayFile || p.file}</span>
                      <span className="mono" style={{ color: st === "active" ? "var(--accent)" : "var(--text-mute)", width: 42, textAlign: "right" }}>{pct.toFixed(0)}%</span>
                    </div>
                  );
                })}
                {fl.progress && (
                  <div style={{ padding: "var(--s2) var(--s4)", borderTop: "1px solid var(--border)", display: "flex", justifyContent: "space-between", fontSize: 11, color: "var(--text-mute)" }}>
                    <span className="mono">{fmtBytes(fl.progress.done)} / {fmtBytes(fl.progress.total)}</span>
                    <span className="mono">{fl.progress.speed ? fmtBytes(fl.progress.speed) + "/s" : ""} {fl.progress.eta ? "· eta " + fl.progress.eta.toFixed(1) + "s" : ""}</span>
                  </div>
                )}
              </div>
            )}

            {success && !fl.progress && (
              <div style={{ marginTop: "var(--s4)", display: "flex", alignItems: "center", gap: "var(--s3)", padding: "var(--s4)",
                            background: "var(--ok-weak)", border: "1px solid color-mix(in srgb, var(--ok) 30%, transparent)", borderRadius: "var(--r3)" }}>
                <span style={{ width: 30, height: 30, borderRadius: "50%", background: "var(--ok)", color: "#fff", display: "grid", placeItems: "center", animation: "popIn .4s" }}>
                  <Icon d={ICON.check} size={18} />
                </span>
                <div>
                  <div style={{ fontWeight: 600, fontSize: 13.5, color: "var(--ok)" }}>{tr("韌體燒錄成功")}</div>
                  <div style={{ fontSize: 12, color: "var(--text-mute)" }}>{tr("裝置正在重新啟動，稍候即可使用。")}</div>
                </div>
              </div>
            )}
          </Step>
        </div>

        {/* device sidebar */}
        <div style={{ borderLeft: "1px solid var(--border)", background: "var(--panel)", display: "flex", flexDirection: "column", minHeight: 0 }}>
          <DeviceCard fl={fl} />
          <div style={{ padding: "var(--s5)", borderBottom: "1px solid var(--border)" }}>
            <div style={{ fontSize: 11, fontWeight: 700, letterSpacing: ".08em", color: "var(--text-mute)" }}>{tr("遙控")}</div>
            <div style={{ fontSize: 10.5, color: "var(--text-mute)", marginBottom: "var(--s3)" }}>{tr("遙控僅在 115200 可用")}</div>
            <Dpad fl={fl} />
          </div>
          <div style={{ flex: 1, display: "flex", flexDirection: "column", minHeight: 0, padding: "var(--s5)" }}>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "var(--s2)" }}>
              <span style={{ fontSize: 11, fontWeight: 700, letterSpacing: ".08em", color: "var(--text-mute)" }}>{tr("操作記錄")}</span>
              <span className="mono" style={{ fontSize: 10.5, color: "var(--text-mute)" }}>{fl.port} · {fl.baud}</span>
            </div>
            <LogView log={fl.log} style={{ flex: 1, fontSize: 11.5, lineHeight: 1.7, color: "var(--text-dim)" }} />
          </div>
        </div>
      </div>
    </div>
  );
}

// ── Empty state for not-yet-designed tabs ───────────────────────────────────
function StudioEmpty({ label, icon }) {
  return (
    <div style={{ flex: 1, display: "grid", placeItems: "center", minWidth: 0 }}>
      <div style={{ textAlign: "center", maxWidth: 320 }}>
        <div style={{ width: 56, height: 56, borderRadius: "var(--r4)", background: "var(--panel-2)", border: "1px solid var(--border)",
                      display: "grid", placeItems: "center", margin: "0 auto var(--s4)", color: "var(--text-mute)" }}>
          <Icon d={icon} size={24} />
        </div>
        <div className="disp" style={{ fontSize: 18, fontWeight: 600 }}>{label}</div>
        <div style={{ fontSize: 13, color: "var(--text-mute)", marginTop: "var(--s2)", lineHeight: 1.6 }}>
          這個分頁沿用同一套設計系統，規劃中。先確定燒錄頁的方向後即可逐頁套用。
        </div>
      </div>
    </div>
  );
}

// ── Settings page: appearance lives here (accent + font) ────────────────────
function SwatchRow({ value, onChange }) {
  const opts = [["#1f6aa5", "Tool 藍"], ["#3b82f6", "經典藍"], ["#22b8a6", "青"], ["#7c6cf0", "紫"]];
  return (
    <div style={{ display: "flex", gap: "var(--s3)", flexWrap: "wrap" }}>
      {opts.map(([c, name]) => {
        const on = value === c;
        return (
          <button key={c} onClick={() => onChange(c)} title={tr(name)}
            style={{ display: "flex", alignItems: "center", gap: "var(--s2)", cursor: "pointer",
                     padding: "6px 12px 6px 8px", borderRadius: "var(--rpill)", transition: "all .14s",
                     border: "1px solid " + (on ? "var(--accent-line)" : "var(--border)"),
                     background: on ? "var(--accent-weak)" : "var(--panel-2)", color: on ? "var(--text)" : "var(--text-dim)" }}>
            <span style={{ width: 18, height: 18, borderRadius: "50%", background: c, flex: "none",
                           boxShadow: on ? "0 0 0 2px var(--panel), 0 0 0 4px " + c : "none" }} />
            <span style={{ fontSize: 12.5, fontWeight: 600 }}>{tr(name)}</span>
          </button>
        );
      })}
    </div>
  );
}

function SettingRow({ label, sub, children }) {
  return (
    <div style={{ display: "flex", alignItems: "flex-start", justifyContent: "space-between", gap: "var(--s5)",
                  padding: "var(--s4) 0", borderTop: "1px solid var(--border)" }}>
      <div style={{ minWidth: 0 }}>
        <div style={{ fontSize: 13.5, fontWeight: 600 }}>{label}</div>
        {sub && <div style={{ fontSize: 12, color: "var(--text-mute)", marginTop: 2 }}>{sub}</div>}
      </div>
      <div style={{ flex: "none" }}>{children}</div>
    </div>
  );
}

function StudioSettings({ accent, font, onAccent, onFont, fonts, lang, onLang }) {
  const LANGS = [["tw", "繁體中文"], ["cn", "简体中文"], ["en", "English"]];
  return (
    <div style={{ flex: 1, display: "flex", flexDirection: "column", minWidth: 0 }}>
      <div style={{ padding: "var(--s5) var(--s6)", borderBottom: "1px solid var(--border)" }}>
        <div className="disp" style={{ fontSize: 21, fontWeight: 600, letterSpacing: "-.02em" }}>{tr("工具設定")}</div>
        <div style={{ fontSize: 13, color: "var(--text-mute)" }}>{tr("調整介面外觀，設定會自動保存")}</div>
      </div>
      <div className="scroll" style={{ overflow: "auto", padding: "var(--s6)" }}>
        <div style={{ maxWidth: 640 }}>
          <div style={{ fontSize: 11, fontWeight: 700, letterSpacing: ".08em", color: "var(--text-mute)", marginBottom: "var(--s2)" }}>{tr("外觀")}</div>
          <div style={{ background: "var(--panel)", border: "1px solid var(--border)", borderRadius: "var(--r4)", padding: "0 var(--s5)" }}>
            <SettingRow label={tr("語言")} sub={tr("工具介面的顯示語言")}>
              <div style={{ display: "flex", gap: 2, background: "var(--panel-3)", borderRadius: "var(--rpill)", padding: 2 }}>
                {LANGS.map(([k, l]) => (
                  <button key={k} onClick={() => onLang(k)} style={{ border: "none", cursor: "pointer", fontSize: 12.5, padding: "5px 14px", borderRadius: "var(--rpill)",
                    background: lang === k ? "var(--accent)" : "transparent", color: lang === k ? "#fff" : "var(--text-mute)", fontWeight: 600, whiteSpace: "nowrap" }}>{l}</button>
                ))}
              </div>
            </SettingRow>
            <SettingRow label={tr("主色")} sub={tr("按鈕、進度、強調元素的顏色")}>
              <SwatchRow value={accent} onChange={onAccent} />
            </SettingRow>
            <SettingRow label={tr("字體")} sub={tr("介面文字字型")}>
              <select className="fld" style={{ width: 200 }} value={font} onChange={(e) => onFont(e.target.value)}>
                {fonts.map((f) => <option key={f} value={f}>{tr(f)}</option>)}
              </select>
            </SettingRow>
          </div>
          <div style={{ fontSize: 11.5, color: "var(--text-mute)", marginTop: "var(--s4)", lineHeight: 1.6 }}>
            {tr("介面固定為深色主題。語言切換即時套用於導覽與各頁標題等主要介面文字。")}
          </div>
        </div>
      </div>
    </div>
  );
}

Object.assign(window, { StudioFlasher, StudioEmpty, StudioSettings });
