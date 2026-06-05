//! Progress events emitted while flashing.
//!
//! `Flasher::erase` and `Flasher::write_partitions` accept an
//! `mpsc::Sender<FlashEvent>`. Events fire in this order per partition:
//!
//! ```text
//! PartitionStart { index, total_bytes }
//! Progress { index, bytes_written }   (zero or more)
//! PartitionDone { index }
//! ```
//!
//! `Erase` operations emit `EraseStart` then `EraseDone` (no granular
//! progress — espflash doesn't surface chip-erase progress).

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::Sender;
use std::sync::Arc;

/// One event emitted from a flash operation.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum FlashEvent {
    /// Chip-erase started.
    EraseStart,
    /// Chip-erase finished successfully.
    EraseDone,
    /// Writing partition `index` (0-based in the caller's supplied list)
    /// will write `total_bytes`.
    PartitionStart {
        /// Zero-based index in the caller's `Vec<Partition>`.
        index: usize,
        /// Total bytes to write for this partition.
        total_bytes: u64,
    },
    /// Progress update for the in-progress partition.
    Progress {
        /// Zero-based partition index.
        index: usize,
        /// Bytes written so far for this partition.
        bytes_written: u64,
    },
    /// Partition `index` finished writing successfully.
    PartitionDone {
        /// Zero-based partition index.
        index: usize,
    },
}

/// Bridge between espflash's `ProgressCallbacks` trait and our `mpsc::Sender`.
///
/// Used by `Flasher::erase` for the EraseStart / EraseDone bookend (espflash
/// 3.3.0's `erase_flash` does not surface per-block progress, so there's no
/// `ProgressCallbacks` hookup there). `Flasher::write_partitions` uses a
/// dedicated `CallbackAdapter` instead — see `flasher.rs`.
pub(crate) struct ProgressBridge {
    pub(crate) tx: Sender<FlashEvent>,
    pub(crate) cancel: Arc<AtomicBool>,
}

impl ProgressBridge {
    pub(crate) fn new(tx: Sender<FlashEvent>, cancel: Arc<AtomicBool>) -> Self {
        Self { tx, cancel }
    }

    pub(crate) fn is_cancelled(&self) -> bool {
        self.cancel.load(Ordering::Relaxed)
    }

    pub(crate) fn send(&self, evt: FlashEvent) {
        // If the UI dropped its receiver, swallow the SendError — the user
        // closed the window mid-flash and we're about to be dropped anyway.
        let _ = self.tx.send(evt);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::AtomicBool;
    use std::sync::mpsc::channel;
    use std::sync::Arc;

    #[test]
    fn bridge_starts_uncancelled() {
        let (tx, _rx) = channel();
        let cancel = Arc::new(AtomicBool::new(false));
        let b = ProgressBridge::new(tx, cancel);
        assert!(!b.is_cancelled());
    }

    #[test]
    fn bridge_observes_cancel_flag() {
        let (tx, _rx) = channel();
        let cancel = Arc::new(AtomicBool::new(false));
        let b = ProgressBridge::new(tx, cancel.clone());
        cancel.store(true, Ordering::Relaxed);
        assert!(b.is_cancelled());
    }

    #[test]
    fn bridge_send_to_dropped_receiver_does_not_panic() {
        let (tx, rx) = channel();
        let cancel = Arc::new(AtomicBool::new(false));
        let b = ProgressBridge::new(tx, cancel);
        drop(rx);
        b.send(FlashEvent::EraseStart); // must not panic
    }

    #[test]
    fn events_forwarded_in_order() {
        let (tx, rx) = channel();
        let cancel = Arc::new(AtomicBool::new(false));
        let b = ProgressBridge::new(tx, cancel);
        b.send(FlashEvent::EraseStart);
        b.send(FlashEvent::PartitionStart {
            index: 0,
            total_bytes: 1024,
        });
        b.send(FlashEvent::Progress {
            index: 0,
            bytes_written: 512,
        });
        b.send(FlashEvent::PartitionDone { index: 0 });
        b.send(FlashEvent::EraseDone);

        let collected: Vec<FlashEvent> = rx.try_iter().collect();
        assert_eq!(collected.len(), 5);
        assert_eq!(collected[0], FlashEvent::EraseStart);
        assert!(matches!(
            collected[1],
            FlashEvent::PartitionStart {
                index: 0,
                total_bytes: 1024
            }
        ));
    }
}
