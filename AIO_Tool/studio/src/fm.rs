//! File-Manager worker thread for the Tauri Studio host.
//!
//! Mirrors the egui tool's `file_manager_worker.rs` semantically — a
//! long-lived TCP transport, a FIFO of pending requests (so B2's
//! DirList-piggybacked Properties responses can be disambiguated), and
//! a stream parser that drains complete messages from a byte
//! accumulator on every tick. The differences from the egui worker:
//!
//! - Events are emitted directly on a Tauri `AppHandle` via
//!   `Emitter::emit("fm:event", _)` rather than over `AppEventTx`.
//! - The Properties / file-read responses ride the same event channel
//!   instead of being separate enum variants.

use std::collections::VecDeque;
use std::net::SocketAddr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{channel, Receiver, Sender, TryRecvError};
use std::sync::Arc;
use std::time::Duration;

use serde::Serialize;
use tauri::{AppHandle, Emitter};

use aio_device::tcp::TcpTransport;
use aio_device::{Transport, TransportError};
use aio_protocol::file::{DirList, FileGetInfo, FileRead, FileRemove, FileRename};
use aio_protocol::{ActionType, MsgHead, WireDecode, HEADER_SIZE};

/// Commands the Tauri command surface sends to the running worker.
#[derive(Debug)]
pub enum FmCmd {
    /// Request a directory listing.
    ListDir { path: String },
    /// Request file contents.
    ReadFile { path: String },
    /// Remove a file (no response).
    RemoveFile { name: String },
    /// Rename a file (no response). Preserved-bug B1 — see aio-protocol.
    RenameFile { name: String },
    /// Query file metadata. Preserved-bug B2 — response uses DirList action.
    GetFileInfo { name: String },
}

/// Tracks which response shape to expect for each in-flight request.
#[derive(Debug)]
enum RequestKind {
    ListDir,
    ReadFile { path: String },
    GetFileInfo { path: String },
}

/// One event emitted to JS via `fm:event`.
///
/// `kind` is the JSON tag; the prototype's `useFiles` hook dispatches
/// on it. The shapes here intentionally mirror the egui app's
/// `AppEvent::FileManager*` variants so reasoning about the two paths
/// stays parallel.
#[derive(Serialize, Clone)]
#[serde(tag = "kind", rename_all = "kebab-case")]
#[allow(dead_code)] // Warning is part of the JS-facing event surface; reserved for non-fatal paths.
pub enum FmEventDto {
    /// Worker started, transport reaching for the device.
    Connected,
    /// `ListDir` response — `entries` are decoded from `dir_info` JSON.
    DirListed {
        /// Path that was listed.
        path: String,
        /// Names (files + dirs interleaved, as legacy firmware returns).
        entries: Vec<String>,
    },
    /// `ReadFile` response — full file bytes for `path`.
    FileBytes {
        /// Path of the file the bytes belong to.
        path: String,
        /// Base64-encoded contents (JSON-safe across the IPC bridge).
        bytes_b64: String,
    },
    /// Raw metadata bytes from a B2 GetFileInfo response.
    Properties {
        /// Path queried.
        path: String,
        /// Raw bytes after the 8-byte file header. Hex-rendered in JS.
        raw_b64: String,
    },
    /// Human-readable status line for the prototype's log column.
    Log { message: String },
    /// Recoverable warning (still alive, but worth surfacing).
    Warning { message: String },
    /// Worker finished — clean shutdown or fatal error.
    Finished {
        /// True on graceful shutdown (Disconnect / cancel).
        ok: bool,
        /// Populated on fatal error.
        error: Option<String>,
    },
}

/// Spawn the worker thread. Returns `(cmd_tx, cancel_flag)`.
///
/// The Tauri command surface owns the returned sender; the cancel flag
/// goes to the `disconnect_fm` command which flips it to break the loop.
pub fn spawn(addr: SocketAddr, app: AppHandle) -> (Sender<FmCmd>, Arc<AtomicBool>) {
    let (cmd_tx, cmd_rx) = channel();
    let cancel = Arc::new(AtomicBool::new(false));
    let cancel_for_worker = cancel.clone();
    std::thread::spawn(move || worker_loop(addr, cmd_rx, app, cancel_for_worker));
    (cmd_tx, cancel)
}

fn worker_loop(addr: SocketAddr, cmd_rx: Receiver<FmCmd>, app: AppHandle, cancel: Arc<AtomicBool>) {
    let mut transport = TcpTransport::new(addr)
        .with_read_timeout(Duration::from_millis(500))
        .with_reconnect_interval(Duration::from_millis(500));

    emit(&app, FmEventDto::Connected);

    let mut accum: Vec<u8> = Vec::new();
    let mut read_buf = vec![0u8; 8192];
    let mut pending: VecDeque<RequestKind> = VecDeque::new();
    let mut final_error: Option<String> = None;

    loop {
        if cancel.load(Ordering::Relaxed) {
            break;
        }

        match cmd_rx.try_recv() {
            Ok(cmd) => {
                if let Err(e) = handle_cmd(&mut transport, cmd, &mut pending, &app) {
                    final_error = Some(e);
                    break;
                }
            }
            Err(TryRecvError::Empty) => {}
            Err(TryRecvError::Disconnected) => break,
        }

        match transport.read(&mut read_buf) {
            Ok(n) if n > 0 => {
                accum.extend_from_slice(&read_buf[..n]);
                while drain_one(&mut accum, &mut pending, &app) {}
            }
            Ok(_) => {}
            Err(TransportError::TimedOut) => {}
            Err(e) => {
                final_error = Some(format!("read: {e}"));
                break;
            }
        }
    }

    transport.close();
    let ok = final_error.is_none();
    emit(
        &app,
        FmEventDto::Finished {
            ok,
            error: final_error,
        },
    );
}

fn handle_cmd(
    transport: &mut TcpTransport,
    cmd: FmCmd,
    pending: &mut VecDeque<RequestKind>,
    app: &AppHandle,
) -> Result<(), String> {
    let (bytes, kind, log_msg) = match cmd {
        FmCmd::ListDir { path } => {
            let bytes = DirList::request(&path)
                .to_wire()
                .map_err(|e| format!("encode DirList: {e}"))?;
            let log = format!("\u{2192} DirList {path}");
            (bytes, Some(RequestKind::ListDir), log)
        }
        FmCmd::ReadFile { path } => {
            let bytes = FileRead::request(&path)
                .to_wire()
                .map_err(|e| format!("encode FileRead: {e}"))?;
            let log = format!("\u{2192} FileRead {path}");
            (bytes, Some(RequestKind::ReadFile { path }), log)
        }
        FmCmd::RemoveFile { name } => {
            let bytes = FileRemove::new(&name)
                .to_wire()
                .map_err(|e| format!("encode FileRemove: {e}"))?;
            let log = format!("\u{2192} FileRemove {name}");
            (bytes, None, log)
        }
        FmCmd::RenameFile { name } => {
            let bytes = FileRename::new(&name)
                .to_wire()
                .map_err(|e| format!("encode FileRename: {e}"))?;
            let log = format!("\u{2192} FileRename {name}");
            (bytes, None, log)
        }
        FmCmd::GetFileInfo { name } => {
            let bytes = FileGetInfo::request(&name)
                .to_wire()
                .map_err(|e| format!("encode FileGetInfo: {e}"))?;
            let log = format!("\u{2192} GetInfo {name}");
            (bytes, Some(RequestKind::GetFileInfo { path: name }), log)
        }
    };
    transport.write_all(&bytes).map_err(|e| e.to_string())?;
    if let Some(k) = kind {
        pending.push_back(k);
    }
    emit(app, FmEventDto::Log { message: log_msg });
    Ok(())
}

/// Drain ONE complete message from `accum`. Returns true if a message
/// was consumed (caller may try again), false if more bytes are needed.
fn drain_one(accum: &mut Vec<u8>, pending: &mut VecDeque<RequestKind>, app: &AppHandle) -> bool {
    if accum.len() < HEADER_SIZE + 1 {
        return false;
    }
    let header = match MsgHead::decode(accum) {
        Ok((h, _)) => h,
        Err(_) => return false,
    };
    match header.action {
        ActionType::DirList => {
            let event = match pending.pop_front() {
                Some(RequestKind::GetFileInfo { path }) => {
                    let start = HEADER_SIZE + 1;
                    let raw = if accum.len() > start {
                        accum[start..].to_vec()
                    } else {
                        Vec::new()
                    };
                    FmEventDto::Properties {
                        path,
                        raw_b64: base64_encode(&raw),
                    }
                }
                _ => {
                    let msg = match DirList::from_wire(accum) {
                        Ok((m, _)) => m,
                        Err(_) => return false,
                    };
                    let entries: Vec<String> =
                        serde_json::from_slice(&msg.dir_info).unwrap_or_default();
                    FmEventDto::DirListed {
                        path: msg.dir_path,
                        entries,
                    }
                }
            };
            emit(app, event);
            let n = accum.len();
            accum.drain(..n);
            true
        }
        ActionType::FileRead => {
            let msg = match FileRead::from_wire(accum) {
                Ok((m, _)) => m,
                Err(_) => return false,
            };
            let path = match pending.pop_front() {
                Some(RequestKind::ReadFile { path }) => path,
                _ => String::new(),
            };
            emit(
                app,
                FmEventDto::FileBytes {
                    path,
                    bytes_b64: base64_encode(&msg.data),
                },
            );
            let n = accum.len();
            accum.drain(..n);
            true
        }
        _ => false,
    }
}

fn emit(app: &AppHandle, evt: FmEventDto) {
    let _ = app.emit("fm:event", evt);
}

/// Minimal base64 encoder (RFC 4648, no padding option used).
///
/// Avoids dragging in the `base64` crate for a single use site. Encodes
/// 3 bytes → 4 chars; pads with `=` to a multiple of 4.
fn base64_encode(input: &[u8]) -> String {
    const TABLE: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut out = String::with_capacity(input.len().div_ceil(3) * 4);
    let mut i = 0;
    while i + 3 <= input.len() {
        let n = ((input[i] as u32) << 16) | ((input[i + 1] as u32) << 8) | input[i + 2] as u32;
        out.push(TABLE[((n >> 18) & 0x3f) as usize] as char);
        out.push(TABLE[((n >> 12) & 0x3f) as usize] as char);
        out.push(TABLE[((n >> 6) & 0x3f) as usize] as char);
        out.push(TABLE[(n & 0x3f) as usize] as char);
        i += 3;
    }
    let rem = input.len() - i;
    if rem == 1 {
        let n = (input[i] as u32) << 16;
        out.push(TABLE[((n >> 18) & 0x3f) as usize] as char);
        out.push(TABLE[((n >> 12) & 0x3f) as usize] as char);
        out.push('=');
        out.push('=');
    } else if rem == 2 {
        let n = ((input[i] as u32) << 16) | ((input[i + 1] as u32) << 8);
        out.push(TABLE[((n >> 18) & 0x3f) as usize] as char);
        out.push(TABLE[((n >> 12) & 0x3f) as usize] as char);
        out.push(TABLE[((n >> 6) & 0x3f) as usize] as char);
        out.push('=');
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn b64_empty() {
        assert_eq!(base64_encode(b""), "");
    }

    #[test]
    fn b64_known_vectors() {
        // RFC 4648 §10 vectors.
        assert_eq!(base64_encode(b"f"), "Zg==");
        assert_eq!(base64_encode(b"fo"), "Zm8=");
        assert_eq!(base64_encode(b"foo"), "Zm9v");
        assert_eq!(base64_encode(b"foob"), "Zm9vYg==");
        assert_eq!(base64_encode(b"fooba"), "Zm9vYmE=");
        assert_eq!(base64_encode(b"foobar"), "Zm9vYmFy");
    }
}
