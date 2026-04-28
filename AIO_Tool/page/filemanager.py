################################################################################
#
# Author: ClimbSnail(HQ)
# original source is here.
#   https://github.com/ClimbSnail/HoloCubic_AIO_Tool
#
#
################################################################################

import tkinter as tk
import traceback
from tkinter import ttk

import customtkinter as ctk

from util.common import get_resource_path
from util.file_info import DirList, FileGetInfo, FileRead, FileSystem
from util.i18n import get_i18n
from util.logger import get_logger
from util.massagehead import AT, MsgHead
from util.robotsocket import RobotSocketClient

logger = get_logger(__name__)


# 文件数据的结构
# FileObj {"type":"file", "name":"Filore_1", "path":"", "sub_file":[]}


class FileManager:
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
        FileManager initialization
        :param father: Parent window
        :param engine: Engine object for component communication
        :param lock: Thread lock
        :return: None
        """
        self.__engine = engine
        self.__father = father
        self.__tree_map_file = {}
        self.__path_map_file = {}
        self.__clientsocket = None
        self.__is_freestatus = False
        self.i18n = get_i18n()

        # 連線控制（最上方一條，pack 自然響應寬度）
        self.m_conn_frame = ctk.CTkFrame(self.__father, fg_color="transparent")
        self.init_connect(self.m_conn_frame)
        self.m_conn_frame.pack(side=tk.TOP, pady=5)

        # 目錄樹（左半，固定起點 x=10、y=50；寬高隨視窗變化）
        self.path_tree_frame = ctk.CTkFrame(father, width=600, height=600)
        self.path_tree_frame.place(x=10, y=50)
        self.path_tree_frame.pack_propagate(False)
        self.init_path_tree(self.path_tree_frame)

        # 視圖區（目前空殼，預留給檔案內容預覽）
        self.view_file_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.init_view_file(self.view_file_frame)
        self.view_file_frame.place(x=620, y=50)

        # 響應式佈局：tab 父容器尺寸變化時，目錄樹跟著伸縮
        self.__father.bind("<Configure>", self._on_father_resize)

        # 初始化右击操作项(默认不显示)
        self.init_section(father)

        # legacy debug snippets removed (replaced by logger calls elsewhere)

    def init_section(self, father: tk.Misc) -> None:
        """
        初始化右击的操作栏(实现右键菜单)
        :param father: 父容器
        """

        def op_file_download() -> None:
            logger.debug("Enter op_file_download")
            pass

        def op_file_rename() -> None:
            logger.debug("Enter op_rename")
            pass

        def op_file_delect() -> None:
            logger.debug("Enter op_delect")
            pass

        def op_file_read_param() -> None:
            logger.debug("Enter op_read_param")
            pass

        # Create file operation menu
        self.__file_op_menu = tk.Menu(father, tearoff=0)
        self.__file_op_menu.add_command(label=self.i18n.t("download"), command=op_file_download)
        self.__file_op_menu.add_separator()
        self.__file_op_menu.add_command(label=self.i18n.t("rename"), command=op_file_rename)
        self.__file_op_menu.add_separator()
        self.__file_op_menu.add_command(label=self.i18n.t("delete"), command=op_file_delect)
        self.__file_op_menu.add_separator()
        self.__file_op_menu.add_command(label=self.i18n.t("properties"), command=op_file_read_param)

        def op_folder_upload_file() -> None:
            logger.debug("Enter op_folder_upload_file")
            pass

        def op_folder_create_subfolder() -> None:
            logger.debug("Enter op_folder_create_subfolder")
            pass

        def op_folder_rename() -> None:
            logger.debug("Enter op_folder_rename")
            pass

        def op_folder_delect() -> None:
            logger.debug("Enter op_folder_delect")
            pass

        # Create folder operation menu
        self.__folder_op_menu = tk.Menu(father, tearoff=0)
        self.__folder_op_menu.add_command(label=self.i18n.t("upload_file"), command=op_folder_upload_file)
        self.__folder_op_menu.add_separator()
        self.__folder_op_menu.add_command(label=self.i18n.t("new_folder"), command=op_folder_create_subfolder)
        self.__folder_op_menu.add_separator()
        self.__folder_op_menu.add_command(label=self.i18n.t("rename"), command=op_folder_rename)
        self.__folder_op_menu.add_separator()
        self.__folder_op_menu.add_command(label=self.i18n.t("delete"), command=op_folder_delect)

    def init_connect(self, father: tk.Misc) -> None:
        """
        初始化连接
        :param father: 父容器
        :return: None
        """
        border_padx = 10  # 兩個控件的間距

        ip_frame = ctk.CTkFrame(father, fg_color="transparent")
        self.m_ip_label = ctk.CTkLabel(ip_frame, text=self.i18n.t("ip_address"))
        self.m_ip_label.pack(side=tk.LEFT, padx=border_padx)
        # IP 輸入框 (width=20 chars ≈ 160 px)
        self.m_ip_entry = ctk.CTkEntry(ip_frame, width=160)
        self.m_ip_entry.pack(side=tk.LEFT, padx=border_padx)
        self.m_ip_entry.delete(0, tk.END)
        self.m_ip_entry.insert(tk.END, "本功能目前不可用")
        # Connect button
        self.conn_botton = ctk.CTkButton(
            ip_frame,
            text=self.i18n.t("connect"),
            command=self.connect_holocubic,
            width=80,
            height=28,
        )
        self.conn_botton.pack(side=tk.RIGHT, fill=tk.X, padx=5)

        ip_frame.pack(side=tk.TOP, pady=5)

    def connect_holocubic(self) -> None:
        # 客户端范例
        def my_recv_handle(dat: bytes) -> None:  # 接收函数
            msg_head = MsgHead()
            msg_head.decode(dat)
            logger.debug("Massages Len = %s", msg_head.msg_len)

            msg_fs = FileSystem()
            # Wire layout for AT_DIR_LIST response: 7-byte MsgHead + 1-byte
            # action_type + 99-byte dir_path (NUL-padded) + variable dir_info
            # (TAB-separated entries; folders end with /). See util/file_info.py.
            msg_fs.decode(dat)
            logger.debug("Massages action_type = %s", msg_fs.action_type)

            display_data = f"Client recv {dat}\n".encode()
            logger.debug("Massages dat = %s", display_data)

            # 消息处理
            if msg_fs.action_type == AT.AT_FREE_STATUS:
                logger.debug("AT_FREE_STATUS")
                self.__is_freestatus = True
                return None
            elif msg_fs.action_type == AT.AT_DIR_CREATE:
                logger.debug("AT_DIR_CREATE")
            elif msg_fs.action_type == AT.AT_DIR_REMOVE:
                logger.debug("AT_DIR_REMOVE")
            elif msg_fs.action_type == AT.AT_DIR_RENAME:
                logger.debug("AT_DIR_RENAME")
            elif msg_fs.action_type == AT.AT_DIR_LIST:
                logger.debug("AT_DIR_LIST")
                msg = DirList()
                msg.decode(dat)
                dir_path = msg.dir_path.decode("utf-8").strip(b"\x00".decode())
                sub_file_list = msg.dir_info.decode("utf-8").split("\t")[:-1]
                logger.debug("dir_path len: %s", len(dir_path))
                logger.debug("DirList info: %s", dir_path)
                logger.debug("DirList info: %s", sub_file_list)
                self.reflush_folder(dir_path, sub_file_list)
            elif msg_fs.action_type == AT.AT_FILE_CREATE:
                logger.debug("AT_FILE_CREATE")
            elif msg_fs.action_type == AT.AT_FILE_WRITE:
                logger.debug("AT_FILE_WRITE")
            elif msg_fs.action_type == AT.AT_FILE_READ:
                logger.debug("AT_FILE_READ")
                msg = FileRead()
                msg.decode(dat)
            elif msg_fs.action_type == AT.AT_FILE_REMOVE:
                logger.debug("AT_FILE_REMOVE")
            elif msg_fs.action_type == AT.AT_FILE_RENAME:
                logger.debug("AT_FILE_RENAME")
            elif msg_fs.action_type == AT.AT_FILE_GET_INFO:
                logger.debug("AT_FILE_GET_INFO")
                msg = FileGetInfo()
                msg.decode(dat)

        if self.conn_botton.cget("text") == self.i18n.t("connect"):
            try:
                ip_port = self.m_ip_entry.get().strip()
                ip, port = ip_port.split(":")
                logger.debug("connecting to %s:%s", ip, port)
                # 初始化端口並設定接收回呼
                self.__clientsocket = RobotSocketClient(ip, int(port), my_recv_handle)
                self.__clientsocket.start()

                self.conn_botton.configure(text=self.i18n.t("disconnect"))
            except Exception as err:
                logger.error("connect_holocubic failed: %s", err)
        else:
            self.conn_botton.configure(text=self.i18n.t("connect"))
            if self.__clientsocket is not None:
                self.__clientsocket.__del__()
                # del self.__clientsocket
                self.__clientsocket = None

    def init_path_tree(self, father: tk.Misc) -> None:
        """
        初始化连接
        :param father: 父容器
        :return: None
        """

        def display_op_menu(event: tk.Event) -> None:
            """
            显示一个文件操作框
            """
            try:
                for item in self.tree.selection():
                    logger.debug("tree focus: %s", self.tree.focus())
                    logger.debug("tree item: %s", item)
                    if self.__tree_map_file[item]["type"] == "file":
                        # 右击的是文件
                        self.__file_op_menu.post(event.x_root, event.y_root)
                    elif self.__tree_map_file[item]["type"] == "folder":
                        # 右击的是文件夹
                        self.__folder_op_menu.post(event.x_root, event.y_root)
                    logger.debug("tree item path: %s", self.__tree_map_file[item]["path"])
            except Exception as err:
                logger.error("display_op_menu failed:\n%s", traceback.format_exc())
                logger.error("display_op_menu error: %s", err)

        path_tree_frame = ctk.CTkFrame(father, fg_color="transparent")

        self.tree = ttk.Treeview(
            path_tree_frame,
            show="tree",
            selectmode="browse",
            height=28,
        )
        tree_y_scroll_bar = ctk.CTkScrollbar(
            path_tree_frame,
            command=self.tree.yview,
            orientation="vertical",
        )
        tree_y_scroll_bar.pack(side=tk.RIGHT, fill=tk.Y)
        self.tree["yscrollcommand"] = tree_y_scroll_bar.set
        tree_x_scroll_bar = ctk.CTkScrollbar(
            path_tree_frame,
            command=self.tree.xview,
            orientation="horizontal",
        )
        tree_x_scroll_bar.pack(side=tk.BOTTOM, fill=tk.X)
        self.tree["xscrollcommand"] = tree_x_scroll_bar.set
        self.tree.pack(expand=1, fill=tk.BOTH)

        # <<TreeviewSelect>>
        # <<TreeviewOpen>>
        # <<TreeviewClose>>
        # <Button-1> <Button-2> <Button-3> 鼠标、左、右键单机
        self.tree.bind("<<TreeviewOpen>>", lambda event: self.tree_open())
        self.tree.bind("<Button-3>", display_op_menu)
        # text.bind("<MouseWheel>", lambda event : self.update_line())
        folder_img_path = get_resource_path("image/folder.png")
        file_img_path = get_resource_path("image/text_file.png")
        self.folder_img = tk.PhotoImage(file=folder_img_path)
        self.file_img = tk.PhotoImage(file=file_img_path)

        # 初始化根
        self.tree_root = self.tree.insert(
            "", tk.END, text=self.i18n.t("sd_card_files"), open=True, image=self.folder_img
        )
        root_file = {
            "tree": self.tree_root,
            "type": "folder",
            "name": self.i18n.t("sd_card_files"),
            "path": "/",
            "sub_file": [],
        }
        self.__tree_map_file[self.tree_root] = root_file  # 初始化总目录
        self.__path_map_file[root_file["path"]] = root_file
        self.display_path_tree(self.tree_root, root_file)
        # path_tree_frame.pack(side=tk.TOP, pady=5)
        path_tree_frame.pack(side=tk.RIGHT, fill=tk.Y)

    def display_path_tree(self, cur_tree_root: str, file_obj: dict[str, object]) -> None:
        """
        显示目录树
        :param cur_tree_root: 当前树根
        :param file_obj: 当前要更新的文件父对象
        :return: None
        """
        if file_obj["sub_file"] is None:
            logger.debug("文件元素不需要显示")
            return None  # 文件元素不需要显示

        # 删除之前创建的节点
        for sub_item in self.tree.get_children(cur_tree_root):
            self.__tree_map_file[sub_item]["tree"] = None  # 由于下一步需要删除
            del self.__tree_map_file[sub_item]
            self.tree.delete(sub_item)

        # 刷新显示子元素
        for sub_file in file_obj["sub_file"]:
            if sub_file["type"] == "folder":
                image = self.folder_img
            else:
                image = self.file_img
            sub_tree = self.tree.insert(
                cur_tree_root,
                tk.END,
                text=sub_file["name"],
                values=(sub_file["path"],),
                open=True,
                image=image,
            )

            # 绑定tree与文件对象的关系
            self.__tree_map_file[sub_tree] = sub_file
            sub_file["tree"] = sub_tree
            # self.__file_map_tree[sub_file["path"]] = sub_tree

    def reflush_folder(self, updata_path: str, sub_file_list: list[str]) -> None:
        """
        刷新目录

        """
        try:
            self.__path_map_file[updata_path]["sub_file"].clear()
            for sub_file_name in sub_file_list:
                sub_tmp = None
                if "/" == sub_file_name[-1]:
                    sub_tmp = {
                        "tree": None,
                        "type": "folder",
                        "name": sub_file_name[:-1],
                        "path": updata_path + sub_file_name[:-1],
                        "sub_file": [],
                    }
                else:
                    sub_tmp = {
                        "tree": None,
                        "type": "file",
                        "name": sub_file_name,
                        "path": updata_path + sub_file_name,
                        "sub_file": None,
                    }

                # 添加节点
                self.__path_map_file[sub_tmp["path"]] = sub_tmp
                # 将子节点添加到父节点中
                self.__path_map_file[updata_path]["sub_file"].append(sub_tmp)

            logger.debug("reflush_folder result: %s", self.__path_map_file[updata_path])
        except Exception as err:
            logger.error("reflush_folder failed:\n%s", traceback.format_exc())
            logger.error("reflush_folder error: %s", err)

        # 刷新显示
        self.display_path_tree(self.__path_map_file[updata_path]["tree"], self.__path_map_file[updata_path])

    def tree_open(self) -> None:
        """
        Tree元素被打开
        """
        for item in self.tree.selection():
            # 得到当前选中的节点
            logger.debug("open---> %s", self.tree.item(item, "open"))
            if self.__tree_map_file[item]["type"] == "file":
                return None
            if self.__clientsocket is not None:
                path = self.__tree_map_file[item]["path"]
                path = path if path == "/" else path.rstrip("/")
                send_data = DirList(path).encode()
                logger.debug("Send ---> len=%d data=%s", len(send_data), send_data)
                # 发送查询数据
                self.__clientsocket.send_to_ser(send_data)

    def init_view_file(self, father: tk.Misc) -> None:
        """初始化視圖區（目前空容器）。"""
        view_file_frame = ctk.CTkFrame(father, fg_color="transparent")
        view_file_frame.pack(side=tk.TOP, pady=5)

    def click_model_create(self) -> None:
        """
        点击模型"创建"菜单项触发的函数
        :return: None
        """
        logger.debug("click_model_create")
        # self.__engine.on_thread_message(mh.M_CTRLMENU, mh.M_MODEL_FILEMANAGER,
        #                               mh.A_FILE_CREATE, self.m_model_filepath)

    def _on_father_resize(self, event: tk.Event) -> None:
        """父容器尺寸變化時，目錄樹寬高跟著伸縮（連線列由 pack 自動處理）。"""
        pw, ph = event.width, event.height
        # 目錄樹從 x=10、y=50 起，左右各 10 px、底部 10 px 邊距
        tree_w = max(400, pw - 20)
        tree_h = max(300, ph - 60)
        self.path_tree_frame.configure(width=tree_w, height=tree_h)

    def __del__(self) -> None:
        if self.__clientsocket is not None:
            self.__clientsocket.close()
            self.__clientsocket.__del__()
            self.__clientsocket = None
