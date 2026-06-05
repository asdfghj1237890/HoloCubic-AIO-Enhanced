//! MsgHead — the 7-byte header at the front of every protocol message.

use byteorder::{BigEndian, ByteOrder, LittleEndian};
use bytes::BufMut;

use crate::error::DecodeError;
use crate::types::{ActionType, ModuleType};

/// Wire magic for `header_mark` (= ASCII "##").
pub const HEADER_MARK: u16 = 0x2323;

/// Size of `MsgHead` on the wire, in bytes.
pub const HEADER_SIZE: usize = 7;

/// The fixed-format header shared by every protocol message.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MsgHead {
    /// Header magic. Always `HEADER_MARK` on a valid message.
    pub header_mark: u16,
    /// Total message length (header + payload). Decorative on firmware side
    /// (firmware parser ignores it), preserved for wire compatibility.
    pub msg_len: u16,
    /// Sender module identifier.
    pub from: ModuleType,
    /// Receiver module identifier.
    pub to: ModuleType,
    /// Action requested.
    pub action: ActionType,
}

impl MsgHead {
    /// Construct a new header with `header_mark` defaulted and `msg_len = HEADER_SIZE`.
    pub fn new(from: ModuleType, to: ModuleType, action: ActionType) -> Self {
        Self {
            header_mark: HEADER_MARK,
            msg_len: HEADER_SIZE as u16,
            from,
            to,
            action,
        }
    }
}

/// Encode a message into a byte buffer.
pub trait WireEncode {
    /// Append the message's wire bytes to `buf`.
    fn encode(&self, buf: &mut Vec<u8>);

    /// Size in bytes this message will write.
    fn wire_size(&self) -> usize;
}

/// Decode a message from a byte slice. Returns the parsed value and the number of bytes consumed.
pub trait WireDecode: Sized {
    /// Parse `Self` from the start of `buf`.
    fn decode(buf: &[u8]) -> Result<(Self, usize), DecodeError>;
}

impl WireEncode for MsgHead {
    fn encode(&self, buf: &mut Vec<u8>) {
        // PRESERVED-BUG-FROM-V2 (B3): encode uses little-endian to match Python's
        // `struct.pack("=...")` default. The asymmetric byte order vs decode is
        // intentional — see plan section D1.
        let mut tmp = [0u8; HEADER_SIZE];
        LittleEndian::write_u16(&mut tmp[0..2], self.header_mark);
        LittleEndian::write_u16(&mut tmp[2..4], self.msg_len);
        tmp[4] = self.from as u8;
        tmp[5] = self.to as u8;
        tmp[6] = self.action as u8;
        buf.put_slice(&tmp);
    }

    fn wire_size(&self) -> usize {
        HEADER_SIZE
    }
}

impl WireDecode for MsgHead {
    fn decode(buf: &[u8]) -> Result<(Self, usize), DecodeError> {
        if buf.len() < HEADER_SIZE {
            return Err(DecodeError::TooShort {
                needed: HEADER_SIZE,
                got: buf.len(),
            });
        }
        // PRESERVED-BUG-FROM-V2 (B3): decode uses big-endian to match the
        // firmware's hand-rolled encode (msg[2]=high, msg[3]=low) and
        // Python's struct.unpack("!...") default.
        let header_mark = BigEndian::read_u16(&buf[0..2]);
        if header_mark != HEADER_MARK {
            return Err(DecodeError::BadHeaderMark(header_mark));
        }
        let msg_len = BigEndian::read_u16(&buf[2..4]);
        let from = ModuleType::from_wire(buf[4])?;
        let to = ModuleType::from_wire(buf[5])?;
        let action = ActionType::from_wire(buf[6])?;
        Ok((
            Self {
                header_mark,
                msg_len,
                from,
                to,
                action,
            },
            HEADER_SIZE,
        ))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn encode_get_sys_ssid_matches_handcrafted_bytes() {
        // SettingGet from TOOL_SETTINGS (4) to CUBIC_SETTINGS (3).
        // Header only; payload comes from SettingMsg later.
        let mut head = MsgHead::new(
            ModuleType::ToolSettings,
            ModuleType::CubicSettings,
            ActionType::SettingGet,
        );
        head.msg_len = 19; // arbitrary realistic value for a SettingMsg payload
        let mut buf = Vec::new();
        head.encode(&mut buf);
        assert_eq!(
            buf,
            // LE encoded: header_mark(2) + msg_len(2) + from(1) + to(1) + action(1)
            &[0x23, 0x23, 0x13, 0x00, 0x04, 0x03, 0x0d]
        );
    }

    #[test]
    fn decode_firmware_response_be() {
        // A firmware-encoded SettingGet response: header_mark + msg_len(BE) + from + to + action
        let wire = [0x23, 0x23, 0x00, 0x13, 0x03, 0x04, 0x0d];
        let (head, consumed) = MsgHead::decode(&wire).unwrap();
        assert_eq!(consumed, 7);
        assert_eq!(head.header_mark, 0x2323);
        assert_eq!(head.msg_len, 0x0013); // 19 in decimal
        assert_eq!(head.from, ModuleType::CubicSettings);
        assert_eq!(head.to, ModuleType::ToolSettings);
        assert_eq!(head.action, ActionType::SettingGet);
    }

    #[test]
    fn decode_rejects_short_buffer() {
        let short = [0x23, 0x23, 0x00];
        assert_eq!(
            MsgHead::decode(&short),
            Err(DecodeError::TooShort { needed: 7, got: 3 })
        );
    }

    #[test]
    fn decode_rejects_bad_header_mark() {
        let bogus = [0xFF, 0xFF, 0x00, 0x13, 0x03, 0x04, 0x0d];
        assert_eq!(
            MsgHead::decode(&bogus),
            Err(DecodeError::BadHeaderMark(0xFFFF))
        );
    }
}
