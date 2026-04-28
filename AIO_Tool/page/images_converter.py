################################################################################
#
# Author: ClimbSnail(HQ)
# original source is here.
#   https://github.com/ClimbSnail/HoloCubic_AIO_Tool
#
#
################################################################################

import os
import os.path
import shutil
import tkinter as tk
from tkinter import ttk

import customtkinter as ctk
from PIL import Image

from util.common import CACHE_PATH, ROOT_PATH
from util.convertor_core import Converter, _Const
from util.i18n import get_i18n
from util.widget_base import EntryWithPlaceholder

FLAG = _Const()

color_dict = {
    "CF_TRUE_COLOR": FLAG.CF_TRUE_COLOR,
    "CF_TRUE_COLOR_ALPHA": FLAG.CF_TRUE_COLOR_ALPHA,
    "CF_TRUE_COLOR_CHROMA": FLAG.CF_TRUE_COLOR_CHROMA,
    "CF_INDEXED_1_BIT": FLAG.CF_INDEXED_1_BIT,
    "CF_INDEXED_2_BIT": FLAG.CF_INDEXED_2_BIT,
    "CF_INDEXED_4_BIT": FLAG.CF_INDEXED_4_BIT,
    "CF_INDEXED_8_BIT": FLAG.CF_INDEXED_8_BIT,
    "CF_ALPHA_1_BIT": FLAG.CF_ALPHA_1_BIT,
    "CF_ALPHA_2_BIT": FLAG.CF_ALPHA_2_BIT,
    "CF_ALPHA_4_BIT": FLAG.CF_ALPHA_4_BIT,
    "CF_ALPHA_8_BIT": FLAG.CF_ALPHA_8_BIT,
    "CF_RAW": FLAG.CF_RAW,
    "CF_RAW_ALPHA": FLAG.CF_RAW_ALPHA,
    "CF_RAW_CHROMA": FLAG.CF_RAW_CHROMA,
}

output_dict = {
    "C_array": -1,
    "Binary_332": FLAG.CF_TRUE_COLOR_332,
    "Binary_565": FLAG.CF_TRUE_COLOR_565,
    "Binary_565_SWAP": FLAG.CF_TRUE_COLOR_565_SWAP,
    "Binary_888": FLAG.CF_TRUE_COLOR_888,
}


class ImagesConverter:
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
        VideoTool初始化
        :param father:父类窗口
        :param engine:引擎对象，用于推送与其他控件的请求
        :param lock:线程锁
        :return:None
        """
        self.__engine = engine  # 负责各个组件之间数据调度的引擎
        self.__father = father  # 保存父窗口
        self.i18n = get_i18n()

        self.m_select_frame = ctk.CTkFrame(self.__father, fg_color="transparent")
        self.init_setting(self.m_select_frame)
        self.m_select_frame.pack(side=tk.TOP, pady=5)

        self.m_path_frame = ctk.CTkFrame(self.__father, fg_color="transparent")
        self.init_image_path(self.m_path_frame)
        self.m_path_frame.pack(side=tk.TOP, pady=5)

        # CTk 沒有 LabelFrame，使用 CTkFrame + 標題 CTkLabel
        self.m_info_frame = ctk.CTkFrame(self.__father)
        info_title = ctk.CTkLabel(
            self.m_info_frame,
            text="Conversion Log",
            font=ctk.CTkFont(weight="bold"),
        )
        info_title.pack(anchor=tk.W, padx=10, pady=(8, 4))
        self.init_info(self.m_info_frame)
        self.m_info_frame.pack(side=tk.TOP, pady=5, fill=tk.BOTH, expand=True)

    def init_setting(self, father: tk.Misc) -> None:
        """
        初始化设置条
        :param father: 父容器
        :return: None
        """
        border_padx = 15  # 两个控件的间距

        self.m_jpg_label = ctk.CTkLabel(father, text=self.i18n.t("jpg_output"))
        self.m_jpg_label.pack(side=tk.LEFT)
        # 勾选框的键值对象
        self.__jpg_enable_val = tk.IntVar()
        self.__jpg_enable_val.set(1)
        # 勾选框
        self.__jpg_enable = ctk.CTkCheckBox(
            father,
            text="",
            variable=self.__jpg_enable_val,
            onvalue=1,
            offvalue=0,
            command=self.enable_jpg,
            width=24,
        )
        self.__jpg_enable.pack(side=tk.LEFT)
        # 色彩格式
        color_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_color_label = ctk.CTkLabel(color_frame, text=self.i18n.t("color_format"))
        self.m_color_label.pack(side=tk.LEFT)
        self.m_color_select = ttk.Combobox(color_frame, width=20, state="readonly")
        self.m_color_select["value"] = (
            "CF_TRUE_COLOR",
            "CF_TRUE_COLOR_ALPHA",
            "CF_TRUE_COLOR_CHROMA",
            "CF_INDEXED_1_BIT",
            "CF_INDEXED_2_BIT",
            "CF_INDEXED_4_BIT",
            "CF_INDEXED_8_BIT",
            "CF_ALPHA_1_BIT",
            "CF_ALPHA_2_BIT",
            "CF_ALPHA_4_BIT",
            "CF_ALPHA_8_BIT",
            "CF_RAW",
            "CF_RAW_ALPHA",
            "CF_RAW_CHROMA",
        )
        self.m_color_select["state"] = tk.DISABLED
        # 设置默认值，即默认下拉框中的内容
        self.m_color_select.current(1)
        self.m_color_select.pack(side=tk.RIGHT, padx=border_padx)
        color_frame.pack(side=tk.LEFT, pady=5)

        # 输出格式
        output_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_output_label = ctk.CTkLabel(output_frame, text=self.i18n.t("output_format"))
        self.m_output_label.pack(side=tk.LEFT)
        self.m_output_select = ttk.Combobox(output_frame, width=15, state="readonly")
        self.m_output_select["value"] = (
            "C_array",
            "Binary_332",
            "Binary_565",
            "Binary_565_SWAP",
            "Binary_888",
        )
        self.m_output_select["state"] = tk.DISABLED
        # 设置默认值，即默认下拉框中的内容
        self.m_output_select.current(2)
        self.m_output_select.pack(side=tk.LEFT, padx=border_padx)
        output_frame.pack(side=tk.LEFT, pady=5)

        # 解析度
        out_ratio_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_output_width = ctk.CTkLabel(out_ratio_frame, text=self.i18n.t("resolution"))
        self.m_output_width.pack(side=tk.LEFT)
        # 创建宽输入框 (CTkEntry width 為像素，原 width=6 字符 ≈ 60px)
        self.m_width_val = tk.StringVar()
        self.m_width_entry = EntryWithPlaceholder(
            out_ratio_frame,
            width=60,
            placeholder=self.i18n.t("width_placeholder"),
            placeholder_color="grey",
            textvariable=self.m_width_val,
        )
        self.m_width_entry.pack(side=tk.LEFT, padx=5)
        # 创建高输入框
        self.m_height_val = tk.StringVar()
        self.m_height_entry = EntryWithPlaceholder(
            out_ratio_frame,
            width=60,
            placeholder=self.i18n.t("height_placeholder"),
            placeholder_color="grey",
            textvariable=self.m_height_val,
        )
        self.m_height_entry.pack(side=tk.LEFT, padx=5)
        out_ratio_frame.pack(side=tk.LEFT, pady=5)

        # 设置默认值输出分辨率
        self.default_ratio()

    def default_ratio(self) -> None:
        """
        设置默认值输出分辨率
        :return: None
        """
        self.m_width_val.set("240")
        self.m_height_val.set("240")
        # self.m_boot_path_entry.refresh()

    def init_image_path(self, father: tk.Misc) -> None:
        """
        初始化设置条
        :param father: 父容器
        :return: None
        """
        border_padx = 15  # 两个控件的间距
        image_path_frame = ctk.CTkFrame(father, fg_color="transparent")
        # 创建路径输入框 (width=80 chars ≈ 600 px)
        self.m_image_path_val = tk.StringVar()
        self.m_image_path_entry = EntryWithPlaceholder(
            image_path_frame,
            width=600,
            placeholder=self.i18n.t("select_images"),
            placeholder_color="grey",
            textvariable=self.m_image_path_val,
        )
        self.m_image_path_entry.pack(side=tk.LEFT, padx=border_padx)
        # 选择文件按钮
        self.m_image_path_botton = ctk.CTkButton(
            image_path_frame,
            text=self.i18n.t("select_button"),
            command=self.choose_image_files,
            width=60,
            height=28,
        )
        self.m_image_path_botton.pack(side=tk.LEFT, fill=tk.X, padx=5)

        # 转化按钮
        self.m_trans_botton = ctk.CTkButton(
            image_path_frame,
            text=self.i18n.t("start_convert"),
            command=self.trans_images,
            width=80,
            height=28,
        )
        self.m_trans_botton.pack(side=tk.LEFT, fill=tk.X, padx=5)

        # 提示文字
        self.m_tip_label = ctk.CTkLabel(
            image_path_frame,
            text=self.i18n.t("click_to_convert"),
            text_color="green",
        )
        self.m_tip_label.pack(side=tk.LEFT, padx=border_padx)

        image_path_frame.pack(side=tk.LEFT, pady=5)

    def enable_jpg(self) -> None:
        if self.__jpg_enable_val.get() == 1:
            self.m_color_select["state"] = tk.DISABLED
            self.m_output_select["state"] = tk.DISABLED
        else:
            self.m_color_select["state"] = tk.NORMAL
            self.m_output_select["state"] = tk.NORMAL

    def choose_image_files(self) -> None:
        """
        点击"user_data"文件触发的函数
        :return:
        """
        # 打开文件对话框 获取文件路径
        # defaultextension 为选取保存类型中的拓展名为文件名
        # filetypes为文件拓展名
        filepath = tk.filedialog.askopenfilenames(
            title=self.i18n.t("select_images_title"),
            defaultextension=".espace",
            filetypes=[
                (self.i18n.t("image_files"), ".jpg .JPG .png .PNG"),
                (self.i18n.t("all_files"), ".* .*"),
            ],
        )
        if filepath is None or filepath == "":
            return None
        else:
            filepaths = ";".join(filepath)
            self.m_image_path_val.set(filepaths)
            # self.m_image_path_entry.delete(0, tk.END)  # 清空文本框
            # self.m_image_path_entry.insert(tk.END, filepath)

    def trans_images(self) -> int | bool | None:
        """
        转化图片
        """
        self.m_tip_label.configure(text=self.i18n.t("converting"))
        self.__father.update()
        images_path = self.m_image_path_val.get().strip()
        if images_path is None:
            self.log_message("Error: could not load image", "error")
            return 0

        # 设计的图片地址栏如果是选择的话只有一张图片
        # 如果是批量拉取到界面中，则地址栏为多个图片地址的组合字符串(分号分隔)，如：path1;path2;path3
        for img_path in images_path.strip().split(";"):
            # 循环处理每张照片
            img_path = img_path.strip()
            if img_path == "":
                self.log_message("Error: Path is empty", "error")
                return False

            input_path = None  # 真正参与转化的图片
            # 图片输出后的文件后缀
            save_suffix = os.path.splitext(img_path)[-1]  # '.png' '.jpg'
            try:
                width = int(self.m_width_val.get().strip())
                height = int(self.m_height_val.get().strip())
                src_im: Image.Image = Image.open(img_path)  # : Image.Image
                if self.__jpg_enable_val.get() == 1:
                    # 如果转化成jpg的话
                    # 由于PNG是RGBA四个通道 而jpg只有RGB三个通道
                    src_im = src_im.convert("RGB")
                    save_suffix = ".jpg"  # '.png' '.jpg'
                new_im = None
                if src_im.height == height and src_im.width == width:
                    new_im = src_im
                    input_path = img_path
                else:
                    new_filename = os.path.basename(img_path).split(".")[0] + save_suffix
                    self.log_message(f"Resizing image to: {new_filename}")
                    input_path = os.path.join(ROOT_PATH, CACHE_PATH, new_filename)
                    new_im = src_im.resize((width, height))
                    new_im.save(input_path, quality=95)  # , format='JPEG', quality=95
            except Exception as err:
                self.log_message(f"Error: {err}", "error")

            self.log_message(f"Converting image: {os.path.basename(img_path)} ...")
            output_file = None
            if self.__jpg_enable_val.get() == 1:
                output_file = os.path.join(ROOT_PATH, os.path.basename(input_path))
                shutil.copy(input_path, ROOT_PATH)
            else:
                color_format = color_dict[self.m_color_select.get()]
                output_format = output_dict[self.m_output_select.get()]
                self.log_message(f"Color format: {color_format}")
                self.log_message(f"Output format: {output_format}")
                if output_format == -1:
                    out_obj = Converter(input_path, True, color_format, cf_palette_bgr_en=1)
                    output_file = out_obj.get_c_code_file(outpath=ROOT_PATH)
                else:
                    self.log_message(f"Input path: {input_path}")
                    out_obj = Converter(input_path, True, output_format, cf_palette_bgr_en=1)
                    output_file = out_obj.get_bin_file(outpath=ROOT_PATH)

            if output_file:
                abs_output_path = os.path.abspath(output_file)
                self.log_message(f"Output file path: {abs_output_path}")

            self.m_tip_label.configure(text=self.i18n.t("convert_complete"))
            self.log_message(self.i18n.t("convert_complete"))

    def log_message(self, message: str, tag: str = "normal") -> None:
        """
        Append log message to the info text widget
        :param message: message to log
        :param tag: text tag for formatting
        :return: None
        """
        self.m_project_info.config(state=tk.NORMAL)
        self.m_project_info.insert(tk.END, message + "\n", tag)
        self.m_project_info.see(tk.END)
        self.m_project_info.config(state=tk.DISABLED)
        self.__father.update()

    def init_info(self, father: tk.Misc) -> None:
        """初始化信息打印框（保留 tk.Text + 字型 tag，CTkTextbox 不支援 font tag）。"""
        info = self.i18n.t("image_converter_info")

        scrollbar = ctk.CTkScrollbar(father)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y, padx=(0, 5), pady=5)

        self.m_project_info = tk.Text(
            father,
            width=100,
            yscrollcommand=scrollbar.set,
            wrap=tk.WORD,
            borderwidth=0,
            highlightthickness=0,
            bg="#1f1f1f",
            fg="#dcdcdc",
            insertbackground="#dcdcdc",
        )
        self.m_project_info.tag_configure(
            "bold_italics",
            font=("Arial", 12, "bold", "italic"),
        )
        self.m_project_info.tag_configure("big", font=("Verdana", 13))
        self.m_project_info.tag_configure(
            "color",
            foreground="#476042",
            font=("Tempus Sans ITC", 12, "bold"),
        )
        self.m_project_info.tag_configure("normal", font=("Courier", 10))
        self.m_project_info.tag_configure(
            "error",
            foreground="red",
            font=("Courier", 10),
        )

        self.m_project_info.pack(padx=5, pady=5, fill=tk.BOTH, expand=True)
        scrollbar.configure(command=self.m_project_info.yview)

        self.m_project_info.config(state=tk.NORMAL)
        self.m_project_info.insert(tk.END, info, "big")
        self.m_project_info.config(state=tk.DISABLED)
