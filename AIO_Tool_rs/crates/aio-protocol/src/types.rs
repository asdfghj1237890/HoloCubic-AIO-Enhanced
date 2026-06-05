//! Protocol identifier enums. Wire values MUST match `AIO_Firmware_PIO/src/message.h`.

use crate::error::DecodeError;

/// Module identifier in the `from_who` / `to_who` header fields.
///
/// Wire values match firmware enum `MODULE_TYPE`.
#[repr(u8)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum ModuleType {
    /// Unknown / unset.
    Unknown = 0,
    /// HoloCubic device-side file manager.
    CubicFileManager = 1,
    /// PC tool-side file manager.
    CFileManager = 2,
    /// HoloCubic device-side settings module.
    CubicSettings = 3,
    /// PC tool-side settings module.
    ToolSettings = 4,
}

impl ModuleType {
    /// Decode a byte to the enum, surfacing unknown values as `DecodeError::UnknownModule`.
    pub fn from_wire(b: u8) -> Result<Self, DecodeError> {
        match b {
            0 => Ok(Self::Unknown),
            1 => Ok(Self::CubicFileManager),
            2 => Ok(Self::CFileManager),
            3 => Ok(Self::CubicSettings),
            4 => Ok(Self::ToolSettings),
            other => Err(DecodeError::UnknownModule(other)),
        }
    }
}

/// Action identifier in the `action_type` header field.
///
/// Wire values match firmware enum `ACTION_TYPE`.
#[repr(u8)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum ActionType {
    /// Unknown / unset.
    Unknown = 0,
    /// Heartbeat / status ping.
    FreeStatus = 1,

    /// Create directory.
    DirCreate = 2,
    /// Remove directory.
    DirRemove = 3,
    /// Rename directory.
    DirRename = 4,
    /// List directory contents.
    DirList = 5,

    /// Create file.
    FileCreate = 6,
    /// Write file contents.
    FileWrite = 7,
    /// Read file contents.
    FileRead = 8,
    /// Remove file.
    FileRemove = 9,
    /// Rename file.
    FileRename = 10,
    /// Query file metadata.
    FileGetInfo = 11,

    /// Set a setting value.
    SettingSet = 12,
    /// Get a setting value.
    SettingGet = 13,
}

impl ActionType {
    /// Decode a byte to the enum; unknown values become `DecodeError::UnknownAction`.
    pub fn from_wire(b: u8) -> Result<Self, DecodeError> {
        match b {
            0 => Ok(Self::Unknown),
            1 => Ok(Self::FreeStatus),
            2 => Ok(Self::DirCreate),
            3 => Ok(Self::DirRemove),
            4 => Ok(Self::DirRename),
            5 => Ok(Self::DirList),
            6 => Ok(Self::FileCreate),
            7 => Ok(Self::FileWrite),
            8 => Ok(Self::FileRead),
            9 => Ok(Self::FileRemove),
            10 => Ok(Self::FileRename),
            11 => Ok(Self::FileGetInfo),
            12 => Ok(Self::SettingSet),
            13 => Ok(Self::SettingGet),
            other => Err(DecodeError::UnknownAction(other)),
        }
    }
}

/// Setting value-type tag used by SettingMsg payloads.
#[repr(u8)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum ValueType {
    /// Unknown / unparseable.
    Unknown = 0,
    /// 16-bit integer.
    Int = 1,
    /// Unsigned 8-bit integer.
    Uchar = 2,
    /// UTF-8 string.
    String = 3,
}

impl ValueType {
    /// Decode a byte to the enum; unknown values become `DecodeError::UnknownValueType`.
    pub fn from_wire(b: u8) -> Result<Self, DecodeError> {
        match b {
            0 => Ok(Self::Unknown),
            1 => Ok(Self::Int),
            2 => Ok(Self::Uchar),
            3 => Ok(Self::String),
            other => Err(DecodeError::UnknownValueType(other)),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn module_type_wire_values_pinned() {
        // SAFETY: these values must match AIO_Firmware_PIO/src/message.h MODULE_TYPE.
        assert_eq!(ModuleType::Unknown as u8, 0);
        assert_eq!(ModuleType::CubicFileManager as u8, 1);
        assert_eq!(ModuleType::CFileManager as u8, 2);
        assert_eq!(ModuleType::CubicSettings as u8, 3);
        assert_eq!(ModuleType::ToolSettings as u8, 4);
    }

    #[test]
    fn module_type_from_wire_roundtrip() {
        for v in 0u8..=4 {
            let m = ModuleType::from_wire(v).unwrap();
            assert_eq!(m as u8, v);
        }
    }

    #[test]
    fn module_type_from_wire_rejects_unknown() {
        match ModuleType::from_wire(99) {
            Err(DecodeError::UnknownModule(99)) => {}
            other => panic!("expected UnknownModule(99), got {other:?}"),
        }
    }

    #[test]
    fn action_type_wire_values_pinned() {
        // SAFETY: must match AIO_Firmware_PIO/src/message.h ACTION_TYPE.
        assert_eq!(ActionType::SettingGet as u8, 13);
        assert_eq!(ActionType::DirList as u8, 5);
        assert_eq!(ActionType::FileRead as u8, 8);
    }

    #[test]
    fn action_type_from_wire_full_range() {
        for v in 0u8..=13 {
            let a = ActionType::from_wire(v).unwrap();
            assert_eq!(a as u8, v);
        }
    }

    #[test]
    fn action_type_rejects_unknown() {
        assert!(matches!(ActionType::from_wire(14), Err(DecodeError::UnknownAction(14))));
    }

    #[test]
    fn value_type_wire_values_pinned() {
        // SAFETY: must match AIO_Firmware_PIO/src/message.h VALUE_TYPE.
        assert_eq!(ValueType::Unknown as u8, 0);
        assert_eq!(ValueType::Int as u8, 1);
        assert_eq!(ValueType::Uchar as u8, 2);
        assert_eq!(ValueType::String as u8, 3);
    }
}
