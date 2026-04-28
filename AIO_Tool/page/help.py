################################################################################
#
# Author: ClimbSnail(HQ)
# original source is here.
#   https://github.com/ClimbSnail/HoloCubic_AIO_Tool
#
#
################################################################################

import tkinter as tk

import customtkinter as ctk

from util.i18n import get_i18n


class Helper:
    """帮助信息页类。"""

    def __init__(self, father, engine, lock=None):
        """
        :param father: 父类窗口
        :param engine: 引擎对象，用于推送与其他控件的请求
        :param lock: 线程锁
        """
        self.m_engine = engine
        self.m_father = father
        self.i18n = get_i18n()
        self.init_info(self.m_father)

    def init_info(self, father):
        """初始化信息打印框。"""
        info = self.i18n.t("help_info")

        # 用 CTkFrame 當外殼，內部仍使用 tk.Text —— CTkTextbox 不支援 font tag。
        wrapper = ctk.CTkFrame(father, fg_color="transparent")
        wrapper.pack(fill="both", expand=True, padx=8, pady=8)

        self.m_project_info = tk.Text(
            wrapper, height=45, width=140, wrap="word", borderwidth=0, highlightthickness=0
        )
        self.m_project_info.tag_configure(
            "bold_italics",
            font=("Arial", 12, "bold", "italic"),
        )
        self.m_project_info.tag_configure("big", font=("Verdana", 16, "bold"))
        self.m_project_info.tag_configure(
            "color",
            foreground="#476042",
            font=("Tempus Sans ITC", 12, "bold"),
        )

        self.m_project_info.pack(fill="both", expand=True)
        self.m_project_info.config(state=tk.NORMAL)
        self.m_project_info.insert(tk.END, info, "big")
        self.m_project_info.config(state=tk.DISABLED)
