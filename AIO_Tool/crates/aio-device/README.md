# aio-device

HoloCubic AIO device transport abstraction — `Transport` trait over USB
serial (via `serialport`) and WiFi TCP, plus a `MockTransport` for tests.

## Design notes

- **Trait, not enum**: implementations differ enough (serial has no
  reconnect, TCP does) that a single enum would push variant-specific
  logic everywhere. `Box<dyn Transport>` lets the UI store one of either
  variant uniformly.
- **No internal threads**: per design spec Section 2, the crate is
  blocking-with-timeout. Callers are expected to push reads onto a
  `std::thread::spawn` and forward bytes via `std::sync::mpsc`. This
  keeps shutdown semantics simple (close the transport, dropping the
  channel sender wakes the reader).
- **Inline reconnect** for TCP: the reconnect state machine lives inside
  `maybe_reconnect()` called from every read/write rather than in a
  background thread. Tradeoff: slightly bursty I/O during reconnect, but
  no extra thread to manage.
- **Buffer / timeout choices**: serial read timeout 500 ms (improved
  vs Python's 10 s); TCP recv 64 KiB buffer (vs Python's 128 KiB —
  HoloCubic protocol messages are tiny). See Plan 3 D4 for full table.

## Testing

```sh
cargo test -p aio-device
```

Three layers:

- Unit tests for `MockTransport` (6) and `SerialTransport::kind()` (1).
- Integration test in `tests/tcp_loopback.rs` (4) drives `TcpTransport`
  against a localhost echo server — covers happy path, kind, connection
  refused, and close.
- **Hardware integration** for real serial / WiFi: not in CI; tested
  pre-release per spec Section 6 Layer 6.

## When to use MockTransport

Downstream crates (e.g. `aio-tool` in Plan 7) needing tests against the
transport-aware code paths add this to their `Cargo.toml`:

```toml
[dev-dependencies]
aio-device = { workspace = true, features = ["mock"] }
```

Then import `aio_device::mock::{MockTransport, MockHandle}` in tests.
