#!/usr/bin/env python3
import os
import sys
from pathlib import Path


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit("usage: pad.py INPUT OUTPUT SIZE")
    source = Path(sys.argv[1]).read_bytes()
    target = int(sys.argv[3], 0)
    if source[:4] != b"\x7fELF":
        raise SystemExit(f"{sys.argv[1]} is not an ELF executable")
    if len(source) > target:
        raise SystemExit(f"{sys.argv[1]} is {len(source)} bytes; target is {target}")
    output = Path(sys.argv[2])
    output.write_bytes(source + bytes(target - len(source)))
    os.chmod(output, 0o755)
    print(f"{output}: {len(source)} -> {target} bytes")


if __name__ == "__main__":
    main()
