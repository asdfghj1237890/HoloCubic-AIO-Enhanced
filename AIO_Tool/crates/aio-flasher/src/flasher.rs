//! Flasher implementation.
//!
//! Wraps espflash 3.3.0's [`espflash::flasher::Flasher`] with the aio-flasher
//! contract: `new(port, baud)` opens the serial port and connects to the ROM
//! bootloader; `erase(progress, cancel)` chip-erases; and
//! `write_partitions(parts, progress, cancel)` writes every partition in a
//! single espflash session. Progress events flow through an
//! `mpsc::Sender<FlashEvent>`; cancellation flows in via `Arc<AtomicBool>`
//! and is observed by silencing the [`espflash::flasher::ProgressCallbacks`]
//! bridge — the in-flight write itself can't be interrupted (see the
//! cancellation note on [`crate::progress`]).
//!
//! espflash 3.3.0 API surface used:
//! - [`espflash::flasher::Flasher::connect`] — port + UsbPortInfo + baud +
//!   reset strategy in / out.
//! - [`espflash::flasher::Flasher::erase_flash`] — full-chip erase.
//! - [`espflash::flasher::Flasher::write_bins_to_flash`] — multi-segment write
//!   in a single session (one `begin` / N `write_segment` / one `finish`),
//!   avoiding the post-partition hard reset that `write_bin_to_flash` would
//!   trigger between each segment. See `Flasher::write_partitions` for the
//!   full reasoning.
//! - [`espflash::flasher::ProgressCallbacks`] — `init(addr, total)`,
//!   `update(current)`, `finish()`. None return `Result`, so cancel is
//!   observed by skipping further sends; espflash carries on writing.

use std::borrow::Cow;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::Sender;
use std::sync::Arc;
use std::time::Duration;

use espflash::connection::reset::{ResetAfterOperation, ResetBeforeOperation};
use espflash::elf::RomSegment;
use espflash::flasher::{FlashSize, Flasher as EspFlasher, ProgressCallbacks};
use espflash::targets::Chip;
use serialport::{FlowControl, SerialPort, UsbPortInfo};

use crate::error::FlashError;
use crate::partition::{self, Partition};
use crate::progress::FlashEvent;

/// Human-readable chip identity, populated by [`Flasher::device_info`].
///
/// Every field is a pre-formatted display string so the caller (Tauri / UI)
/// doesn't need to depend on `espflash` enums or unit conversions. Missing
/// data is reported as `"—"` rather than `None` for the same reason.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DeviceSummary {
    /// Chip family — `"ESP32"`, `"ESP32-C3"`, `"ESP32-S3"`, etc.
    pub chip: String,
    /// Silicon revision — `"v3.0"` etc., or `"—"` if espflash couldn't read it.
    pub revision: String,
    /// MAC address — `"a4:cf:12:34:56:78"`. Pulled from BLK0 efuses.
    pub mac: String,
    /// Total flash size — `"4 MB"`. Auto-detected during connect; falls back
    /// to espflash's 4 MB default if the SPI flash chip's size ID is unknown.
    pub flash_size: String,
}

/// HoloCubic flasher. Holds the open serial connection to the ROM bootloader.
pub struct Flasher {
    inner: Option<EspFlasher>,
    port: String,
    baud: u32,
}

impl Flasher {
    /// Open `port` at `baud` and connect to the ROM bootloader.
    ///
    /// Two-step: first open the native serial port at the standard
    /// 115_200 sync baud, then hand it to `espflash::flasher::Flasher::connect`
    /// which performs the ROM-bootloader handshake, detects the chip, and
    /// (if `baud > 115_200`) re-negotiates to the requested baud.
    pub fn new(port: &str, baud: u32) -> Result<Self, FlashError> {
        // Step 1: open the native serial port. espflash::Flasher::connect
        // wants the platform's `Port` type (COMPort on Windows, TTYPort on
        // Unix) which `open_native()` returns.
        let serial = serialport::new(port, 115_200)
            .flow_control(FlowControl::None)
            .open_native()
            .map_err(|e| FlashError::OpenPort {
                port: port.to_owned(),
                baud,
                source: espflash::error::Error::from(e),
            })?;

        // Step 2: connect to the ROM bootloader. We don't know the VID/PID
        // here (caller passed only a port name), so synthesize an empty
        // UsbPortInfo — this matches what espflash's own CLI does for
        // PciPort / Unknown port types (see espflash/src/cli/mod.rs ~L321).
        let port_info = UsbPortInfo {
            vid: 0,
            pid: 0,
            serial_number: None,
            manufacturer: None,
            product: None,
        };

        // verify = true: espflash runs its per-write MD5 check so a corrupt
        // write is caught before reboot. The board has a single app partition
        // (no OTA rollback / recovery), so an unverified bad flash bricks it
        // until re-flashed over USB. The retired Python tool left this off.
        let inner = EspFlasher::connect(
            serial,
            port_info,
            Some(baud),
            true,  // use_stub — faster + supports erase_region
            true,  // verify (see note above)
            false, // skip — don't skip identical regions
            None,  // chip — autodetect via magic register
            ResetAfterOperation::default(),
            ResetBeforeOperation::default(),
        )
        .map_err(FlashError::Connect)?;

        Ok(Self {
            inner: Some(inner),
            port: port.to_owned(),
            baud,
        })
    }

    /// Read chip identity (model, revision, MAC, flash size) from the
    /// already-connected device.
    ///
    /// Thin wrapper over [`espflash::flasher::Flasher::device_info`] that
    /// returns a [`DeviceSummary`] of pre-formatted display strings — no
    /// `espflash` types leak through. Espflash auto-detects flash size
    /// during `new`; if detection fails it falls back to its 4 MB default
    /// (espflash logs a warning to its `log` facade).
    pub fn device_info(&mut self) -> Result<DeviceSummary, FlashError> {
        let f = self
            .inner
            .as_mut()
            .ok_or(FlashError::Connect(espflash::error::Error::FlashConnect))?;
        let info = f.device_info().map_err(FlashError::DeviceInfo)?;
        Ok(DeviceSummary {
            chip: format_chip(info.chip),
            revision: info
                .revision
                .map(|(maj, min)| format!("v{maj}.{min}"))
                .unwrap_or_else(|| "—".to_owned()),
            mac: info.mac_address,
            flash_size: format_flash_size(info.flash_size),
        })
    }

    /// Erase the entire flash.
    ///
    /// **Cancellation note:** the cancel flag is checked once before
    /// espflash's `erase_flash` is invoked. Once erase is in flight
    /// (~10 s on a 4 MB ESP32), it can NOT be interrupted — espflash
    /// 3.3.0's `erase_flash` doesn't accept progress callbacks.
    /// `EraseDone` will fire even if the user clicked Cancel mid-erase.
    /// Plan 7 UI should show "Cancelling…" and ignore the trailing
    /// `EraseDone`. See module doc on [`crate::progress`] for the full
    /// cancellation contract.
    pub fn erase(
        &mut self,
        progress_tx: Sender<FlashEvent>,
        cancel: Arc<AtomicBool>,
    ) -> Result<(), FlashError> {
        if cancel.load(Ordering::Relaxed) {
            return Err(FlashError::Cancelled);
        }
        // Swallow SendError — receiver dropped means UI is gone; we still
        // need to finish the erase op cleanly.
        let _ = progress_tx.send(FlashEvent::EraseStart);

        let f = self
            .inner
            .as_mut()
            .ok_or(FlashError::Connect(espflash::error::Error::FlashConnect))?;

        f.erase_flash().map_err(FlashError::Erase)?;

        let _ = progress_tx.send(FlashEvent::EraseDone);
        Ok(())
    }

    /// Write a list of partitions in a single espflash session. Validates the
    /// list (no overlapping address ranges) before touching hardware.
    ///
    /// **Why one session.** espflash 3.3's `write_bin_to_flash` is a thin
    /// wrapper around `write_bins_to_flash(&[single_segment], ...)`, which
    /// calls `target.finish(connection, reboot=true)` on the way out — and
    /// `reboot=true` on the ESP32 target invokes
    /// `connection.reset_after(use_stub)`, **hard-resetting the chip after
    /// every single partition**. Calling it in a loop, as we used to, would
    /// reset between the bootloader and the partition table; the chip then
    /// boots the newly-written bootloader (which doesn't speak the esptool
    /// protocol) and the next `FlashDeflBegin` times out after ~11 s with
    /// `Communication error while flashing device`. esptool.py avoids this
    /// by passing all (addr, bin) pairs to a single `write_flash` invocation;
    /// we mirror that by handing all segments to `write_bins_to_flash` once.
    pub fn write_partitions(
        &mut self,
        parts: Vec<Partition>,
        progress_tx: Sender<FlashEvent>,
        cancel: Arc<AtomicBool>,
    ) -> Result<(), FlashError> {
        // Pre-flight: validate before touching hardware. The unit test
        // `write_partitions_rejects_overlapping_input_before_touching_hardware`
        // exercises this path with `inner = None`.
        partition::validate(&parts)?;

        if cancel.load(Ordering::Relaxed) {
            return Err(FlashError::Cancelled);
        }

        let f = self
            .inner
            .as_mut()
            .ok_or(FlashError::Connect(espflash::error::Error::FlashConnect))?;

        // Per-partition byte sizes for the adapter — espflash's
        // ProgressCallbacks::init receives only an address, so the adapter
        // looks up which partition (and its true byte size) by address.
        let sizes: Vec<(u32, u64)> = parts
            .iter()
            .map(|p| (p.address, p.data.len() as u64))
            .collect();

        let segments: Vec<RomSegment<'_>> = parts
            .iter()
            .map(|p| RomSegment {
                addr: p.address,
                data: Cow::Borrowed(&p.data),
            })
            .collect();

        let mut adapter = CallbackAdapter {
            tx: progress_tx,
            cancel: cancel.clone(),
            sizes,
            current: None,
        };

        f.write_bins_to_flash(&segments, Some(&mut adapter))
            .map_err(|e| {
                // Best-effort: report the partition we were on when the error
                // fired. Falls back to partition 0 / address 0 if init never
                // ran (e.g. failure inside `begin`).
                let (index, address) = adapter
                    .current
                    .as_ref()
                    .map(|c| (c.index, c.address))
                    .unwrap_or((0, 0));
                FlashError::WritePartition {
                    index,
                    address,
                    source: e,
                }
            })?;

        Ok(())
    }

    /// Port name passed to `new` (for UI / logging).
    pub fn port(&self) -> &str {
        &self.port
    }

    /// Baud rate the flasher is using.
    pub fn baud(&self) -> u32 {
        self.baud
    }
}

impl Drop for Flasher {
    fn drop(&mut self) {
        // espflash::Flasher drops its serial port cleanly on Drop. Nothing
        // extra to do here — explicit `take` for symmetry with future
        // explicit-close additions.
        let _ = self.inner.take();
    }
}

/// Map espflash's `Chip` enum to the marketing name (uppercase, with the
/// family suffix dash — `ESP32-S3`, not the strum default `esp32s3`).
///
/// `Chip` is `#[non_exhaustive]`, so future variants fall back to an
/// uppercased Display rather than panicking — degrades gracefully when a
/// newer espflash adds chips before we update this table.
fn format_chip(chip: Chip) -> String {
    match chip {
        Chip::Esp32 => "ESP32".to_owned(),
        Chip::Esp32c2 => "ESP32-C2".to_owned(),
        Chip::Esp32c3 => "ESP32-C3".to_owned(),
        Chip::Esp32c6 => "ESP32-C6".to_owned(),
        Chip::Esp32h2 => "ESP32-H2".to_owned(),
        Chip::Esp32p4 => "ESP32-P4".to_owned(),
        Chip::Esp32s2 => "ESP32-S2".to_owned(),
        Chip::Esp32s3 => "ESP32-S3".to_owned(),
        other => format!("{other}").to_uppercase(),
    }
}

/// Format a `FlashSize` as a short human-readable string — `"4 MB"`,
/// `"256 KB"`, etc. espflash's Display would give `"4MB"` (SCREAMING_SNAKE
/// case on the variant name) which reads poorly in a UI label.
fn format_flash_size(size: FlashSize) -> String {
    let bytes = size.size();
    let mb = bytes / (1024 * 1024);
    if mb >= 1 {
        format!("{mb} MB")
    } else {
        format!("{} KB", bytes / 1024)
    }
}

/// Reboot the device into its firmware by pulsing the chip's EN (reset)
/// line via the USB-serial adapter's RTS control line.
///
/// **Why control lines, not a serial command.** The HoloCubic firmware's
/// remote protocol only understands the 2-byte `~U/~D/~L/~R/~H/~F` D-pad
/// codes — there is no "reboot" opcode, so a serial write can't reset it.
/// The auto-reset circuit every HoloCubic carrier board has (RTS → EN,
/// DTR → GPIO0) is the firmware-agnostic way to do it, and it's the only
/// way to bring the chip back out of the ROM bootloader it gets parked in
/// after [`Flasher::new`] (espflash's `DefaultReset` resets *into* the
/// bootloader to read chip info, and closing the port doesn't undo that).
///
/// **Sequence.** Mirrors espflash 3.3's `reset_after_flash` for non-JTAG
/// USB-serial bridges (CH340 / CP210x): hold DTR de-asserted (GPIO0 high →
/// normal application boot, *not* download mode), pulse RTS to drive EN low
/// then high. espflash documents that esptool's "ClassicReset" DTR+RTS
/// dance breaks on Windows (esp-rs/espflash#592); the RTS-only pulse works
/// on every platform, so that's what we use.
///
/// Opens its own short-lived port handle — there is no long-lived serial
/// connection to conflict with (`connect_device` drops its flasher; the
/// File Manager talks TCP/WiFi).
pub fn reboot(port: &str) -> Result<(), FlashError> {
    reboot_inner(port).map_err(|e| FlashError::Reboot {
        port: port.to_owned(),
        source: espflash::error::Error::from(e),
    })
}

/// Inner reboot that yields the raw `serialport::Error`; [`reboot`] wraps it.
/// Split out so the whole open-and-toggle sequence maps through one error
/// conversion instead of four.
fn reboot_inner(port: &str) -> Result<(), serialport::Error> {
    let mut sp = serialport::new(port, 115_200)
        .timeout(Duration::from_millis(500))
        .open_native()?;
    sp.write_data_terminal_ready(false)?; // GPIO0 = HIGH → boot application
    sp.write_request_to_send(true)?; // EN = LOW → chip held in reset
    std::thread::sleep(Duration::from_millis(100));
    sp.write_request_to_send(false)?; // EN = HIGH → chip boots firmware
    Ok(())
}

/// Adapter from espflash's `ProgressCallbacks` trait to our `mpsc` channel.
///
/// A single instance handles every partition in one `write_bins_to_flash`
/// call. `init(addr, num_chunks)` is the per-partition boundary: it fires
/// once at the start of each segment, telling us the address (we look up the
/// true byte size in `sizes`) and the zlib-compressed chunk count.
///
/// **Cancellation.** espflash's `ProgressCallbacks` methods don't return
/// `Result`, so the cancel flag can't abort an in-flight `write_bins_to_flash`
/// call — it can only silence further progress events (so the UI stops
/// reporting motion). Cancel set BEFORE `write_partitions` is called is still
/// honored as an early `FlashError::Cancelled` return.
///
/// **Unit translation.** espflash reports progress in zlib-compressed chunk
/// units (see `targets/flash_target/esp32.rs:194-225` in espflash 3.3): `init`
/// receives the number of compressed chunks, `update` receives a 1-based
/// chunk index. We translate to bytes using the partition's true raw size:
///   bytes_written = current * total_bytes / total_chunks  (clamped to total)
struct CallbackAdapter {
    tx: Sender<FlashEvent>,
    cancel: Arc<AtomicBool>,
    /// (address, total_bytes) per partition, in the caller's supplied order.
    /// `init(addr, _)` looks here by address to recover (index, byte size).
    sizes: Vec<(u32, u64)>,
    /// State of the partition currently being written. Set by `init`,
    /// cleared by `finish`. Read by `write_partitions` to populate the
    /// `FlashError::WritePartition` index/address on failure.
    current: Option<CurrentPartition>,
}

struct CurrentPartition {
    index: usize,
    address: u32,
    total_bytes: u64,
    total_chunks: u64,
}

impl CallbackAdapter {
    fn send(&self, evt: FlashEvent) {
        if self.cancel.load(Ordering::Relaxed) {
            return;
        }
        // Receiver may have been dropped (UI window closed) — swallow.
        let _ = self.tx.send(evt);
    }
}

impl ProgressCallbacks for CallbackAdapter {
    fn init(&mut self, addr: u32, total: usize) {
        // Look up which partition this address corresponds to and recover
        // its real byte size. Falls back to (0, 0) defensively if espflash
        // ever supplies an address we didn't hand it — currently impossible.
        let (index, total_bytes) = self
            .sizes
            .iter()
            .enumerate()
            .find(|(_, (a, _))| *a == addr)
            .map(|(i, (_, b))| (i, *b))
            .unwrap_or((0, 0));
        self.current = Some(CurrentPartition {
            index,
            address: addr,
            total_bytes,
            total_chunks: total as u64,
        });
        self.send(FlashEvent::PartitionStart { index, total_bytes });
    }

    fn update(&mut self, current: usize) {
        if let Some(cp) = self.current.as_ref() {
            let bytes_written = if cp.total_chunks == 0 {
                0
            } else {
                ((current as u64).saturating_mul(cp.total_bytes) / cp.total_chunks)
                    .min(cp.total_bytes)
            };
            self.send(FlashEvent::Progress {
                index: cp.index,
                bytes_written,
            });
        }
    }

    fn finish(&mut self) {
        if let Some(cp) = self.current.as_ref() {
            self.send(FlashEvent::PartitionDone { index: cp.index });
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::AtomicBool;
    use std::sync::mpsc::channel;
    use std::sync::Arc;

    #[test]
    fn write_partitions_rejects_overlapping_input_before_touching_hardware() {
        // Verify the validate() guard runs FIRST, before any espflash I/O.
        // Construct a Flasher with inner=None — write_partitions returns the
        // overlap error before reaching the inner.as_mut() unwrap.
        let mut flasher = Flasher {
            inner: None,
            port: "/dev/null".to_owned(),
            baud: 115_200,
        };
        let parts = vec![
            Partition {
                address: 0x1000,
                data: vec![0u8; 0x200],
            },
            Partition {
                address: 0x10ff,
                data: vec![0u8; 0x100],
            },
        ];
        let (tx, _rx) = channel();
        let cancel = Arc::new(AtomicBool::new(false));
        let err = flasher.write_partitions(parts, tx, cancel).unwrap_err();
        assert!(matches!(err, FlashError::OverlappingPartitions { .. }));
    }

    // --- CallbackAdapter tests ---
    //
    // These verify the adapter emits the right FlashEvent in response to each
    // ProgressCallbacks method and silences itself when the cancel flag is
    // tripped. The bridge between espflash and our mpsc channel is the most
    // failure-prone bit of glue in the crate; one test per method + one for
    // cancel-silencing is the minimum useful coverage.

    fn make_adapter(
        sizes: Vec<(u32, u64)>,
    ) -> (
        CallbackAdapter,
        std::sync::mpsc::Receiver<FlashEvent>,
        Arc<AtomicBool>,
    ) {
        let (tx, rx) = channel();
        let cancel = Arc::new(AtomicBool::new(false));
        let adapter = CallbackAdapter {
            tx,
            cancel: cancel.clone(),
            sizes,
            current: None,
        };
        (adapter, rx, cancel)
    }

    #[test]
    fn callback_init_emits_partition_start_with_real_byte_count() {
        // espflash hands us (addr, num_chunks); we look up the partition by
        // address and emit PartitionStart with its TRUE byte size.
        let (mut adapter, rx, _cancel) = make_adapter(vec![(0x1000, 18_528), (0x8000, 3_072)]);
        adapter.init(0x1000, 13);
        assert_eq!(
            rx.try_recv().unwrap(),
            FlashEvent::PartitionStart {
                index: 0,
                total_bytes: 18_528,
            }
        );
        adapter.init(0x8000, 1);
        assert_eq!(
            rx.try_recv().unwrap(),
            FlashEvent::PartitionStart {
                index: 1,
                total_bytes: 3_072,
            }
        );
    }

    #[test]
    fn callback_update_scales_chunk_index_to_bytes() {
        // 13 chunks, 18_528 bytes total → chunk 1 ≈ 1425 bytes, chunk 13
        // clamps to 18_528. Matches the real-world bootloader case from the
        // bug report.
        let (mut adapter, rx, _cancel) = make_adapter(vec![(0x1000, 18_528)]);
        adapter.init(0x1000, 13);
        let _ = rx.try_recv(); // PartitionStart
        adapter.update(1);
        assert_eq!(
            rx.try_recv().unwrap(),
            FlashEvent::Progress {
                index: 0,
                bytes_written: 1425, // 1 * 18528 / 13 = 1425
            }
        );
        adapter.update(13);
        assert_eq!(
            rx.try_recv().unwrap(),
            FlashEvent::Progress {
                index: 0,
                bytes_written: 18_528, // clamped to total
            }
        );
    }

    #[test]
    fn callback_update_before_init_is_silent() {
        // Defensive: update without prior init is a no-op (espflash always
        // calls init first; this guards a hypothetical contract change).
        let (mut adapter, rx, _cancel) = make_adapter(vec![(0x1000, 1000)]);
        adapter.update(5);
        assert!(rx.try_recv().is_err());
    }

    #[test]
    fn callback_finish_emits_partition_done_for_current_partition() {
        let (mut adapter, rx, _cancel) = make_adapter(vec![(0x1000, 1024)]);
        adapter.init(0x1000, 2);
        let _ = rx.try_recv(); // PartitionStart
        adapter.finish();
        assert_eq!(
            rx.try_recv().unwrap(),
            FlashEvent::PartitionDone { index: 0 }
        );
    }

    #[test]
    fn callback_init_with_unknown_address_falls_back_safely() {
        // espflash currently can't supply an unknown address (we hand it
        // every segment), but if the contract ever changes we want a
        // controlled fallback rather than a panic.
        let (mut adapter, rx, _cancel) = make_adapter(vec![(0x1000, 18_528)]);
        adapter.init(0xdead_beef, 5);
        assert_eq!(
            rx.try_recv().unwrap(),
            FlashEvent::PartitionStart {
                index: 0,
                total_bytes: 0,
            }
        );
    }

    #[test]
    fn format_chip_marketing_names() {
        assert_eq!(format_chip(Chip::Esp32), "ESP32");
        assert_eq!(format_chip(Chip::Esp32c3), "ESP32-C3");
        assert_eq!(format_chip(Chip::Esp32s3), "ESP32-S3");
        assert_eq!(format_chip(Chip::Esp32h2), "ESP32-H2");
    }

    #[test]
    fn format_flash_size_renders_mb_and_kb() {
        assert_eq!(format_flash_size(FlashSize::_256Kb), "256 KB");
        assert_eq!(format_flash_size(FlashSize::_512Kb), "512 KB");
        assert_eq!(format_flash_size(FlashSize::_4Mb), "4 MB");
        assert_eq!(format_flash_size(FlashSize::_16Mb), "16 MB");
    }

    #[test]
    fn reboot_on_nonexistent_port_returns_reboot_error() {
        // Can't exercise the control-line toggle without hardware, but the
        // open-failure path is the realistic error case and confirms the
        // error is wrapped as FlashError::Reboot (not a panic / wrong variant).
        let err = reboot("COM_DOES_NOT_EXIST_99999").unwrap_err();
        match err {
            FlashError::Reboot { port, .. } => assert_eq!(port, "COM_DOES_NOT_EXIST_99999"),
            other => panic!("expected FlashError::Reboot, got {other:?}"),
        }
    }

    #[test]
    fn callback_silenced_after_cancel_flag_set() {
        let (mut adapter, rx, cancel) = make_adapter(vec![(0x1000, 4096)]);
        cancel.store(true, Ordering::Relaxed);
        adapter.init(0x1000, 4);
        adapter.update(2);
        adapter.finish();
        // Receiver got nothing — adapter swallowed all events.
        assert!(rx.try_recv().is_err());
    }
}
