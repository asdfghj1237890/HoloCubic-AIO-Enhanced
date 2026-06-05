//! TCP-over-WiFi transport with auto-reconnect.
//!
//! Wraps `std::net::TcpStream` with a small reconnect state machine that
//! matches Python's `RobotSocketClient` behavior: on disconnect or initial
//! connect failure, retry every `disconntime` (default 500 ms) until
//! `close()` is called.
//!
//! Unlike the Python version, the reconnect loop runs INLINE in `read` /
//! `write_all` rather than in a dedicated background thread. This keeps the
//! Transport implementation thread-free (per design spec — no internal
//! threads). The downside is slightly bursty I/O during reconnect; the
//! upside is much simpler shutdown semantics.

use std::io::{self, Read, Write};
use std::net::{SocketAddr, TcpStream};
use std::time::{Duration, Instant};

use crate::error::TransportError;
use crate::transport::{Transport, TransportKind};

/// TCP transport with inline reconnect.
pub struct TcpTransport {
    addr: SocketAddr,
    stream: Option<TcpStream>,
    /// Time of last failed connect attempt, used to throttle the retry loop.
    last_attempt: Option<Instant>,
    /// Interval between reconnect attempts.
    reconnect_interval: Duration,
    /// Read timeout per recv call.
    read_timeout: Duration,
    /// User has called `close()` — stop reconnecting.
    closed: bool,
}

impl TcpTransport {
    /// Construct a new `TcpTransport`. Does NOT connect immediately — the
    /// first `read` / `write_all` triggers connect (matches Python's lazy
    /// behavior where the `reconner` thread does the actual connect).
    pub fn new(addr: SocketAddr) -> Self {
        Self {
            addr,
            stream: None,
            last_attempt: None,
            reconnect_interval: Duration::from_millis(500),
            read_timeout: Duration::from_millis(500),
            closed: false,
        }
    }

    /// Override the reconnect interval. Default: 500 ms.
    pub fn with_reconnect_interval(mut self, d: Duration) -> Self {
        self.reconnect_interval = d;
        self
    }

    /// Override the read timeout. Default: 500 ms.
    pub fn with_read_timeout(mut self, d: Duration) -> Self {
        self.read_timeout = d;
        self
    }

    /// Try to (re)connect if the stream isn't open. Returns `Ok(())` on
    /// success or `Err(TransportError::TimedOut)` if not enough time has
    /// passed since the last attempt.
    fn maybe_reconnect(&mut self) -> Result<(), TransportError> {
        if self.closed {
            return Err(TransportError::Closed);
        }
        if self.stream.is_some() {
            return Ok(());
        }
        // Throttle retries by reconnect_interval.
        if let Some(t) = self.last_attempt {
            if t.elapsed() < self.reconnect_interval {
                return Err(TransportError::TimedOut);
            }
        }
        self.last_attempt = Some(Instant::now());
        match TcpStream::connect_timeout(&self.addr, self.read_timeout) {
            Ok(s) => {
                let _ = s.set_read_timeout(Some(self.read_timeout));
                let _ = s.set_write_timeout(Some(self.read_timeout));
                self.stream = Some(s);
                Ok(())
            }
            Err(e) => {
                // Map ConnectionRefused / TimedOut to TimedOut so callers
                // can keep polling.
                Err(io_error_to_transport(e))
            }
        }
    }
}

impl Transport for TcpTransport {
    fn kind(&self) -> TransportKind {
        TransportKind::Tcp
    }

    fn write_all(&mut self, data: &[u8]) -> Result<(), TransportError> {
        self.maybe_reconnect()?;
        let s = self.stream.as_mut().ok_or(TransportError::Closed)?;
        match s.write_all(data) {
            Ok(()) => Ok(()),
            Err(e) => {
                // Drop the stream — next call will reconnect.
                self.stream = None;
                Err(io_error_to_transport(e))
            }
        }
    }

    fn read(&mut self, buf: &mut [u8]) -> Result<usize, TransportError> {
        self.maybe_reconnect()?;
        let s = self.stream.as_mut().ok_or(TransportError::Closed)?;
        match s.read(buf) {
            Ok(0) => {
                // EOF — remote closed.
                self.stream = None;
                Err(TransportError::Closed)
            }
            Ok(n) => Ok(n),
            Err(e)
                if e.kind() == io::ErrorKind::WouldBlock || e.kind() == io::ErrorKind::TimedOut =>
            {
                Err(TransportError::TimedOut)
            }
            Err(e) => {
                self.stream = None;
                Err(io_error_to_transport(e))
            }
        }
    }

    fn is_open(&self) -> bool {
        self.stream.is_some() && !self.closed
    }

    fn close(&mut self) {
        self.closed = true;
        if let Some(s) = self.stream.take() {
            let _ = s.shutdown(std::net::Shutdown::Both);
        }
    }
}

fn io_error_to_transport(e: io::Error) -> TransportError {
    match e.kind() {
        io::ErrorKind::TimedOut | io::ErrorKind::WouldBlock => TransportError::TimedOut,
        _ => TransportError::Io(e),
    }
}
