//! The `Transport` trait — every byte channel to a HoloCubic implements this.

use crate::error::TransportError;

/// Runtime introspection for transport kind. Useful for UI surfacing
/// ("Connected via Serial COM3" vs "Connected via WiFi 192.168.1.50:6000").
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum TransportKind {
    /// USB serial / UART transport.
    Serial,
    /// TCP-over-WiFi transport.
    Tcp,
    /// In-process mock for tests.
    Mock,
}

/// Byte-level read/write contract for talking to a HoloCubic.
///
/// Implementations are `Send` so callers can move them to a background thread.
/// They are NOT required to be `Sync` — typical usage is one transport per
/// thread, with bytes forwarded to the UI via `std::sync::mpsc`.
///
/// `read` returns `Err(TransportError::TimedOut)` instead of blocking
/// indefinitely when no data is available. Callers loop on that.
pub trait Transport: Send {
    /// Identify this transport's kind (for UI / logging).
    fn kind(&self) -> TransportKind;

    /// Write `data` in full or return an error. Implementations are responsible
    /// for retrying short writes internally; callers should NOT loop.
    fn write_all(&mut self, data: &[u8]) -> Result<(), TransportError>;

    /// Try to read up to `buf.len()` bytes. Returns the number of bytes written
    /// into `buf` (0 ≤ n ≤ buf.len()), or:
    /// - `Err(TransportError::TimedOut)` if no data arrived within the
    ///   transport's configured timeout.
    /// - `Err(TransportError::Closed)` if the remote went away or `close()`
    ///   was called.
    fn read(&mut self, buf: &mut [u8]) -> Result<usize, TransportError>;

    /// True if the transport is currently usable for I/O.
    fn is_open(&self) -> bool;

    /// Close the transport. Idempotent — second call is a no-op. After
    /// closing, `read` and `write_all` return `Err(TransportError::Closed)`.
    fn close(&mut self);
}
