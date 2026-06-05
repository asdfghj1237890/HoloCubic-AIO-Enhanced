//! Long-lived worker thread for the Settings tab.
//!
//! Unlike the Flasher tab (fire-and-forget per op), Settings needs a
//! persistent connection: connect once, then read-all / write-changes
//! over the same `SerialTransport`. The worker owns the transport;
//! the UI thread communicates via a `Sender<SettingsCmd>`.
//!
//! Worker loop:
//! - On each iteration, try_recv a command (non-blocking).
//! - Sleep 10ms between iterations to avoid spinning.
//! - On `Disconnect` cmd, transport error, or cancel: drop transport, exit.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{channel, Receiver, Sender};
use std::sync::Arc;
use std::time::Duration;

use aio_device::serial::SerialTransport;
use aio_device::{Transport, TransportError};
use aio_protocol::{SettingMsg, ValueType};

use crate::bus::{AppEvent, AppEventTx};

/// Commands the UI thread sends to the Settings worker.
#[derive(Debug)]
pub enum SettingsCmd {
    /// Send a `SettingGet` for one key.
    Get {
        /// Namespace string ("sys" etc.).
        prefs_name: String,
        /// Key name.
        key: String,
    },
    /// Send a `SettingSet` for one key.
    Set {
        /// Namespace string.
        prefs_name: String,
        /// Key name.
        key: String,
        /// Value type ("String" or "UChar").
        value_type: ValueType,
        /// New value.
        value: String,
    },
    /// Terminate the worker loop cleanly.
    Disconnect,
}

/// Spawn the worker. Returns a `(cmd_tx, cancel_flag)` pair the UI keeps.
/// Connection failures are reported via `AppEvent::SettingsFinished(Err)`.
pub fn spawn(
    port: String,
    baud: u32,
    bus_tx: AppEventTx,
) -> (Sender<SettingsCmd>, Arc<AtomicBool>) {
    let (cmd_tx, cmd_rx) = channel();
    let cancel = Arc::new(AtomicBool::new(false));
    let cancel_for_worker = cancel.clone();

    std::thread::spawn(move || {
        worker_loop(port, baud, cmd_rx, bus_tx, cancel_for_worker);
    });

    (cmd_tx, cancel)
}

fn worker_loop(
    port: String,
    baud: u32,
    cmd_rx: Receiver<SettingsCmd>,
    bus_tx: AppEventTx,
    cancel: Arc<AtomicBool>,
) {
    // Connect.
    let mut transport = match SerialTransport::open(&port, baud) {
        Ok(t) => {
            let _ = bus_tx.send(AppEvent::SettingsConnected);
            t
        }
        Err(e) => {
            let _ = bus_tx.send(AppEvent::SettingsFinished(Err(format!("connect: {e}"))));
            return;
        }
    };

    let mut read_buf = vec![0u8; 4096];

    loop {
        if cancel.load(Ordering::Relaxed) {
            break;
        }

        // 1. Try to process a command (non-blocking).
        match cmd_rx.try_recv() {
            Ok(SettingsCmd::Get { prefs_name, key }) => {
                let msg = SettingMsg::get(&prefs_name, &key);
                if let Err(e) = encode_and_send(&mut transport, &msg) {
                    let _ = bus_tx.send(AppEvent::SettingsFinished(Err(format!(
                        "write Get {prefs_name}/{key}: {e}"
                    ))));
                    break;
                }
            }
            Ok(SettingsCmd::Set {
                prefs_name,
                key,
                value_type,
                value,
            }) => {
                let msg = SettingMsg::set(&prefs_name, &key, value_type, &value);
                if let Err(e) = encode_and_send(&mut transport, &msg) {
                    let _ = bus_tx.send(AppEvent::SettingsFinished(Err(format!(
                        "write Set {prefs_name}/{key}: {e}"
                    ))));
                    break;
                }
            }
            Ok(SettingsCmd::Disconnect) => break,
            Err(std::sync::mpsc::TryRecvError::Empty) => {}
            Err(std::sync::mpsc::TryRecvError::Disconnected) => break,
        }

        // 2. Try to read incoming bytes (non-blocking via TimedOut).
        match transport.read(&mut read_buf) {
            Ok(n) if n > 0 => {
                let _ = bus_tx.send(AppEvent::SettingsReceived(read_buf[..n].to_vec()));
            }
            Ok(_) => {}
            Err(TransportError::TimedOut) => {}
            Err(e) => {
                let _ = bus_tx.send(AppEvent::SettingsFinished(Err(format!("read: {e}"))));
                break;
            }
        }

        std::thread::sleep(Duration::from_millis(10));
    }

    transport.close();
    let _ = bus_tx.send(AppEvent::SettingsFinished(Ok(())));
}

fn encode_and_send(transport: &mut SerialTransport, msg: &SettingMsg) -> Result<(), String> {
    let bytes = msg.to_wire().map_err(|e| format!("encode: {e}"))?;
    transport.write_all(&bytes).map_err(|e| e.to_string())?;
    Ok(())
}
