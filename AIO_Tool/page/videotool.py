# -*- coding: utf-8 -*-
################################################################################
#
# Author: ClimbSnail(HQ)
# original source is here.
#   https://github.com/ClimbSnail/HoloCubic_AIO_Tool
# 
#
################################################################################

from util.common import CACHE_PATH, ROOT_PATH
from util.i18n import get_i18n

import os
import tkinter as tk
import util.tkutils as tku
from tkinter import ttk
from tkinter import filedialog
import subprocess
import threading
from tkinter import scrolledtext


class VideoTool(object):
    """
    视频转化页类
    """

    def __init__(self, father, engine, lock=None):
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

        output_param_frame = tk.Frame(father, bg=father["bg"])
        # 路径
        path_frame = tk.Frame(output_param_frame, bg=father["bg"])
        self.init_path(path_frame)
        path_frame.pack(side=tk.LEFT, pady=5)

        # 连接器相关控件
        # 使用LabelFrame控件 框出连接相关的控件
        self.connor_param_frame = tk.LabelFrame(output_param_frame, text=self.i18n.t("output_settings"),
                                                #  labelanchor="nw",
                                                bg="white")
        # self.connor_param_frame.place(anchor="ne", relx=100.0, rely=100.0)
        # self.connor_param_frame.grid(row=1, column=1)
        # self.connor_param_frame.place(x=self.__father.winfo_width()+5, y=0)
        # self.connor_param_frame.update()
        self.connor_param_frame.pack(side=tk.LEFT, pady=5)
        self.init_options(self.connor_param_frame)  # 初始化参数

        output_param_frame.pack(side=tk.TOP, pady=5)

        # Add log display area
        log_frame = tk.LabelFrame(father, text="Conversion Log", bg="white")
        self.log_text = scrolledtext.ScrolledText(log_frame, width=100, height=15, 
                                                   wrap=tk.WORD, bg="black", fg="white",
                                                   font=("Consolas", 9))
        self.log_text.pack(padx=5, pady=5, fill=tk.BOTH, expand=True)
        log_frame.pack(side=tk.TOP, pady=5, fill=tk.BOTH, expand=True)

    def init_path(self, father):
        """
        初始化输入文件路径 输出文件路径
        :param father: 父容器
        :return: None
        """
        border_padx = 10  # 两个控件的间距

        # 输入原文件
        src_frame = tk.Frame(father, bg=father["bg"])
        # 创建输入框
        self.m_src_path_entry = tk.Entry(src_frame, width=80, highlightcolor="LightGrey")
        # self.m_src_path_entry["state"] = tk.DISABLED
        self.m_src_path_entry.pack(side=tk.LEFT, padx=border_padx)
        # 原视频输入按钮
        self.src_path_botton = tk.Frame(father, bg=father["bg"])
        self.src_path_botton = tk.Button(src_frame, text=self.i18n.t("select_video"), fg='black',
                                         command=self.choose_src_file, width=8, height=1)

        self.src_path_botton.pack(side=tk.RIGHT, fill=tk.X, padx=5)

        src_frame.pack(side=tk.TOP, pady=5)

        # 输入输出路径
        dst_frame = tk.Frame(father, bg=father["bg"])
        # 创建输入框
        self.m_dst_path_entry = tk.Entry(dst_frame, width=80, highlightcolor="LightGrey")
        # self.m_dst_path_entry["state"] = tk.DISABLED
        self.m_dst_path_entry.pack(side=tk.LEFT, padx=border_padx)
        defualt_outpath = os.path.join(os.getcwd(), ROOT_PATH)
        self.m_dst_path_entry.delete(0, tk.END)  # 清空文本框
        self.m_dst_path_entry.insert(tk.END, defualt_outpath)
        # 原视频输入按钮
        self.dst_path_botton = tk.Frame(father, bg=father["bg"])
        self.dst_path_botton = tk.Button(dst_frame, text=self.i18n.t("output_path"), fg='black',
                                         command=self.choose_dst_path, width=8, height=1)

        self.dst_path_botton.pack(side=tk.RIGHT, fill=tk.X, padx=5)

        dst_frame.pack(side=tk.TOP, pady=5)

        # 转换按钮
        button_frame = tk.Frame(father, bg=father["bg"])
        # 转换按钮
        self.trans_botton = tk.Frame(father, bg=father["bg"])
        self.trans_botton = tk.Button(button_frame, text=self.i18n.t("start_conversion"), fg='black',
                                      command=self.trans_format, width=8, height=1)

        self.trans_botton.pack(side=tk.TOP, fill=tk.X, padx=5)

        button_frame.pack(side=tk.TOP, pady=5)

    def choose_src_file(self):
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
            filetypes=[(self.i18n.t("common_formats"), '.mp4 .MP4 .avi .AVI .mov .MOV .gif .GIF'),
                (self.i18n.t("all_files"), '.* .*')]
            )
        if filepath == None or filepath == "":
            return None
        else:
            # self.m_src_path_entry["state"] = tk.NORMAL
            self.m_src_path_entry.delete(0, tk.END)  # 清空文本框
            self.m_src_path_entry.insert(tk.END, filepath)
            # self.m_src_path_entry["state"] = tk.DISABLED

    def choose_dst_path(self):

        # 打开文件对话框 获取文件路径
        # defaultextension 为选取保存类型中的拓展名为文件名
        # filetypes为文件拓展名
        filepath = filedialog.askdirectory()
        if filepath == None or filepath == "":
            return None
        else:
            self.m_dst_path_entry.delete(0, tk.END)  # 清空文本框
            self.m_dst_path_entry.insert(tk.END, filepath)

    def log(self, message):
        """Add message to log display"""
        self.log_text.insert(tk.END, message + "\n")
        self.log_text.see(tk.END)
        self.log_text.update()

    def clear_log(self):
        """Clear log display"""
        self.log_text.delete(1.0, tk.END)

    def run_ffmpeg_command(self, cmd, description):
        """Run ffmpeg command and capture output in real-time"""
        self.log(f"\n{'='*60}")
        self.log(f"[{description}]")
        try:
            self.log(f"Command: {cmd}")
        except:
            self.log(f"Command: [Command contains non-ASCII characters]")
        self.log(f"{'='*60}\n")
        
        try:
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                shell=True,
                universal_newlines=False,
                bufsize=1
            )
            
            for line in iter(process.stdout.readline, b''):
                try:
                    # Try UTF-8 first, then fallback to system encoding with error handling
                    try:
                        decoded_line = line.decode('utf-8').strip()
                    except UnicodeDecodeError:
                        try:
                            decoded_line = line.decode('gbk').strip()
                        except UnicodeDecodeError:
                            decoded_line = line.decode('utf-8', errors='ignore').strip()
                    
                    if decoded_line:
                        self.log(decoded_line)
                except Exception as e:
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

    def trans_format(self):
        """
        格式转化
        """
        # Run conversion in a separate thread to avoid blocking UI
        thread = threading.Thread(target=self._trans_format_thread)
        thread.daemon = True
        thread.start()

    def _trans_format_thread(self):
        """
        Actual conversion logic running in thread
        """
        cur_dir = os.getcwd()  # 当前目录
        self.trans_botton["text"] = self.i18n.t("converting_video")
        self.trans_botton["state"] = tk.DISABLED
        self.clear_log()
        
        param = self.get_output_param()
        
        # Validate input
        if not param["src_path"]:
            self.log("✗ Error: Please select a source video file!")
            self.trans_botton["text"] = self.i18n.t("start_conversion")
            self.trans_botton["state"] = tk.NORMAL
            return
        
        if not os.path.exists(param["src_path"]):
            self.log(f"✗ Error: Source file does not exist: {param['src_path']}")
            self.trans_botton["text"] = self.i18n.t("start_conversion")
            self.trans_botton["state"] = tk.NORMAL
            return
        
        self.log(f"Starting video conversion...")
        self.log(f"Source: {os.path.basename(param['src_path'])}")
        self.log(f"Resolution: {param['width']}x{param['height']}")
        self.log(f"FPS: {param['fps']}")
        self.log(f"Quality: {param['quality']}")
        self.log(f"Format: {param['format']}")
        
        cmd_resize = 'ffmpeg -y -i "%s" -vf scale=%s:%s "%s"'  # 缩放转化
        cmd_to_rgb = 'ffmpeg -y -i "%s" -vf "fps=%s,scale=-1:%s:flags=lanczos,crop=%s:in_h:(in_w-%s)/2:0" -c:v rawvideo -pix_fmt rgb565be -q:v %s "%s"'
        cmd_to_mjpeg = 'ffmpeg -y -i "%s" -vf "fps=%s,scale=-1:%s:flags=lanczos,crop=%s:in_h:(in_w-%s)/2:0" -q:v %s "%s"'

        name_suffix = os.path.basename(param["src_path"]).split(".")
        suffix = name_suffix[-1]  # 后缀名
        video_cache_name = name_suffix[0] + "_" + param["width"] + "x" + param["height"] + "_cache." + suffix
        video_cache = os.path.join(cur_dir, ROOT_PATH, CACHE_PATH, video_cache_name)
        
        if param["format"] == 'rgb565be':
            out_format_tail = ".rgb"
            trans_cmd = cmd_to_rgb
        elif param["format"] == 'MJPEG':
            out_format_tail = ".mjpeg"
            trans_cmd = cmd_to_mjpeg
        else:
            out_format_tail = ".mjpeg"
            trans_cmd = cmd_to_mjpeg
            
        final_out = os.path.join(param["dst_path"],
                                 name_suffix[0] + "_" + param["width"] + "x" + param["height"] + out_format_tail)

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
            self.trans_botton["text"] = self.i18n.t("start_conversion")
            self.trans_botton["state"] = tk.NORMAL
            return

        # Step 2: Convert format
        out_cmd = trans_cmd % (video_cache, param["fps"], param["height"],
                               param["width"], param["width"], param["quality"], final_out)
        if not self.run_ffmpeg_command(out_cmd, f"Step 2: Converting to {param['format']}"):
            self.log("\n✗ Conversion failed at format conversion step!")
            self.trans_botton["text"] = self.i18n.t("start_conversion")
            self.trans_botton["state"] = tk.NORMAL
            return

        # Clean up cache file
        try:
            if os.path.exists(video_cache):
                os.remove(video_cache)
                self.log(f"Cleaned up cache file: {os.path.basename(video_cache)}")
        except Exception as err:
            self.log(f"Warning: Failed to remove cache file: {err}")

        self.log("\n" + "="*60)
        self.log("✓ CONVERSION COMPLETED SUCCESSFULLY!")
        self.log(f"Output file: {final_out}")
        self.log("="*60)
        
        self.trans_botton["text"] = self.i18n.t("start_conversion")
        self.trans_botton["state"] = tk.NORMAL

    def init_options(self, father):
        """
        初始化模型菜单子项
        :param father: 父容器
        :return: None
        """
        border_padx = 10  # 两个控件的间距

        # 单选按钮
        self.m_radio_val = tk.IntVar()  # IntVar
        radio_frame = tk.Frame(father, bg="DimGray")
        tk.Radiobutton(radio_frame, variable=self.m_radio_val, value=0,
                       text=self.i18n.t("default_option"), width=10, bg="DimGray",
                       command=self.radio_select).pack(side=tk.LEFT, pady=5)

        tk.Radiobutton(radio_frame, variable=self.m_radio_val, value=1,
                       text=self.i18n.t("custom_option"), width=10, bg="DimGray",
                       command=self.radio_select).pack(side=tk.RIGHT)
        self.m_radio_val.set(0)
        radio_frame.pack(side=tk.TOP, padx=5, fill="x")

        # 分辨率(长宽)
        ratio_frame = tk.Frame(father, bg=father["bg"])
        self.m_ratio_label = tk.Label(ratio_frame, text=self.i18n.t("resolution"),
                                      # font=self.my_ft1,
                                      bg=father['bg'])
        self.m_ratio_label.pack(side=tk.LEFT, padx=border_padx)
        self.m_width_entry = tk.Entry(ratio_frame, width=6, highlightcolor="LightGrey")
        self.m_width_entry.insert(tk.END, '240')
        self.m_width_entry.pack(side=tk.LEFT, padx=border_padx)
        self.m_height_entry = tk.Entry(ratio_frame, width=6, highlightcolor="LightGrey")
        self.m_height_entry.insert(tk.END, '240')
        self.m_height_entry.pack(side=tk.LEFT, padx=border_padx)
        ratio_frame.pack(side=tk.TOP, pady=5)

        # 帧率（fps）
        fps_frame = tk.Frame(father, bg=father["bg"])
        self.m_fps_label = tk.Label(fps_frame, text=self.i18n.t("fps"),
                                    # font=self.my_ft1,
                                    bg=father['bg'])
        self.m_fps_label.pack(side=tk.LEFT, padx=border_padx)
        # 创建输入框
        # self.m_fps_entry = tk.Entry(father, font=self.my_ft1, width=5, highlightcolor="LightGrey")
        self.m_fps_entry = tk.Entry(fps_frame, width=5, highlightcolor="LightGrey")
        self.m_fps_entry.insert(tk.END, '20')
        # self.m_pre_val_text = "1500"    # 保存修改前m_val_text输入框中的内容，供错误输入时使用
        # self.m_fps_entry.bind("<Return>", self.change_val)  # 绑定enter键的触发
        self.m_fps_entry.pack(side=tk.LEFT, padx=border_padx)
        # 质量
        self.m_quality_label = tk.Label(fps_frame, text=self.i18n.t("quality"),
                                    # font=self.my_ft1,
                                    bg=father['bg'])
        self.m_quality_label.pack(side=tk.LEFT, padx=border_padx)
        self.m_quality_select = ttk.Combobox(fps_frame, width=5, state='readonly')
        self.m_quality_select["value"] = ('1', '2', '3', '4', '5', '6', '7', '8', '9')  # , 'GIF'
        # 设置默认值，即默认下拉框中的内容
        self.m_quality_select.current(4)
        self.m_quality_select.pack(side=tk.LEFT, padx=border_padx)
        fps_frame.pack(side=tk.TOP, pady=5)

        # 格式
        format_frame = tk.Frame(father, bg=father["bg"])
        self.m_format_label = tk.Label(format_frame, text=self.i18n.t("format"),
                                       # font=self.my_ft1,
                                       bg=father['bg'])
        self.m_format_label.pack(side=tk.LEFT, padx=border_padx)
        self.m_format_select = ttk.Combobox(format_frame, width=10, state='readonly')
        self.m_format_select["value"] = ('MJPEG', 'rgb565be')  # , 'GIF'
        # 设置默认值，即默认下拉框中的内容
        self.m_format_select.current(0)
        self.m_format_select.pack(side=tk.RIGHT, padx=border_padx)
        format_frame.pack(side=tk.TOP, pady=5)

        self.radio_select()

    def radio_select(self):
        """
        选择触发的函数
        :return:
        """
        if self.m_radio_val.get() == 0:
            self.m_width_entry["state"] = tk.DISABLED
            self.m_height_entry["state"] = tk.DISABLED
            self.m_fps_entry["state"] = tk.DISABLED
            self.m_quality_select["state"] = tk.DISABLED
            self.m_format_select["state"] = tk.DISABLED
        elif self.m_radio_val.get() == 1:
            self.m_width_entry["state"] = tk.NORMAL
            self.m_height_entry["state"] = tk.NORMAL
            self.m_fps_entry["state"] = tk.NORMAL
            self.m_quality_select["state"] = tk.NORMAL
            self.m_format_select["state"] = tk.NORMAL

    def get_output_param(self):
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
            "format": self.m_format_select.get().strip()
        }
