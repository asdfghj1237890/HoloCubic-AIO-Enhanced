#!/usr/bin/env python3
"""Dump byte-exact i18n translations for golden comparison.

Run from the AIO_Tool/ directory so `util.i18n` imports resolve:

    cd AIO_Tool && uv run python ../AIO_Tool_rs/scripts/dump_i18n_goldens.py

Writes raw UTF-8 (no trailing newline) into
../AIO_Tool_rs/crates/aio-i18n/tests/golden/<key>.<locale>.txt.

The Rust integration test loads each file with include_str! and asserts
t(key) on the matching language returns the identical string.
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path.cwd()))

from util.i18n import get_i18n  # noqa: E402

OUT_DIR = Path("..") / "AIO_Tool_rs" / "crates" / "aio-i18n" / "tests" / "golden"
OUT_DIR.mkdir(parents=True, exist_ok=True)

# Strategic sample covering short ASCII, multi-line, embedded CJK, and
# bidirectional mixed scripts. All 9 keys were verified to exist in the
# corresponding JSON file via a presence check before this script was written.
# Format: (key, [locales])
SAMPLES: list[tuple[str, list[str]]] = [
    ("tab_help", ["en_US", "zh_CN", "zh_TW"]),
    ("app_title", ["en_US", "zh_TW"]),
    ("ok", ["en_US"]),
    ("port_number", ["en_US"]),
    ("flash_firmware", ["en_US", "zh_CN"]),
    ("language_changed", ["en_US", "zh_CN"]),
    ("language_label", ["en_US", "zh_TW"]),
    ("help_info", ["zh_CN"]),
    ("image_converter_info", ["zh_TW"]),
]


def main() -> None:
    i = get_i18n()
    for key, locales in SAMPLES:
        for locale in locales:
            i.set_language(locale)
            value = i.t(key)
            out = OUT_DIR / f"{key}.{locale}.txt"
            # Write raw bytes so Python's text-mode newline translation does
            # not turn embedded "\n" into "\r\n" on Windows. The Rust test
            # uses include_str! which returns the file bytes verbatim and
            # compares against I18n.t() which preserves "\n".
            out.write_bytes(value.encode("utf-8"))
            print(f"wrote {out} ({len(value.encode('utf-8'))} bytes)")


if __name__ == "__main__":
    main()
