################################################################################
#
# Author: ClimbSnail(HQ)
# original source is here.
#   https://github.com/ClimbSnail/HoloCubic_AIO_Tool
#
#
################################################################################

import os
import re
import tkinter as tk
from tkinter import ttk

import customtkinter as ctk
import requests

import util.massagehead as mh
import util.tkutils as tku
from page.download_debug import DownloadDebug
from page.filemanager import FileManager
from page.help import Helper
from page.images_converter import ImagesConverter
from page.setting import Setting
from page.tool_settings import ToolSettings
from page.videotool import VideoTool
from util.common import (
    TOOL_VERSION,
    TOOL_VERSION_INFO_URL,
    get_resource_path,
)
from util.i18n import get_i18n
from util.logger import get_logger, setup_logging

logger = get_logger(__name__)


class Engine:
    """
    引擎
    """

    def __init__(self, root: ctk.CTk) -> None:
        """
        Engine initialization
        :param root: Window widget
        """
        self.root = root
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        icon_path = get_resource_path("image/holo_256.ico")
        if os.path.exists(icon_path):
            self.root.iconbitmap(icon_path)

        # Initialize i18n
        self.i18n = get_i18n()

        # Create output directory for file conversion
        try:
            dir_path = os.path.join("OutFile", "Cache")
            os.makedirs(dir_path)
        except Exception:
            pass

        self.width = 700
        self.height = 500

        # Tab manager
        self.m_tab_manager = ttk.Notebook(self.root)

        # Download Debug page
        self.m_debug_tab = ctk.CTkFrame(self.m_tab_manager, fg_color="transparent")
        self.m_tab_manager.add(self.m_debug_tab, text=self.i18n.t("tab_download_debug"))
        self.m_debug_tab_windows = DownloadDebug(self.m_debug_tab, self)

        # Device Settings page
        self.m_setting_tab = ctk.CTkFrame(self.m_tab_manager, fg_color="transparent")
        self.m_tab_manager.add(self.m_setting_tab, text=self.i18n.t("tab_setting"))
        self.m_setting_tab_windows = Setting(self.m_setting_tab, self)

        # File Manager page
        self.m_file_tab = ctk.CTkFrame(self.m_tab_manager, fg_color="transparent")
        self.m_tab_manager.add(self.m_file_tab, text=self.i18n.t("tab_file_manager"))
        self.m_file_tab_windows = FileManager(self.m_file_tab, self)

        # Image Converter page
        self.m_image_tab = ctk.CTkFrame(self.m_tab_manager, fg_color="transparent")
        self.m_tab_manager.add(self.m_image_tab, text=self.i18n.t("tab_image_converter"))
        self.m_image_tab_windows = ImagesConverter(self.m_image_tab, self)

        # Video Converter page
        self.m_video_tab = ctk.CTkFrame(self.m_tab_manager, fg_color="transparent")
        self.m_tab_manager.add(self.m_video_tab, text=self.i18n.t("tab_video_converter"))
        self.m_video_tab_windows = VideoTool(self.m_video_tab, self)

        # Screen Share page
        self.m_srceen_tab = ctk.CTkFrame(self.m_tab_manager, fg_color="transparent")
        self.m_tab_manager.add(self.m_srceen_tab, text=self.i18n.t("tab_screen_share"))

        # Help page
        self.m_help_tab = ctk.CTkFrame(self.m_tab_manager, fg_color="transparent")
        self.m_tab_manager.add(self.m_help_tab, text=self.i18n.t("tab_help"))
        self.m_help_tab_windows = Helper(self.m_help_tab, self)

        # Tool Settings page (new)
        self.m_tool_settings_tab = ctk.CTkFrame(self.m_tab_manager, fg_color="transparent")
        self.m_tab_manager.add(self.m_tool_settings_tab, text=self.i18n.t("tab_tool_settings"))
        self.m_tool_settings_tab_windows = ToolSettings(self.m_tool_settings_tab, self)

        self.m_tab_manager.pack(expand=True, fill=tk.BOTH)

    def OnThreadMessage(self, fromwho: str, towho: str, action: str, param: object = None) -> None:
        """
        引擎調度函數，各模組透過此函數間接操作或取得其他模組的資源。
        :param fromwho: 發送方識別字串
        :param towho:   接收方識別字串
        :param action:  操作類型
        :param param:   操作參數
        """
        logger.debug(
            "OnThreadMessage from=%s to=%s action=%s param=%s", fromwho, towho, action, param
        )

        if towho == mh.M_DOWNLOAD_DEBUG:
            self.m_debug_tab_windows.api(action, param)

        elif towho == mh.M_SETTING:
            self.m_setting_tab_windows.api(action, param)

        elif towho == mh.M_ENGINE and action == mh.A_UPDATALANG:
            for page in [
                self.m_debug_tab_windows,
                self.m_setting_tab_windows,
                self.m_tool_settings_tab_windows,
            ]:
                if hasattr(page, "api"):
                    page.api(mh.A_UPDATALANG)

    def on_closing(self) -> None:
        """
        Trigger function when closing main window
        :return: None
        """
        if self.m_file_tab_windows is not None:
            self.m_file_tab_windows.__del__()
            del self.m_file_tab_windows
            self.m_file_tab_windows = None

        # if messagebox.askokcancel("Quit", "Do you want to quit?"):
        self.root.destroy()

        if self.m_debug_tab_windows is not None:
            del self.m_debug_tab_windows
            self.m_debug_tab_windows = None

        if self.m_setting_tab_windows is not None:
            del self.m_setting_tab_windows
            self.m_setting_tab_windows = None

        if self.m_tool_settings_tab_windows is not None:
            del self.m_tool_settings_tab_windows
            self.m_tool_settings_tab_windows = None

    def __del__(self) -> None:
        """
        Release resources
        """
        # del self.m_debug_tab_windows
        self.m_debug_tab_windows = None

        if self.m_file_tab_windows is not None:
            self.m_file_tab_windows.__del__()
            del self.m_file_tab_windows
            self.m_file_tab_windows = None

        if self.m_tool_settings_tab_windows is not None:
            del self.m_tool_settings_tab_windows
            self.m_tool_settings_tab_windows = None


def get_version() -> str:
    try:
        response = requests.get(TOOL_VERSION_INFO_URL, timeout=5)
        if response.status_code != 200:
            logger.warning("tool version URL returned %s", response.status_code)
            return "[请到 GitHub 查看最新版本]"
        # pyproject.toml 內 ``version = "X.Y.Z"`` 行
        match = re.search(r'^version\s*=\s*"(\d+\.\d+\.\d+)"', response.text, re.MULTILINE)
        if match is None:
            return "[无法解析最新版本]"
        new_version = "v" + match.group(1)
        if TOOL_VERSION == new_version:
            return "[已是最新版本]"
        return "[推荐升级最新版本 " + new_version + "]"
    except Exception as err:
        logger.error("get_version failed: %s", err)
        return "[无法获取到最新版本]"


if __name__ == "__main__":
    import threading

    setup_logging()
    logger = get_logger(__name__)

    # CustomTkinter：強制深色主題（Windows 11 標題列也會跟著深色）
    ctk.set_appearance_mode("Dark")
    ctk.set_default_color_theme("blue")

    tool_windows = ctk.CTk()
    tool_windows.title("HoloCubic_AIO Tools\t  " + TOOL_VERSION)
    tool_windows.geometry("1000x655+10+10")
    tool_windows.resizable(False, False)

    # 將 ttk 元件（Notebook、Combobox、Treeview、Scrollbar）也染成深色，
    # 讓未遷移到 CTk 的部份不會出現白底突兀感
    _DARK_BG = "#2b2b2b"
    _DARK_PANEL = "#1f1f1f"
    _DARK_FG = "#dcdcdc"
    _DARK_ACCENT = "#1f6aa5"
    _DARK_HOVER = "#144870"
    _ttk_style = ttk.Style(tool_windows)
    _ttk_style.theme_use("default")
    _ttk_style.configure(
        "TNotebook", background=_DARK_BG, borderwidth=0, tabmargins=[2, 5, 2, 0],
    )
    _ttk_style.configure(
        "TNotebook.Tab",
        background=_DARK_BG, foreground=_DARK_FG,
        padding=[12, 4], borderwidth=0,
    )
    _ttk_style.map(
        "TNotebook.Tab",
        background=[("selected", _DARK_ACCENT), ("active", _DARK_HOVER)],
        foreground=[("selected", "#ffffff"), ("active", "#ffffff")],
    )
    _ttk_style.configure(
        "TCombobox",
        fieldbackground=_DARK_PANEL, background=_DARK_PANEL,
        foreground=_DARK_FG, arrowcolor=_DARK_FG, bordercolor=_DARK_BG,
        lightcolor=_DARK_BG, darkcolor=_DARK_BG, selectbackground=_DARK_ACCENT,
    )
    _ttk_style.map(
        "TCombobox",
        fieldbackground=[("readonly", _DARK_PANEL), ("disabled", _DARK_BG)],
        foreground=[("disabled", "#888888")],
    )
    # 下拉清單本身要在 option_add 設定（ttk 不直接管）
    tool_windows.option_add("*TCombobox*Listbox.background", _DARK_PANEL)
    tool_windows.option_add("*TCombobox*Listbox.foreground", _DARK_FG)
    tool_windows.option_add("*TCombobox*Listbox.selectBackground", _DARK_ACCENT)
    tool_windows.option_add("*TCombobox*Listbox.selectForeground", "#ffffff")
    _ttk_style.configure(
        "Treeview",
        background=_DARK_PANEL, foreground=_DARK_FG,
        fieldbackground=_DARK_PANEL, borderwidth=0,
    )
    _ttk_style.map(
        "Treeview",
        background=[("selected", _DARK_ACCENT)],
        foreground=[("selected", "#ffffff")],
    )

    engine = Engine(tool_windows)
    tku.center_window(tool_windows)

    def _fetch_version() -> None:
        hint = get_version()
        tool_windows.after(
            0, lambda: tool_windows.title(f"HoloCubic_AIO Tools\t  {TOOL_VERSION} {hint}")
        )

    threading.Thread(target=_fetch_version, daemon=True).start()
    tool_windows.mainloop()
