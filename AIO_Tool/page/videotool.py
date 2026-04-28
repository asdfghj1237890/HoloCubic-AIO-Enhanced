################################################################################
#
# Author: ClimbSnail(HQ)
# original source is here.
#   https://github.com/ClimbSnail/HoloCubic_AIO_Tool
#
#
################################################################################

import os
import subprocess
import threading
import tkinter as tk
from tkinter import filedialog, scrolledtext, ttk

import customtkinter as ctk

from util.common import CACHE_PATH, ROOT_PATH
from util.i18n import get_i18n


class VideoTool:
    """
    视频转化页类
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

        output_param_frame = ctk.CTkFrame(father, fg_color="transparent")
        # 路径
        path_frame = ctk.CTkFrame(output_param_frame, fg_color="transparent")
        self.init_path(path_frame)
        path_frame.pack(side=tk.LEFT, pady=5)

        # 输出设定区块（CTk 沒有 LabelFrame，改用 CTkFrame + 標題 CTkLabel）
        self.connor_param_frame = ctk.CTkFrame(output_param_frame)
        connor_title = ctk.CTkLabel(
            self.connor_param_frame,
            text=self.i18n.t("output_settings"),
            font=ctk.CTkFont(weight="bold"),
        )
        connor_title.pack(anchor=tk.W, padx=10, pady=(8, 4))
        self.connor_param_frame.pack(side=tk.LEFT, pady=5)
        self.init_options(self.connor_param_frame)

        output_param_frame.pack(side=tk.TOP, pady=5)

        # Conversion log（保留 ScrolledText：black/white 配色與 font 需要 tk.Text）
        log_frame = ctk.CTkFrame(father)
        log_title = ctk.CTkLabel(
            log_frame,
            text="Conversion Log",
            font=ctk.CTkFont(weight="bold"),
        )
        log_title.pack(anchor=tk.W, padx=10, pady=(8, 4))
        self.log_text = scrolledtext.ScrolledText(
            log_frame,
            width=100,
            height=15,
            wrap=tk.WORD,
            bg="black",
            fg="white",
            font=("Consolas", 9),
            borderwidth=0,
            highlightthickness=0,
        )
        self.log_text.pack(padx=5, pady=5, fill=tk.BOTH, expand=True)
        log_frame.pack(side=tk.TOP, pady=5, fill=tk.BOTH, expand=True)

    def init_path(self, father: tk.Misc) -> None:
        """
        初始化输入文件路径 输出文件路径
        :param father: 父容器
        :return: None
        """
        border_padx = 10  # 两个控件的间距

        # 輸入原檔
        src_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_src_path_entry = ctk.CTkEntry(src_frame, width=600)
        self.m_src_path_entry.pack(side=tk.LEFT, padx=border_padx)
        self.src_path_botton = ctk.CTkButton(
            src_frame,
            text=self.i18n.t("select_video"),
            command=self.choose_src_file,
            width=80,
            height=28,
        )
        self.src_path_botton.pack(side=tk.RIGHT, fill=tk.X, padx=5)
        src_frame.pack(side=tk.TOP, pady=5)

        # 輸出路徑
        dst_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_dst_path_entry = ctk.CTkEntry(dst_frame, width=600)
        self.m_dst_path_entry.pack(side=tk.LEFT, padx=border_padx)
        defualt_outpath = os.path.join(os.getcwd(), ROOT_PATH)
        self.m_dst_path_entry.delete(0, tk.END)
        self.m_dst_path_entry.insert(tk.END, defualt_outpath)
        self.dst_path_botton = ctk.CTkButton(
            dst_frame,
            text=self.i18n.t("output_path"),
            command=self.choose_dst_path,
            width=80,
            height=28,
        )
        self.dst_path_botton.pack(side=tk.RIGHT, fill=tk.X, padx=5)
        dst_frame.pack(side=tk.TOP, pady=5)

        # 轉換按鈕
        button_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.trans_botton = ctk.CTkButton(
            button_frame,
            text=self.i18n.t("start_conversion"),
            command=self.trans_format,
            width=80,
            height=28,
        )
        self.trans_botton.pack(side=tk.TOP, fill=tk.X, padx=5)
        button_frame.pack(side=tk.TOP, pady=5)

    def choose_src_file(self) -> None:
        """
        点击"打开"菜单项触发的函数
        :return:
        """
        # 打开文件对话框 获取文件路径
        # defaultextension 为选取保存类型中的拓展名为文件名
        # filetypes为文件拓展名
        filepath = filedialog.askopenfilename(
            title=self.i18n.t("select_video_title"),
            defaultextension=".espace",
            # filetypes=[('mp4', '.mp4 .MP4'), ('avi', '.avi .AVI'),
            #     ('mov', '.mov .MOV'), ('gif', '.gif .GIF'), ('所有文件', '.* .*')]
            # )
            filetypes=[
                (self.i18n.t("common_formats"), ".mp4 .MP4 .avi .AVI .mov .MOV .gif .GIF"),
                (self.i18n.t("all_files"), ".* .*"),
            ],
        )
        if filepath is None or filepath == "":
            return None
        else:
            # self.m_src_path_entry["state"] = tk.NORMAL
            self.m_src_path_entry.delete(0, tk.END)  # 清空文本框
            self.m_src_path_entry.insert(tk.END, filepath)
            # self.m_src_path_entry["state"] = tk.DISABLED

    def choose_dst_path(self) -> None:

        # 打开文件对话框 获取文件路径
        # defaultextension 为选取保存类型中的拓展名为文件名
        # filetypes为文件拓展名
        filepath = filedialog.askdirectory()
        if filepath is None or filepath == "":
            return None
        else:
            self.m_dst_path_entry.delete(0, tk.END)  # 清空文本框
            self.m_dst_path_entry.insert(tk.END, filepath)

    def log(self, message: str) -> None:
        """Add message to log display"""
        self.log_text.insert(tk.END, message + "\n")
        self.log_text.see(tk.END)
        self.log_text.update()

    def clear_log(self) -> None:
        """Clear log display"""
        self.log_text.delete(1.0, tk.END)

    def run_ffmpeg_command(self, cmd: str, description: str) -> bool:
        """Run ffmpeg command and capture output in real-time"""
        self.log(f"\n{'=' * 60}")
        self.log(f"[{description}]")
        try:
            self.log(f"Command: {cmd}")
        except UnicodeEncodeError:
            self.log("Command: [Command contains non-ASCII characters]")
        self.log(f"{'=' * 60}\n")

        try:
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                shell=True,
                universal_newlines=False,
                bufsize=1,
            )

            for line in iter(process.stdout.readline, b""):
                try:
                    # Try UTF-8 first, then fallback to system encoding with error handling
                    try:
                        decoded_line = line.decode("utf-8").strip()
                    except UnicodeDecodeError:
                        try:
                            decoded_line = line.decode("gbk").strip()
                        except UnicodeDecodeError:
                            decoded_line = line.decode("utf-8", errors="ignore").strip()

                    if decoded_line:
                        self.log(decoded_line)
                except Exception:
                    # Skip lines that cannot be decoded
                    pass

            process.wait()

            if process.returncode == 0:
                self.log(f"\n✓ {description} completed successfully!\n")
                return True
            else:
                self.log(f"\n✗ {description} failed with return code {process.returncode}\n")
                return False
        except Exception as e:
            self.log(f"\n✗ Error: {str(e)}\n")
            return False

    def trans_format(self) -> None:
        """
        格式转化
        """
        # Run conversion in a separate thread to avoid blocking UI
        thread = threading.Thread(target=self._trans_format_thread)
        thread.daemon = True
        thread.start()

    def _trans_format_thread(self) -> None:
        """
        Actual conversion logic running in thread
        """
        cur_dir = os.getcwd()  # 当前目录
        self.trans_botton.configure(text=self.i18n.t("converting_video"), state=tk.DISABLED)
        self.clear_log()

        param = self.get_output_param()

        # Validate input
        if not param["src_path"]:
            self.log("✗ Error: Please select a source video file!")
            self.trans_botton.configure(text=self.i18n.t("start_conversion"), state=tk.NORMAL)
            return

        if not os.path.exists(param["src_path"]):
            self.log(f"✗ Error: Source file does not exist: {param['src_path']}")
            self.trans_botton.configure(text=self.i18n.t("start_conversion"), state=tk.NORMAL)
            return

        self.log("Starting video conversion...")
        self.log(f"Source: {os.path.basename(param['src_path'])}")
        self.log(f"Resolution: {param['width']}x{param['height']}")
        self.log(f"FPS: {param['fps']}")
        self.log(f"Quality: {param['quality']}")
        self.log(f"Format: {param['format']}")

        cmd_resize = 'ffmpeg -y -i "%s" -vf scale=%s:%s "%s"'  # 缩放转化
        # ffmpeg 命令長字串若硬斷會降低可讀性，明確標註 noqa
        cmd_to_rgb = 'ffmpeg -y -i "%s" -vf "fps=%s,scale=-1:%s:flags=lanczos,crop=%s:in_h:(in_w-%s)/2:0" -c:v rawvideo -pix_fmt rgb565be -q:v %s "%s"'  # noqa: E501
        cmd_to_mjpeg = (
            'ffmpeg -y -i "%s" -vf "fps=%s,scale=-1:%s:flags=lanczos,crop=%s:in_h:(in_w-%s)/2:0" -q:v %s "%s"'  # noqa: E501
        )

        name_suffix = os.path.basename(param["src_path"]).split(".")
        suffix = name_suffix[-1]  # 后缀名
        video_cache_name = name_suffix[0] + "_" + param["width"] + "x" + param["height"] + "_cache." + suffix
        video_cache = os.path.join(cur_dir, ROOT_PATH, CACHE_PATH, video_cache_name)

        if param["format"] == "rgb565be":
            out_format_tail = ".rgb"
            trans_cmd = cmd_to_rgb
        elif param["format"] == "MJPEG":
            out_format_tail = ".mjpeg"
            trans_cmd = cmd_to_mjpeg
        else:
            out_format_tail = ".mjpeg"
            trans_cmd = cmd_to_mjpeg

        final_out = os.path.join(
            param["dst_path"],
            name_suffix[0] + "_" + param["width"] + "x" + param["height"] + out_format_tail,
        )

        # Clean up previous files
        try:
            if os.path.exists(video_cache):
                os.remove(video_cache)
            if os.path.exists(final_out):
                os.remove(final_out)
        except Exception as err:
            self.log(f"Warning: Failed to remove old files: {err}")

        # Step 1: Resize
        middle_cmd = cmd_resize % (param["src_path"], param["width"], param["height"], video_cache)
        if not self.run_ffmpeg_command(middle_cmd, "Step 1: Resizing video"):
            self.log("\n✗ Conversion failed at resize step!")
            self.trans_botton.configure(text=self.i18n.t("start_conversion"), state=tk.NORMAL)
            return

        # Step 2: Convert format
        out_cmd = trans_cmd % (
            video_cache,
            param["fps"],
            param["height"],
            param["width"],
            param["width"],
            param["quality"],
            final_out,
        )
        if not self.run_ffmpeg_command(out_cmd, f"Step 2: Converting to {param['format']}"):
            self.log("\n✗ Conversion failed at format conversion step!")
            self.trans_botton.configure(text=self.i18n.t("start_conversion"), state=tk.NORMAL)
            return

        # Clean up cache file
        try:
            if os.path.exists(video_cache):
                os.remove(video_cache)
                self.log(f"Cleaned up cache file: {os.path.basename(video_cache)}")
        except Exception as err:
            self.log(f"Warning: Failed to remove cache file: {err}")

        self.log("\n" + "=" * 60)
        self.log("✓ CONVERSION COMPLETED SUCCESSFULLY!")
        self.log(f"Output file: {final_out}")
        self.log("=" * 60)

        self.trans_botton.configure(text=self.i18n.t("start_conversion"), state=tk.NORMAL)

    def init_options(self, father: tk.Misc) -> None:
        """
        初始化模型菜单子项
        :param father: 父容器
        :return: None
        """
        border_padx = 10  # 兩個控件的間距

        # 單選按鈕
        self.m_radio_val = tk.IntVar()
        radio_frame = ctk.CTkFrame(father)
        ctk.CTkRadioButton(
            radio_frame,
            variable=self.m_radio_val,
            value=0,
            text=self.i18n.t("default_option"),
            command=self.radio_select,
        ).pack(side=tk.LEFT, padx=10, pady=5)
        ctk.CTkRadioButton(
            radio_frame,
            variable=self.m_radio_val,
            value=1,
            text=self.i18n.t("custom_option"),
            command=self.radio_select,
        ).pack(side=tk.RIGHT, padx=10, pady=5)
        self.m_radio_val.set(0)
        radio_frame.pack(side=tk.TOP, padx=5, fill="x")

        # 解析度（長寬）
        ratio_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_ratio_label = ctk.CTkLabel(ratio_frame, text=self.i18n.t("resolution"))
        self.m_ratio_label.pack(side=tk.LEFT, padx=border_padx)
        self.m_width_entry = ctk.CTkEntry(ratio_frame, width=60)
        self.m_width_entry.insert(tk.END, "240")
        self.m_width_entry.pack(side=tk.LEFT, padx=border_padx)
        self.m_height_entry = ctk.CTkEntry(ratio_frame, width=60)
        self.m_height_entry.insert(tk.END, "240")
        self.m_height_entry.pack(side=tk.LEFT, padx=border_padx)
        ratio_frame.pack(side=tk.TOP, pady=5)

        # 幀率（fps）
        fps_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_fps_label = ctk.CTkLabel(fps_frame, text=self.i18n.t("fps"))
        self.m_fps_label.pack(side=tk.LEFT, padx=border_padx)
        self.m_fps_entry = ctk.CTkEntry(fps_frame, width=50)
        self.m_fps_entry.insert(tk.END, "20")
        self.m_fps_entry.pack(side=tk.LEFT, padx=border_padx)
        # 品質
        self.m_quality_label = ctk.CTkLabel(fps_frame, text=self.i18n.t("quality"))
        self.m_quality_label.pack(side=tk.LEFT, padx=border_padx)
        self.m_quality_select = ttk.Combobox(fps_frame, width=5, state="readonly")
        self.m_quality_select["value"] = ("1", "2", "3", "4", "5", "6", "7", "8", "9")
        self.m_quality_select.current(4)
        self.m_quality_select.pack(side=tk.LEFT, padx=border_padx)
        fps_frame.pack(side=tk.TOP, pady=5)

        # 格式
        format_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_format_label = ctk.CTkLabel(format_frame, text=self.i18n.t("format"))
        self.m_format_label.pack(side=tk.LEFT, padx=border_padx)
        self.m_format_select = ttk.Combobox(format_frame, width=10, state="readonly")
        self.m_format_select["value"] = ("MJPEG", "rgb565be")
        self.m_format_select.current(0)
        self.m_format_select.pack(side=tk.RIGHT, padx=border_padx)
        format_frame.pack(side=tk.TOP, pady=5)

        self.radio_select()

    def radio_select(self) -> None:
        """根據單選按鈕選擇啟用/停用各輸入欄位。"""
        new_state = tk.DISABLED if self.m_radio_val.get() == 0 else tk.NORMAL
        self.m_width_entry.configure(state=new_state)
        self.m_height_entry.configure(state=new_state)
        self.m_fps_entry.configure(state=new_state)
        # ttk.Combobox 仍使用 ["state"] 索引存取
        self.m_quality_select["state"] = "readonly" if new_state == tk.NORMAL else tk.DISABLED
        self.m_format_select["state"] = "readonly" if new_state == tk.NORMAL else tk.DISABLED

    def get_output_param(self) -> dict[str, str]:
        """
        得到输出参数
        """
        return {
            "src_path": self.m_src_path_entry.get().strip(),
            "dst_path": self.m_dst_path_entry.get().strip(),
            "width": self.m_width_entry.get().strip(),
            "height": self.m_height_entry.get().strip(),
            "fps": self.m_fps_entry.get().strip(),
            "quality": self.m_quality_select.get().strip(),
            "format": self.m_format_select.get().strip(),
        }
