"""Tests for util.robotsocket — graceful shutdown via threading.Event."""

from __future__ import annotations

import socket
import threading
import time

from util.robotsocket import RobotSocketClient, RobotSocketServer


def _free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class TestRobotSocketServer:
    def test_server_starts_and_stops_cleanly(self) -> None:
        port = _free_port()
        server = RobotSocketServer("127.0.0.1", port, callback_func=lambda d, a: None)
        server.start()
        time.sleep(0.2)

        # Stop should not hang
        start = time.time()
        server.stop()
        elapsed = time.time() - start
        assert elapsed < 5.0, f"server.stop() took {elapsed:.2f}s — should be fast"

    def test_server_receives_client_data(self) -> None:
        port = _free_port()
        received: list[bytes] = []
        ev = threading.Event()

        def on_recv(data: bytes, addr: tuple[str, int]) -> None:
            received.append(data)
            ev.set()

        server = RobotSocketServer("127.0.0.1", port, on_recv)
        server.start()
        time.sleep(0.2)

        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as cli:
                cli.connect(("127.0.0.1", port))
                cli.sendall(b"hello world")
                # Wait up to 2s for callback
                assert ev.wait(timeout=2.0), "server did not receive data"
            assert received == [b"hello world"]
        finally:
            server.stop()


class TestRobotSocketClient:
    def test_client_starts_and_stops_cleanly(self) -> None:
        # Connect to a port nothing is listening on — client will keep retrying.
        # stop() must still tear it down promptly.
        port = _free_port()
        client = RobotSocketClient("127.0.0.1", port, callback_func=lambda d: None,
                                   disconntime=0.1)
        client.start()
        time.sleep(0.2)

        start = time.time()
        client.stop()
        elapsed = time.time() - start
        assert elapsed < 5.0, f"client.stop() took {elapsed:.2f}s — should be fast"

    def test_client_to_server_round_trip(self) -> None:
        port = _free_port()
        server_received: list[bytes] = []
        server_ev = threading.Event()
        client_received: list[bytes] = []
        client_ev = threading.Event()

        def server_cb(data: bytes, addr: tuple[str, int]) -> None:
            server_received.append(data)
            server_ev.set()

        def client_cb(data: bytes) -> None:
            client_received.append(data)
            client_ev.set()

        server = RobotSocketServer("127.0.0.1", port, server_cb)
        server.start()
        time.sleep(0.2)

        client = RobotSocketClient("127.0.0.1", port, client_cb, disconntime=0.1)
        client.start()
        # Allow reconnect loop to establish connection
        time.sleep(0.5)

        try:
            client.send_to_ser(b"ping")
            assert server_ev.wait(timeout=2.0), "server did not receive ping"
            assert b"ping" in server_received[0]
        finally:
            client.stop()
            server.stop()
