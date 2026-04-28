################################################################################
#
# Author: ClimbSnail(HQ)
# original source is here.
#   https://github.com/ClimbSnail/HoloCubic_AIO_Tool
#
#
################################################################################

import codecs
import json
import threading
import time
import tkinter as tk
import traceback
from tkinter import ttk

import customtkinter as ctk
import serial
import serial.tools.list_ports  # noqa: F401  — submodule must be imported explicitly

import util.common as common
import util.massagehead as mh
from util.i18n import get_i18n
from util.logger import get_logger

logger = get_logger(__name__)


class Setting:
    """
    参数设置类
    """

    def __init__(
        self,
        father: tk.Misc,
        engine: object,
        lock: object | None = None,
    ) -> None:
        """
        Setting initialization
        :param father: Parent window
        :param engine: Engine object for component communication
        :param lock: Thread lock
        :return: None
        """
        self.m_engine = engine
        self.__father = father
        self.cfg_name = common.get_resource_path("cubictool.json")
        self.data_info = None
        self.receive_thread = None
        self.ser = None
        # 序列埠讀取執行緒的停止訊號（取代舊的全域 BOOL 旗標）
        self._serial_stop_event = threading.Event()
        self._serial_stop_event.set()  # 預設停止狀態
        self.i18n = get_i18n()

        fp = codecs.open(self.cfg_name, "r", "utf8")
        self.data_info = json.load(fp)
        fp.close()

        # WiFi settings frame —— 左上角固定 (11, 10)，寬度跟著視窗變化
        self.wifi_grid_frame = ctk.CTkFrame(self.__father, width=600, height=400)
        self.wifi_grid_frame.place(x=11, y=10)
        self.wifi_grid_frame.pack_propagate(False)
        wifi_title = ctk.CTkLabel(
            self.wifi_grid_frame,
            text=self.i18n.t("wifi_settings"),
            font=ctk.CTkFont(weight="bold"),
        )
        wifi_title.pack(anchor=tk.W, padx=10, pady=(8, 4))
        self.create_wifi(self.wifi_grid_frame)

        # UART connection frame —— 右上角，靠右對齊，寬 540
        self.uart_father = ctk.CTkFrame(self.__father, width=540, height=80)
        self.uart_father.place(x=620, y=10)
        self.uart_father.pack_propagate(False)
        self.connect_uart(self.uart_father)

        # 響應式佈局：tab 父容器尺寸變化時，更新 frame 寬高
        self.__father.bind("<Configure>", self._on_father_resize)

    def connect_uart(self, father: tk.Misc) -> None:
        """
        创建串口连接控件
        :param father: 父类窗口
        :return: None
        """
        # 获取可用COM口名字
        com_obj_list = list(serial.tools.list_ports.comports())
        com_tuple = [com_obj[0] for com_obj in com_obj_list]
        if len(com_tuple) == 0:
            com_tuple = [""]

        border_padx = 15
        # Port selection (ttk.Combobox 保留 — CTkOptionMenu 不支援 readonly+bind('<FocusOut>'))
        com_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_com_label = ctk.CTkLabel(com_frame, text=self.i18n.t("port_number"))
        self.m_com_label.pack(side=tk.LEFT, padx=border_padx)

        self.m_com_select = ttk.Combobox(com_frame, width=8, state="readonly")
        self.m_com_select["value"] = tuple(com_tuple)
        self.m_com_select.bind("<FocusOut>", self.com_pull_down)
        self.m_com_select.current(0)
        self.m_com_select.pack(side=tk.LEFT, padx=border_padx)
        com_frame.pack(side=tk.LEFT, pady=5)

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
        self.m_baud_select.current(3)
        self.m_baud_select.pack(side=tk.LEFT, padx=border_padx)
        baud_frame.pack(side=tk.LEFT, pady=5)

        # Connect button
        botton_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_connect_button = ctk.CTkButton(
            botton_frame,
            text=self.i18n.t("open_serial"),
            command=self.com_connect,
            width=90,
            height=28,
        )
        self.m_connect_button.pack(side=tk.LEFT, fill=tk.X, padx=5)

        botton_frame.pack(side=tk.LEFT, pady=5)

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
        # 先关闭下载页的串口
        self.m_engine.on_thread_message(mh.M_SETTING, mh.M_DOWNLOAD_DEBUG, mh.A_CLOSE_UART, None)

        if self.m_connect_button.cget("text") == self.i18n.t("open_serial"):
            port = self.m_com_select.get().strip()
            baud = self.m_baud_select.get().strip()
            if self.ser is not None:
                self.ser.close()  # 关闭串口
            self.ser = serial.Serial(port, baud, timeout=10)

            # 判断是否打开成功
            if self.ser.is_open:
                self._serial_stop_event.clear()
                self.receive_thread = threading.Thread(
                    target=self.read_data,
                    args=(self.ser,),
                    daemon=True,
                )
                self.receive_thread.start()

                self.m_connect_button.configure(text=self.i18n.t("close_serial"))
                self.m_com_select["state"] = tk.DISABLED
                self.m_baud_select["state"] = tk.DISABLED
        else:
            self.m_connect_button.configure(text=self.i18n.t("open_serial"))
            self.m_com_select["state"] = tk.NORMAL
            self.m_baud_select["state"] = tk.NORMAL

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
                data = ser.read(ser.in_waiting)
                logger.debug("Receive---> %s", data)
            time.sleep(0.2)

    def print_log(self, msg: str) -> None:
        logger.info(msg)
        self.set_param("ssid_1", "12345678")

    # 帧格式为
    # 帧头0x2323（2字节）+ 帧长度（2字节）+ 发送者（2字节）
    #                  + 接收者（2字节）+ 消息类型（2字节）
    #                   + 消息数据（帧长度-10）+ 帧尾/r/n（2字节）
    def set_param(self, key: str, value: str) -> None:
        """
        设置参数
        :param key: 设置的key
        :param value: 值
        :return: None
        """
        value_type = {
            "String": mh.VT.VALUE_TYPE_STRING,
            "UChar": mh.VT.VALUE_TYPE_UCHAR,
            "Int": mh.VT.VALUE_TYPE_INT,
        }
        try:
            info = self.data_info[key]
            logger.debug("set_param info: %s", info)
            send_data = mh.SettingMsg()
            send_data.action_type = mh.AT.AT_SETTING_SET
            send_data.prefs_name = bytes(info["namespace"], encoding="utf8")
            send_data.key = bytes(key, encoding="utf8")
            send_data.type = value_type[info["type"]].to_bytes(1, byteorder="little", signed=True)
            logger.debug("set_param type bytes: %s", send_data.type)
            send_data.value = bytes(value, encoding="utf8")
            logger.debug("set_param encoded: %s", send_data.encode())
            if self.ser is not None:
                self.ser.write(send_data.encode())
        except Exception as err:
            logger.error("set_param failed:\n%s", traceback.format_exc())
            logger.error("set_param error: %s", err)

    # 帧格式为
    # 帧头0x2323（2字节）+ 帧长度（2字节）+ 发送者（2字节）
    #                  + 接收者（2字节）+ 消息类型（2字节）
    #                   + 消息数据（帧长度-10）+ 帧尾/r/n（2字节）
    def get_param(self, key: str) -> None:
        """
        获取参数
        :param key:
        :return: string(value)
        """
        value_type = {
            "String": mh.VT.VALUE_TYPE_STRING,
            "UChar": mh.VT.VALUE_TYPE_UCHAR,
            "Int": mh.VT.VALUE_TYPE_INT,
        }
        try:
            info = self.data_info[key]
            logger.debug("get_param info: %s", info)
            send_data = mh.SettingMsg()
            send_data.action_type = mh.AT.AT_SETTING_GET
            send_data.prefs_name = bytes(info["namespace"], encoding="utf8")
            send_data.key = bytes(key, encoding="utf8")
            send_data.type = value_type[info["type"]].to_bytes(1, byteorder="little", signed=True)
            logger.debug("get_param type bytes: %s", send_data.type)
            logger.debug("send_data --> %s", send_data.encode(">"))
            if self.ser is not None:
                self.ser.write(send_data.encode(">"))
        except Exception as err:
            logger.error("get_param failed:\n%s", traceback.format_exc())
            logger.error("get_param error: %s", err)

    def create_wifi(self, father: tk.Misc) -> None:
        """建立 WiFi 控件。"""
        get_botton = ctk.CTkButton(
            father,
            text=self.i18n.t("get_button"),
            command=lambda: self.get_param("ssid"),
            width=60,
            height=28,
        )
        get_botton.pack(side=tk.RIGHT, fill=tk.X, padx=5, pady=8)

    def _on_father_resize(self, event: tk.Event) -> None:
        """父容器尺寸變化時，WiFi 框跟著伸縮、UART 框靠右對齊保持固定寬度。"""
        pw, ph = event.width, event.height
        # WiFi 框：左 11 邊距，右邊到 uart 框左邊（保留 uart 寬 540 + 10 px gap + 11 px 右邊距）
        wifi_w = max(300, pw - 540 - 10 - 22)
        wifi_h = max(200, ph - 20)
        self.wifi_grid_frame.configure(width=wifi_w, height=wifi_h)
        # UART 框：靠右，x = 視窗寬 - 540 - 11
        uart_x = max(620, pw - 540 - 11)
        self.uart_father.place_configure(x=uart_x)

    def __del__(self) -> None:
        """資源釋放：通知 receive_thread 停止並等待結束。"""
        if self.ser is not None:
            self.ser.close()  # 关闭串口
            self.ser = None
        # 合作式停止 receive_thread
        self._serial_stop_event.set()
        if self.receive_thread is not None and self.receive_thread.is_alive():
            self.receive_thread.join(timeout=1.0)
