#!/usr/bin/env python3
"""Dump byte-exact wire goldens from the legacy Python tool's encode paths.

Run from the AIO_Tool/ directory so the `util.*` imports resolve:

    cd AIO_Tool && uv run python ../AIO_Tool_rs/scripts/dump_goldens.py

Writes hex strings into ../AIO_Tool_rs/crates/aio-protocol/tests/golden/.
The Rust tests load these via include_str!.
"""
from __future__ import annotations

import sys
from pathlib import Path

# Importing the legacy modules requires running from AIO_Tool/.
sys.path.insert(0, str(Path.cwd()))

from util.massagehead import MT, AT, VT, SettingMsg  # noqa: E402
from util.file_info import (  # noqa: E402
    DirCreate, DirRemove, DirRename, DirList,
    FileCreate, FileWrite, FileRead, FileRemove, FileRename, FileGetInfo,
)

OUT_DIR = Path("..") / "AIO_Tool_rs" / "crates" / "aio-protocol" / "tests" / "golden"
OUT_DIR.mkdir(parents=True, exist_ok=True)


def dump(name: str, raw: bytes) -> None:
    p = OUT_DIR / f"{name}.hex"
    p.write_text(raw.hex(), encoding="ascii")
    print(f"wrote {p} ({len(raw)} bytes)")


def main() -> None:
    # --- SettingMsg ---
    m = SettingMsg(AT.AT_SETTING_GET)
    m.prefs_name = b"sys"
    m.key = b"ssid"
    dump("setting_get_sys_ssid", m.encode())

    m = SettingMsg(AT.AT_SETTING_SET)
    m.prefs_name = b"zhixin"
    m.key = b"cityname"
    m.type = b"3"  # VALUE_TYPE_STRING as string-of-digit
    m.value = b"Taipei"
    dump("setting_set_zhixin_cityname", m.encode())

    # --- Dir messages ---
    dump("dir_create_root", DirCreate("/test").encode())
    dump("dir_remove_old", DirRemove("/old").encode())
    dump("dir_rename_a_to_b", DirRename("/a", "/b").encode())
    dump("dir_list_sd", DirList("/sd", "").encode())

    # --- File messages ---
    dump("file_create_foo", FileCreate("/sd/foo.bin", 0x1234).encode())
    dump("file_write_hello", FileWrite("hello").encode())
    dump("file_read_log", FileRead("/sd/log.txt").encode())
    dump("file_remove_bad", FileRemove("/sd/bad.bin").encode())
    # B1 / B2 preserved bugs:
    dump("file_rename_foo", FileRename("/sd/foo.txt").encode())
    # FileGetInfo's `dir_info` attribute is never declared. Patch on the instance to
    # avoid runtime AttributeError when computing encode field list.
    fgi = FileGetInfo("/sd/foo.txt")
    fgi.dir_info = b""  # PRESERVED-BUG-FROM-V2 B2 workaround for the dumper
    dump("file_get_info_foo", fgi.encode())


if __name__ == "__main__":
    main()
