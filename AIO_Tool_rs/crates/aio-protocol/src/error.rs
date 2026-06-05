//! Encode / decode error types.

use thiserror::Error;

/// Errors produced while decoding bytes off the wire.
#[derive(Debug, Error, PartialEq, Eq)]
pub enum DecodeError {
    /// Buffer was shorter than the minimum required (header is 7 bytes; sub-messages may need more).
    #[error("buffer too short: need {needed} bytes, got {got}")]
    TooShort {
        /// Bytes needed to attempt the parse.
        needed: usize,
        /// Bytes actually available.
        got: usize,
    },

    /// `header_mark` was not `0x2323`.
    #[error("invalid header_mark: expected 0x2323, got {0:#06x}")]
    BadHeaderMark(u16),

    /// `from_who` / `to_who` byte was outside the known `ModuleType` range.
    #[error("unknown module type: {0}")]
    UnknownModule(u8),

    /// `action_type` byte was outside the known `ActionType` range.
    #[error("unknown action type: {0}")]
    UnknownAction(u8),

    /// Setting payload `value_type` byte was outside the known `ValueType` range.
    #[error("unknown value type: {0}")]
    UnknownValueType(u8),

    /// A SettingMsg field is missing its terminating `\0`.
    #[error("missing \\0 terminator for SettingMsg field `{0}`")]
    MissingNullTerminator(&'static str),
}

/// Errors produced while encoding a message for the wire.
#[derive(Debug, Error, PartialEq, Eq)]
pub enum EncodeError {
    /// A string field contained an embedded `\0` byte, which would corrupt the SettingMsg framing.
    #[error("SettingMsg field `{field}` contains an embedded \\0 byte at offset {offset}")]
    EmbeddedNull {
        /// Which field is offending (e.g. `"prefs_name"`).
        field: &'static str,
        /// Byte offset of the first `\0` found.
        offset: usize,
    },
}
