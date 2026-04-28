################################################################################
#
# Author: ClimbSnail(HQ)
# original source is here.
#   https://github.com/ClimbSnail/HoloCubic_AIO_Tool
#
#
################################################################################

from util.logger import get_logger
from util.massagehead import AT, MT, MsgHead

logger = get_logger(__name__)


class FileSystem(MsgHead):
    def __init__(self, action_type: int = AT.AT_FREE_STATUS) -> None:
        MsgHead.__init__(
            self, MT.MODULE_TYPE_C_FILE_MANAGER, MT.MODULE_TYPE_CUBIC_FILE_MANAGER
        )  # 一定要初始化父类
        self.action_type = action_type
        self.fmt = self.fmt + "1B"

    def __dir__(self) -> list[str]:
        super_param = super().__dir__()
        return super_param + ["action_type"]


class DirCreate(FileSystem):
    def __init__(self, dir_path: str = "") -> None:
        FileSystem.__init__(self, AT.AT_DIR_CREATE)  # 一定要初始化父类
        self.dir_path: bytes = bytes(dir_path, encoding="utf8")
        self.fmt = self.fmt + "99s"
        logger.debug("self.fmt = %s", self.fmt)

    def __dir__(self) -> list[str]:
        super_param = super().__dir__()
        return super_param + ["dir_path"]


class DirRemove(FileSystem):
    def __init__(self, dir_path: str = "") -> None:
        FileSystem.__init__(self, AT.AT_DIR_REMOVE)  # 一定要初始化父类
        self.dir_path: bytes = bytes(dir_path, encoding="utf8")
        self.fmt = self.fmt + "99s"
        logger.debug("self.fmt = %s", self.fmt)

    def __dir__(self) -> list[str]:
        super_param = super().__dir__()
        return super_param + ["dir_path"]


class DirRename(FileSystem):
    def __init__(self, dir_cur_name: str = "", dir_new_name: str = "") -> None:
        FileSystem.__init__(self, AT.AT_DIR_RENAME)  # 一定要初始化父类
        self.dir_cur_name: bytes = bytes(dir_cur_name, encoding="utf8")
        self.dir_new_name: bytes = bytes(dir_new_name, encoding="utf8")
        self.fmt = self.fmt + "99s99s"
        logger.debug("self.fmt = %s", self.fmt)

    def __dir__(self) -> list[str]:
        super_param = super().__dir__()
        return super_param + ["dir_cur_name", "dir_new_name"]


class DirList(FileSystem):
    def __init__(self, dir_path: str = "", dir_info: str = "") -> None:
        FileSystem.__init__(self, AT.AT_DIR_LIST)  # 一定要初始化父类
        self.dir_path: bytes = bytes(dir_path, encoding="utf8")
        self.dir_info: bytes = bytes(dir_info, encoding="utf8")
        self.fmt = self.fmt + "99s%ds" % len(self.dir_info)
        logger.debug("self.fmt = %s", self.fmt)

    def decode(self, network_data: bytes, byte_order: str = "!") -> None:
        """
        消息的解码
        """
        size = super().decode(network_data, byte_order)
        # 处理不定长的数据
        self.dir_info = network_data[size:]

    def __dir__(self) -> list[str]:
        super_param = super().__dir__()
        return super_param + ["dir_path", "dir_info"]


class FileCreate(FileSystem):
    def __init__(self, file_name: str, file_size: int) -> None:
        FileSystem.__init__(self, AT.AT_FILE_CREATE)  # 一定要初始化父类
        self.file_name: bytes = bytes(file_name, encoding="utf8")
        self.file_size = file_size
        self.fmt = self.fmt + "99s1H"
        logger.debug("self.fmt = %s", self.fmt)

    def __dir__(self) -> list[str]:
        super_param = super().__dir__()
        return super_param + ["file_name", "file_size"]


class FileWrite(FileSystem):
    def __init__(self, data: str = "") -> None:
        FileSystem.__init__(self, AT.AT_FILE_WRITE)  # 一定要初始化父类
        self.data: bytes = bytes(data, encoding="utf8")
        self.fmt = self.fmt + "%ds" % len(self.data)
        logger.debug("self.fmt = %s", self.fmt)

    def decode(self, network_data: bytes, byte_order: str = "!") -> None:
        """
        消息的解码
        """
        size = super().decode(network_data, byte_order)
        # 处理不定长的数据
        self.data = network_data[size:]

    def __dir__(self) -> list[str]:
        super_param = super().__dir__()
        return super_param + ["data"]


class FileRead(FileSystem):
    def __init__(self, data: str = "") -> None:
        FileSystem.__init__(self, AT.AT_FILE_READ)  # 一定要初始化父类
        self.data: bytes = bytes(data, encoding="utf8")
        self.fmt = self.fmt + "%ds" % len(self.data)
        logger.debug("self.fmt = %s", self.fmt)

    def decode(self, network_data: bytes, byte_order: str = "!") -> None:
        """
        消息的解码
        """
        size = super().decode(network_data, byte_order)
        # 处理不定长的数据
        self.data = network_data[size:]

    def __dir__(self) -> list[str]:
        super_param = super().__dir__()
        return super_param + ["data"]


class FileRemove(FileSystem):
    def __init__(self, file_name: str) -> None:
        FileSystem.__init__(self, AT.AT_FILE_REMOVE)  # 一定要初始化父类
        self.file_name: bytes = bytes(file_name, encoding="utf8")
        self.fmt = self.fmt + "99s"
        logger.debug("self.fmt = %s", self.fmt)

    def __dir__(self) -> list[str]:
        super_param = super().__dir__()
        return super_param + ["file_name"]


class FileRename(FileSystem):
    def __init__(self, file_name: str) -> None:
        FileSystem.__init__(self, AT.AT_DIR_RENAME)  # 一定要初始化父类
        self.dir_cur_name: bytes = bytes(file_name, encoding="utf8")
        self.dir_new_name: bytes = bytes(file_name, encoding="utf8")
        self.fmt = self.fmt + "99s99s"
        logger.debug("self.fmt = %s", self.fmt)

    def __dir__(self) -> list[str]:
        super_param = super().__dir__()
        return super_param + ["dir_cur_name", "dir_new_name"]


class FileGetInfo(FileSystem):
    def __init__(self, file_name: str, file_info: str = "") -> None:
        FileSystem.__init__(self, AT.AT_DIR_LIST)  # 一定要初始化父类
        self.file_name: bytes = bytes(file_name, encoding="utf8")
        self.file_info: bytes = bytes(file_info, encoding="utf8")
        self.fmt = self.fmt + "99s%ds" % len(self.file_info)
        logger.debug("self.fmt = %s", self.fmt)

    def decode(self, network_data: bytes, byte_order: str = "!") -> None:
        """
        消息的解码
        """
        size = super().decode(network_data, byte_order)
        # 处理不定长的数据
        self.dir_info = network_data[size:]

    def __dir__(self) -> list[str]:
        super_param = super().__dir__()
        return super_param + ["file_name", "file_info"]
