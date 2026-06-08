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
//! progress — espflash 3.3.0's `erase_flash` does not surface per-block
//! progress).
//!
//! ## Cancellation semantics
//!
//! Cancellation is checked once, just before handing data to espflash.
//! Once espflash is mid-`erase_flash` or mid-`write_bins_to_flash`, the
//! cancel flag can NOT interrupt the in-flight op (espflash 3.3.0's
//! `ProgressCallbacks` methods return `()`, not `Result`, and all
//! partitions go through one `write_bins_to_flash` call to avoid the
//! per-segment hard-reset bug — see `Flasher::write_partitions` for why).
//! Plan 7 UI implications:
//!
//! - On `Cancel` during chip-erase (~10 s on 4 MB ESP32): set a
//!   "Cancelling…" UI state and **ignore the trailing `EraseDone`**.
//!   The next user-initiated op (e.g. `write_partitions`) WILL see the
//!   cancel flag and return `FlashError::Cancelled` early.
//! - On `Cancel` mid-flash: all partitions finish writing (one espflash
//!   session, can't be cut). The adapter stops emitting progress events
//!   after cancel, so the UI sees motion stop even though writes
//!   continue under the hood.

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
