# aio-flasher

HoloCubic AIO firmware flasher — wraps the [`espflash`](https://crates.io/crates/espflash)
crate for the use case of "erase the chip, then write 4 partitions".

## Design notes

- **Real progress, not estimated**: events from espflash's
  `ProgressCallbacks` are forwarded as a typed `FlashEvent` enum through
  an `mpsc::Sender`. The legacy Python tool's progress bar was estimated
  from baud × size and frequently lied to users.
- **Cooperative cancellation**: `Arc<AtomicBool>` flag checked on every
  partition boundary. espflash 3.3.0's `ProgressCallbacks` methods don't
  return `Result`, so cancellation can't abort mid-`write_bin_to_flash`
  — the in-flight partition completes, then the loop in
  `write_partitions` returns `FlashError::Cancelled`. The Python tool
  used a ctypes `_async_raise` hack that could leave the serial port in
  a half-open state; this design avoids that class of bug entirely.
- **No internal threads**: the crate is blocking. Callers (Plan 7
  Flasher tab) spawn a `std::thread::spawn`, run the operation on it,
  and receive events via the channel on the UI thread.
- **Typed errors**: `FlashError` covers `OpenPort`, `Connect`, `Erase`,
  `WritePartition`, `OverlappingPartitions`, `Cancelled`. No `Exception`
  fishing.
- **Partition validation before hardware touch**:
  `Flasher::write_partitions` calls `partition::validate()` before any
  I/O. A bad input fails fast without leaving the chip in a partially-
  written state.

## Testing

```sh
cargo test -p aio-flasher
```

Three layers:

- Unit tests in `partition.rs` (overlap detection, HoloCubic address
  constants), `progress.rs` (event ordering, cancel flag observation),
  and `flasher.rs` (the pre-hardware validate path).
- **Hardware integration**: NOT in CI; tested manually pre-release per
  spec Section 6 Layer 6. Connect a real HoloCubic in download mode,
  flash a known-good firmware, verify it boots.

## Linux build dep

The workspace's `serialport` dep has `default-features = false`
(Plan 3 choice — skips libudev). `espflash 3.3.0` was pulled in with
`features = ["serialport"]` only — no `cli` feature — which keeps the
dep tree compatible with our 1.82 toolchain pin (the `cli` feature
transitively requires `edition2024` / Rust 1.85+).

PR #92 confirmed the Ubuntu CI runner builds cleanly without
`libudev-dev` installed. If a future espflash version reintroduces the
libudev hard requirement, CI will fail and `Docs/superpowers/specs/`
spec Section 7 R3 should be re-evaluated.

## espflash version pinning

`espflash` is pinned to exactly `=3.3.0` because espflash has had
breaking changes between minor versions. Bumping the pin requires a
deliberate PR that re-tests the flash flow end-to-end on real hardware.
`Cargo.lock` additionally pins `indexmap = 2.7.1` to prevent a
transitive bump into the `edition2024` hashbrown 0.17 / indexmap 2.14
zone.

## When to use which API

- `Flasher::new(port, baud)` — opens the device, blocks for ~1 second
  during chip detection (espflash uses DTR/RTS toggling to enter
  download mode automatically).
- `Flasher::erase(progress_tx, cancel)` — wipes the entire flash. Emits
  `EraseStart`/`EraseDone` events. Use before a partition write only
  when explicitly requested by the user (Python tool exposes this as a
  separate "Clear Flash" button).
- `Flasher::write_partitions(parts, progress_tx, cancel)` — writes a
  list of `Partition { address, data }` items. Validates overlap before
  starting; partitions are written sequentially in the supplied order.
  Emits `PartitionStart`/`Progress`/`PartitionDone` per partition.

Standard HoloCubic flash layout exposed as compile-time consts:

| Const | Value | Purpose |
|-------|-------|---------|
| `PARTITION_BOOTLOADER` | `0x1000` | Second-stage bootloader |
| `PARTITION_PARTITIONS` | `0x8000` | Partition table |
| `PARTITION_BOOTAPP0`   | `0xe000` | OTA selector |
| `PARTITION_FIRMWARE`   | `0x10000` | HoloCubic firmware payload |
