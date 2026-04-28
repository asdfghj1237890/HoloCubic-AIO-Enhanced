# -*- coding: utf-8 -*-
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

from util.common import _async_raise
from util.logger import get_logger

logger = get_logger(__name__)

# Callback signature for received data on the server side
ServerRecvCallback = Callable[[bytes, tuple[str, int]], None]
# Callback signature for received data on the client side
ClientRecvCallback = Callable[[bytes], None]


class RobotSocket(object):

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

    def close(self) -> None:
        try:
            self.connfd.close()  # 关闭连接
            self.connfd = None
        except Exception as err:
            logger.error("close failed: %s", err)

    @property
    def callback_func(self) -> Callable[..., None] | None:
        return self._callback_func

    @callback_func.setter
    def callback_func(self, callback: Callable[..., None] | None) -> None:
        self._callback_func = callback

    def start(self) -> None:
        # override
        pass

    def __del__():  # type: ignore[no-untyped-def]  # noqa: ANN  # original signature missing self
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
        self.__sersocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)  # 定义socket类型，网络通信，TCP
        self.__sersocket.bind((self._ip, self._port))  # 套接字绑定的IP与端口
        self.__max_bind = max_bind
        self.__client_link_dict: dict[tuple[str, int], dict[str, object]] = {}
        self.__sersocket.listen(self.__max_bind)  # 开始TCP监听
        self.__recv_buff = 1024 * 128

    def start(self) -> None:
        '''
        启动socket实例
        :return:
        '''

        def scanner() -> None:
            while True:
                connfd, addr = self.__sersocket.accept()  # 接受TCP连接，并返回新的套接字与IP地址
                logger.info("Connected by %s", addr)  # 输出客户端的IP地址
                run_thread = threading.Thread(target=self.recvfrom_client, args=(connfd, addr))
                run_thread.start()
                self.__client_link_dict[addr] = {"fd": connfd, 'pthread': run_thread}

        run_thread = threading.Thread(target=scanner, args=())
        run_thread.start()

    def recvfrom_client(self, connfd: socket.socket, addr: tuple[str, int]) -> None:
        """
        客户端连接状态的数据处理
        :param connfd: 连接客户端的文件句柄
        :param addr: 客户端的地址
        :return:
        """
        try:
            while True:
                recv = connfd.recv(self.__recv_buff)  # 把接收的数据实例化
                if recv == b'':  # 断开连接
                    break
                if self.callback_func is not None:
                    self.callback_func(recv, addr)
        except Exception as err:
            logger.info("Client disconnected, recv thread exiting: %s", err)

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

    def __del__(self) -> None:
        try:
            for conninfo in self.__client_link_dict.items():
                connfd = conninfo["fd"]
                connfd.close()  # 关闭连接
                _async_raise(conninfo['pthread'])
                del conninfo
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
        self.__disconntime = disconntime  # 掉线重连的时间
        self.__recv_buff = 1024 * 128

    def start(self) -> None:
        '''
        启动socket实例
        :return:
        '''

        def reconner() -> None:
            while True:
                try:
                    addr = (self._ip, self._port)
                    if self.__connFlag is False:
                        logger.info("Try to reconnect......")
                        del self.__clientsocket
                        self.__clientsocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)  # 定义socket类型，网络通信，TCP
                        self.__clientsocket.connect(addr)
                        logger.info("Connected by %s", addr)  # 输出客户端的IP地址
                        self.__connFlag = True
                except Exception as err:
                    logger.error("reconnect failed: %s", err)
                    time.sleep(self.__disconntime)

        self.reconner_thread = threading.Thread(target=reconner, args=())
        self.reconner_thread.start()
        self.recvfrom_ser_thread = threading.Thread(target=self.recvfrom_ser, args=())
        self.recvfrom_ser_thread.start()

    def recvfrom_ser(self) -> None:
        """
        客户端连接状态的数据处理
        :param client: 连接客户端的文件句柄
        :param addr: 客户端的地址
        :return:
        """""
        try:
            while True:
                try:
                    if self.__connFlag is True:
                        recv = self.__clientsocket.recv(self.__recv_buff)  # 把接收的数据实例化
                        if recv == b'':  # 断开连接
                            break
                        if self.callback_func is not None:
                            self.callback_func(recv)

                except Exception as err:
                    self.__clientsocket.close()
                    self.__connFlag = False
                    logger.error("recv from server failed: %s", err)  # 发生异常所在的文件
                    time.sleep(self.__disconntime * 0.2)

        except Exception as err:
            logger.error("recvfrom_ser outer failure: %s", err)

    def send_to_ser(self, dat: bytes) -> None:
        """
        向本次连接的客户端发送数据
        :param dat: 要发送的数据（bytes类型）
        :return:
        """
        try:
            self.__clientsocket.sendall(dat)
        except Exception as err:
            logger.error("send_to_ser failed: %s", err)

    def __del__(self) -> None:
        try:
            self.__clientsocket.close()  # 关闭连接
            del self.__clientsocket
            self.__clientsocket = None
        except Exception as err:
            logger.error("client cleanup failed: %s", err)

        self.__connFlag = False
        _async_raise(self.reconner_thread)
        _async_raise(self.recvfrom_ser_thread)


if __name__ == "__main__":
    # This is demo

    # 服务器端范例
    def myRecvHandle(dat: bytes, addr: tuple[str, int]) -> None:  # 接收函数
        sersocket.send_to_client(dat, addr)
        dat = ("Server recv %s from %s\n" % (dat, addr)).encode(encoding="utf-8")
        logger.info("server demo received: %s", dat)


    # 初始化端口并设置接收数据的函数(当接收到数据，自动被调用)
    sersocket = RobotSocketServer("192.168.123.244", 6666, myRecvHandle, max_bind=10)
    sersocket.start()  # socket开始工作
    import time

    while True:
        time.sleep(1)
    # sersocket.send_to_client(b'Hello \n')
    """
    # 客户端范例
    def myRecvHandle(dat):  # 接收函数
        dat = ("Client recv %s\n" % dat).encode(encoding="utf-8")
        logger.info("client demo received: %s", dat)

    # 初始化端口并设置接收数据的函数(当接收到数据，自动被调用)
    clientsocket = RobotSocketClient("192.168.123.244", 6666, myRecvHandle)
    clientsocket.start()    # socket开始工作
    import time
    while True:
        dataIn = input("输入要发送给服务端的数据：")
        dat = ("%s\n" % dataIn).encode(encoding="utf-8")
        clientsocket.send_to_ser(dat)
    """
