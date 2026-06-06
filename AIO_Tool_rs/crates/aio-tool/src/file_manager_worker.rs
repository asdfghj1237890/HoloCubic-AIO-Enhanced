//! Long-lived worker thread for the File Manager tab.
//!
//! Owns a [`TcpTransport`] and forwards typed commands from the UI:
//! `ListDir(path)`, `ReadFile(path)`, `RemoveFile(name)`, `RenameFile(name)`,
//! `GetFileInfo(name)`. Replies arrive over the bus as
//! `AppEvent::FileManager*` variants.
//!
//! Carries the Plan 7 polish lessons:
//! - No internal sleep — TcpTransport's 500 ms read timeout paces the loop.
//! - Cancel flag is the sole shutdown signal (no Disconnect cmd variant).
//! - bus_tx.send() failure breaks the loop (App dropped the receiver).

use std::net::SocketAddr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{channel, Receiver, Sender};
use std::sync::Arc;
use std::time::Duration;

use aio_device::tcp::TcpTransport;
use aio_device::{Transport, TransportError};
use aio_protocol::file::{DirList, FileGetInfo, FileRead, FileRemove, FileRename};
use aio_protocol::{ActionType, MsgHead, WireDecode, HEADER_SIZE};

use crate::bus::{AppEvent, AppEventTx};

/// Commands the UI thread sends to the File Manager worker.
#[derive(Debug)]
pub enum FmCmd {
    /// Request directory listing for `path`.
    ListDir {
        /// Absolute path of the directory.
        path: String,
    },
    /// Request file contents for `path`.
    ReadFile {
        /// Absolute path of the file.
        path: String,
    },
    /// Delete file at `name` (full path on device).
    RemoveFile {
        /// Path of the file to remove.
        name: String,
    },
    /// Rename file. NOTE: Plan 1 B1 means action_type goes out as
    /// `DirRename` and both name fields hold the same input — no actual
    /// rename happens until firmware-side is fixed.
    RenameFile {
        /// Path of the file (used for both fields per B1).
        name: String,
    },
    /// Query file metadata for `name`. Plan 1 B2: action_type goes out
    /// as `DirList`; firmware response format isn't reverse-engineered,
    /// UI displays raw bytes as hex preview.
    GetFileInfo {
        /// Path of the file to query.
        name: String,
    },
}

/// Spawn the worker. Returns `(cmd_tx, cancel_flag)`.
pub fn spawn(addr: SocketAddr, bus_tx: AppEventTx) -> (Sender<FmCmd>, Arc<AtomicBool>) {
    let (cmd_tx, cmd_rx) = channel();
    let cancel = Arc::new(AtomicBool::new(false));
    let cancel_for_worker = cancel.clone();

    std::thread::spawn(move || worker_loop(addr, cmd_rx, bus_tx, cancel_for_worker));
    (cmd_tx, cancel)
}

fn worker_loop(
    addr: SocketAddr,
    cmd_rx: Receiver<FmCmd>,
    bus_tx: AppEventTx,
    cancel: Arc<AtomicBool>,
) {
    let mut transport = TcpTransport::new(addr)
        .with_read_timeout(Duration::from_millis(500))
        .with_reconnect_interval(Duration::from_millis(500));

    // First send — if the receiver is already gone (App closed mid-connect),
    // bail before spending more cycles.
    if bus_tx.send(AppEvent::FileManagerConnected).is_err() {
        return;
    }

    // Stream parser accumulator — TCP delivers bytes; messages may arrive
    // partial across reads.
    let mut accum: Vec<u8> = Vec::new();
    let mut read_buf = vec![0u8; 8192];

    loop {
        if cancel.load(Ordering::Relaxed) {
            break;
        }

        // 1. Try to process a command (non-blocking).
        match cmd_rx.try_recv() {
            Ok(cmd) => {
                if let Err(e) = handle_cmd(&mut transport, cmd) {
                    let _ = bus_tx.send(AppEvent::FileManagerFinished(Err(e)));
                    break;
                }
            }
            Err(std::sync::mpsc::TryRecvError::Empty) => {}
            // UI dropped the cmd_tx (app closed) — clean shutdown.
            Err(std::sync::mpsc::TryRecvError::Disconnected) => break,
        }

        // 2. Read available bytes; accumulate; parse complete messages.
        //    transport.read blocks up to TcpTransport's 500 ms timeout —
        //    this is what paces the loop (no explicit sleep needed).
        match transport.read(&mut read_buf) {
            Ok(n) if n > 0 => {
                accum.extend_from_slice(&read_buf[..n]);
                // Drain as many complete messages as we have.
                loop {
                    match try_parse_one(&accum, &bus_tx) {
                        ParseStep::Consumed(n) => {
                            accum.drain(..n);
                            continue;
                        }
                        ParseStep::Incomplete => break,
                        ParseStep::BusGone => return,
                    }
                }
            }
            Ok(_) => {}
            Err(TransportError::TimedOut) => {}
            Err(e) => {
                let _ = bus_tx.send(AppEvent::FileManagerFinished(Err(format!("read: {e}"))));
                break;
            }
        }
    }

    transport.close();
    let _ = bus_tx.send(AppEvent::FileManagerFinished(Ok(())));
}

/// Outcome of one parsing attempt against the accumulator.
enum ParseStep {
    /// One complete message parsed; drain N bytes and try again.
    Consumed(usize),
    /// Need more bytes; stop parsing for this iteration.
    Incomplete,
    /// `bus_tx.send` failed — App dropped the receiver. Caller should exit.
    BusGone,
}

fn handle_cmd(transport: &mut TcpTransport, cmd: FmCmd) -> Result<(), String> {
    let bytes = match cmd {
        FmCmd::ListDir { path } => DirList::request(&path)
            .to_wire()
            .map_err(|e| format!("encode DirList: {e}"))?,
        FmCmd::ReadFile { path } => FileRead::request(&path)
            .to_wire()
            .map_err(|e| format!("encode FileRead: {e}"))?,
        FmCmd::RemoveFile { name } => FileRemove::new(&name)
            .to_wire()
            .map_err(|e| format!("encode FileRemove: {e}"))?,
        FmCmd::RenameFile { name } => FileRename::new(&name)
            .to_wire()
            .map_err(|e| format!("encode FileRename: {e}"))?,
        FmCmd::GetFileInfo { name } => FileGetInfo::request(&name)
            .to_wire()
            .map_err(|e| format!("encode FileGetInfo: {e}"))?,
    };
    transport.write_all(&bytes).map_err(|e| e.to_string())?;
    Ok(())
}

/// Attempt to parse one message from `accum`.
///
/// Returns:
/// - `Consumed(n)` if a complete message was parsed; drain `n` bytes.
/// - `Incomplete` if we need more bytes.
/// - `BusGone` if `bus_tx.send` failed (App dropped); caller exits.
fn try_parse_one(accum: &[u8], bus_tx: &AppEventTx) -> ParseStep {
    // Need at least the 8-byte file header (7-byte MsgHead + 1-byte
    // duplicate action) before we can decide what message this is.
    if accum.len() < HEADER_SIZE + 1 {
        return ParseStep::Incomplete;
    }
    let header = match MsgHead::decode(accum) {
        Ok((h, _)) => h,
        Err(_) => return ParseStep::Incomplete,
    };
    match header.action {
        ActionType::DirList => {
            // DirList::from_wire reads header + 1 + 99 (path) + remaining
            // as dir_info. We assume one complete message per network read
            // arrival (legacy firmware sends complete chunks).
            let msg = match DirList::from_wire(accum) {
                Ok((m, _)) => m,
                Err(_) => return ParseStep::Incomplete,
            };
            let entries: Vec<String> = serde_json::from_slice(&msg.dir_info).unwrap_or_default();
            if bus_tx
                .send(AppEvent::FileManagerDirListed {
                    path: msg.dir_path,
                    entries,
                })
                .is_err()
            {
                return ParseStep::BusGone;
            }
            ParseStep::Consumed(accum.len())
        }
        ActionType::FileRead => {
            let msg = match FileRead::from_wire(accum) {
                Ok((m, _)) => m,
                Err(_) => return ParseStep::Incomplete,
            };
            if bus_tx
                .send(AppEvent::FileManagerFileBytes {
                    // Firmware response has no path echo — Settings UI
                    // pairs with the last ReadFile request locally.
                    path: String::new(),
                    bytes: msg.data,
                })
                .is_err()
            {
                return ParseStep::BusGone;
            }
            ParseStep::Consumed(accum.len())
        }
        _ => {
            // Unhandled action — Plan 8 doesn't expect anything else
            // from firmware in scope. Treat as incomplete so we don't
            // stall; subsequent bytes may include a known action.
            ParseStep::Incomplete
        }
    }
}
