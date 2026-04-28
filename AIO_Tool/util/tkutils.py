import tkinter as tk
import tkinter.font as tk_font
from pathlib import Path
from tkinter import messagebox
from typing import Literal

from PIL import Image, ImageTk  # pip3 install pillow


def show_info(message: str = "") -> None:
    messagebox.showinfo("提示框", message)


def show_confirm(message: str = "") -> bool:
    """
    True  : yes
    False : no
    """
    return messagebox.askyesno("确认框", message)


def center_window(win: tk.Wm, width: int | None = None, height: int | None = None) -> None:
    """将窗口屏幕居中"""
    screenwidth = win.winfo_screenwidth()
    screenheight = win.winfo_screenheight()
    if width is None:
        width, height = get_window_size(win)[:2]
    size = "%dx%d+%d+%d" % (width, height, (screenwidth - width) / 2, (screenheight - height) / 3)
    win.geometry(size)


def get_window_size(win: tk.Misc, update: bool = True) -> tuple[int, int, int, int]:
    """获得窗体的尺寸"""
    if update:
        win.update()
    return win.winfo_width(), win.winfo_height(), win.winfo_x(), win.winfo_y()


def tkimg_resized(
    img: Image.Image,
    w_box: int,
    h_box: int,
    keep_ratio: bool = True,
) -> ImageTk.PhotoImage:
    """对图片进行按比例缩放处理"""
    w, h = img.size

    if keep_ratio:
        if w > h:
            width = w_box
            height = int(h_box * (1.0 * h / w))

        if h >= w:
            height = h_box
            width = int(w_box * (1.0 * w / h))
    else:
        width = w_box
        height = h_box

    img1 = img.resize((width, height), Image.Resampling.LANCZOS)
    tkimg = ImageTk.PhotoImage(img1)
    return tkimg


def image_label(
    frame: tk.Misc,
    img: str | Path | Image.Image,
    width: int,
    height: int,
    keep_ratio: bool = True,
) -> tk.Label:
    """输入图片信息，及尺寸，返回界面组件"""
    if isinstance(img, (str, Path)):
        _img = Image.open(img)
    else:
        _img = img
    lbl_image = tk.Label(frame, width=width, height=height)

    tk_img = tkimg_resized(_img, width, height, keep_ratio)
    lbl_image.image = tk_img  # type: ignore[attr-defined]
    lbl_image.config(image=tk_img)
    return lbl_image


def _font(
    fname: str = "微软雅黑",
    size: int = 12,
    bold: Literal["normal", "bold"] = tk_font.NORMAL,
) -> tk_font.Font:
    """设置字体"""
    ft = tk_font.Font(family=fname, size=size, weight=bold)
    return ft


def _ft(size: int = 12, bold: bool = False) -> tk_font.Font:
    """极简字体设置函数"""
    if bold:
        return _font(size=size, bold=tk_font.BOLD)
    else:
        return _font(size=size, bold=tk_font.NORMAL)


def h_seperator(parent: tk.Misc, height: int = 2) -> None:  # height 单位为像素值
    """水平分割线, 水平填充"""
    tk.Frame(parent, height=height, bg="whitesmoke").pack(fill=tk.X)


def v_seperator(
    parent: tk.Misc, width: int, bg: str = "whitesmoke"
) -> tk.Frame:  # width 单位为像素值
    """垂直分割线 , fill=tk.Y, 但如何定位不确定，直接返回对象，由容器决定"""
    frame = tk.Frame(parent, width=width, bg=bg)
    return frame
