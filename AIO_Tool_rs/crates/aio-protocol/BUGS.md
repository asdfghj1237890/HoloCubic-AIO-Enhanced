# Preserved bugs from v2.x Python tool

The Rust port preserves these bugs byte-for-byte to stay wire-compatible
with v2.6.x firmware on the wire. Each is tagged in source with
`// PRESERVED-BUG-FROM-V2: <id>`. Decide per-bug in Plan 8 (File Manager)
whether to fix and bump the firmware in lockstep.

| ID | Site | Description | Source |
|----|------|-------------|--------|
| B1 | `file.rs::FileRename` | `action_type = ActionType::DirRename` (Python sets `AT.AT_DIR_RENAME`); `dir_new_name = file_name` (same value as `dir_cur_name`) | `util/file_info.py:152` |
| B2 | `file.rs::FileGetInfo` | `action_type = ActionType::DirList` (Python sets `AT.AT_DIR_LIST`) | `util/file_info.py:165` |
| B3 | `header.rs::MsgHead` | Asymmetric byte order: encode LE, decode BE | `util/massagehead.py:139,153` |
| B4 | (none yet — SettingMsg payload format mismatch with firmware) | See plan D4 | (deferred to Plan 3) |
| B5 | `file.rs::file_header` | `msg_len = 0` on dir/file messages (Python's `FileSystem.encode` never sets it; default `__init__` leaves it as 0). Discovered by golden tests; firmware ignores `msg_len`. | `util/massagehead.py:128` |
