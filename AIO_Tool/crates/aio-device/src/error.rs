//! Typed transport errors.

use std::io;
use thiserror::Error;

/// Errors a transport can surface to callers.
#[derive(Debug, Error)]
pub enum TransportError {
    /// Underlying OS / driver / network error.
    #[error("io: {0}")]
    Io(#[from] io::Error),

    /// `serialport` crate error (port not found, busy, permission denied).
    #[error("serialport: {0}")]
    SerialPort(#[from] serialport::Error),

    /// Operation attempted on a closed transport (after `close()` or when the
    /// remote went away).
    #[error("transport is closed")]
    Closed,

    /// Read timed out without data. Callers normally treat this as "try again
    /// later" rather than fatal.
    #[error("read timed out")]
    TimedOut,
}

impl TransportError {
    /// Convenience: did the read time out (callers usually retry these silently).
    pub fn is_timeout(&self) -> bool {
        matches!(self, Self::TimedOut)
    }
}
