// flash-sim.jsx — shared HoloCubic flashing simulation engine.
// Exposes a React hook (useFlasher) + constants to window so all three
// design directions drive identical, authentic-feeling behaviour:
// port enumeration, connect, espflash-style streaming log, per-partition
// progress with byte counts / speed / ETA, cancel, and D-pad remote sends.

// --- Mock environment -------------------------------------------------------
const FLASH_PORTS = [
  { name: "COM5", desc: "USB-SERIAL CH340 (COM5)" },
  { name: "COM3", desc: "Silicon Labs CP210x (COM3)" },
  { name: "COM7", desc: "USB JTAG/serial debug unit (COM7)" },
];

const BAUD_RATES = ["9600", "38400", "57600", "115200", "230400", "460800", "576000", "921600", "1152000"];

// The four ESP32 partitions the HoloCubic flasher writes.
const DEFAULT_PARTITIONS = [
  { addr: 0x1000,  key: "bootloader", name: "Bootloader", file: "bootloader_qio_80m.bin", bytes: 17616,  enabled: true,  required: false },
  { addr: 0x8000,  key: "partitions", name: "Partitions", file: "partitions.bin",          bytes: 3072,   enabled: true,  required: false },
  { addr: 0xe000,  key: "boot_app0",  name: "boot_app0",  file: "boot_app0.bin",            bytes: 8192,   enabled: true,  required: false },
  { addr: 0x10000, key: "firmware",   name: "韌體 user_data", file: "HoloCubic_AIO_firmware_v2.6.7.bin", bytes: 1939456, enabled: true, required: true },
];

const hex = (n) => "0x" + n.toString(16).padStart(5, "0");
function fmtBytes(n) {
  if (n >= 1024 * 1024) return (n / 1048576).toFixed(2) + " MB";
  if (n >= 1024) return (n / 1024).toFixed(1) + " KB";
  return n + " B";
}
const now = () => {
  const d = new Date();
  const p = (x, n = 2) => String(x).padStart(n, "0");
  return `${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}.${p(d.getMilliseconds(), 3)}`;
};

const REMOTE = {
  up:    { label: "上",  cmd: "~U" },
  left:  { label: "左",  cmd: "~L" },
  right: { label: "右",  cmd: "~R" },
  ok:    { label: "確認", cmd: "~F" },
  home:  { label: "首頁", cmd: "~H" },
};

// --- The hook ---------------------------------------------------------------
function useFlasher() {
  const { useState, useRef, useCallback, useEffect } = React;
  const [ports, setPorts] = useState(FLASH_PORTS);
  const [port, setPort] = useState(FLASH_PORTS[0].name);
  const [baud, setBaud] = useState("115200");
  const [conn, setConn] = useState("disconnected"); // disconnected|connecting|connected
  const [chip, setChip] = useState(null);
  const [parts, setParts] = useState(() => DEFAULT_PARTITIONS.map((p) => ({ ...p })));
  const [op, setOp] = useState("none");             // none|erasing|flashing|done|error
  const [progress, setProgress] = useState(null);   // {idx,name,addr,percent,done,total,speed,eta}
  const [log, setLog] = useState([
    { level: "muted", text: "HoloCubic AIO Flasher — 待命中。連接裝置後即可開始。" },
  ]);
  const timer = useRef(null);
  const cancelRef = useRef(false);

  const pushLog = useCallback((text, level = "info") => {
    setLog((L) => [...L.slice(-220), { level, text, t: now() }]);
  }, []);

  const refreshPorts = useCallback(() => {
    pushLog("掃描序列埠…", "muted");
    setPorts(FLASH_PORTS);
    pushLog(`找到 ${FLASH_PORTS.length} 個裝置：${FLASH_PORTS.map((p) => p.name).join(", ")}`, "ok");
  }, [pushLog]);

  const stop = useCallback(() => {
    if (timer.current) { clearInterval(timer.current); timer.current = null; }
  }, []);

  const connect = useCallback(() => {
    if (conn === "connected" || conn === "connecting") return;
    setConn("connecting");
    pushLog(`開啟 ${port} @ ${baud} …`, "info");
    setTimeout(() => {
      setChip({ model: "ESP32-D0WD-V3", rev: "v3.0", mac: "7c:9e:bd:48:1a:30", flash: "4 MB" });
      setConn("connected");
      pushLog("Chip: ESP32-D0WD-V3 (revision v3.0)", "ok");
      pushLog("Features: WiFi, BT, Dual Core 240MHz · Flash 4 MB · MAC 7c:9e:bd:48:1a:30", "muted");
    }, 900);
  }, [conn, port, baud, pushLog]);

  const disconnect = useCallback(() => {
    stop(); setConn("disconnected"); setChip(null); setOp("none"); setProgress(null);
    pushLog(`關閉 ${port}。`, "muted");
  }, [stop, port, pushLog]);

  const togglePart = useCallback((i) => {
    setParts((P) => P.map((p, j) => (j === i ? { ...p, enabled: !p.enabled } : p)));
  }, []);
  const pickFile = useCallback((i) => {
    setParts((P) => P.map((p, j) => (j === i ? { ...p, enabled: true } : p)));
    pushLog(`已選擇 ${parts[i].name} 的 bin 檔。`, "muted");
  }, [parts, pushLog]);

  const cancel = useCallback(() => {
    cancelRef.current = true;
    pushLog("已要求取消…", "warn");
  }, [pushLog]);

  // Core simulated write loop, shared by erase + flash.
  const run = useCallback((mode) => {
    if (conn !== "connected" || op === "erasing" || op === "flashing") return;
    cancelRef.current = false;
    const queue = mode === "flash" ? parts.filter((p) => p.enabled) : [];
    if (mode === "flash" && queue.length === 0) { pushLog("沒有勾選任何分割區。", "warn"); return; }

    setOp(mode === "flash" ? "flashing" : "erasing");
    pushLog(mode === "flash"
      ? `開始燒錄 ${queue.length} 個分割區…`
      : "Erasing flash (this may take a while)…", "info");

    if (mode === "erase") {
      let pct = 0;
      setProgress({ idx: 0, name: "清空晶片", addr: 0, percent: 0, done: 0, total: 1, speed: 0, eta: 0 });
      timer.current = setInterval(() => {
        if (cancelRef.current) {
          stop(); setOp("none"); setProgress(null);
          pushLog("Operation cancelled (chip erase already completed).", "warn");
          return;
        }
        pct = Math.min(100, pct + 7 + Math.random() * 6);
        setProgress((pr) => ({ ...pr, percent: pct }));
        if (pct >= 100) {
          stop(); setOp("done"); setProgress(null);
          pushLog("Chip erase completed successfully.", "ok");
          setTimeout(() => setOp("none"), 400);
        }
      }, 160);
      return;
    }

    // flash: walk the queue partition by partition
    let qi = 0;
    const startPart = () => {
      const p = queue[qi];
      let done = 0;
      const total = p.bytes;
      const t0 = performance.now();
      pushLog(`Writing ${fmtBytes(total)} at ${hex(p.addr)} — ${p.file}`, "info");
      setProgress({ idx: qi, name: p.name, addr: p.addr, percent: 0, done: 0, total, speed: 0, eta: 0 });
      timer.current = setInterval(() => {
        if (cancelRef.current) {
          stop(); setOp("none"); setProgress(null);
          pushLog("燒錄已取消。", "warn");
          return;
        }
        const step = Math.max(8192, total * (0.08 + Math.random() * 0.06));
        done = Math.min(total, done + step);
        const secs = (performance.now() - t0) / 1000;
        const speed = done / Math.max(0.001, secs);
        const eta = (total - done) / Math.max(1, speed);
        setProgress({ idx: qi, name: p.name, addr: p.addr, percent: (done / total) * 100, done, total, speed, eta });
        if (done >= total) {
          stop();
          pushLog(`Wrote ${fmtBytes(total)} at ${hex(p.addr)} in ${secs.toFixed(1)}s (${fmtBytes(speed)}/s)`, "ok");
          qi += 1;
          if (qi < queue.length) { startPart(); }
          else {
            setProgress(null); setOp("done");
            pushLog("Hard resetting via RTS pin…", "muted");
            pushLog("✓ 韌體燒錄成功！裝置即將重新啟動。", "ok");
            setTimeout(() => setOp("none"), 600);
          }
        }
      }, 130);
    };
    startPart();
  }, [conn, op, parts, pushLog, stop]);

  const erase = useCallback(() => run("erase"), [run]);
  const flash = useCallback(() => run("flash"), [run]);

  const reboot = useCallback(() => {
    if (conn !== "connected") { pushLog("請先連接裝置再重新開機。", "warn"); return; }
    pushLog("→ 重新開機指令 (RTS/DTR reset)", "accent");
    pushLog("裝置重新啟動中…", "muted");
  }, [conn, pushLog]);

  const sendRemote = useCallback((dir) => {
    const r = REMOTE[dir];
    if (!r) return;
    if (conn !== "connected") { pushLog("請先連接裝置再使用遙控。", "warn"); return; }
    pushLog(`→ ${r.cmd}  (${r.label})`, "accent");
  }, [conn, pushLog]);

  useEffect(() => () => stop(), [stop]);

  const busy = op === "erasing" || op === "flashing";
  return {
    ports, port, setPort, baud, setBaud, conn, chip, parts, op, busy, progress, log,
    refreshPorts, connect, disconnect, togglePart, pickFile, erase, flash, cancel, sendRemote, reboot,
    enabledCount: parts.filter((p) => p.enabled).length,
    totalBytes: parts.filter((p) => p.enabled).reduce((s, p) => s + p.bytes, 0),
  };
}

Object.assign(window, {
  useFlasher, FLASH_PORTS, BAUD_RATES, DEFAULT_PARTITIONS, REMOTE, fmtBytes, hexAddr: hex,
});
