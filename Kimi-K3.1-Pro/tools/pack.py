#!/usr/bin/env python3
import sys
from pathlib import Path
from lzr import pack, decompress


def main() -> None:
    if len(sys.argv) not in (3, 4):
        raise SystemExit("usage: pack.py PAYLOAD OUTPUT_HEADER [COMPRESSED_OUTPUT]")
    source = Path(sys.argv[1]).read_bytes()
    compressed = pack(source, shift=3)
    assert decompress(compressed, len(source), shift=3) == source
    values = ",".join(str(value) for value in compressed)
    text = (f"#define PAYLOAD_SIZE {len(source)}\n"
            f"static const unsigned char compressed[]={{{values}}};\n")
    Path(sys.argv[2]).write_text(text)
    if len(sys.argv) == 4:
        Path(sys.argv[3]).write_bytes(compressed)
    print(f"packed {sys.argv[1]}: {len(source)} -> {len(compressed)} bytes")


if __name__ == "__main__":
    main()
