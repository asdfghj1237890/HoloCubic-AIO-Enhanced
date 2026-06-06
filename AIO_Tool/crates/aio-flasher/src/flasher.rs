//! Flasher implementation.
//!
//! Wraps espflash 3.3.0's [`espflash::flasher::Flasher`] with the aio-flasher
//! contract: `new(port, baud)` opens the serial port and connects to the ROM
//! bootloader; `erase(progress, cancel)` chip-erases; and
//! `write_partitions(parts, progress, cancel)` writes a list of partitions
//! sequentially. Progress events go through an `mpsc::Sender<FlashEvent>`;
//! cancellation flows in via `Arc<AtomicBool>` and is checked at partition
//! boundaries and inside the [`espflash::flasher::ProgressCallbacks`] bridge.
//!
//! espflash 3.3.0 API surface used:
//! - [`espflash::flasher::Flasher::connect`] — port + UsbPortInfo + baud +
//!   reset strategy in / out.
//! - [`espflash::flasher::Flasher::erase_flash`] — full-chip erase.
//! - [`espflash::flasher::Flasher::write_bin_to_flash`] — single segment write
//!   at `addr` with `&[u8]` payload and an optional `&mut dyn ProgressCallbacks`.
//! - [`espflash::flasher::ProgressCallbacks`] — `init(addr, total)`,
//!   `update(current)`, `finish()`. None return `Result`, so cancel is checked
//!   inside `update` by skipping further sends (espflash carries on writing,
//!   the caller-loop catches cancel at the next partition boundary).

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::Sender;
use std::sync::Arc;

use espflash::connection::reset::{ResetAfterOperation, ResetBeforeOperation};
use espflash::flasher::{Flasher as EspFlasher, ProgressCallbacks};
use serialport::{FlowControl, UsbPortInfo};

use crate::error::FlashError;
use crate::partition::{self, Partition};
use crate::progress::FlashEvent;

/// HoloCubic flasher. Holds the open serial connection to the ROM bootloader.
pub struct Flasher {
    inner: Option<EspFlasher>,
    port: String,
    baud: u32,
}

impl Flasher {
    /// Open `port` at `baud` and connect to the ROM bootloader.
    ///
    /// Two-step: first open the native serial port at the standard
    /// 115_200 sync baud, then hand it to `espflash::flasher::Flasher::connect`
    /// which performs the ROM-bootloader handshake, detects the chip, and
    /// (if `baud > 115_200`) re-negotiates to the requested baud.
    pub fn new(port: &str, baud: u32) -> Result<Self, FlashError> {
        // Step 1: open the native serial port. espflash::Flasher::connect
        // wants the platform's `Port` type (COMPort on Windows, TTYPort on
        // Unix) which `open_native()` returns.
        let serial = serialport::new(port, 115_200)
            .flow_control(FlowControl::None)
            .open_native()
            .map_err(|e| FlashError::OpenPort {
                port: port.to_owned(),
                baud,
                source: espflash::error::Error::from(e),
            })?;

        // Step 2: connect to the ROM bootloader. We don't know the VID/PID
        // here (caller passed only a port name), so synthesize an empty
        // UsbPortInfo — this matches what espflash's own CLI does for
        // PciPort / Unknown port types (see espflash/src/cli/mod.rs ~L321).
        let port_info = UsbPortInfo {
            vid: 0,
            pid: 0,
            serial_number: None,
            manufacturer: None,
            product: None,
        };

        let inner = EspFlasher::connect(
            serial,
            port_info,
            Some(baud),
            true,  // use_stub — faster + supports erase_region
            false, // verify — espflash's per-write MD5 verify, off for parity with Python tool
            false, // skip — don't skip identical regions
            None,  // chip — autodetect via magic register
            ResetAfterOperation::default(),
            ResetBeforeOperation::default(),
        )
        .map_err(FlashError::Connect)?;

        Ok(Self {
            inner: Some(inner),
            port: port.to_owned(),
            baud,
        })
    }

    /// Erase the entire flash.
    ///
    /// **Cancellation note:** the cancel flag is checked once before
    /// espflash's `erase_flash` is invoked. Once erase is in flight
    /// (~10 s on a 4 MB ESP32), it can NOT be interrupted — espflash
    /// 3.3.0's `erase_flash` doesn't accept progress callbacks.
    /// `EraseDone` will fire even if the user clicked Cancel mid-erase.
    /// Plan 7 UI should show "Cancelling…" and ignore the trailing
    /// `EraseDone`. See module doc on [`crate::progress`] for the full
    /// cancellation contract.
    pub fn erase(
        &mut self,
        progress_tx: Sender<FlashEvent>,
        cancel: Arc<AtomicBool>,
    ) -> Result<(), FlashError> {
        if cancel.load(Ordering::Relaxed) {
            return Err(FlashError::Cancelled);
        }
        // Swallow SendError — receiver dropped means UI is gone; we still
        // need to finish the erase op cleanly.
        let _ = progress_tx.send(FlashEvent::EraseStart);

        let f = self
            .inner
            .as_mut()
            .ok_or(FlashError::Connect(espflash::error::Error::FlashConnect))?;

        f.erase_flash().map_err(FlashError::Erase)?;

        let _ = progress_tx.send(FlashEvent::EraseDone);
        Ok(())
    }

    /// Write a list of partitions sequentially. Validates partition list
    /// (no overlapping address ranges) before starting any writes.
    pub fn write_partitions(
        &mut self,
        parts: Vec<Partition>,
        progress_tx: Sender<FlashEvent>,
        cancel: Arc<AtomicBool>,
    ) -> Result<(), FlashError> {
        // Pre-flight: validate before touching hardware. The unit test
        // `write_partitions_rejects_overlapping_input_before_touching_hardware`
        // exercises this path with `inner = None`.
        partition::validate(&parts)?;

        let f = self
            .inner
            .as_mut()
            .ok_or(FlashError::Connect(espflash::error::Error::FlashConnect))?;

        for (i, part) in parts.iter().enumerate() {
            if cancel.load(Ordering::Relaxed) {
                return Err(FlashError::Cancelled);
            }

            // The bridge sends PartitionStart from ProgressCallbacks::init —
            // espflash calls init() at the start of each write_bin_to_flash
            // with the same addr + size we pass.
            let mut adapter = CallbackAdapter {
                tx: progress_tx.clone(),
                cancel: cancel.clone(),
                partition_index: i,
            };

            f.write_bin_to_flash(part.address, &part.data, Some(&mut adapter))
                .map_err(|e| FlashError::WritePartition {
                    index: i,
                    address: part.address,
                    source: e,
                })?;
        }
        Ok(())
    }

    /// Port name passed to `new` (for UI / logging).
    pub fn port(&self) -> &str {
        &self.port
    }

    /// Baud rate the flasher is using.
    pub fn baud(&self) -> u32 {
        self.baud
    }
}

impl Drop for Flasher {
    fn drop(&mut self) {
        // espflash::Flasher drops its serial port cleanly on Drop. Nothing
        // extra to do here — explicit `take` for symmetry with future
        // explicit-close additions.
        let _ = self.inner.take();
    }
}

/// Adapter from espflash's `ProgressCallbacks` trait to our `mpsc` channel.
///
/// Constructed fresh per partition so `partition_index` is fixed for the
/// callbacks fired during one `write_bin_to_flash` call. espflash's
/// `ProgressCallbacks` methods do not return `Result`, so cancellation can't
/// abort mid-write — instead we (a) stop emitting progress events once the
/// cancel flag is set, and (b) let the outer loop in `write_partitions`
/// catch cancel at the next partition boundary.
struct CallbackAdapter {
    tx: Sender<FlashEvent>,
    cancel: Arc<AtomicBool>,
    partition_index: usize,
}

impl CallbackAdapter {
    fn send(&self, evt: FlashEvent) {
        if self.cancel.load(Ordering::Relaxed) {
            return;
        }
        // Receiver may have been dropped (UI window closed) — swallow.
        let _ = self.tx.send(evt);
    }
}

impl ProgressCallbacks for CallbackAdapter {
    fn init(&mut self, _addr: u32, total: usize) {
        self.send(FlashEvent::PartitionStart {
            index: self.partition_index,
            total_bytes: total as u64,
        });
    }

    fn update(&mut self, current: usize) {
        self.send(FlashEvent::Progress {
            index: self.partition_index,
            bytes_written: current as u64,
        });
    }

    fn finish(&mut self) {
        self.send(FlashEvent::PartitionDone {
            index: self.partition_index,
        });
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::AtomicBool;
    use std::sync::mpsc::channel;
    use std::sync::Arc;

    #[test]
    fn write_partitions_rejects_overlapping_input_before_touching_hardware() {
        // Verify the validate() guard runs FIRST, before any espflash I/O.
        // Construct a Flasher with inner=None — write_partitions returns the
        // overlap error before reaching the inner.as_mut() unwrap.
        let mut flasher = Flasher {
            inner: None,
            port: "/dev/null".to_owned(),
            baud: 115_200,
        };
        let parts = vec![
            Partition {
                address: 0x1000,
                data: vec![0u8; 0x200],
            },
            Partition {
                address: 0x10ff,
                data: vec![0u8; 0x100],
            },
        ];
        let (tx, _rx) = channel();
        let cancel = Arc::new(AtomicBool::new(false));
        let err = flasher.write_partitions(parts, tx, cancel).unwrap_err();
        assert!(matches!(err, FlashError::OverlappingPartitions { .. }));
    }

    // --- CallbackAdapter tests ---
    //
    // These verify the adapter emits the right FlashEvent in response to each
    // ProgressCallbacks method and silences itself when the cancel flag is
    // tripped. The bridge between espflash and our mpsc channel is the most
    // failure-prone bit of glue in the crate; one test per method + one for
    // cancel-silencing is the minimum useful coverage.

    fn make_adapter(
        idx: usize,
    ) -> (
        CallbackAdapter,
        std::sync::mpsc::Receiver<FlashEvent>,
        Arc<AtomicBool>,
    ) {
        let (tx, rx) = channel();
        let cancel = Arc::new(AtomicBool::new(false));
        let adapter = CallbackAdapter {
            tx,
            cancel: cancel.clone(),
            partition_index: idx,
        };
        (adapter, rx, cancel)
    }

    #[test]
    fn callback_init_emits_partition_start() {
        let (mut adapter, rx, _cancel) = make_adapter(2);
        adapter.init(0x1000, 4096);
        let evt = rx.try_recv().unwrap();
        assert_eq!(
            evt,
            FlashEvent::PartitionStart {
                index: 2,
                total_bytes: 4096
            }
        );
    }

    #[test]
    fn callback_update_emits_progress() {
        let (mut adapter, rx, _cancel) = make_adapter(0);
        adapter.update(512);
        let evt = rx.try_recv().unwrap();
        assert_eq!(
            evt,
            FlashEvent::Progress {
                index: 0,
                bytes_written: 512
            }
        );
    }

    #[test]
    fn callback_finish_emits_partition_done() {
        let (mut adapter, rx, _cancel) = make_adapter(7);
        adapter.finish();
        let evt = rx.try_recv().unwrap();
        assert_eq!(evt, FlashEvent::PartitionDone { index: 7 });
    }

    #[test]
    fn callback_silenced_after_cancel_flag_set() {
        let (mut adapter, rx, cancel) = make_adapter(0);
        cancel.store(true, Ordering::Relaxed);
        adapter.init(0x1000, 4096);
        adapter.update(2048);
        adapter.finish();
        // Receiver got nothing — adapter swallowed all three events.
        assert!(rx.try_recv().is_err());
    }
}
