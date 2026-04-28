################################################################################
#
# Author: ClimbSnail(HQ)
# original source is here.
#   https://github.com/ClimbSnail/HoloCubic_AIO_Tool
#
#
################################################################################

import ctypes
import inspect
import sys
import threading
from pathlib import Path

from util.logger import get_logger

logger = get_logger(__name__)


# Get the base path for resources (works for both frozen exe and script)
def get_resource_path(relative_path: str | Path) -> Path:
    """Get absolute path to resource, works for dev and for PyInstaller."""
    try:
        # PyInstaller creates a temp folder and stores path in _MEIPASS
        base_path = Path(sys._MEIPASS)  # type: ignore[attr-defined]
    except AttributeError:
        base_path = Path.cwd()
    return base_path / relative_path


TOOL_VERSION: str = "v2.5.0"
#: GitHub raw URL — pyproject.toml 內含 ``version = "X.Y.Z"`` 可抓取比對
TOOL_VERSION_INFO_URL: str = (
    "https://raw.githubusercontent.com/asdfghj1237890/HoloCubic-AIO-Enhanced/main/AIO_Tool/pyproject.toml"
)
#: GitHub repo URL — 使用者下載最新版工具與韌體的入口
GITHUB_REPO_URL: str = "https://github.com/asdfghj1237890/HoloCubic-AIO-Enhanced"
ROOT_PATH: str = "OutFile"
CACHE_PATH: str = "Cache"

# 字節序對照表參考：https://docs.python.org/3/library/struct.html#byte-order-size-and-alignment


def _async_raise(thread_obj: threading.Thread) -> None:
    """
    释放进程
    :param thread_obj: 进程对象
    :return:
    """
    try:
        tid = thread_obj.ident
        tid = ctypes.c_long(tid)
        exctype: type = SystemExit
        """raises the exception, performs cleanup if needed"""
        if not inspect.isclass(exctype):
            exctype = type(exctype)
        res = ctypes.pythonapi.PyThreadState_SetAsyncExc(tid, ctypes.py_object(exctype))
        if res == 0:
            raise ValueError("invalid thread id")
        elif res != 1:
            # """if it returns a number greater than one, you're in trouble,
            # and you should call it again with exc=NULL to revert the effect"""
            ctypes.pythonapi.PyThreadState_SetAsyncExc(tid, None)
            raise SystemError("PyThreadState_SetAsyncExc failed")
    except Exception:
        pass
