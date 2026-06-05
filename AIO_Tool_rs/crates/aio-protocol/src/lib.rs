//! HoloCubic AIO wire protocol.
//!
//! Encodes/decodes messages exchanged with HoloCubic firmware over USB serial
//! or TCP. Wire format is preserved byte-for-byte from the legacy Python tool
//! (see `Docs/superpowers/plans/2026-06-05-plan-1-workspace-and-protocol.md`
//! sections D1-D5 for the rationale, including the asymmetric byte order).
#![deny(missing_docs)]
#![deny(unsafe_code)]

pub mod types;
pub mod error;
pub mod header;
pub mod setting;
pub mod file;

// TODO(group-B/C): restore re-exports as modules gain real content:
pub use error::{DecodeError, EncodeError};
pub use header::{MsgHead, WireDecode, WireEncode, HEADER_MARK, HEADER_SIZE};
// pub use setting::SettingMsg;
pub use types::{ActionType, ModuleType, ValueType};
