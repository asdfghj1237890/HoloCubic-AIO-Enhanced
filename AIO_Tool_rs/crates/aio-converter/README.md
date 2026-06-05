# aio-converter

HoloCubic AIO LVGL image converter — pure-Rust port of W-Mai's
`lvgl_image_converter` Python tool, packaged behind a simple Rust API.

## Design notes

- **Single decode, multiple encodes**: `Converter::new(bytes, fmt, dither)`
  decodes the input PNG/JPG/BMP once via the `image` crate;
  `encode_bin` / `encode_c_array` dispatch by format without re-decoding.
- **Byte-for-byte parity with Python** for **8 of 10** golden cases.
  RGB332/565/565_swap/888 + ALPHA 1/4/8 all match `convertor_core.py`
  exactly, including Floyd-Steinberg dithering output. The 2 ignored
  cases are JPEG-sourced — PIL and `image 0.25.6` decode JPEG with
  small (1 LSB) differences at some pixels, which compounds through
  dither + quantize and can shift downstream bytes. Not a converter
  bug; an inherent JPEG-decoder cross-language quirk.
- **Indexed formats diverge** from Python (BUGS.md B10): we use a
  hand-rolled simple median-cut, Python uses PIL's. Visually
  equivalent, byte-for-byte different.
- **LVGL header**: 4-byte LE u32 — `lv_cf | (w << 10) | (h << 21)`.
  Dimensions over 2047 px are rejected with `DimensionTooLarge` rather
  than silently overflowing (improvement over Python).
- **RGB888 emits 4 bytes per pixel** (B, G, R, A) — the legacy Python
  unconditionally appends alpha for `CF_TRUE_COLOR_888`. Plan 5's task
  description initially had this wrong; verified against
  `convertor_core.py:427-431` and fixed mid-implementation.
- **Cancellation**: row-granularity, same pattern as Plan 4's
  `aio-flasher` — `Arc<AtomicBool>` flag checked at every row boundary.
- **No internal threads**: caller spawns a `std::thread::spawn`,
  forwards `mpsc::Sender<ConvertEvent>` for progress.

## Attribution

The conversion algorithms in `src/encoders/` are ported from
[lvgl_image_converter](https://github.com/W-Mai/lvgl_image_converter)
by W-Mai, MIT licensed.

## Testing

```sh
cargo test -p aio-converter
```

Three layers:

- Unit tests in `src/{format,header,image_input,dither,encoders/*,converter}.rs`
  cover individual pieces (~65 tests).
- Golden snapshots in `tests/golden/*.bin` are byte-exact captures of
  the legacy Python tool's output (10 tests, 2 marked `#[ignore]` per
  JPEG quirk above). Regenerate with:

  ```sh
  cd AIO_Tool && uv run python ../AIO_Tool_rs/scripts/dump_converter_goldens.py
  ```

  `tests/golden/.gitattributes` keeps `.bin` as binary (no CRLF).

To run the ignored JPEG cases (will fail by design):

```sh
cargo test -p aio-converter --test goldens -- --ignored
```

## When Plan 9's Image Converter tab calls this

Plan 9 spawns a background thread per conversion:

```rust
use aio_converter::{ColorFormat, Converter, ConvertEvent};
use std::sync::{atomic::AtomicBool, mpsc, Arc};
use std::thread;

let cancel = Arc::new(AtomicBool::new(false));
let (tx, rx) = mpsc::channel();
let cancel_for_op = cancel.clone();
let conv = Converter::new(&png_bytes, ColorFormat::Rgb565, true)?;
thread::spawn(move || {
    let _ = conv.encode_bin(Some(tx), Some(cancel_for_op));
});

// UI thread:
//   - iterates rx for ConvertEvent::{Start, Progress, Done}
//   - sets cancel.store(true, Ordering::Relaxed) on Cancel click
```

## Bug ledger

See `BUGS.md` for documented Python-vs-Rust behavioral differences
(currently B7–B10).
