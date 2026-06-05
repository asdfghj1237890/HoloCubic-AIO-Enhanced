//! File / directory protocol messages.
//!
//! All messages here share the legacy "8-byte header" layout: a 7-byte MsgHead
//! followed by a duplicate 1-byte action_type (D2 in plan-1). Payload encoding
//! varies per action.

use byteorder::{ByteOrder, LittleEndian};

use crate::error::{DecodeError, EncodeError};
use crate::header::{MsgHead, WireDecode, WireEncode, HEADER_SIZE};
use crate::types::{ActionType, ModuleType};

/// Fixed dir/file path width (99 bytes, null-padded, matching Python's `99s` format).
pub const PATH_WIDTH: usize = 99;

/// Construct the common dir/file header (FROM=CFileManager, TO=CubicFileManager) for `action`.
///
/// PRESERVED-BUG-FROM-V2 (B5): Python's `FileSystem.encode` never sets `msg_len`,
/// so the wire bytes always carry `msg_len = 0` for dir/file messages. Mirror that
/// here to keep byte-exact wire compatibility (verified by the golden tests).
/// Firmware ignores `msg_len` anyway (see `header.rs`), so this is purely a
/// wire-format fidelity concern.
fn file_header(action: ActionType) -> MsgHead {
    let mut head = MsgHead::new(
        ModuleType::CFileManager,
        ModuleType::CubicFileManager,
        action,
    );
    head.msg_len = 0;
    head
}

/// Encode header + duplicate action_type byte (the 8-byte "FileSystem" prefix).
fn encode_file_prefix(header: &MsgHead, out: &mut Vec<u8>) {
    header.encode(out);
    out.push(header.action as u8);
}

/// Write `s` into a fixed-width null-padded buffer of `width` bytes.
/// Truncates if `s` is longer than `width`.
fn write_fixed(s: &str, width: usize, out: &mut Vec<u8>) {
    let bytes = s.as_bytes();
    let take = bytes.len().min(width);
    out.extend_from_slice(&bytes[..take]);
    for _ in take..width {
        out.push(0);
    }
}

/// Strip trailing NUL bytes (Python's `99s` format pads, our decoder un-pads).
fn read_fixed(bytes: &[u8]) -> String {
    let end = bytes.iter().position(|b| *b == 0).unwrap_or(bytes.len());
    String::from_utf8_lossy(&bytes[..end]).into_owned()
}

// ============================================================================
// Dir messages
// ============================================================================

/// `AT_DIR_CREATE` — create a directory.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DirCreate {
    /// Header (sender / receiver / action).
    pub header: MsgHead,
    /// Path to create. Encoded as 99-byte null-padded.
    pub dir_path: String,
}

impl DirCreate {
    /// Build a new `DirCreate` message.
    pub fn new(dir_path: &str) -> Self {
        Self {
            header: file_header(ActionType::DirCreate),
            dir_path: dir_path.to_owned(),
        }
    }

    /// Encode to wire bytes.
    pub fn to_wire(&self) -> Result<Vec<u8>, EncodeError> {
        let mut buf = Vec::with_capacity(HEADER_SIZE + 1 + PATH_WIDTH);
        encode_file_prefix(&self.header, &mut buf);
        write_fixed(&self.dir_path, PATH_WIDTH, &mut buf);
        Ok(buf)
    }
}

/// `AT_DIR_REMOVE` — remove a directory.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DirRemove {
    /// Header.
    pub header: MsgHead,
    /// Path to remove. 99-byte null-padded.
    pub dir_path: String,
}

impl DirRemove {
    /// Build a new `DirRemove` message.
    pub fn new(dir_path: &str) -> Self {
        Self {
            header: file_header(ActionType::DirRemove),
            dir_path: dir_path.to_owned(),
        }
    }

    /// Encode to wire bytes.
    pub fn to_wire(&self) -> Result<Vec<u8>, EncodeError> {
        let mut buf = Vec::with_capacity(HEADER_SIZE + 1 + PATH_WIDTH);
        encode_file_prefix(&self.header, &mut buf);
        write_fixed(&self.dir_path, PATH_WIDTH, &mut buf);
        Ok(buf)
    }
}

/// `AT_DIR_RENAME` — rename a directory. Two fixed-99-byte path fields back to back.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DirRename {
    /// Header.
    pub header: MsgHead,
    /// Current name (null-padded).
    pub dir_cur_name: String,
    /// New name (null-padded).
    pub dir_new_name: String,
}

impl DirRename {
    /// Build a new `DirRename`.
    pub fn new(dir_cur_name: &str, dir_new_name: &str) -> Self {
        Self {
            header: file_header(ActionType::DirRename),
            dir_cur_name: dir_cur_name.to_owned(),
            dir_new_name: dir_new_name.to_owned(),
        }
    }

    /// Encode.
    pub fn to_wire(&self) -> Result<Vec<u8>, EncodeError> {
        let mut buf = Vec::with_capacity(HEADER_SIZE + 1 + 2 * PATH_WIDTH);
        encode_file_prefix(&self.header, &mut buf);
        write_fixed(&self.dir_cur_name, PATH_WIDTH, &mut buf);
        write_fixed(&self.dir_new_name, PATH_WIDTH, &mut buf);
        Ok(buf)
    }
}

/// `AT_DIR_LIST` — list directory contents. Variable-length `dir_info` payload.
/// Matches Python's `99s{len(dir_info)}s` — see plan D3 (firmware expects fixed 400 bytes
/// but Python sends variable length; preserved for wire compatibility with v2.6.x).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DirList {
    /// Header.
    pub header: MsgHead,
    /// Fixed-99-byte directory path.
    pub dir_path: String,
    /// Variable-length payload (JSON listing on responses; empty on requests).
    pub dir_info: Vec<u8>,
}

impl DirList {
    /// Build a new `DirList` request (no payload).
    pub fn request(dir_path: &str) -> Self {
        Self {
            header: file_header(ActionType::DirList),
            dir_path: dir_path.to_owned(),
            dir_info: Vec::new(),
        }
    }

    /// Encode.
    pub fn to_wire(&self) -> Result<Vec<u8>, EncodeError> {
        let mut buf = Vec::with_capacity(HEADER_SIZE + 1 + PATH_WIDTH + self.dir_info.len());
        encode_file_prefix(&self.header, &mut buf);
        write_fixed(&self.dir_path, PATH_WIDTH, &mut buf);
        buf.extend_from_slice(&self.dir_info);
        Ok(buf)
    }

    /// Decode (used for firmware responses).
    pub fn from_wire(buf: &[u8]) -> Result<(Self, usize), DecodeError> {
        let (header, hdr_used) = MsgHead::decode(buf)?;
        let after_hdr = hdr_used + 1; // skip duplicate action byte
        if buf.len() < after_hdr + PATH_WIDTH {
            return Err(DecodeError::TooShort {
                needed: after_hdr + PATH_WIDTH,
                got: buf.len(),
            });
        }
        let dir_path = read_fixed(&buf[after_hdr..after_hdr + PATH_WIDTH]);
        let dir_info = buf[after_hdr + PATH_WIDTH..].to_vec();
        Ok((
            Self {
                header,
                dir_path,
                dir_info,
            },
            buf.len(),
        ))
    }
}

// ============================================================================
// File messages
// ============================================================================

/// `AT_FILE_CREATE` — create a file. Payload: 99-byte path + 2-byte u16 size.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FileCreate {
    /// Header.
    pub header: MsgHead,
    /// File name (99-byte null-padded).
    pub file_name: String,
    /// File size in bytes (u16 — wire format limitation inherited from Python's `"99s1H"`).
    pub file_size: u16,
}

impl FileCreate {
    /// Build a new `FileCreate`.
    pub fn new(file_name: &str, file_size: u16) -> Self {
        Self {
            header: file_header(ActionType::FileCreate),
            file_name: file_name.to_owned(),
            file_size,
        }
    }

    /// Encode.
    pub fn to_wire(&self) -> Result<Vec<u8>, EncodeError> {
        let mut buf = Vec::with_capacity(HEADER_SIZE + 1 + PATH_WIDTH + 2);
        encode_file_prefix(&self.header, &mut buf);
        write_fixed(&self.file_name, PATH_WIDTH, &mut buf);
        let mut size_bytes = [0u8; 2];
        // Python encodes "1H" via "=" which is LE on x86 — match it.
        LittleEndian::write_u16(&mut size_bytes, self.file_size);
        buf.extend_from_slice(&size_bytes);
        Ok(buf)
    }
}

/// `AT_FILE_WRITE` — write file contents. Variable-length data payload.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FileWrite {
    /// Header.
    pub header: MsgHead,
    /// File data to send.
    pub data: Vec<u8>,
}

impl FileWrite {
    /// Build a new `FileWrite`.
    pub fn new(data: impl Into<Vec<u8>>) -> Self {
        Self {
            header: file_header(ActionType::FileWrite),
            data: data.into(),
        }
    }

    /// Encode.
    pub fn to_wire(&self) -> Result<Vec<u8>, EncodeError> {
        let mut buf = Vec::with_capacity(HEADER_SIZE + 1 + self.data.len());
        encode_file_prefix(&self.header, &mut buf);
        buf.extend_from_slice(&self.data);
        Ok(buf)
    }
}

/// `AT_FILE_READ` — request file contents. Variable-length payload (typically a path string).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FileRead {
    /// Header.
    pub header: MsgHead,
    /// Variable-length payload (path on request; data on response).
    pub data: Vec<u8>,
}

impl FileRead {
    /// Build a new `FileRead` request from a path.
    pub fn request(path: &str) -> Self {
        Self {
            header: file_header(ActionType::FileRead),
            data: path.as_bytes().to_vec(),
        }
    }

    /// Encode.
    pub fn to_wire(&self) -> Result<Vec<u8>, EncodeError> {
        let mut buf = Vec::with_capacity(HEADER_SIZE + 1 + self.data.len());
        encode_file_prefix(&self.header, &mut buf);
        buf.extend_from_slice(&self.data);
        Ok(buf)
    }

    /// Decode a response: header + duplicate action + raw payload.
    pub fn from_wire(buf: &[u8]) -> Result<(Self, usize), DecodeError> {
        let (header, hdr_used) = MsgHead::decode(buf)?;
        let after_hdr = hdr_used + 1;
        if buf.len() < after_hdr {
            return Err(DecodeError::TooShort {
                needed: after_hdr,
                got: buf.len(),
            });
        }
        Ok((
            Self {
                header,
                data: buf[after_hdr..].to_vec(),
            },
            buf.len(),
        ))
    }
}

/// `AT_FILE_REMOVE` — remove a file.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FileRemove {
    /// Header.
    pub header: MsgHead,
    /// File name (99-byte null-padded).
    pub file_name: String,
}

impl FileRemove {
    /// Build a new `FileRemove`.
    pub fn new(file_name: &str) -> Self {
        Self {
            header: file_header(ActionType::FileRemove),
            file_name: file_name.to_owned(),
        }
    }

    /// Encode.
    pub fn to_wire(&self) -> Result<Vec<u8>, EncodeError> {
        let mut buf = Vec::with_capacity(HEADER_SIZE + 1 + PATH_WIDTH);
        encode_file_prefix(&self.header, &mut buf);
        write_fixed(&self.file_name, PATH_WIDTH, &mut buf);
        Ok(buf)
    }
}

/// `AT_FILE_RENAME` — rename a file.
///
/// PRESERVED-BUG-FROM-V2 (B1): Python sets action_type to `AT_DIR_RENAME` (4)
/// and copies `file_name` into BOTH `dir_cur_name` and `dir_new_name` (same value).
/// This preserves byte-for-byte wire compatibility with v2.6.x firmware which
/// observes the same broken format on the wire today.
///
/// `new()` here mirrors the bug: only one filename, copied to both fields, action=DirRename.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FileRename {
    /// Header (note: action_type = DirRename, not FileRename — see PRESERVED-BUG-FROM-V2 B1).
    pub header: MsgHead,
    /// "Current name" — Python copies single input to both fields.
    pub dir_cur_name: String,
    /// "New name" — same value as `dir_cur_name` in Python; preserved.
    pub dir_new_name: String,
}

impl FileRename {
    /// Build a new `FileRename`. The single `file_name` argument matches Python's API,
    /// which copies it to both `dir_cur_name` and `dir_new_name` (PRESERVED-BUG-FROM-V2 B1).
    pub fn new(file_name: &str) -> Self {
        Self {
            // PRESERVED-BUG-FROM-V2 (B1): action is DirRename not FileRename
            header: file_header(ActionType::DirRename),
            dir_cur_name: file_name.to_owned(),
            dir_new_name: file_name.to_owned(),
        }
    }

    /// Encode.
    pub fn to_wire(&self) -> Result<Vec<u8>, EncodeError> {
        let mut buf = Vec::with_capacity(HEADER_SIZE + 1 + 2 * PATH_WIDTH);
        encode_file_prefix(&self.header, &mut buf);
        write_fixed(&self.dir_cur_name, PATH_WIDTH, &mut buf);
        write_fixed(&self.dir_new_name, PATH_WIDTH, &mut buf);
        Ok(buf)
    }
}

/// `AT_FILE_GET_INFO` — query file metadata.
///
/// PRESERVED-BUG-FROM-V2 (B2): Python sets action_type to `AT_DIR_LIST` (5), not
/// `AT_FILE_GET_INFO` (11). Preserved for wire compatibility with v2.6.x firmware.
/// (Python also references `self.dir_info` which is never declared, raising AttributeError
/// at runtime if `decode()` is called — we avoid the AttributeError by giving the field a
/// real name (`file_info`), but the wire action_type remains DirList.)
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FileGetInfo {
    /// Header (action_type = DirList, see PRESERVED-BUG-FROM-V2 B2).
    pub header: MsgHead,
    /// File name (99-byte null-padded).
    pub file_name: String,
    /// Response payload (file metadata as bytes).
    pub file_info: Vec<u8>,
}

impl FileGetInfo {
    /// Build a new `FileGetInfo` request.
    pub fn request(file_name: &str) -> Self {
        Self {
            // PRESERVED-BUG-FROM-V2 (B2): action is DirList not FileGetInfo
            header: file_header(ActionType::DirList),
            file_name: file_name.to_owned(),
            file_info: Vec::new(),
        }
    }

    /// Encode.
    pub fn to_wire(&self) -> Result<Vec<u8>, EncodeError> {
        let mut buf = Vec::with_capacity(HEADER_SIZE + 1 + PATH_WIDTH + self.file_info.len());
        encode_file_prefix(&self.header, &mut buf);
        write_fixed(&self.file_name, PATH_WIDTH, &mut buf);
        buf.extend_from_slice(&self.file_info);
        Ok(buf)
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    // --- helpers ---

    #[test]
    fn fixed_field_roundtrip() {
        let mut buf = Vec::new();
        write_fixed("hello", 10, &mut buf);
        assert_eq!(buf, b"hello\0\0\0\0\0");
        assert_eq!(read_fixed(&buf), "hello");
    }

    #[test]
    fn fixed_field_truncates_overflow() {
        let mut buf = Vec::new();
        write_fixed("verylongpath", 5, &mut buf);
        assert_eq!(buf, b"veryl"); // truncated, no NUL because width hit
    }

    // --- dir messages ---

    #[test]
    fn dir_create_root() {
        let msg = DirCreate::new("/test");
        let wire = msg.to_wire().unwrap();
        // 7 header + 1 duplicate action + 99 path
        assert_eq!(wire.len(), 7 + 1 + PATH_WIDTH);
        assert_eq!(&wire[0..2], &[0x23, 0x23]);
        assert_eq!(wire[4], ModuleType::CFileManager as u8);
        assert_eq!(wire[5], ModuleType::CubicFileManager as u8);
        assert_eq!(wire[6], ActionType::DirCreate as u8);
        // Duplicate action byte:
        assert_eq!(wire[7], ActionType::DirCreate as u8);
        // Path "/test" null-padded:
        assert_eq!(&wire[8..13], b"/test");
        for &b in &wire[13..(8 + PATH_WIDTH)] {
            assert_eq!(b, 0);
        }
    }

    #[test]
    fn dir_remove_root() {
        let msg = DirRemove::new("/old");
        let wire = msg.to_wire().unwrap();
        assert_eq!(wire[6], ActionType::DirRemove as u8);
        assert_eq!(wire[7], ActionType::DirRemove as u8);
        assert_eq!(&wire[8..12], b"/old");
    }

    #[test]
    fn dir_rename_layout() {
        let msg = DirRename::new("/a", "/b");
        let wire = msg.to_wire().unwrap();
        assert_eq!(wire.len(), 7 + 1 + 2 * PATH_WIDTH);
        assert_eq!(wire[6], ActionType::DirRename as u8);
        assert_eq!(wire[7], ActionType::DirRename as u8);
        assert_eq!(&wire[8..10], b"/a");
        assert_eq!(&wire[(8 + PATH_WIDTH)..(8 + PATH_WIDTH + 2)], b"/b");
    }

    #[test]
    fn dir_list_request_layout() {
        let msg = DirList::request("/sd");
        let wire = msg.to_wire().unwrap();
        assert_eq!(wire.len(), 7 + 1 + PATH_WIDTH); // empty dir_info
        assert_eq!(&wire[8..11], b"/sd");
    }

    #[test]
    fn dir_list_from_wire_rejects_short_buffer() {
        // 7-byte MsgHead + 1-byte dup action = 8 bytes; need PATH_WIDTH=99 more for a valid frame.
        // Supply only header + dup action; decode should see `after_hdr + PATH_WIDTH = 107` needed.
        let mut wire = vec![0x23, 0x23, 0x00, 0x00, 0x02, 0x01, 0x05]; // header (DirList)
        wire.push(ActionType::DirList as u8); // dup action
                                              // No path bytes — frame is 1 + HEADER_SIZE = 8 bytes total.
        assert_eq!(
            DirList::from_wire(&wire),
            Err(DecodeError::TooShort {
                needed: 8 + PATH_WIDTH,
                got: 8
            })
        );
    }

    // --- file messages ---

    #[test]
    fn file_create_encodes_size_le() {
        let msg = FileCreate::new("/sd/foo.bin", 0x1234);
        let wire = msg.to_wire().unwrap();
        assert_eq!(wire.len(), 7 + 1 + PATH_WIDTH + 2);
        // size at the tail (LE)
        let n = wire.len();
        assert_eq!(&wire[n - 2..], &[0x34, 0x12]);
    }

    #[test]
    fn file_write_appends_raw_data() {
        let msg = FileWrite::new(b"hello".to_vec());
        let wire = msg.to_wire().unwrap();
        assert_eq!(wire.len(), 7 + 1 + 5);
        assert_eq!(&wire[8..], b"hello");
    }

    #[test]
    fn file_read_request_path_in_payload() {
        let msg = FileRead::request("/sd/log.txt");
        let wire = msg.to_wire().unwrap();
        assert_eq!(&wire[8..], b"/sd/log.txt");
    }

    #[test]
    fn file_remove_correct() {
        let msg = FileRemove::new("/sd/bad.bin");
        let wire = msg.to_wire().unwrap();
        assert_eq!(wire[6], ActionType::FileRemove as u8);
    }

    // --- preserved bugs ---

    #[test]
    fn file_rename_preserves_b1() {
        // Bug: action_type = DirRename (4), both name fields = input
        let msg = FileRename::new("/sd/foo.txt");
        let wire = msg.to_wire().unwrap();
        assert_eq!(
            wire[6],
            ActionType::DirRename as u8,
            "B1: should still be DirRename"
        );
        assert_eq!(
            wire[7],
            ActionType::DirRename as u8,
            "B1: duplicate also DirRename"
        );
        // Both name fields contain the same path
        let first = &wire[8..(8 + PATH_WIDTH)];
        let second = &wire[(8 + PATH_WIDTH)..(8 + 2 * PATH_WIDTH)];
        assert_eq!(first[..11], *b"/sd/foo.txt");
        assert_eq!(second[..11], *b"/sd/foo.txt");
    }

    #[test]
    fn file_get_info_preserves_b2() {
        // Bug: action_type = DirList (5), not FileGetInfo (11)
        let msg = FileGetInfo::request("/sd/foo.txt");
        let wire = msg.to_wire().unwrap();
        assert_eq!(
            wire[6],
            ActionType::DirList as u8,
            "B2: should still be DirList"
        );
        assert_eq!(wire[7], ActionType::DirList as u8);
    }
}
