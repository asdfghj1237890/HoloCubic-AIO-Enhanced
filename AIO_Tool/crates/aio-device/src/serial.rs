//! USB serial transport via the `serialport` crate.
//!
//! No reconnect — when the device disappears (cable yanked), `read` /
//! `write_all` start returning errors and the caller is expected to close
//! and ask the user to reconnect. This matches `setting.py` semantics in the
//! legacy tool: the Settings tab requires explicit Close → Reopen on cable
//! events.

use std::io::{Read, Write};
use std::time::Duration;

use serialport::SerialPort;

use crate::error::TransportError;
use crate::transport::{Transport, TransportKind};

/// USB serial transport. Constructor opens the port immediately; failure
/// surfaces as `serialport::Error` via the typed `TransportError::SerialPort`
/// variant.
pub struct SerialTransport {
    port: Option<Box<dyn SerialPort>>,
    /// For logging / kind() introspection.
    port_name: String,
}

impl SerialTransport {
    /// Open `port_name` (e.g. `"COM3"` on Windows, `/dev/ttyUSB0` on Linux)
    /// at `baud_rate` (e.g. `115200`).
    ///
    /// Read timeout defaults to 500 ms (improved over Python's 10 s) so
    /// reads return `TransportError::TimedOut` promptly when idle, letting
    /// callers poll without UI freezes.
    pub fn open(port_name: &str, baud_rate: u32) -> Result<Self, TransportError> {
        let port = serialport::new(port_name, baud_rate)
            .timeout(Duration::from_millis(500))
            .open()?;
        Ok(Self {
            port: Some(port),
            port_name: port_name.to_owned(),
        })
    }

    /// The OS-level port name passed to `open` (for UI display).
    pub fn port_name(&self) -> &str {
        &self.port_name
    }
}

impl Transport for SerialTransport {
    fn kind(&self) -> TransportKind {
        TransportKind::Serial
    }

    fn write_all(&mut self, data: &[u8]) -> Result<(), TransportError> {
        let port = self.port.as_mut().ok_or(TransportError::Closed)?;
        port.write_all(data).map_err(TransportError::Io)?;
        Ok(())
    }

    fn read(&mut self, buf: &mut [u8]) -> Result<usize, TransportError> {
        let port = self.port.as_mut().ok_or(TransportError::Closed)?;
        match port.read(buf) {
            Ok(0) => Err(TransportError::TimedOut),
            Ok(n) => Ok(n),
            Err(e) if e.kind() == std::io::ErrorKind::TimedOut => Err(TransportError::TimedOut),
            Err(e) => Err(TransportError::Io(e)),
        }
    }

    fn is_open(&self) -> bool {
        self.port.is_some()
    }

    fn close(&mut self) {
        // Dropping the boxed serialport closes the OS handle.
        self.port = None;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn kind_and_is_open_on_default() {
        // We can't actually open a port without hardware. Construct a struct
        // manually to validate the kind() and is_open() paths without I/O.
        let t = SerialTransport {
            port: None,
            port_name: "FAKE".to_owned(),
        };
        assert_eq!(t.kind(), TransportKind::Serial);
        assert!(!t.is_open());
        assert_eq!(t.port_name(), "FAKE");
    }
}
