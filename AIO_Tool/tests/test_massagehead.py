"""Tests for util.massagehead — protocol enums + MsgHead wire format."""

from __future__ import annotations

import struct

import pytest

from util.massagehead import (
    AT,
    MT,
    VT,
    ActionType,
    ModuleType,
    MsgHead,
    SettingMsg,
    ValueType,
)


class TestEnumStability:
    """Lock down integer values of protocol enums against accidental reorder."""

    def test_module_type_values(self) -> None:
        assert int(ModuleType.MODULE_TYPE_UNKNOW) == 0
        assert int(ModuleType.MODULE_TYPE_CUBIC_FILE_MANAGER) == 1
        assert int(ModuleType.MODULE_TYPE_C_FILE_MANAGER) == 2
        assert int(ModuleType.MODULE_TYPE_CUBIC_SETTINGS) == 3
        assert int(ModuleType.MODULE_TYPE_TOOL_SETTINGS) == 4

    def test_action_type_values(self) -> None:
        # These integers are wire-format protocol values; never renumber.
        assert int(ActionType.AT_UNKNOWN) == 0
        assert int(ActionType.AT_FREE_STATUS) == 1
        assert int(ActionType.AT_DIR_CREATE) == 2
        assert int(ActionType.AT_DIR_REMOVE) == 3
        assert int(ActionType.AT_DIR_RENAME) == 4
        assert int(ActionType.AT_DIR_LIST) == 5
        assert int(ActionType.AT_FILE_CREATE) == 6
        assert int(ActionType.AT_FILE_WRITE) == 7
        assert int(ActionType.AT_FILE_READ) == 8
        assert int(ActionType.AT_FILE_REMOVE) == 9
        assert int(ActionType.AT_FILE_RENAME) == 10
        assert int(ActionType.AT_FILE_GET_INFO) == 11
        assert int(ActionType.AT_SETTING_SET) == 12
        assert int(ActionType.AT_SETTING_GET) == 13

    def test_value_type_values(self) -> None:
        assert int(ValueType.VALUE_TYPE_UNKNOWN) == 0
        assert int(ValueType.VALUE_TYPE_INT) == 1
        assert int(ValueType.VALUE_TYPE_UCHAR) == 2
        assert int(ValueType.VALUE_TYPE_STRING) == 3

    def test_aliases_point_to_classes(self) -> None:
        # MT/AT/VT are class-level aliases used as namespaces.
        assert MT is ModuleType
        assert AT is ActionType
        assert VT is ValueType


class TestMsgHeadWireFormat:
    """Lock down the bytes-on-wire format produced by MsgHead.encode()."""

    def test_default_encode_size_is_seven_bytes(self) -> None:
        m = MsgHead()
        assert len(m.encode()) == 7

    def test_header_mark_is_0x2323(self) -> None:
        m = MsgHead()
        encoded = m.encode()
        # First 2 bytes = header_mark = 8995 = 0x2323
        # Native byte order on x86 = little-endian, so bytes are 0x23 0x23
        # On big-endian, also 0x23 0x23 — palindromic value
        assert encoded[:2] == b"\x23\x23"

    def test_encode_includes_from_to_action(self) -> None:
        m = MsgHead(
            from_who=int(MT.MODULE_TYPE_C_FILE_MANAGER),
            to_who=int(MT.MODULE_TYPE_CUBIC_FILE_MANAGER),
            action_type=int(AT.AT_DIR_CREATE),
        )
        encoded = m.encode()
        # Bytes 4, 5, 6 = from_who, to_who, action_type
        assert encoded[4] == 2  # MODULE_TYPE_C_FILE_MANAGER
        assert encoded[5] == 1  # MODULE_TYPE_CUBIC_FILE_MANAGER
        assert encoded[6] == 2  # AT_DIR_CREATE

    def test_decode_recovers_encoded_fields(self) -> None:
        original = MsgHead(from_who=4, to_who=3, action_type=12)
        encoded = original.encode()

        # Note: encode default byteOrder is "=", decode default is "!".
        # We round-trip with matching byte orders.
        decoded = MsgHead()
        decoded.decode(struct.pack("=HHBBB", 8995, 0, 4, 3, 12), byteOrder="=")
        assert decoded.from_who == 4
        assert decoded.to_who == 3
        assert decoded.action_type == 12
        assert decoded.header_mark == 8995

    def test_dir_returns_wire_order_field_names(self) -> None:
        m = MsgHead()
        assert m.__dir__() == [
            "header_mark",
            "msg_len",
            "from_who",
            "to_who",
            "action_type",
        ]


class TestSettingMsg:
    """SettingMsg appends a NUL-separated payload after the header."""

    def test_encode_appends_nul_separated_payload(self) -> None:
        msg = SettingMsg(action_type=int(AT.AT_SETTING_SET))
        msg.prefs_name = b"sys"
        msg.key = b"ssid"
        msg.type = b"String"
        msg.value = b"home"

        encoded = msg.encode()

        # Header is still 7 bytes; payload follows
        assert len(encoded) >= 7
        payload = encoded[7:]
        # Format: prefs\x00 key\x00 type\x00 value\r\n
        assert payload == b"sys\x00ssid\x00String\x00home\r\n"

    def test_decode_extracts_left_info(self) -> None:
        # Build a synthetic 7-byte header + payload, decode it
        header = struct.pack(
            "!HHBBB",
            8995,
            14,
            int(MT.MODULE_TYPE_TOOL_SETTINGS),
            int(MT.MODULE_TYPE_CUBIC_SETTINGS),
            int(AT.AT_SETTING_GET),
        )
        payload = b"a\x00b\x00c\x00d\r\n"
        msg = SettingMsg()
        msg.decode(header + payload, byteOrder="!")
        assert msg.left_info == payload


class TestSubclassCompatibility:
    """Ensure file_info subclasses still encode after the MsgHead refactor."""

    def test_dir_create_encodes_full_struct(self) -> None:
        from util.file_info import DirCreate

        d = DirCreate("/tmp/example")
        encoded = d.encode()
        # Header (7) + action_type (1) + path field (99) = 107
        assert len(encoded) == 107

    def test_file_create_encodes_with_size(self) -> None:
        from util.file_info import FileCreate

        f = FileCreate("test.txt", 1024)
        encoded = f.encode()
        # Header (7) + action_type (1) + name (99) + size (2) = 109
        assert len(encoded) == 109


@pytest.mark.parametrize("byte_order", ["!", "=", "<", ">"])
def test_msghead_round_trip_all_byte_orders(byte_order: str) -> None:
    """encode/decode round-trip must preserve from/to/action regardless of order."""
    raw = struct.pack(byte_order + "HHBBB", 8995, 0, 7, 8, 13)
    m = MsgHead()
    m.decode(raw, byteOrder=byte_order)
    assert m.from_who == 7
    assert m.to_who == 8
    assert m.action_type == 13
