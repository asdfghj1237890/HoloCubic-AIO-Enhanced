# HoloCubic AIO Firmware

**Language / 语言 / 語言:** [English](README.md) | [简体中文](README.zh-CN.md) | [繁體中文](README.zh-TW.md)

This is the firmware project for HoloCubic_AIO, built with PlatformIO for ESP32 (PICO-D4).

## Build Requirements

- [PlatformIO Core](https://platformio.org/) or [PlatformIO IDE](https://platformio.org/platformio-ide)
- Platform: pioarduino `platform-espressif32 55.03.39`
- Framework: Arduino-ESP32 `3.3.9` on ESP-IDF `5.5.4`
- UI runtime: LVGL `9.5.0`
- JSON runtime: ArduinoJson `7.4.3`

## Current Firmware Baseline (2026-06)

This firmware has moved past the old PlatformIO espressif32 `~3.5.0` / LVGL 8 / ArduinoJson 6 baseline. The current release path uses the pioarduino ESP32 platform because official PlatformIO espressif32 did not yet provide Arduino-ESP32 3.x with ESP-IDF 5.5.x in the same package line.

| Layer | Current |
|---|---|
| Board | ESP32-PICO-D4 (`pico32`) |
| PlatformIO platform | `pioarduino/platform-espressif32 55.03.39` |
| Arduino core | `framework-arduinoespressif32 3.3.9` |
| ESP-IDF libs | `5.5.4` |
| LVGL | `9.5.0` |
| ArduinoJson | `7.4.3` |

Recent firmware hardening includes the LVGL 9 boot tick fix, ESP32 core 3.x RGB startup fix, Stock config parser bounds checks, Stock long-company-name header truncation, 2048 input/rendering fixes, and responsive CSS for the WebServer Glass UI.

## Build Instructions

### Using Command Line

```bash
# Navigate to the firmware directory
cd AIO_Firmware_PIO

# Build the release version (default)
pio run
# or, without a global PlatformIO install:
uvx platformio run

# Build with specific environment
pio run -e HoloCubic_AIO_Releases

# Build debug version
pio run -e HoloCubic_AIO_Debug

# Clean build files
pio run -t clean

# Build and upload to device
pio run -t upload
```

### Using VS Code with PlatformIO Extension

1. Open VS Code
2. Install the **PlatformIO IDE** extension
3. Open the `AIO_Firmware_PIO` folder
4. Click the PlatformIO icon in the sidebar
5. Under **Project Tasks** → **HoloCubic_AIO_Releases** → click **Build**

## Build Environments

The project has two build configurations:

### HoloCubic_AIO_Releases (Default)
- Optimized for production use
- Located in `platformio.ini` as `[env:HoloCubic_AIO_Releases]`

### HoloCubic_AIO_Debug
- Debug build with logging enabled
- Optimization level: -O0
- Arduino HAL log level: 1

## Output Files

After successful compilation, binary files are located in:

```
.pio/build/HoloCubic_AIO_Releases/
```

Key files:
- **firmware.bin** - Main application firmware
- **bootloader.bin** - ESP32 bootloader
- **partitions.bin** - Partition table

## Flashing to ESP32

### Required Files and Flash Addresses

| File | Flash Address | Description |
|------|--------------|-------------|
| `bootloader.bin` | 0x1000 | ESP32 bootloader |
| `partitions.bin` | 0x8000 | Partition table |
| `boot_app0.bin` | 0xe000 | Boot application selector |
| `firmware.bin` | 0x10000 | Main firmware |

### Flash Using PlatformIO

```bash
# Upload via PlatformIO (easiest method)
pio run -t upload

# Specify custom port
pio run -t upload --upload-port COM5
```

### Flash Using esptool

```bash
# Windows (PowerShell, esptool v5)
$env:PYTHONIOENCODING='utf-8'
[Console]::OutputEncoding=[System.Text.Encoding]::UTF8
& "$env:USERPROFILE\.platformio\penv\Scripts\esptool.exe" `
  --chip esp32 --port COM5 --baud 115200 --connect-attempts 3 `
  --before default-reset --after hard-reset write-flash -z `
  --flash-mode dio --flash-freq 80m --flash-size detect `
  0x1000 .pio\build\HoloCubic_AIO_Releases\bootloader.bin `
  0x8000 .pio\build\HoloCubic_AIO_Releases\partitions.bin `
  0xe000 "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin" `
  0x10000 .pio\build\HoloCubic_AIO_Releases\firmware.bin

# Linux/macOS (adjust serial port and boot_app0 path if needed)
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 115200 --connect-attempts 3 \
  --before default-reset --after hard-reset write-flash -z \
  --flash-mode dio --flash-freq 80m --flash-size detect \
  0x1000 .pio/build/HoloCubic_AIO_Releases/bootloader.bin \
  0x8000 .pio/build/HoloCubic_AIO_Releases/partitions.bin \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/HoloCubic_AIO_Releases/firmware.bin
```

**Note:** Change `COM5` to your actual serial port (e.g., `COM3`, `/dev/ttyUSB0`, `/dev/cu.usbserial-*`). On Windows, set UTF-8 output before esptool v5; otherwise the progress bar can fail under the default CP950 console encoding.

### Erase Flash (if needed)

```bash
esptool.py --chip esp32 --port COM5 erase-flash
```

## Serial Monitor

To view serial output:

```bash
# Using PlatformIO
pio device monitor

# With specific baud rate
pio device monitor -b 115200
```

For long boot/crash captures on Windows, `uvx --from pyserial python` is preferred because timed-out PlatformIO monitors can leave COM ports held open. A healthy post-flash smoke log should show `AIO (All in one) version ...`, `Initialization MPU6050 success.`, and no repeated `rst:0xc` / Guru Meditation loop for at least 45-70 seconds.

Default monitor settings:
- Baud rate: 115200
- Filter: esp32_exception_decoder

## Board Configuration

- **Board:** ESP32 PICO-D4 (`pico32`)
- **CPU Frequency:** 240 MHz
- **Flash Frequency:** 80 MHz
- **Flash Mode:** QIO in the PlatformIO board config; the conservative manual esptool command above uses DIO and has been validated on ESP32-PICO-D4 + CH9102
- **Upload Speed:** 921600 baud
- **Partition Scheme:** `partitions-no-ota.csv` (No OTA updates)

## Host-Side Tests

```bash
# Lean firmware logic tests
pio test -e native_unit

# ESP32FtpServer command/auth/transfer harness
pio test -e native_ftp

# SDL2 + LVGL GUI regression harness
cd ../lv_simulater_platformio
pio run -e native_test
./.pio/build/native_test/program --scenario ../test/scenarios/stockmarket/long_company_name.scn --headless
```

Run PlatformIO tests sequentially when they share the same `.pio/build` tree. Running multiple PlatformIO test/build commands in parallel can corrupt the build cache on Windows/WSL.

## Project Structure

```
AIO_Firmware_PIO/
├── src/                    # Source code
│   ├── HoloCubic_AIO.cpp  # Main application
│   ├── app/               # Application modules
│   ├── driver/            # Hardware drivers
│   ├── sys/               # System utilities
│   └── resource/          # Embedded resources
├── lib/                   # Project libraries
├── include/               # Header files
├── platformio.ini         # PlatformIO configuration
├── partitions-no-ota.csv  # Partition table definition
└── README.md             # This file
```

## Build Flags

Common build flags (defined in `platformio.ini`):
- `-fPIC` - Position independent code
- `-Wreturn-type` - Warn about return type issues
- `-Werror=return-type` - Treat return type warnings as errors

## Troubleshooting

### Build Fails
- Ensure PlatformIO is properly installed: `pio --version`
- Clean build files: `pio run -t clean`
- Update platform: `pio platform update espressif32`

### Upload Fails
- Check if device is connected: `pio device list`
- Verify upload port in `platformio.ini` (line 33)
- Try holding the BOOT button during upload
- Reduce upload speed: change `upload_speed = 921600` to `115200`

### Serial Monitor Shows Garbage
- Verify baud rate matches (115200)
- Check that ESP32 is properly powered
- Try different USB cable or port

## Available Baud Rates

Supported upload/monitor baud rates:
- 115200
- 230400
- 460800
- 576000
- 921600
- 1152000

## Additional Resources

- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [HoloCubic_AIO Main Repository](https://github.com/ClimbSnail/HoloCubic_AIO)

## License

See the LICENSE file in the root directory of this repository.

