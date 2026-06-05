//! `SettingMsg` — variable-length setting get/set payload after MsgHead.
//!
//! Payload format (Python-compatible; see plan D4):
//!   `prefs_name \0 key \0 type_string \0 value \r\n`
//!
//! Note: this format does NOT match the firmware's `SettingsMsg::decode`
//! expectation — round-trip is broken on existing main. We preserve the
//! Python format byte-for-byte and revisit in Plan 3.

use crate::error::{DecodeError, EncodeError};
use crate::header::{MsgHead, WireDecode, WireEncode, HEADER_SIZE};
use crate::types::{ActionType, ModuleType, ValueType};

/// Setting get/set message: extends MsgHead with four null-separated fields and a CRLF terminator.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SettingMsg {
    /// Inherited header. `from`/`to` are forced to TOOL_SETTINGS → CUBIC_SETTINGS at construction.
    pub header: MsgHead,
    /// Namespace, e.g. `"sys"`, `"zhixin"`, `"tianqi"`, `"other"`.
    pub prefs_name: String,
    /// Setting key within the namespace.
    pub key: String,
    /// Value type tag — preserved as a string in the wire payload (`""` for empty / get requests).
    pub value_type_str: String,
    /// Setting value (empty for get requests).
    pub value: String,
}

impl SettingMsg {
    /// Build a `SETTING_GET` request. `from`/`to` follow the legacy Python defaults.
    pub fn get(prefs_name: &str, key: &str) -> Self {
        Self {
            header: MsgHead::new(
                ModuleType::ToolSettings,
                ModuleType::CubicSettings,
                ActionType::SettingGet,
            ),
            prefs_name: prefs_name.to_owned(),
            key: key.to_owned(),
            value_type_str: String::new(),
            value: String::new(),
        }
    }

    /// Build a `SETTING_SET` request.
    pub fn set(prefs_name: &str, key: &str, vt: ValueType, value: &str) -> Self {
        Self {
            header: MsgHead::new(
                ModuleType::ToolSettings,
                ModuleType::CubicSettings,
                ActionType::SettingSet,
            ),
            prefs_name: prefs_name.to_owned(),
            key: key.to_owned(),
            value_type_str: vt_to_str(vt).to_owned(),
            value: value.to_owned(),
        }
    }

    /// Reject any field that contains an embedded `\0` (would corrupt the framing).
    fn check_no_embedded_nulls(&self) -> Result<(), EncodeError> {
        for (name, s) in [
            ("prefs_name", &self.prefs_name),
            ("key", &self.key),
            ("value_type_str", &self.value_type_str),
            ("value", &self.value),
        ] {
            if let Some(offset) = s.as_bytes().iter().position(|&b| b == 0) {
                return Err(EncodeError::EmbeddedNull { field: name, offset });
            }
        }
        Ok(())
    }

    /// Encode to a `Vec<u8>`, validating no embedded nulls first.
    pub fn to_wire(&self) -> Result<Vec<u8>, EncodeError> {
        self.check_no_embedded_nulls()?;
        // payload = prefs_name \0 key \0 type \0 value \r\n
        let payload_len =
            self.prefs_name.len() + 1
            + self.key.len() + 1
            + self.value_type_str.len() + 1
            + self.value.len() + 2;
        let total = HEADER_SIZE + payload_len;

        let mut buf = Vec::with_capacity(total);
        let mut head = self.header.clone();
        head.msg_len = total as u16;
        head.encode(&mut buf);
        buf.extend_from_slice(self.prefs_name.as_bytes());
        buf.push(0);
        buf.extend_from_slice(self.key.as_bytes());
        buf.push(0);
        buf.extend_from_slice(self.value_type_str.as_bytes());
        buf.push(0);
        buf.extend_from_slice(self.value.as_bytes());
        buf.extend_from_slice(b"\r\n");
        Ok(buf)
    }

    /// Decode a `SettingMsg` from a complete frame (header + payload). Returns the parsed
    /// message and the number of bytes consumed (which equals `buf.len()` on success because
    /// the payload runs to the trailing `\r\n`).
    pub fn from_wire(buf: &[u8]) -> Result<(Self, usize), DecodeError> {
        let (header, header_used) = MsgHead::decode(buf)?;
        let rest = &buf[header_used..];

        // Strip trailing \r\n if present.
        let payload = match rest.strip_suffix(b"\r\n") {
            Some(p) => p,
            None => rest, // tolerate missing terminator (matches Python's permissive decode)
        };

        // Split on \0 — expect 4 fields.
        let mut parts = payload.split(|b| *b == 0);
        let prefs_name = parts.next().ok_or(DecodeError::MissingNullTerminator("prefs_name"))?;
        let key = parts.next().ok_or(DecodeError::MissingNullTerminator("key"))?;
        let value_type_str = parts.next().ok_or(DecodeError::MissingNullTerminator("value_type_str"))?;
        let value = parts.next().unwrap_or(&[]);

        Ok((
            Self {
                header,
                prefs_name: String::from_utf8_lossy(prefs_name).into_owned(),
                key: String::from_utf8_lossy(key).into_owned(),
                value_type_str: String::from_utf8_lossy(value_type_str).into_owned(),
                value: String::from_utf8_lossy(value).into_owned(),
            },
            buf.len(),
        ))
    }
}

fn vt_to_str(vt: ValueType) -> &'static str {
    match vt {
        ValueType::Unknown => "",
        ValueType::Int => "1",
        ValueType::Uchar => "2",
        ValueType::String => "3",
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn get_sys_ssid_wire_bytes() {
        let msg = SettingMsg::get("sys", "ssid");
        let bytes = msg.to_wire().unwrap();
        // Header (LE): 23 23 13 00 04 03 0d
        // Payload format: prefs \0 key \0 type \0 value \r\n
        // With empty type and value, payload bytes are: "sys\0ssid\0\0\r\n"
        // Total: 7 header + 3 prefs + 1 + 4 key + 1 + 0 type + 1 + 0 value + 2 crlf = 19 bytes
        assert_eq!(bytes.len(), 19);
        assert_eq!(
            bytes,
            [
                0x23, 0x23, 0x13, 0x00, 0x04, 0x03, 0x0d, // header
                b's', b'y', b's', 0x00,                   // prefs_name + sep
                b's', b's', b'i', b'd', 0x00,             // key + sep
                // value_type_str is empty; only its trailing \0 separator follows
                0x00,                                     // sep after empty type
                // value is empty; no bytes
                b'\r', b'\n',                             // terminator
            ]
        );
    }

    #[test]
    fn embedded_null_rejected() {
        let mut msg = SettingMsg::get("sys", "wifi");
        msg.key = String::from("with\0null");
        let err = msg.to_wire().unwrap_err();
        assert_eq!(
            err,
            EncodeError::EmbeddedNull { field: "key", offset: 4 }
        );
    }

    #[test]
    fn roundtrip_basic_set() {
        let msg = SettingMsg::set("zhixin", "cityname", ValueType::String, "Taipei");
        let bytes = msg.to_wire().unwrap();
        let (parsed, used) = SettingMsg::from_wire(&bytes).unwrap();
        assert_eq!(used, bytes.len());
        assert_eq!(parsed.prefs_name, "zhixin");
        assert_eq!(parsed.key, "cityname");
        assert_eq!(parsed.value_type_str, "3");
        assert_eq!(parsed.value, "Taipei");
    }
}

#[cfg(test)]
mod proptest_block {
    use super::*;
    use proptest::prelude::*;

    proptest! {
        #[test]
        fn setting_msg_roundtrip(
            prefs in "[a-z_]{1,15}",
            key   in "[a-zA-Z0-9_]{1,16}",
            value in "[ -~]{0,32}",
        ) {
            let msg = SettingMsg::set(&prefs, &key, ValueType::String, &value);
            let bytes = msg.to_wire().unwrap();
            let (parsed, used) = SettingMsg::from_wire(&bytes).unwrap();
            prop_assert_eq!(used, bytes.len());
            prop_assert_eq!(parsed.prefs_name, prefs);
            prop_assert_eq!(parsed.key, key);
            prop_assert_eq!(parsed.value, value);
        }
    }
}
