#!/usr/bin/env python3
import struct
import sys
from pathlib import Path


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: sstrip.py ELF")
    path = Path(sys.argv[1])
    data = bytearray(path.read_bytes())
    if data[:5] != b"\x7fELF\x02" or data[5] != 1:
        raise SystemExit("expected a little-endian ELF64 file")
    phoff = struct.unpack_from("<Q", data, 32)[0]
    phentsize, phnum = struct.unpack_from("<HH", data, 54)
    end = phoff + phentsize * phnum
    for i in range(phnum):
        offset = phoff + i * phentsize
        kind = struct.unpack_from("<I", data, offset)[0]
        file_offset, file_size = struct.unpack_from("<QQ", data, offset + 8)[0], struct.unpack_from("<Q", data, offset + 32)[0]
        if kind == 1:
            end = max(end, file_offset + file_size)
    struct.pack_into("<Q", data, 40, 0)
    struct.pack_into("<HHH", data, 58, 0, 0, 0)
    path.write_bytes(data[:end])
    print(f"sstrip: {len(data)} -> {end} bytes")


if __name__ == "__main__":
    main()
