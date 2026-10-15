#!/usr/bin/env python3
"""
Strips trailing non-loaded ELF section headers (sstrip) and pads the executable
to the exact required byte size while verifying ELF integrity.
"""
import os
import struct
import sys


def sstrip_and_pad(in_path: str, out_path: str, target_size: int):
    with open(in_path, "rb") as f:
        data = bytearray(f.read())

    if data[:4] != b"\x7fELF" or data[4] != 2:
        raise ValueError(f"{in_path} is not a valid 64-bit ELF file")

    e_phoff = struct.unpack_from("<Q", data, 32)[0]
    e_phentsize = struct.unpack_from("<H", data, 54)[0]
    e_phnum = struct.unpack_from("<H", data, 56)[0]

    max_end = 0
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = (
            struct.unpack_from("<IIQQQQQQ", data, off)
        )
        if p_filesz > 0:
            max_end = max(max_end, p_offset + p_filesz)

    # Zero out section header table fields in ELF64 header
    struct.pack_into("<Q", data, 40, 0)  # e_shoff = 0
    struct.pack_into("<H", data, 58, 0)  # e_shentsize = 0
    struct.pack_into("<H", data, 60, 0)  # e_shnum = 0
    struct.pack_into("<H", data, 62, 0)  # e_shstrndx = 0

    stripped = data[:max_end]
    raw_len = len(stripped)
    if raw_len > target_size:
        raise RuntimeError(
            f"{out_path}: stripped ELF size ({raw_len} bytes) exceeds target {target_size} bytes by {raw_len - target_size} bytes!"
        )

    pad_len = target_size - raw_len
    padded = bytes(stripped) + (b"\x00" * pad_len)
    assert len(padded) == target_size

    with open(out_path, "wb") as f:
        f.write(padded)
    os.chmod(out_path, 0o755)
    print(
        f"[OK] {out_path}: raw={raw_len}B, pad={pad_len}B -> exact={len(padded)}B ({raw_len/target_size*100:.1f}% full)"
    )


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <in_elf> <out_elf> <target_bytes>")
        sys.exit(1)
    sstrip_and_pad(sys.argv[1], sys.argv[2], int(sys.argv[3]))
