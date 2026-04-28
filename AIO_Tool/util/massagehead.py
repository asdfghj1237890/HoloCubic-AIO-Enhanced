################################################################################
#
# Author: ClimbSnail(HQ)
# original source is here.
#   https://github.com/ClimbSnail/HoloCubic_AIO_Tool
#
#
################################################################################

import struct
from ctypes import Array, Structure, c_char_p, cast
from enum import IntEnum

from util.logger import get_logger

logger = get_logger(__name__)

# 模块名 M_
M_ALL = "M_ALL"
M_ENGINE = "M_ENGINE"
M_DOWNLOAD_DEBUG = "M_DOWNLOAD_DEBUG"
M_SETTING = "M_SETTING"
M_FILE_MANAGER = "M_FILE_MANAGER"
M_PICTURE = "M_PICTURE"
M_VIDEO_TOOL = "M_VIDEO_TOOL"
M_SRCEEN_SHARE = "M_SRCEEN_SHARE"
M_HELP = "M_HELP"

# 动作类型 A_
A_CLOSE_UART = "A_CLOSE_UART"
A_OPEN_UART = "A_OPEN_UART"


class ModuleType(IntEnum):
    """Module identifiers for inter-module messaging."""

    # 模块名 未知
    MODULE_TYPE_UNKNOW = 0
    # 模块名 Holocubic
    MODULE_TYPE_CUBIC_FILE_MANAGER = 1
    # 上位机控制器
    MODULE_TYPE_C_FILE_MANAGER = 2
    # Holocubic的settings模块
    MODULE_TYPE_CUBIC_SETTINGS = 3
    # 上位机控制器的settings模块
    MODULE_TYPE_TOOL_SETTINGS = 4


class ActionType(IntEnum):
    """Action identifiers for messages."""

    # 未知类型
    AT_UNKNOWN = 0
    AT_FREE_STATUS = 1

    # 目录操作
    AT_DIR_CREATE = 2  # 创建
    AT_DIR_REMOVE = 3  # 删除
    AT_DIR_RENAME = 4  # 重命名
    AT_DIR_LIST = 5  # 列举目录文件

    # 文件操作
    AT_FILE_CREATE = 6  # 创建
    AT_FILE_WRITE = 7  # 文件信息流写
    AT_FILE_READ = 8  # 文件信息读
    AT_FILE_REMOVE = 9  # 删除
    AT_FILE_RENAME = 10  # 重命名
    AT_FILE_GET_INFO = 11  # 查询文件大小

    AT_SETTING_SET = 12  # 设置属性
    AT_SETTING_GET = 13  # 获取属性


class ValueType(IntEnum):
    """Value type identifiers for setting messages."""

    # 值类型 未知
    VALUE_TYPE_UNKNOWN = 0
    # int
    VALUE_TYPE_INT = 1
    # uchar
    VALUE_TYPE_UCHAR = 2
    # String
    VALUE_TYPE_STRING = 3


# Backward-compatible aliases (the IntEnum class itself acts as namespace)
MT = ModuleType  # 模块类型
AT = ActionType
VT = ValueType  # Setting中值的类型


class MsgHead:
    """网络通信的消息头.

    Wire layout (7 bytes, format ``1H1H1B1B1B``):
        header_mark  (uint16, 2 bytes) — 0x2323 magic ("##")
        msg_len      (uint16, 2 bytes)
        from_who     (uint8,  1 byte)
        to_who       (uint8,  1 byte)
        action_type  (uint8,  1 byte)

    Subclasses extend ``self.fmt`` and the field list returned by ``__dir__``
    to append additional payload fields. The base ``encode``/``decode``
    iterate over those fields generically.
    """

    #: Default header magic — 0x2323 (two ASCII '#' bytes)
    HEADER_MARK_DEFAULT: int = 8995
    #: Header-only struct format
    HEADER_FMT: str = "1H1H1B1B1B"
    #: Field names in wire order (subclasses override to append payload fields)
    _FIELD_ORDER: tuple[str, ...] = (
        "header_mark",
        "msg_len",
        "from_who",
        "to_who",
        "action_type",
    )

    def __init__(
        self,
        from_who: int = 0,
        to_who: int = 0,
        action_type: int = AT.AT_UNKNOWN,
    ) -> None:
        self.header_mark: int = self.HEADER_MARK_DEFAULT
        self.msg_len: int = 0
        self.from_who: int = int(from_who)
        self.to_who: int = int(to_who)
        self.action_type: int = int(action_type)
        # fmt — 規定上述參數的位元組數，子類別可在尾端追加欄位
        self.fmt: str = self.HEADER_FMT

    def __dir__(self) -> list[str]:
        """Return wire-order field names. Subclasses extend this list."""
        return list(self._FIELD_ORDER)

    def decode(self, network_data: bytes, byte_order: str = "!") -> int:
        """Decode bytes into instance attributes. Returns bytes consumed."""
        members = [
            attr
            for attr in self.__dir__()
            if not callable(getattr(self, attr))
            and not attr.startswith("__")
            and not attr.startswith("fmt")
        ]
        # 取得當前實例（可能是子類別）的 struct 大小
        size = struct.Struct(self.fmt).size
        get_data = struct.unpack(byte_order + self.fmt, network_data[:size])
        for attr, value in zip(members, get_data):
            setattr(self, attr, value)
        return size

    def encode(self, byte_order: str = "=") -> bytes:
        """Pack instance attributes into wire bytes."""
        members = [attr for attr in self.__dir__() if not callable(getattr(self, attr))]
        params = [getattr(self, param) for param in members]
        return struct.pack(byte_order + self.fmt, *params)


class SettingMsg(MsgHead):
    """設定相關訊息：以空字元分隔的可變長度欄位 (prefs/key/type/value)."""

    def __init__(self, action_type: int = AT.AT_SETTING_GET) -> None:
        super().__init__(
            MT.MODULE_TYPE_TOOL_SETTINGS,
            MT.MODULE_TYPE_CUBIC_SETTINGS,
            action_type,
        )
        # 不定長字串欄位，使用 \x00 作為分隔符
        self.prefs_name: bytes = b""
        self.key: bytes = b""
        self.type: bytes = b""
        self.value: bytes = b""
        self.left_info: bytes = b""

    def decode(self, network_data: bytes, byte_order: str = "!") -> int:
        size = super().decode(network_data, byte_order)
        self.left_info = network_data[size:]
        logger.debug("SettingMsg left_info: %s", self.left_info)
        return size

    def encode(self, byte_order: str = "=") -> bytes:
        info = (
            self.prefs_name
            + b"\x00"
            + self.key
            + b"\x00"
            + self.type
            + b"\x00"
            + self.value
            + b"\r\n"
        )
        self.msg_len = struct.Struct(self.fmt).size + len(info)
        return super().encode(byte_order) + info

    def __dir__(self) -> list[str]:
        return super().__dir__()


def dump_dict(obj: Structure) -> dict[str, object]:
    """Convert a ctypes Structure instance to a plain dict."""
    info: dict[str, object] = {}
    for k, v in obj._fields_:
        av = getattr(obj, k)
        if type(v) == type(Structure):
            logger.debug("dump_dict struct field: %s", av)
        elif type(v) == type(Array):
            av = cast(av, c_char_p).value.decode()
        info[k] = av
    return info
