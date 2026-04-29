# Release boot + partition binaries

Three small ESP32 binaries that complement `firmware.bin` for a from-scratch flash.
They are bundled into every `vX.Y.Z` GitHub Release alongside `HoloCubic_AIO_firmware_<ver>.bin`
and `HoloCubic_AIO_Tool_<ver>.exe` so end users can flash a blank chip without
needing to dig through a PlatformIO install.

| File | Flash address | What it is |
|---|---|---|
| `bootloader_qio_80m.bin` | `0x1000` | ESP32 second-stage bootloader (QIO @ 80 MHz) |
| `partitions.bin` | `0x8000` | **Custom** partition table for HoloCubic AIO firmware |
| `boot_app0.bin` | `0xe000` | OTA boot selector (selects app0 vs app1) |
| `firmware.bin` | `0x10000` | Application code (built per release by `release.yml`) |

## Why a custom `partitions.bin`?

The HoloCubic AIO firmware uses a custom partition layout (large SPIFFS for
fonts/icons, OTA dual-app slots). `AIO_Firmware_PIO/partitions/` contains the
source `.csv` files; `partitions.bin` here is the compiled binary checked in for
release packaging. **Don't replace this with a default partitions table** — the
firmware will boot but lose access to its data partition.

The other two bin files are stock ESP32 framework artifacts and match those
under `~/.platformio/packages/framework-arduinoespressif32/`.

## Flashing

The simplest path is the AIO Tool's "韌體刷寫" (Firmware flash) tab — drop all
4 binaries in their addresses and press 刷寫固件. Manual `esptool` invocation:

```bash
esptool.py --port COM7 --baud 921600 write_flash -fm dio -fs 4MB \
    0x1000   bootloader_qio_80m.bin \
    0x8000   partitions.bin \
    0xe000   boot_app0.bin \
    0x10000  HoloCubic_AIO_firmware_<ver>.bin
```
