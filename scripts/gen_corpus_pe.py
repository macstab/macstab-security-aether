#!/usr/bin/env python3
"""Minimal PE32+ with .text of pure x86-64 functions (multi-format extract)."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_corpus_elf import build_text  # type: ignore


def align(n: int, a: int) -> int:
    return (n + a - 1) & ~(a - 1)


def build_pe(text: bytes) -> bytes:
    file_align = 0x200
    sect_align = 0x1000
    size_headers = 0x400
    code_size = align(len(text), file_align)
    image_size = align(0x1000 + code_size, sect_align)
    image_base = 0x140000000

    dos = bytearray(128)
    dos[0:2] = b"MZ"
    struct.pack_into("<I", dos, 0x3C, 128)

    coff = struct.pack(
        "<HHIIIHH",
        0x8664,  # machine
        1,  # sections
        0,
        0,
        0,
        240,  # optional header size
        0x22,  # characteristics
    )

    opt = bytearray(240)
    struct.pack_into("<H", opt, 0, 0x20B)  # PE32+
    struct.pack_into("<I", opt, 4, code_size)
    struct.pack_into("<I", opt, 16, 0x1000)  # entry RVA
    struct.pack_into("<I", opt, 20, 0x1000)  # base of code
    struct.pack_into("<Q", opt, 24, image_base)
    struct.pack_into("<I", opt, 32, sect_align)
    struct.pack_into("<I", opt, 36, file_align)
    struct.pack_into("<HH", opt, 40, 6, 0)
    struct.pack_into("<HH", opt, 48, 6, 0)
    struct.pack_into("<I", opt, 56, image_size)
    struct.pack_into("<I", opt, 60, size_headers)
    struct.pack_into("<H", opt, 68, 3)  # console
    struct.pack_into("<H", opt, 70, 0x160)
    struct.pack_into("<Q", opt, 72, 0x100000)
    struct.pack_into("<Q", opt, 80, 0x1000)
    struct.pack_into("<Q", opt, 88, 0x100000)
    struct.pack_into("<Q", opt, 96, 0x1000)
    struct.pack_into("<I", opt, 108, 16)  # data directories

    sec = bytearray(40)
    sec[0:5] = b".text"
    struct.pack_into("<I", sec, 8, len(text))
    struct.pack_into("<I", sec, 12, 0x1000)
    struct.pack_into("<I", sec, 16, code_size)
    struct.pack_into("<I", sec, 20, size_headers)
    struct.pack_into("<I", sec, 36, 0x60000020)

    nt = b"PE\0\0" + coff + bytes(opt) + bytes(sec)
    body = bytearray(dos) + bytearray(nt)
    if len(body) > size_headers:
        size_headers = align(len(body), file_align)
        struct.pack_into("<I", opt, 60, size_headers)
        struct.pack_into("<I", sec, 20, size_headers)
        nt = b"PE\0\0" + coff + bytes(opt) + bytes(sec)
        body = bytearray(dos) + bytearray(nt)
    body.extend(b"\x00" * (size_headers - len(body)))
    body.extend(text)
    body.extend(b"\x00" * (code_size - len(text)))
    return bytes(body)


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    pe = build_pe(build_text(n))
    out = root / "corpus" / "real_corpus.pe"
    out.write_bytes(pe)
    print(f"wrote {out} ({len(pe)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
