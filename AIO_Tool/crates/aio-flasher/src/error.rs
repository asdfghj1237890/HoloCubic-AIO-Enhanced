//! Typed flasher errors.

use thiserror::Error;

/// Errors that can occur during flashing.
#[derive(Debug, Error)]
pub enum FlashError {
    /// Couldn't open the serial port (busy, not found, permission denied).
    #[error("open serial port `{port}` at {baud} baud: {source}")]
    OpenPort {
        /// Port name attempted.
        port: String,
        /// Baud rate attempted.
        baud: u32,
        /// Underlying error.
        #[source]
        source: espflash::error::Error,
    },

    /// Chip detection / connect to ROM bootloader failed.
    #[error("ROM bootloader connect failed (is the device in download mode?): {0}")]
    Connect(#[source] espflash::error::Error),

    /// Reading chip metadata (revision / MAC / flash size / features) failed.
    #[error("read device info: {0}")]
    DeviceInfo(#[source] espflash::error::Error),

    /// Rebooting the device via the serial control lines (RTS → EN pulse)
    /// failed — either the port couldn't be opened or a control-line write
    /// was rejected by the driver.
    #[error("reboot device on `{port}`: {source}")]
    Reboot {
        /// Port name attempted.
        port: String,
        /// Underlying serial error (open or DTR/RTS write).
        #[source]
        source: espflash::error::Error,
    },

    /// Erase operation failed.
    #[error("erase flash failed: {0}")]
    Erase(#[source] espflash::error::Error),

    /// A partition write failed (partial flash). The device is in an
    /// inconsistent state — user should erase and try again.
    #[error("write partition {index} at 0x{address:08x} failed: {source}")]
    WritePartition {
        /// Zero-based partition index in the user-supplied list.
        index: usize,
        /// Flash address where the failure occurred.
        address: u32,
        /// Underlying espflash error.
        #[source]
        source: espflash::error::Error,
    },

    /// Two partitions in the same operation cover overlapping address ranges.
    #[error(
        "partitions {a} and {b} overlap: partition {a} covers \
         [0x{a_start:08x}, 0x{a_end:08x}), partition {b} covers \
         [0x{b_start:08x}, 0x{b_end:08x})"
    )]
    OverlappingPartitions {
        /// Index of the first partition.
        a: usize,
        /// Start address of partition a.
        a_start: u32,
        /// End address (exclusive) of partition a.
        a_end: u32,
        /// Index of the second partition.
        b: usize,
        /// Start address of partition b.
        b_start: u32,
        /// End address (exclusive) of partition b.
        b_end: u32,
    },

    /// User clicked Cancel; the operation aborted at the partition / step boundary.
    #[error("flash cancelled by user")]
    Cancelled,
}
