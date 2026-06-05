//! In-process mock transport.
//!
//! Provides `MockTransport` for unit / integration tests that need to drive
//! the same code paths used by `SerialTransport` / `TcpTransport` without
//! hardware or network. Available behind the crate's `mock` feature.

use std::collections::VecDeque;
use std::sync::{Arc, Mutex};

use crate::error::TransportError;
use crate::transport::{Transport, TransportKind};

/// A handle to inject reads into and observe writes from a `MockTransport`.
///
/// Tests typically construct a `MockTransport` along with its `MockHandle`,
/// hand the transport to the system under test, and assert via the handle
/// what bytes the SUT wrote and supply what bytes it should "receive".
#[derive(Debug, Default, Clone)]
pub struct MockHandle {
    /// Bytes the SUT has written (write_all calls append here).
    pub written: Arc<Mutex<Vec<u8>>>,
    /// Bytes available for the SUT to read. Tests push_back to inject.
    pub to_read: Arc<Mutex<VecDeque<Vec<u8>>>>,
    /// Whether close() has been called by the SUT.
    pub closed: Arc<Mutex<bool>>,
}

impl MockHandle {
    /// Snapshot of bytes the SUT has written so far.
    pub fn writes(&self) -> Vec<u8> {
        self.written.lock().expect("mock lock").clone()
    }

    /// Push bytes that the next `read()` will return (up to buf.len()).
    pub fn inject(&self, data: &[u8]) {
        self.to_read
            .lock()
            .expect("mock lock")
            .push_back(data.to_vec());
    }

    /// True if the SUT called `close()`.
    pub fn was_closed(&self) -> bool {
        *self.closed.lock().expect("mock lock")
    }
}

/// Mock transport. Drives the same Transport contract using in-memory buffers.
pub struct MockTransport {
    handle: MockHandle,
}

impl MockTransport {
    /// Create a fresh mock transport + handle pair.
    pub fn new() -> (Self, MockHandle) {
        let handle = MockHandle::default();
        (
            Self {
                handle: handle.clone(),
            },
            handle,
        )
    }
}

impl Transport for MockTransport {
    fn kind(&self) -> TransportKind {
        TransportKind::Mock
    }

    fn write_all(&mut self, data: &[u8]) -> Result<(), TransportError> {
        if *self.handle.closed.lock().expect("mock lock") {
            return Err(TransportError::Closed);
        }
        self.handle
            .written
            .lock()
            .expect("mock lock")
            .extend_from_slice(data);
        Ok(())
    }

    fn read(&mut self, buf: &mut [u8]) -> Result<usize, TransportError> {
        if *self.handle.closed.lock().expect("mock lock") {
            return Err(TransportError::Closed);
        }
        let mut q = self.handle.to_read.lock().expect("mock lock");
        let Some(front) = q.front_mut() else {
            return Err(TransportError::TimedOut);
        };
        let n = front.len().min(buf.len());
        buf[..n].copy_from_slice(&front[..n]);
        if n == front.len() {
            q.pop_front();
        } else {
            front.drain(..n);
        }
        Ok(n)
    }

    fn is_open(&self) -> bool {
        !*self.handle.closed.lock().expect("mock lock")
    }

    fn close(&mut self) {
        *self.handle.closed.lock().expect("mock lock") = true;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn write_appended_to_handle() {
        let (mut t, h) = MockTransport::new();
        t.write_all(b"hello").unwrap();
        t.write_all(b" world").unwrap();
        assert_eq!(h.writes(), b"hello world");
    }

    #[test]
    fn read_returns_injected_bytes() {
        let (mut t, h) = MockTransport::new();
        h.inject(b"abc");
        let mut buf = [0u8; 8];
        let n = t.read(&mut buf).unwrap();
        assert_eq!(n, 3);
        assert_eq!(&buf[..n], b"abc");
    }

    #[test]
    fn read_splits_across_buffer_boundary() {
        let (mut t, h) = MockTransport::new();
        h.inject(b"abcdef");
        let mut buf = [0u8; 3];

        let n = t.read(&mut buf).unwrap();
        assert_eq!(n, 3);
        assert_eq!(&buf[..n], b"abc");

        let n = t.read(&mut buf).unwrap();
        assert_eq!(n, 3);
        assert_eq!(&buf[..n], b"def");
    }

    #[test]
    fn read_returns_timed_out_when_empty() {
        let (mut t, _h) = MockTransport::new();
        let mut buf = [0u8; 4];
        match t.read(&mut buf) {
            Err(TransportError::TimedOut) => {}
            other => panic!("expected TimedOut, got {other:?}"),
        }
    }

    #[test]
    fn close_propagates_to_writes_and_reads() {
        let (mut t, h) = MockTransport::new();
        assert!(t.is_open());
        t.close();
        assert!(!t.is_open());
        assert!(h.was_closed());
        assert!(matches!(t.write_all(b"x"), Err(TransportError::Closed)));
        let mut buf = [0u8; 1];
        assert!(matches!(t.read(&mut buf), Err(TransportError::Closed)));
    }

    #[test]
    fn close_is_idempotent() {
        let (mut t, _h) = MockTransport::new();
        t.close();
        t.close(); // must not panic
        assert!(!t.is_open());
    }
}
