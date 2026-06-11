# AGENTS.md

Local operational notes for coding agents. See `CLAUDE.md` for the broader repo guide.

## Firmware Flashing On This Machine

Known local test device: COM5, CH9102 (`USB VID:PID=1A86:55D4`), ESP32-PICO-D4.

Build firmware:

```powershell
cd AIO_Firmware_PIO
uvx platformio run -e HoloCubic_AIO_Releases
```

Flash with explicit esptool on Windows. Set UTF-8 output first; otherwise esptool v5's progress bar can crash under the default CP950 console encoding.

```powershell
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
```

If COM5 is busy after a timed-out monitor or upload, inspect stale serial users and stop only the relevant PlatformIO/esptool children:

```powershell
Get-CimInstance Win32_Process |
  Where-Object { $_.CommandLine -match 'platformio|COM5|esptool|device monitor' } |
  Select-Object ProcessId,Name,CommandLine
```

For serial logs, prefer `uvx --from pyserial python` over `platformio device monitor`; PlatformIO monitor can leave COM5 held after a timeout. A useful post-flash smoke check captures 45-70s and confirms `AIO (All in one) version ...`, `Initialization MPU6050 success.`, and no repeated `rst:0xc` / Guru Meditation loop.
