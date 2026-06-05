# Preserved bugs / quirks ledger — aio-device

| ID | Site | Description | Source |
|----|------|-------------|--------|
| B6 | `aio-protocol::SettingMsg::to_wire` (not in this crate, but documented here for cross-reference) | Python's `setting.py::get_param` calls `SettingMsg.encode(">")` (big-endian) while `set_param` uses default little-endian. Same message type, different byte order on the wire. Rust unifies to LE everywhere — the BE-encode path was an undocumented historical artifact, not a protocol decision. The LE form is what firmware actually parses (msg_len is decorative on firmware side). | `setting.py:273` |
