# Preserved bugs / quirks ledger — aio-tool

| ID | Site | Description | Source |
|----|------|-------------|--------|
| (none) | | Plan 6 introduces no preserved-from-Python bugs. The legacy `download_debug.py` had three behaviors Plan 6 actively improves on (B11 ctypes async_raise → cooperative cancel; B12 silent "params 错误" → visible error log; B13 fake progress → real espflash progress). None are byte-format bugs. | |
