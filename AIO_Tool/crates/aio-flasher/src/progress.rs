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
//! Cancellation is checked at partition boundaries inside
//! `Flasher::write_partitions` and just before the start of `Flasher::erase`.
//! Once espflash is mid-`erase_flash` or mid-`write_bin_to_flash`, cancel
//! can NOT interrupt the in-flight op (espflash 3.3.0's `ProgressCallbacks`
//! methods return `()`, not `Result`). Plan 7 UI implications:
//!
//! - On `Cancel` during chip-erase (~10 s on 4 MB ESP32): set a
//!   "Cancelling…" UI state and **ignore the trailing `EraseDone`**.
//!   The next user-initiated op (e.g. `write_partitions`) WILL see the
//!   cancel flag and return `FlashError::Cancelled` early.
//! - On `Cancel` mid-partition-write: the current partition finishes;
//!   the outer loop returns `FlashError::Cancelled` before the next one.

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
