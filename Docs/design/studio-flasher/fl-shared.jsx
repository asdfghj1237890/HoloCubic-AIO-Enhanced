// fl-shared.jsx — small primitives reused across the three directions.

// Auto-sticks to bottom as new lines arrive (unless the user scrolled up).
function LogView({ log, style, lineStyle }) {
  const { useRef, useLayoutEffect } = React;
  const ref = useRef(null);
  const stick = useRef(true);
  useLayoutEffect(() => {
    const el = ref.current;
    if (el && stick.current) el.scrollTop = el.scrollHeight;
  }, [log]);
  const onScroll = (e) => {
    const el = e.target;
    stick.current = el.scrollHeight - el.scrollTop - el.clientHeight < 28;
  };
  return (
    <div ref={ref} onScroll={onScroll} className="scroll mono" style={{ overflowY:"auto", overflowX:"hidden", ...style }}>
      {log.map((l, i) => (
        <div key={i} className={"lvl-" + (l.level || "info")} style={{ display:"flex", gap:"10px", minWidth:0, ...lineStyle }}>
          {l.t && <span style={{ color:"var(--text-mute)", flex:"none", opacity:.7 }}>{l.t}</span>}
          <span style={{ flex:1, minWidth:0, whiteSpace:"pre-wrap", wordBreak:"break-word" }}>{l.text}</span>
        </div>
      ))}
    </div>
  );
}

// Tiny inline SVG glyphs (no heavy icon lib). stroke = currentColor.
function Icon({ d, size = 16, fill = false, stroke = 2 }) {
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" fill={fill ? "currentColor" : "none"}
         stroke="currentColor" strokeWidth={stroke} strokeLinecap="round" strokeLinejoin="round"
         style={{ flex:"none" }}>
      {Array.isArray(d) ? d.map((p, i) => <path key={i} d={p} />) : <path d={d} />}
    </svg>
  );
}
const ICON = {
  refresh: "M21 12a9 9 0 1 1-2.64-6.36M21 4v4h-4",
  bolt: "M13 2 4 14h7l-1 8 9-12h-7l1-8z",
  plug: ["M9 2v6","M15 2v6","M6 8h12v4a6 6 0 0 1-12 0V8z","M12 18v4"],
  trash: ["M3 6h18","M8 6V4h8v2","M6 6l1 14h10l1-14"],
  x: "M6 6l12 12M18 6 6 18",
  check: "M20 6 9 17l-5-5",
  file: ["M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z","M14 2v6h6"],
  chip: ["M9 9h6v6H9z","M4 9h2M4 15h2M18 9h2M18 15h2M9 4v2M15 4v2M9 18v2M15 18v2","M6 6h12v12H6z"],
  up: "M12 19V5M5 12l7-7 7 7",
  down: "M12 5v14M19 12l-7 7-7-7",
  left: "M19 12H5M12 19l-7-7 7-7",
  right: "M5 12h14M12 5l7 7-7 7",
  home: ["M3 11l9-8 9 8","M5 10v10h14V10"],
  folder: "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z",
};

Object.assign(window, { LogView, Icon, ICON });
