#!/usr/bin/env python3
"""Generate a minimal ELF64 x86-64 with many ret-terminated pure functions in .text.

This gives the morph bench hundreds of *real ELF* functions (machine code inside
a valid ELF), independent of host cross-compiler availability.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path


def emit_u32(v: int) -> bytes:
    return struct.pack("<I", v & 0xFFFFFFFF)


def synth_one(i: int) -> bytes:
    kind = i % 7
    imm = (i * 0x9E3779B9) & 0xFF
    if kind == 0:
        return b"\xB8" + emit_u32(imm) + b"\xC3"
    if kind == 1:
        return b"\x48\x31\xC0\xB8" + emit_u32(imm & 7) + b"\xC3"
    if kind == 2:
        n = imm % 6
        body = b"\x48\x31\xC0" + b"\x48\xFF\xC0" * n + b"\xC3"
        return body
    if kind == 3:
        v = imm if imm else 1
        return b"\xB8" + emit_u32(v) + b"\x50\x58\xC3"
    if kind == 4:
        return b"\x48\x31\xC0\xEB\x03\x90\x90\x90\xC3"
    if kind == 5:
        return b"\x48\x31\xC0\xB8\x01\x00\x00\x00\x90\x90\xC3"
    return b"\x48\x31\xC0\xB8" + emit_u32(imm & 15) + b"\x48\xFF\xC0\xC3"


def build_text(n_funcs: int) -> bytes:
    chunks = []
    for i in range(n_funcs):
        chunks.append(synth_one(i))
        # pad to 4-byte align between functions
        while sum(len(c) for c in chunks) % 4:
            chunks.append(b"\x90")
    text = b"".join(chunks)
    # entry: xor eax,eax; ret
    entry = b"\x48\x31\xC0\xC3"
    return entry + text


def build_elf(text: bytes) -> bytes:
    # Minimal ELF64 ET_EXEC, one PT_LOAD RX covering file, e_entry -> text
    # Layout:
    #  [Ehdr 64]
    #  [Phdr 56]
    #  [pad to 0x1000 optional — keep small: load at 0x400000]
    # We put code immediately after phdr for a compact file.
    ehdr_size = 64
    phdr_size = 56
    phoff = ehdr_size
    code_off = ehdr_size + phdr_size
    # Align code_off to 16
    pad = (16 - (code_off % 16)) % 16
    code_off += pad
    vaddr_base = 0x400000
    entry = vaddr_base + code_off

    e_ident = bytearray(16)
    e_ident[0:4] = b"\x7fELF"
    e_ident[4] = 2  # ELFCLASS64
    e_ident[5] = 1  # ELFDATA2LSB
    e_ident[6] = 1  # EV_CURRENT

    ehdr = struct.pack(
        "<16sHHIQQQIHHHHHH",
        bytes(e_ident),
        2,  # ET_EXEC
        0x3E,  # EM_X86_64
        1,  # EV_CURRENT
        entry,
        phoff,
        0,  # shoff
        0,  # flags
        ehdr_size,
        phdr_size,
        1,  # phnum
        0,  # shentsize
        0,  # shnum
        0,  # shstrndx
    )
    assert len(ehdr) == 64

    filesz = code_off + len(text)
    memsz = filesz
    phdr = struct.pack(
        "<IIQQQQQQ",
        1,  # PT_LOAD
        5,  # PF_R|PF_X
        0,  # p_offset
        vaddr_base,  # p_vaddr
        vaddr_base,  # p_paddr
        filesz,
        memsz,
        0x1000,  # align
    )
    assert len(phdr) == 56

    out = bytearray()
    out += ehdr
    out += phdr
    out += b"\x00" * pad
    out += text
    return bytes(out)


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    out_path = root / "corpus" / "real_corpus.elf"
    n = 400
    if len(sys.argv) > 1:
        n = int(sys.argv[1])
    out_path.parent.mkdir(parents=True, exist_ok=True)
    text = build_text(n)
    elf = build_elf(text)
    out_path.write_bytes(elf)
    print(f"wrote {out_path} ({len(elf)} bytes, ~{n} funcs + entry)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
