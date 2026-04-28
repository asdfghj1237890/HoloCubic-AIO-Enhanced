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
import threading
import time
import tkinter as tk
from tkinter import filedialog, ttk

import customtkinter as ctk
import esptool  # pip install esptool>=4.1,<5
import requests
import serial  # pip install pyserial
import serial.tools.list_ports  # noqa: F401  — submodule must be imported explicitly

import util.common as common
import util.massagehead as mh
from util.i18n import get_i18n
from util.logger import get_logger
from util.widget_base import EntryWithPlaceholder

logger = get_logger(__name__)
# GitHub raw common.h —— 包含 ``#define AIO_VERSION "X.Y"`` 行
VERSION_INFO_URL = (
    "https://raw.githubusercontent.com/asdfghj1237890/HoloCubic-AIO-Enhanced/main/AIO_Firmware_PIO/src/common.h"
)


class DownloadDebug:
    """
    菜单栏类
    """

    def __init__(
        self,
        father: tk.Misc,
        engine: object,  # Engine (avoid circular type import)
        lock: object | None = None,
    ) -> None:
        """
        DownloadDebug initialization
        :param father: Parent window
        :param engine: Engine object for component communication
        :param lock: Thread lock
        :return: None
        """
        self.__engine = engine
        self.__father = father
        self.ser = None
        self.receive_thread = None
        self.download_thread = None
        self.progress_bar_thread = None
        self.clean_flash_thread = None
        self._progress_stop_event = threading.Event()
        # 序列埠讀取執行緒的停止訊號（取代舊的全域 BOOL 旗標）
        self._serial_stop_event = threading.Event()
        self._serial_stop_event.set()  # 預設停止狀態，com_connect 啟動時會 clear()
        self.i18n = get_i18n()

        # Serial settings section（左欄，固定寬度 220 高度 270）
        self.connor_grid_frame = ctk.CTkFrame(self.__father, width=220, height=270)
        self.connor_grid_frame.place(x=11, y=10)
        self.connor_grid_frame.pack_propagate(False)  # 固定外框尺寸，內部 widget 不撐開
        ctk.CTkLabel(
            self.connor_grid_frame,
            text=self.i18n.t("serial_settings"),
            font=ctk.CTkFont(weight="bold"),
        ).pack(anchor=tk.W, padx=10, pady=(8, 4))
        self.create_com(self.connor_grid_frame)

        # 连接器相关控件
        cur_dir = os.getcwd()
        # Firmware download default values
        self.__pre_down_param_list = [
            {
                "bin_addr": "0x1000",
                "bin_path": os.path.join(cur_dir, "base_bin\\bootloader_qio_80m.bin"),
                "placeholder": self.i18n.t("choose_bootloader"),
            },
            {
                "bin_addr": "0x8000",
                "bin_path": os.path.join(cur_dir, "base_bin\\partitions.bin"),
                "placeholder": self.i18n.t("choose_partitions"),
            },
            {
                "bin_addr": "0xe000",
                "bin_path": os.path.join(cur_dir, "base_bin\\boot_app0.bin"),
                "placeholder": self.i18n.t("choose_boot_app0"),
            },
            {"bin_addr": "0x10000", "bin_path": "", "placeholder": self.i18n.t("choose_firmware")},
        ]
        # Firmware flash section（中欄，固定寬度 540 高度 270）
        self.connor_firmware_frame = ctk.CTkFrame(self.__father, width=540, height=270)
        self.connor_firmware_frame.place(x=240, y=10)
        self.connor_firmware_frame.pack_propagate(False)
        ctk.CTkLabel(
            self.connor_firmware_frame,
            text=self.i18n.t("firmware_flash"),
            font=ctk.CTkFont(weight="bold"),
        ).pack(anchor=tk.W, padx=10, pady=(8, 4))
        self.init_firmware(self.connor_firmware_frame)

        # Operation log section（右欄，固定寬度 380）
        self.connor_log_frame = ctk.CTkFrame(self.__father, width=380, height=270)
        self.connor_log_frame.place(x=790, y=10)
        self.connor_log_frame.pack_propagate(False)
        ctk.CTkLabel(
            self.connor_log_frame,
            text=self.i18n.t("operation_log"),
            font=ctk.CTkFont(weight="bold"),
        ).pack(anchor=tk.W, padx=10, pady=(8, 4))
        self.init_log(self.connor_log_frame)

        # Serial receive section（下排，固定寬度 1170 高度 410）
        self.connor_info_frame = ctk.CTkFrame(self.__father, width=1170, height=410)
        self.connor_info_frame.place(x=11, y=290)
        self.connor_info_frame.pack_propagate(False)
        ctk.CTkLabel(
            self.connor_info_frame,
            text=self.i18n.t("serial_receive"),
            font=ctk.CTkFont(weight="bold"),
        ).pack(anchor=tk.W, padx=10, pady=(8, 4))
        self.init_serial_receive(self.connor_info_frame)

        # 響應式佈局：tab 頁面尺寸變化時，log + info frame 跟著伸縮
        # （grid + firmware 維持固定尺寸 — 內容也是固定大小）
        self.__father.bind("<Configure>", self._on_father_resize)

        self.display_version()
        self.get_version_thread = threading.Thread(target=self.display_version)
        self.get_version_thread.start()

    def _on_father_resize(self, event: tk.Event) -> None:
        """父容器尺寸變化時，更新 log + info frame 的寬高使其填滿可用空間。

        CTkFrame 的 ``<Configure>`` 事件實際由內部 Canvas 觸發，event.width
        即為父容器當前寬度，所以不需檢查 ``event.widget``。
        """
        pw, ph = event.width, event.height
        # log frame：右上角，從 x=790 延伸到右邊距 11 px
        log_w = max(200, pw - 790 - 11)
        self.connor_log_frame.configure(width=log_w)
        # info frame：下排，左右各 11 px 邊距，y=290 起延伸到底部 10 px 邊距
        info_w = max(800, pw - 22)
        info_h = max(300, ph - 290 - 10)
        self.connor_info_frame.configure(width=info_w, height=info_h)

    def api(self, action: str, param: object = None) -> None:
        """
        下载模块对外的api接口
        """
        if action == mh.A_CLOSE_UART:  # 关闭串口
            # 取消任何進行中的固件燒錄，然後切換串口開關狀態
            # （com_connect 會根據按鈕當前文字自動切換 open/close）
            self.canle_download_firmware()
            self.com_connect()

    def display_version(self) -> None:
        def update_ui(version_text: str) -> None:
            self.m_version_info["state"] = tk.DISABLED
            self.m_version_var.set(version_text)

        version_text = self.i18n.t("unknown")
        try:
            response = requests.get(VERSION_INFO_URL, timeout=5)
            # common.h 內 ``#define AIO_VERSION "X.Y"`` 或 ``"X.Y.Z"``
            match = re.search(
                r'#define\s+AIO_VERSION\s+"(\d+(?:\.\d+){1,2})"',
                response.text,
            )
            if match:
                version_text = "v" + match.group(1)
        except Exception as err:
            logger.error("fetch firmware version failed: %s", err)

        if threading.current_thread() is threading.main_thread():
            update_ui(version_text)
        else:
            self.__father.after(0, lambda: update_ui(version_text))

    def init_firmware(self, father: tk.Misc) -> None:
        """
        固件操作
        """
        border_padx = 5  # 两个控件的间距
        border_pady = 3  # 两行控件的间距

        firmware_num = 4
        firmware_frame = [None for cnt in range(firmware_num)]
        self.__firmware_enable_val = [None for cnt in range(firmware_num)]
        self.__firmware_enable = [None for cnt in range(firmware_num)]
        self.__firmware_addr_val = [None for cnt in range(firmware_num)]
        self.__firmware_addr_entry = [None for cnt in range(firmware_num)]
        self.__firmware_path_val = [None for cnt in range(firmware_num)]
        self.__firmware_path_entry = [None for cnt in range(firmware_num)]
        self.__firmware_choose_botton = [None for cnt in range(firmware_num)]

        for pos in range(firmware_num):
            firmware_frame[pos] = ctk.CTkFrame(father, fg_color="transparent")
            # 勾選框
            self.__firmware_enable_val[pos] = tk.IntVar()
            self.__firmware_enable_val[pos].set(1)
            self.__firmware_enable[pos] = ctk.CTkCheckBox(
                firmware_frame[pos],
                text="",
                variable=self.__firmware_enable_val[pos],
                onvalue=1,
                offvalue=0,
                width=24,
            )
            self.__firmware_enable[pos].pack(side=tk.LEFT)
            # 地址輸入框 (width=9 chars ≈ 75 px)
            self.__firmware_addr_val[pos] = tk.StringVar()
            self.__firmware_addr_entry[pos] = ctk.CTkEntry(
                firmware_frame[pos],
                width=75,
                textvariable=self.__firmware_addr_val[pos],
            )
            self.__firmware_addr_val[pos].set(self.__pre_down_param_list[pos]["bin_addr"])
            self.__firmware_addr_entry[pos].pack(side=tk.LEFT, padx=border_padx)
            # 路徑輸入框 (width=40 chars ≈ 320 px)
            self.__firmware_path_val[pos] = tk.StringVar()
            self.__firmware_path_entry[pos] = EntryWithPlaceholder(
                firmware_frame[pos],
                width=320,
                placeholder=self.__pre_down_param_list[pos]["placeholder"],
                placeholder_color="grey",
                textvariable=self.__firmware_path_val[pos],
            )
            self.__firmware_path_val[pos].set(self.__pre_down_param_list[pos]["bin_path"])
            self.__firmware_path_entry[pos].pack(side=tk.LEFT, padx=border_padx)
            # 選擇按鈕
            # default-arg trick captures pos at lambda creation (avoids late-binding)
            self.__firmware_choose_botton[pos] = ctk.CTkButton(
                firmware_frame[pos],
                text=self.i18n.t("select_button"),
                command=lambda p=pos: self.choose_file(p),
                width=60,
                height=28,
            )
            self.__firmware_choose_botton[pos].pack(side=tk.RIGHT, fill=tk.X, padx=5)
            firmware_frame[pos].pack(side=tk.TOP, pady=border_pady)

        # 由於"選擇"按鈕在迴圈中綁定，pos 變數會被覆蓋；改顯式綁定每個 index
        self.__firmware_choose_botton[0].configure(command=lambda: self.choose_file(0))
        self.__firmware_choose_botton[1].configure(command=lambda: self.choose_file(1))
        self.__firmware_choose_botton[2].configure(command=lambda: self.choose_file(2))
        self.__firmware_choose_botton[3].configure(command=lambda: self.choose_file(3))

        # version_info_frame = tk.Frame(father, bg=father["bg"])
        # 版本信息
        # version_text = tk.Label(version_info_frame, text="AIO最新版本", bg=version_info_frame['bg'])
        # version_text.pack(side=tk.LEFT)
        # # 创建宽输入框
        # self.m_version_var = tk.StringVar()
        # self.m_version_info = EntryWithPlaceholder(version_info_frame, width=6, highlightcolor="LightGrey",
        #                                           placeholder="未知", placeholder_color="grey",
        #                                           textvariable=self.m_version_var)
        # self.m_version_info.pack(side=tk.LEFT, padx=5)
        # version_info_frame.pack(side=tk.TOP, pady=5)

        # 按鈕群組
        botton_group_frame = ctk.CTkFrame(father, fg_color="transparent")

        version_text = ctk.CTkLabel(
            botton_group_frame,
            text=self.i18n.t("aio_latest_version"),
        )
        version_text.pack(side=tk.LEFT)
        # 版本資訊輸入框
        self.m_version_var = tk.StringVar()
        self.m_version_info = EntryWithPlaceholder(
            botton_group_frame,
            width=60,
            placeholder=self.i18n.t("unknown"),
            placeholder_color="grey",
            textvariable=self.m_version_var,
        )
        self.m_version_info.pack(side=tk.LEFT, padx=5)

        # 清空按鈕
        self.m_clean_flash_botton = ctk.CTkButton(
            botton_group_frame,
            text=self.i18n.t("clear_chip"),
            command=self.clean_flash,
            width=80,
            height=28,
        )
        self.m_clean_flash_botton.pack(side=tk.LEFT, fill=tk.X, padx=border_padx)
        # 下載按鈕
        self.m_download_botton = ctk.CTkButton(
            botton_group_frame,
            text=self.i18n.t("flash_firmware"),
            command=self.down_and_canle,
            width=80,
            height=28,
        )
        self.m_download_botton.pack(side=tk.RIGHT, fill=tk.X, padx=0)
        botton_group_frame.pack(side=tk.TOP, pady=5)

        # 進度條（保留 tk.Canvas — 自繪矩形 fill）
        progress_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.progress_bar = tk.Canvas(
            progress_frame,
            width=450,
            height=15,
            bg="#1f1f1f",
            highlightthickness=0,
            borderwidth=0,
        )
        # 进度条的矩形框
        self.progress_bar_circle = self.progress_bar.create_rectangle(3, 3, 450, 14, outline="green", width=1)
        self.progress_bar_fill = self.progress_bar.create_rectangle(3, 3, 20, 14, outline="", width=1, fill="green")
        self.progress_bar.coords(self.progress_bar_fill, (3, 3, 0, 25))
        self.progress_bar.pack(side=tk.TOP, pady=0)
        progress_frame.pack(side=tk.TOP, pady=0)

    def _stop_progress_thread(self, wait: bool = True) -> None:
        if self.progress_bar_thread is not None:
            self._progress_stop_event.set()
            if wait and self.progress_bar_thread.is_alive():
                try:
                    self.progress_bar_thread.join(timeout=1)
                except Exception:
                    pass
            self.progress_bar_thread = None

    def schedule_display(self, all_time: float, update_interval: float) -> None:
        """
        进度条处理动画，原则上启动一个线程来执行本函数
        all_time：进度条的总时间(s)
        update_interval：更新时间间隔(s)
        """
        cycle_number = max(1, int(all_time / update_interval))
        self.print_log("all_time: " + str(all_time))
        for num in range(cycle_number - 1):
            if self._progress_stop_event.is_set():
                break
            self.progress_bar.coords(self.progress_bar_fill, (3, 3, (num / cycle_number) * 440, 14))
            self.__father.update()
            if self._progress_stop_event.wait(update_interval):
                break

    def choose_file(self, num: int) -> None:
        """
        点击"选择"文件触发的函数
        :pos: 为触发”选择“按钮的编号
        :return:
        """
        # 打开文件对话框 获取文件路径
        # defaultextension 为选取保存类型中的拓展名为文件名
        # filetypes为文件拓展名
        filepath = filedialog.askopenfilename(
            title=self.i18n.t("select_bin_file"),
            defaultextension=".espace",
            filetypes=[("BIN", ".bin .Bin")],
        )
        if filepath is None or filepath == "":
            return None
        else:
            self.__firmware_path_entry[num].delete(0, tk.END)  # 清空文本框
            self.__firmware_path_entry[num].insert(tk.END, filepath)

    def clean_flash(self) -> None:
        """
        擦除闪存
        """

        def clean_func() -> None:
            self.m_clean_flash_botton["text"] = "清空中"
            self.m_connect_button["state"] = tk.DISABLED
            self.m_reboot_button["state"] = tk.DISABLED
            # self.m_clean_flash_botton["state"] = tk.DISABLED
            self.m_download_botton["state"] = tk.DISABLED
            self.print_log("清空芯片中.....")
            self.__father.update()

            # esptool.py erase_region 0x20000 0x4000
            # esptool.py erase_flash
            select_com = self.m_com_select.get().strip()
            cmd = ["--port", select_com, "erase_flash"]
            # cmd = ['erase_flash']
            esptool.main(cmd)
            self.print_log("清空芯片成功！")
            # 更新按钮状态
            self.m_clean_flash_botton["text"] = "清空芯片"
            self.m_connect_button["state"] = tk.NORMAL
            self.m_reboot_button["state"] = tk.NORMAL
            # self.m_clean_flash_botton["state"] = tk.NORMAL
            self.m_download_botton["state"] = tk.NORMAL
            self.__father.update()

        if self.m_clean_flash_botton["text"] == "清空芯片":
            self.clean_flash_thread = threading.Thread(
                target=clean_func,
            )
            self.clean_flash_thread.start()
        else:
            # 杀线程
            common._async_raise(self.clean_flash_thread)
            self.clean_flash_thread = None
            self.m_clean_flash_botton["text"] = "清空芯片"
            self.m_connect_button["state"] = tk.NORMAL
            self.m_reboot_button["state"] = tk.NORMAL
            # self.m_clean_flash_botton["state"] = tk.NORMAL
            self.m_download_botton["state"] = tk.NORMAL
            self.__father.update()

    def down_and_canle(self) -> None:
        """
        下载与取消按钮
        """
        if self.m_download_botton["text"] == self.i18n.t("flash_firmware"):
            self.download_firmware()
        elif self.m_download_botton["text"] == self.i18n.t("cancel_flash"):
            self.canle_download_firmware()

    def canle_download_firmware(self) -> None:
        """
        取消下载固件
        """
        if self.download_thread is not None:
            try:
                # 杀线程
                if self.download_thread.is_alive():
                    common._async_raise(self.download_thread)
            except Exception as err:
                logger.error("kill download thread failed: %s", err)
            finally:
                self.download_thread = None

        self.m_download_botton["text"] = self.i18n.t("flash_firmware")
        self.m_connect_button["state"] = tk.NORMAL
        self.m_reboot_button["state"] = tk.NORMAL
        self.m_clean_flash_botton["state"] = tk.NORMAL
        self.m_download_botton["state"] = tk.NORMAL

        if self.progress_bar_thread is not None:
            self._stop_progress_thread()

        # 复位进度条
        self.progress_bar.coords(self.progress_bar_fill, (3, 3, 0, 25))

    def download_firmware(self) -> None:
        """
        下载固件
        """
        self.m_download_botton["text"] = self.i18n.t("cancel_flash")
        self.m_connect_button["state"] = tk.DISABLED
        self.m_reboot_button["state"] = tk.DISABLED
        self.m_clean_flash_botton["state"] = tk.DISABLED
        self.m_download_botton["state"] = tk.NORMAL
        self.print_log(self.i18n.t("flashing"))
        down_flag, param = self.get_download_param()
        if down_flag == "disable":
            self.print_log("参数错误，刷写终止！")
            self.canle_download_firmware()
            return None

        cmd = [
            "--port",
            param["port"],
            "--baud",
            param["baud"],
            "--after",
            "hard_reset",
            "write_flash",
            "-fs",
            "4MB",
        ]

        all_time = 0  # 粗略认为连接并复位芯片需要0.5s钟
        speed = int(param["baud"])

        del param["port"]
        del param["baud"]

        for key, value in param.items():
            cmd.append(key)
            cmd.append(value)
            try:
                # 波特率中10或11个10个比特能传输一个字节，这里同意取10
                all_time = all_time + os.path.getsize(value) * 10 / speed
            except Exception as err:
                logger.error("estimate flash time failed for %s: %s", value, err)

        self._progress_stop_event.clear()
        self.progress_bar_thread = threading.Thread(
            target=self.schedule_display,
            args=(
                all_time,
                0.1,
            ),
            daemon=True,
        )
        # 进度条进程要在下载进程之前启动（为了在下载失败时可以立即查杀进度条进程）
        self.download_thread = threading.Thread(target=self.down_action, args=(cmd,))
        self.progress_bar_thread.start()
        self.download_thread.start()

    def down_action(self, cmd: list[str]) -> None:
        cmd_str = " ".join(cmd)
        self.print_log("-" * 15)
        self.print_log(cmd_str)
        self.print_log("-" * 15)
        self.__father.update()

        flash_success = False
        try:
            esptool.main(cmd)
            flash_success = True
        except serial.SerialException as err:
            logger.error("flash serial error: %s", err)
            self.print_log(err)
        except Exception as err:
            logger.error("flash failed: %s", err)
            self.print_log(err)
        finally:
            self.m_download_botton["text"] = self.i18n.t("flash_firmware")
            self.m_connect_button["state"] = tk.NORMAL
            self.m_reboot_button["state"] = tk.NORMAL
            self.m_clean_flash_botton["state"] = tk.NORMAL
            self.m_download_botton["state"] = tk.NORMAL

            if self.progress_bar_thread is not None:
                self._stop_progress_thread()

            # 复位进度条
            self.progress_bar.coords(self.progress_bar_fill, (3, 3, 0, 25))

            if flash_success:
                self.print_log(self.i18n.t("flash_success"))
            else:
                self.print_log("Flash firmware failed, please check the serial port.")

    def init_log(self, father: tk.Misc) -> None:
        """初始化日誌打印框（保留 tk.Text 給彩色標記）。"""
        info_width = 39
        info_height = 15

        # CTk Scrollbar
        self.m_log_scrollbar = ctk.CTkScrollbar(father, orientation="vertical")
        # tk.Text — CTkTextbox 不支援 font tag，這裡保留原生（深色主題）
        self.m_log = tk.Text(
            father,
            width=info_width,
            height=info_height,
            yscrollcommand=self.m_log_scrollbar.set,
            state=tk.DISABLED,
            borderwidth=0,
            highlightthickness=0,
            bg="#1f1f1f",
            fg="#dcdcdc",
            insertbackground="#dcdcdc",
        )
        self.m_log_scrollbar.configure(command=self.m_log.yview)

        # 排版：先放底部清空鈕（保留小尺寸不撐滿）+ 右側捲軸，最後文字框 expand 填滿剩餘空間
        m_clear = ctk.CTkButton(
            father,
            text="X",
            command=self.clear_log,
            width=24,
            height=24,
        )
        m_clear.pack(side=tk.BOTTOM, anchor=tk.E, padx=3, pady=(0, 3))
        self.m_log_scrollbar.pack(side=tk.RIGHT, fill=tk.Y, padx=(0, 3), pady=3)
        self.m_log.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=3, pady=3)

    def print_log(self, msg: object) -> None:
        msg = str(msg) + "\n"
        self.m_log.config(state=tk.NORMAL)
        self.m_log.insert(tk.END, msg)
        self.m_log.config(state=tk.DISABLED)
        self.m_log.yview_moveto(1)

    def clear_log(self) -> None:
        """
        清空日志按钮
        :return: None
        """
        self.m_log.config(state=tk.NORMAL)
        self.m_log.delete(1.0, tk.END)
        self.m_log.config(state=tk.DISABLED)

    def init_serial_receive(self, father: tk.Misc) -> None:
        info_width = 135
        info_height = 27

        # CTk Scrollbar
        self.m_scrollbar = ctk.CTkScrollbar(father, orientation="vertical")
        # tk.Text — 保留原生支援 yview_moveto（深色主題）
        self.m_msg = tk.Text(
            father,
            width=info_width,
            height=info_height,
            yscrollcommand=self.m_scrollbar.set,
            state=tk.DISABLED,
            borderwidth=0,
            highlightthickness=0,
            bg="#1f1f1f",
            fg="#dcdcdc",
            insertbackground="#dcdcdc",
        )
        self.m_scrollbar.configure(command=self.m_msg.yview)

        # 排版：先放底部清空鈕（保留小尺寸不撐滿）+ 右側捲軸，最後文字框 expand 填滿剩餘空間
        self.m_clear = ctk.CTkButton(
            father,
            text="X",
            command=self.clear_msg,
            width=24,
            height=24,
        )
        self.m_clear.pack(side=tk.BOTTOM, anchor=tk.E, padx=3, pady=(0, 3))
        self.m_scrollbar.pack(side=tk.RIGHT, fill=tk.Y, padx=(0, 3), pady=3)
        self.m_msg.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=3, pady=3)

    def clear_msg(self) -> None:
        """
        清空日志按钮
        :return: None
        """
        self.m_msg.config(state=tk.NORMAL)
        self.m_msg.delete(1.0, tk.END)
        self.m_msg.config(state=tk.DISABLED)

    def create_com(self, father: tk.Misc) -> None:
        """
        创建Comm相关控件
        :param father: 父类窗口
        :return: None
        """
        # 获取可用COM口名字
        com_obj_list = list(serial.tools.list_ports.comports())
        com_tuple = [com_obj[0] for com_obj in com_obj_list]
        if len(com_tuple) == 0:
            com_tuple = [""]

        border_padx = 15

        # Port selection (ttk.Combobox 保留 — bind('<FocusOut>') 在 CTk 不容易做)
        com_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_com_label = ctk.CTkLabel(com_frame, text=self.i18n.t("port_number"))
        self.m_com_label.pack(side=tk.LEFT, padx=border_padx)
        self.m_com_select = ttk.Combobox(com_frame, width=8, state="readonly")
        self.m_com_select["value"] = tuple(com_tuple)
        self.m_com_select.bind("<FocusOut>", self.com_pull_down)
        self.m_com_select.current(0)
        self.m_com_select.pack(side=tk.RIGHT, padx=border_padx)
        com_frame.pack(side=tk.TOP, pady=5)

        # Baud rate
        baud_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_baud_label = ctk.CTkLabel(baud_frame, text=self.i18n.t("baud_rate"))
        self.m_baud_label.pack(side=tk.LEFT, padx=border_padx)
        self.m_baud_select = ttk.Combobox(baud_frame, width=8, state="readonly")
        self.m_baud_select["value"] = (
            "9600",
            "38400",
            "57600",
            "115200",
            "230400",
            "460800",
            "576000",
            "921600",
            "1152000",
        )
        self.m_baud_select.current(7)
        self.m_baud_select.pack(side=tk.RIGHT, padx=border_padx)
        baud_frame.pack(side=tk.TOP, pady=5)

        # Data bit
        data_bit_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_data_bit_label = ctk.CTkLabel(data_bit_frame, text=self.i18n.t("data_bit"))
        self.m_data_bit_label.pack(side=tk.LEFT, padx=border_padx)
        self.m_data_bit_select = ttk.Combobox(data_bit_frame, width=8, state="readonly")
        self.m_data_bit_select["value"] = ("5", "6", "7", "8")
        self.m_data_bit_select.current(3)
        self.m_data_bit_select.pack(side=tk.RIGHT, padx=border_padx)
        data_bit_frame.pack(side=tk.TOP, pady=5)

        # Parity
        check_bit_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_check_bit_label = ctk.CTkLabel(check_bit_frame, text=self.i18n.t("check_bit"))
        self.m_check_bit_label.pack(side=tk.LEFT, padx=border_padx)
        self.m_check_bit_select = ttk.Combobox(check_bit_frame, width=8, state="readonly")
        self.m_check_bit_select["value"] = (
            self.i18n.t("no_check"),
            self.i18n.t("odd_check"),
            self.i18n.t("even_check"),
            self.i18n.t("zero_check"),
            self.i18n.t("one_check"),
        )
        self.m_check_bit_select.current(0)
        self.m_check_bit_select.pack(side=tk.RIGHT, padx=border_padx)
        check_bit_frame.pack(side=tk.TOP, pady=5)

        # Stop bit
        stop_bit_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_stop_bit_label = ctk.CTkLabel(stop_bit_frame, text=self.i18n.t("stop_bit"))
        self.m_stop_bit_label.pack(side=tk.LEFT, padx=border_padx)
        self.m_stop_bit_select = ttk.Combobox(stop_bit_frame, width=8, state="readonly")
        self.m_stop_bit_select["value"] = (
            self.i18n.t("one_bit"),
            self.i18n.t("one_half_bit"),
            self.i18n.t("two_bit"),
        )
        self.m_stop_bit_select.current(0)
        self.m_stop_bit_select.pack(side=tk.RIGHT, padx=border_padx)
        stop_bit_frame.pack(side=tk.TOP, pady=5)

        # Buttons
        botton_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_connect_button = ctk.CTkButton(
            botton_frame,
            text=self.i18n.t("open_serial"),
            command=self.com_connect,
            width=120,
            height=28,
        )
        self.m_connect_button.pack(side=tk.LEFT, fill=tk.X, padx=5)
        self.m_reboot_button = ctk.CTkButton(
            botton_frame,
            text=self.i18n.t("reboot"),
            command=self.esp_reset,
            width=80,
            height=28,
        )
        self.m_reboot_button.pack(side=tk.RIGHT, fill=tk.X, padx=5)
        self.m_reboot_button.configure(state=tk.NORMAL)

        botton_frame.pack(side=tk.TOP, pady=5)

    def com_pull_down(self, event: tk.Event) -> None:
        """
        comm口下拉框被点击的时候 触发端口扫描
        """
        # 获取可用COM口名字
        com_obj_list = list(serial.tools.list_ports.comports())
        com_tuple = [com_obj[0] for com_obj in com_obj_list]
        if len(com_tuple) == 0:
            com_tuple = [""]
        # 获取当前下拉框的值
        choose_com = self.m_com_select.get()
        choose_index = 0
        if choose_com in com_tuple:
            choose_index = com_tuple.index(choose_com)
        # 设置下拉框里的列表
        self.m_com_select["value"] = tuple(com_tuple)
        # 更改下拉框中的内容
        self.m_com_select.current(choose_index)

    def com_connect(self) -> None:
        if self.m_connect_button["text"] == self.i18n.t("open_serial"):
            down_flag, param = self.get_download_param()
            if self.ser is not None:
                self.ser.close()
            self.ser = serial.Serial(param["port"], param["baud"], timeout=10)

            # Check if opened successfully
            if self.ser.is_open:
                self._serial_stop_event.clear()
                self.receive_thread = threading.Thread(
                    target=self.read_data,
                    args=(self.ser,),
                    daemon=True,
                )
                self.receive_thread.start()

                self.m_connect_button["text"] = self.i18n.t("close_serial")
                self.m_com_select["state"] = tk.DISABLED
                self.m_baud_select["state"] = tk.DISABLED
                self.m_data_bit_select["state"] = tk.DISABLED
                self.m_check_bit_select["state"] = tk.DISABLED
                self.m_stop_bit_select["state"] = tk.DISABLED
                self.m_reboot_button["state"] = tk.DISABLED
                self.m_clean_flash_botton["state"] = tk.DISABLED
                self.m_download_botton["state"] = tk.DISABLED
        else:
            self.m_connect_button["text"] = self.i18n.t("open_serial")
            self.m_com_select["state"] = tk.NORMAL
            self.m_baud_select["state"] = tk.NORMAL
            self.m_data_bit_select["state"] = tk.NORMAL
            self.m_check_bit_select["state"] = tk.NORMAL
            self.m_stop_bit_select["state"] = tk.NORMAL
            self.m_reboot_button["state"] = tk.NORMAL
            self.m_clean_flash_botton["state"] = tk.NORMAL
            self.m_download_botton["state"] = tk.NORMAL

            if self.ser is not None:
                self.ser.close()  # 关闭串口
                del self.ser
                self.ser = None
                # 通知背景執行緒停止，並等待結束
                self._serial_stop_event.set()
                if self.receive_thread is not None and self.receive_thread.is_alive():
                    self.receive_thread.join(timeout=1.0)
                self.receive_thread = None
                self.print_log("Receive_thread stop")

    def read_data(self, ser: serial.Serial) -> None:
        """背景接收序列埠資料；stop event 觸發時跳出迴圈。"""
        self.print_log("Receive_thread start")
        while not self._serial_stop_event.is_set():
            if ser.in_waiting:
                try:
                    data = ser.read(ser.in_waiting).decode("utf8")
                    self.m_msg.config(state=tk.NORMAL)
                    self.m_msg.insert(tk.END, data)
                    self.m_msg.config(state=tk.DISABLED)
                    self.m_msg.yview_moveto(1)
                except Exception as err:
                    logger.debug("read_data ignored: %s", err)
            time.sleep(0.1)

    def get_download_param(self) -> tuple[str, dict[str, str]]:
        """
        获取下载参数
        """
        data_map = {}
        firmware_num = 4  # 四个下载项
        for pos in range(firmware_num):
            firmware_addr = self.__firmware_addr_val[pos].get().strip()
            firmware_path = self.__firmware_path_entry[pos].get().strip()
            if self.__firmware_enable_val[pos].get() == 1 and firmware_addr != "" and firmware_path != "":
                data_map[firmware_addr] = firmware_path

        if data_map == {}:  # 无下载内容
            down_flag = "disable"
        else:
            down_flag = "enable"

        data_map["port"] = self.m_com_select.get().strip()
        data_map["baud"] = self.m_baud_select.get().strip()
        if data_map["port"] == "" or data_map["baud"] == "":
            # 串口信息为空
            down_flag = "disable"

        return down_flag, data_map

    def esp_reset(self) -> None:
        """
        重启芯片
        """
        self.m_connect_button["state"] = tk.DISABLED
        self.m_reboot_button["state"] = tk.DISABLED
        self.m_clean_flash_botton["state"] = tk.DISABLED
        self.m_download_botton["state"] = tk.DISABLED
        down_flag, param = self.get_download_param()
        self.print_log("已发送重启指令！")
        self.__father.update()

        port = serial.Serial(param["port"], param["baud"], timeout=10)

        port.setDTR(False)  # IO0=HIGH
        port.setRTS(True)  # EN=LOW, chip in reset
        time.sleep(0.05)
        port.setRTS(False)  # EN=HIGH, chip out of reset
        # 0.5 needed for ESP32 rev0 and rev1
        time.sleep(0.05)  # 0.5 / 0.05

        port.close()
        del port
        self.m_connect_button["state"] = tk.NORMAL
        self.m_reboot_button["state"] = tk.NORMAL
        self.m_clean_flash_botton["state"] = tk.NORMAL
        self.m_download_botton["state"] = tk.NORMAL

    def __del__(self) -> None:
        """資源釋放：通知 receive_thread 停止；esptool download_thread 仍以 _async_raise 中斷。"""
        if self.ser is not None:
            self.ser.close()  # 关闭串口
            self.ser = None
        # receive_thread 改為合作式停止
        self._serial_stop_event.set()
        if self.receive_thread is not None and self.receive_thread.is_alive():
            self.receive_thread.join(timeout=1.0)
        # esptool 的 download_thread 沒有合作式中斷點，仍以 _async_raise 強制終止
        # TODO: 改用 esptool API 的 stub-loader cancel 介面（需上游支援）
        if self.download_thread is not None:
            if self.download_thread.is_alive():
                common._async_raise(self.download_thread)
        if self.progress_bar_thread is not None:
            self._stop_progress_thread(wait=False)
