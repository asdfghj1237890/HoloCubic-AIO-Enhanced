################################################################################
#
# Author: ClimbSnail(HQ)
# original source is here.
#   https://github.com/ClimbSnail/Robot_For_RaspberryPi
#
#
################################################################################

import socket  # socket模块
import threading
import time
from collections.abc import Callable

from util.logger import get_logger

logger = get_logger(__name__)

#: How often recv loops wake up to re-check the stop event (seconds).
_RECV_TIMEOUT_SEC: float = 0.5
#: Max time to wait for a thread to exit before giving up in __del__.
_THREAD_JOIN_TIMEOUT_SEC: float = 1.0

# Callback signature for received data on the server side
ServerRecvCallback = Callable[[bytes, tuple[str, int]], None]
# Callback signature for received data on the client side
ClientRecvCallback = Callable[[bytes], None]


class RobotSocket:
    """Base class for socket helpers, providing cooperative shutdown via Event."""

    def __init__(
        self,
        ip: str,
        port: int,
        callback_func: Callable[..., None] | None = None,
        name: str = "",
    ) -> None:
        self._name = name
        self._ip = ip
        self._port = port
        self._callback_func = callback_func
        # Cooperative shutdown signal — threads check periodically.
        self._stop_event: threading.Event = threading.Event()

    def stop(self) -> None:
        """Request graceful shutdown of all worker threads."""
        self._stop_event.set()

    @property
    def callback_func(self) -> Callable[..., None] | None:
        return self._callback_func

    @callback_func.setter
    def callback_func(self, callback: Callable[..., None] | None) -> None:
        self._callback_func = callback

    def start(self) -> None:
        # override
        pass


class RobotSocketServer(RobotSocket):
    # 服务端类
    def __init__(
        self,
        ip: str,
        port: int,
        callback_func: ServerRecvCallback | None = None,
        max_bind: int = 1,
        name: str = "RobotSocketServer",
    ) -> None:
        """
        RobotSocketServer类对象的初始化
        :param ip: 点分十进制的ip字符串
        :param port: 端口号整型数据(0-65535)
        :param callback_func: 接收处理函数
        :param max_bind: 最大服务数量
        :param name: socket实例名称
        """
        super().__init__(ip, port, callback_func, name)
        self.__sersocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)  # TCP
        self.__sersocket.settimeout(_RECV_TIMEOUT_SEC)
        self.__sersocket.bind((self._ip, self._port))
        self.__max_bind = max_bind
        self.__client_link_dict: dict[tuple[str, int], dict[str, object]] = {}
        self.__sersocket.listen(self.__max_bind)
        self.__recv_buff = 1024 * 128
        self._scanner_thread: threading.Thread | None = None

    def start(self) -> None:
        """啟動 server，建立背景 accept 迴圈。"""

        def scanner() -> None:
            while not self._stop_event.is_set():
                try:
                    connfd, addr = self.__sersocket.accept()
                except TimeoutError:
                    continue
                except OSError as err:
                    if self._stop_event.is_set():
                        return
                    logger.error("accept failed: %s", err)
                    continue
                logger.info("Connected by %s", addr)
                run_thread = threading.Thread(
                    target=self.recvfrom_client,
                    args=(connfd, addr),
                    daemon=True,
                )
                run_thread.start()
                self.__client_link_dict[addr] = {"fd": connfd, "pthread": run_thread}

        self._scanner_thread = threading.Thread(target=scanner, daemon=True)
        self._scanner_thread.start()

    def recvfrom_client(self, connfd: socket.socket, addr: tuple[str, int]) -> None:
        """處理已連線客戶端的資料。stop_event 觸發時跳出迴圈。"""
        connfd.settimeout(_RECV_TIMEOUT_SEC)
        try:
            while not self._stop_event.is_set():
                try:
                    recv = connfd.recv(self.__recv_buff)
                except TimeoutError:
                    continue
                if recv == b"":
                    break
                if self.callback_func is not None:
                    self.callback_func(recv, addr)
        except Exception as err:
            logger.info("Client disconnected, recv thread exiting: %s", err)
        finally:
            try:
                connfd.close()
            except OSError:
                pass

    def send_to_client(self, dat: bytes, addr: tuple[str, int]) -> None:
        """
        向本次连接的客户端发送数据
        :param dat: 要发送的数据（bytes类型）
        :param addr: 要发送到的客户端地址
        :return:
        """
        try:
            if addr in self.__client_link_dict.keys():
                self.__client_link_dict["fd"].sendall(dat)
            else:
                logger.warning("Address not found or disconnected: %s", addr)
        except Exception as err:
            logger.error("send_to_client failed: %s", err)

    def stop(self) -> None:
        """請求 server 與所有 worker 執行緒優雅關閉。"""
        super().stop()
        try:
            self.__sersocket.close()
        except OSError as err:
            logger.error("server socket close failed: %s", err)
        for info in self.__client_link_dict.values():
            connfd = info.get("fd")
            if isinstance(connfd, socket.socket):
                try:
                    connfd.close()
                except OSError:
                    pass
            pthread = info.get("pthread")
            if isinstance(pthread, threading.Thread):
                pthread.join(timeout=_THREAD_JOIN_TIMEOUT_SEC)
        if self._scanner_thread is not None:
            self._scanner_thread.join(timeout=_THREAD_JOIN_TIMEOUT_SEC)

    def __del__(self) -> None:
        try:
            self.stop()
        except Exception as err:
            logger.error("server cleanup failed: %s", err)


class RobotSocketClient(RobotSocket):
    # 客户端类
    def __init__(
        self,
        ip: str,
        port: int,
        callback_func: ClientRecvCallback | None = None,
        disconntime: float = 0.5,
        name: str = "RobotSocketClient",
    ) -> None:
        """
        RobotSocket类对象的初始化
        :param ip: 点分十进制的ip字符串
        :param port: 端口号整型数据(0-65535)
        :param callback_func: 接收处理函数
        :param name: socket实例名称
        """
        super().__init__(ip, port, callback_func, name)
        self.__clientsocket: socket.socket | None = None
        self.__connFlag = False  # 连接状态
        self.__disconntime = disconntime  # 掉線重連的時間（秒）
        self.__recv_buff = 1024 * 128
        self.reconner_thread: threading.Thread | None = None
        self.recvfrom_ser_thread: threading.Thread | None = None

    def start(self) -> None:
        """啟動 client，建立 reconnect 與 recv 兩條背景執行緒。"""

        def reconner() -> None:
            while not self._stop_event.is_set():
                try:
                    addr = (self._ip, self._port)
                    if self.__connFlag is False:
                        logger.info("Try to reconnect......")
                        if self.__clientsocket is not None:
                            try:
                                self.__clientsocket.close()
                            except OSError:
                                pass
                        self.__clientsocket = socket.socket(
                            socket.AF_INET,
                            socket.SOCK_STREAM,
                        )
                        self.__clientsocket.settimeout(_RECV_TIMEOUT_SEC)
                        self.__clientsocket.connect(addr)
                        logger.info("Connected by %s", addr)
                        self.__connFlag = True
                except Exception as err:
                    logger.error("reconnect failed: %s", err)
                # 用 wait 取代 sleep，可被 stop_event 提前喚醒
                if self._stop_event.wait(timeout=self.__disconntime):
                    return

        self.reconner_thread = threading.Thread(target=reconner, daemon=True)
        self.reconner_thread.start()
        self.recvfrom_ser_thread = threading.Thread(target=self.recvfrom_ser, daemon=True)
        self.recvfrom_ser_thread.start()

    def recvfrom_ser(self) -> None:
        """背景接收伺服器回應，stop_event 觸發時跳出迴圈。"""
        while not self._stop_event.is_set():
            if not self.__connFlag or self.__clientsocket is None:
                if self._stop_event.wait(timeout=self.__disconntime * 0.2):
                    return
                continue
            try:
                recv = self.__clientsocket.recv(self.__recv_buff)
            except TimeoutError:
                continue
            except Exception as err:
                if self.__clientsocket is not None:
                    try:
                        self.__clientsocket.close()
                    except OSError:
                        pass
                self.__connFlag = False
                logger.error("recv from server failed: %s", err)
                if self._stop_event.wait(timeout=self.__disconntime * 0.2):
                    return
                continue
            if recv == b"":
                # 對端關閉連線
                self.__connFlag = False
                continue
            if self.callback_func is not None:
                self.callback_func(recv)

    def send_to_ser(self, dat: bytes) -> None:
        """送出資料到伺服器。"""
        if self.__clientsocket is None:
            logger.warning("send_to_ser called before connection established")
            return
        try:
            self.__clientsocket.sendall(dat)
        except Exception as err:
            logger.error("send_to_ser failed: %s", err)

    def stop(self) -> None:
        """請求 client 與 reconnect 執行緒優雅關閉。"""
        super().stop()
        self.__connFlag = False
        if self.__clientsocket is not None:
            try:
                self.__clientsocket.close()
            except OSError as err:
                logger.error("client socket close failed: %s", err)
            self.__clientsocket = None
        for t in (self.reconner_thread, self.recvfrom_ser_thread):
            if isinstance(t, threading.Thread):
                t.join(timeout=_THREAD_JOIN_TIMEOUT_SEC)

    def __del__(self) -> None:
        try:
            self.stop()
        except Exception as err:
            logger.error("client cleanup failed: %s", err)


if __name__ == "__main__":
    # This is demo

    # 服务器端范例
    def my_recv_handle(dat: bytes, addr: tuple[str, int]) -> None:  # 接收函数
        sersocket.send_to_client(dat, addr)
        dat = f"Server recv {dat} from {addr}\n".encode()
        logger.info("server demo received: %s", dat)

    # 初始化端口并设置接收数据的函数(当接收到数据，自动被调用)
    sersocket = RobotSocketServer("192.168.123.244", 6666, my_recv_handle, max_bind=10)
    sersocket.start()  # socket开始工作
    import time

    while True:
        time.sleep(1)
    # sersocket.send_to_client(b'Hello \n')
    """
    # 客户端范例
    def my_recv_handle(dat):  # 接收函数
        dat = ("Client recv %s\n" % dat).encode(encoding="utf-8")
        logger.info("client demo received: %s", dat)

    # 初始化端口并设置接收数据的函数(当接收到数据，自动被调用)
    clientsocket = RobotSocketClient("192.168.123.244", 6666, my_recv_handle)
    clientsocket.start()    # socket开始工作
    import time
    while True:
        dataIn = input("输入要发送给服务端的数据：")
        dat = ("%s\n" % dataIn).encode(encoding="utf-8")
        clientsocket.send_to_ser(dat)
    """
