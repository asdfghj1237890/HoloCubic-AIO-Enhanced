//! HoloCubic AIO firmware flasher.
//!
//! Wraps the [`espflash`] crate with a small, opinionated API for the use
//! case of "erase the chip, then write 4 partitions". Improvements over
//! the legacy Python tool:
//!
//! - **Real progress** (not estimated from baudrate × size) via espflash's
//!   `ProgressCallbacks` bridged to an `mpsc::Sender<FlashEvent>`.
//! - **Cooperative cancellation** via `Arc<AtomicBool>` — no ctypes
//!   `_async_raise` hack, no half-open serial ports.
//! - **Typed errors** — `FlashError` instead of `Exception`.
//!
//! See `Docs/superpowers/plans/2026-06-05-plan-4-flasher.md` for design.
#![deny(missing_docs)]
#![deny(unsafe_code)]

pub mod error;
pub mod flasher;
pub mod partition;
pub mod progress;

pub use error::FlashError;
pub use flasher::{reboot, DeviceSummary, Flasher};
pub use partition::{
    Partition, PARTITION_BOOTAPP0, PARTITION_BOOTLOADER, PARTITION_FIRMWARE, PARTITION_PARTITIONS,
};
pub use progress::FlashEvent;
