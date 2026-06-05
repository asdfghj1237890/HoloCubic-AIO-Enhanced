#!/usr/bin/env python3
"""Dump byte-exact converter goldens from the legacy Python tool.

Run from AIO_Tool/:
    cd AIO_Tool && uv run python ../AIO_Tool_rs/scripts/dump_converter_goldens.py

The script does two things:

1. (Re)generates the deterministic input fixtures under
   `../AIO_Tool_rs/crates/aio-converter/tests/fixtures/`.
2. Runs `convertor_core.Converter` over each (fixture × format × dither)
   case and writes the resulting `.bin` payload to
   `../AIO_Tool_rs/crates/aio-converter/tests/golden/<name>.<suffix>.bin`.

Skips INDEXED_* formats — Rust uses a hand-rolled median-cut quantizer
(BUGS.md B10), so byte parity with PIL's quantizer is not expected.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

# Importing the legacy modules requires running from AIO_Tool/.
sys.path.insert(0, str(Path.cwd()))

from PIL import Image  # noqa: E402

from util.convertor_core import Converter, _Const  # noqa: E402

FIXTURE_DIR = Path("..") / "AIO_Tool_rs" / "crates" / "aio-converter" / "tests" / "fixtures"
OUT_DIR = Path("..") / "AIO_Tool_rs" / "crates" / "aio-converter" / "tests" / "golden"


def make_fixtures() -> None:
    FIXTURE_DIR.mkdir(parents=True, exist_ok=True)

    # solid red 32x32 — flat opaque
    Image.new("RGBA", (32, 32), (255, 0, 0, 255)).save(FIXTURE_DIR / "solid_red_32x32.png")

    # checkerboard 8x8 — 2-color B/W
    img = Image.new("RGB", (8, 8), 0)
    px = img.load()
    for y in range(8):
        for x in range(8):
            px[x, y] = (255, 255, 255) if (x + y) % 2 == 0 else (0, 0, 0)
    img.save(FIXTURE_DIR / "checkerboard_8x8.png")

    # gradient 16x16 — linear R→B horizontally
    img = Image.new("RGB", (16, 16), 0)
    px = img.load()
    for y in range(16):
        for x in range(16):
            px[x, y] = (255 - 17 * x, 0, 17 * x)
    img.save(FIXTURE_DIR / "gradient_16x16.png")

    # photo 64x64 — synthesized realistic-looking byte distribution
    img = Image.new("RGB", (64, 64), 0)
    px = img.load()
    for y in range(64):
        for x in range(64):
            px[x, y] = ((x * 4) % 256, (y * 4) % 256, ((x + y) * 2) % 256)
    img.save(FIXTURE_DIR / "photo_64x64.jpg", quality=85)

    # alpha gradient 16x16 — vertical 0..255
    img = Image.new("RGBA", (16, 16), 0)
    px = img.load()
    for y in range(16):
        for x in range(16):
            px[x, y] = (128, 128, 128, 17 * y)
    img.save(FIXTURE_DIR / "alpha_gradient_16x16.png")

    # transparent circle 32x32 — opaque blue disc on transparent bg
    img = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    px = img.load()
    for y in range(32):
        for x in range(32):
            dx, dy = x - 15.5, y - 15.5
            if math.sqrt(dx * dx + dy * dy) < 12:
                px[x, y] = (0, 128, 255, 255)
    img.save(FIXTURE_DIR / "transparent_circle_32x32.png")

    print(f"fixtures written to {FIXTURE_DIR}")


# Format suffix → (fixture, cf const, dither, out-name-suffix)
# Skip INDEXED_* formats per BUGS.md B10.
CASES = [
    ("solid_red_32x32.png", _Const.CF_TRUE_COLOR_565, False, "rgb565"),
    ("solid_red_32x32.png", _Const.CF_TRUE_COLOR_888, False, "rgb888"),
    ("solid_red_32x32.png", _Const.CF_TRUE_COLOR_332, False, "rgb332"),
    ("gradient_16x16.png", _Const.CF_TRUE_COLOR_565, True, "rgb565.dither"),
    ("gradient_16x16.png", _Const.CF_TRUE_COLOR_565, False, "rgb565.nodither"),
    ("photo_64x64.jpg", _Const.CF_TRUE_COLOR_565, True, "rgb565.dither"),
    ("photo_64x64.jpg", _Const.CF_TRUE_COLOR_565_SWAP, False, "rgb565_swap"),
    ("alpha_gradient_16x16.png", _Const.CF_ALPHA_8_BIT, False, "alpha8"),
    ("alpha_gradient_16x16.png", _Const.CF_ALPHA_4_BIT, False, "alpha4"),
    ("transparent_circle_32x32.png", _Const.CF_ALPHA_1_BIT, False, "alpha1"),
]


def dump_goldens() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for fixture, cf, dither, suffix in CASES:
        src = FIXTURE_DIR / fixture
        if not src.exists():
            print(f"SKIP {fixture} (not found)")
            continue
        # NOTE: Converter.__init__ ALREADY calls self.convert(self.cf, alpha)
        # internally (convertor_core.py:127). Re-calling .convert() here would
        # replay over dirty r_earr/g_earr/b_earr state and produce different
        # bytes than the GUI tool. The GUI path (page/images_converter.py:340)
        # never calls .convert() again — it goes straight to get_bin_file.
        # Mirror that.
        conv = Converter(str(src), dith=dither, cf=cf)
        stem = fixture.rsplit(".", 1)[0]
        out_name = f"{stem}.{suffix}.bin"
        out = OUT_DIR / out_name
        # get_bin_file always writes `<out_name>.bin` based on the input
        # stem. Capture its returned bytes and rewrite under the explicit
        # case-suffixed name we want.
        bin_bytes = conv.get_bin_file(cf=cf, outpath=str(OUT_DIR))
        out.write_bytes(bin_bytes)
        # Clean up the convertor's default-named output if it lives at a
        # different path under OUT_DIR.
        default_name = OUT_DIR / f"{stem}.bin"
        if default_name.exists() and default_name != out:
            default_name.unlink()
        print(f"wrote {out} ({len(bin_bytes)} bytes)")


def main() -> None:
    make_fixtures()
    dump_goldens()


if __name__ == "__main__":
    main()
