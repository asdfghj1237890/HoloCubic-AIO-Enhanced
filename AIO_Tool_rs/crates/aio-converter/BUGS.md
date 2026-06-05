# Preserved bugs / quirks ledger — aio-converter

| ID | Site | Description | Source |
|----|------|-------------|--------|
| B7 | `header.rs::encode_lvgl_header` | All four `CF_TRUE_COLOR_*` (332/565/565_swap/888) emit `lv_cf=4` in the binary header. The actual pixel encoding is implied by file size and consumer knowledge, NOT by the header. Required for LVGL parser compatibility — preserved as-is. | `convertor_core.py:375-387` |
| B8 | (omitted) | Python's `force_update` helper has unreachable dead `elif` (`check_res == 0` after `if check_res:`). Not called from `convertor_core` itself; Rust port skips entirely. | `convertor_core.py:34-40` |
| B9 | (omitted) | Python's `_conv_px` re-fetches `cx = self.img.getpixel((x, y))` then discards it. Dead code, no output effect. Rust drops. | `convertor_core.py:407` |
| B10 | `encoders/indexed.rs::quantize_median_cut` | Plan 5 chose a standalone simple median-cut over PIL's exact quantizer to avoid hand-rolling PIL's algorithm. Indexed output pixel bytes WILL differ from `convertor_core.py` for the same input, but the palette layout (`R, G, B, 0xFF` × N + padding), bit-packing direction (MSB-first), and structure all match. Plan 9 UI users won't notice unless they bit-compare outputs. Goldens for indexed formats will be added in a later patch once a hardware-validated reference is available. | `convertor_core.py:141-174,464-496` |
