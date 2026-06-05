//! HoloCubic AIO device transport layer.
//!
//! Provides a [`Transport`] trait abstraction over USB serial (via `serialport`)
//! and WiFi TCP (via `std::net::TcpStream` with auto-reconnect). The crate
//! itself does NOT spawn threads — callers are expected to push reads onto a
//! background thread and forward bytes via `std::sync::mpsc` per the design
//! in `Docs/superpowers/plans/2026-06-05-plan-3-device.md` Section D2.
//!
//! Behavior matches `AIO_Tool/util/robotsocket.py` (TCP) and
//! `AIO_Tool/page/setting.py` (serial) per Plan 3 D1.
#![deny(missing_docs)]
#![deny(unsafe_code)]

pub mod error;
pub mod serial;
pub mod tcp;
pub mod transport;

#[cfg(any(test, feature = "mock"))]
pub mod mock;

pub use error::TransportError;
pub use transport::{Transport, TransportKind};
