//! Integration tests for TcpTransport against a localhost echo server.
//!
//! Each test starts a tiny in-process accept loop on a fresh port, hands the
//! port to TcpTransport, drives the SUT, and tears down.

use std::io::{Read, Write};
use std::net::{SocketAddr, TcpListener};
use std::thread;
use std::time::Duration;

use aio_device::tcp::TcpTransport;
use aio_device::{Transport, TransportError, TransportKind};

/// Spawn a one-shot echo server on a free localhost port. Returns the bound
/// address and a JoinHandle that exits when the (single) connection closes.
fn spawn_echo_server() -> (SocketAddr, thread::JoinHandle<()>) {
    let listener = TcpListener::bind("127.0.0.1:0").expect("bind echo");
    let addr = listener.local_addr().unwrap();
    let handle = thread::spawn(move || {
        if let Ok((mut stream, _)) = listener.accept() {
            let mut buf = [0u8; 1024];
            while let Ok(n) = stream.read(&mut buf) {
                if n == 0 {
                    break;
                }
                if stream.write_all(&buf[..n]).is_err() {
                    break;
                }
            }
        }
    });
    (addr, handle)
}

#[test]
fn echo_roundtrip() {
    let (addr, server) = spawn_echo_server();
    let mut t = TcpTransport::new(addr)
        .with_reconnect_interval(Duration::from_millis(50))
        .with_read_timeout(Duration::from_millis(200));

    t.write_all(b"ping").expect("write");
    // Give the echo server a beat.
    thread::sleep(Duration::from_millis(50));
    let mut buf = [0u8; 16];
    let n = t.read(&mut buf).expect("read");
    assert_eq!(&buf[..n], b"ping");

    t.close();
    let _ = server.join();
}

#[test]
fn kind_is_tcp() {
    let t = TcpTransport::new("127.0.0.1:0".parse().unwrap());
    assert_eq!(t.kind(), TransportKind::Tcp);
}

#[test]
fn connect_refused_returns_error() {
    // Port that should be free. We don't bind anything → ConnectionRefused.
    let t_addr: SocketAddr = "127.0.0.1:1".parse().unwrap();
    let mut t = TcpTransport::new(t_addr)
        .with_reconnect_interval(Duration::from_millis(50))
        .with_read_timeout(Duration::from_millis(100));
    let mut buf = [0u8; 4];
    // Either TimedOut (refused mapped) or a different IO error depending on OS.
    // We just assert it's not silently OK.
    assert!(t.read(&mut buf).is_err());
}

#[test]
fn close_makes_subsequent_calls_fail() {
    // No echo server needed: close() is called before any connect attempt,
    // so write_all/read short-circuit on Closed without touching the network.
    let addr: SocketAddr = "127.0.0.1:1".parse().unwrap();
    let mut t = TcpTransport::new(addr);
    t.close();
    assert!(!t.is_open());
    assert!(matches!(t.write_all(b"x"), Err(TransportError::Closed)));
}
