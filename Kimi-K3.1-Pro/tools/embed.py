#!/usr/bin/env python3
import re
import sys
from pathlib import Path


def minify(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    source = re.sub(r"//[^\n]*", "", source)
    lines = []
    for line in source.splitlines():
        line = line.strip()
        if not line:
            continue
        if line.startswith("#"):
            lines.append(line + "\n")
            continue
        line = re.sub(r"\s+", " ", line)
        line = re.sub(r"\s*([{}()\[\],;:?+\-*/%=<>|&])\s*", r"\1", line)
        lines.append(line + "\n")
    return "".join(lines)


def c_string(data: str) -> str:
    chunks = []
    for line in data.splitlines(keepends=True):
        escaped = (line.replace("\\", "\\\\").replace('"', '\\"')
                   .replace("\n", "\\n").replace("\t", "\\t"))
        chunks.append(f'"{escaped}"')
    return "\n".join(chunks)


def main() -> None:
    if len(sys.argv) != 4 or sys.argv[1] not in ("--minify", "--verbatim"):
        raise SystemExit("usage: embed.py (--minify|--verbatim) INPUT OUTPUT")
    source = Path(sys.argv[2]).read_text()
    if sys.argv[1] == "--minify":
        source = minify(source)
    output = "static const char fragment_source[] =\n" + c_string(source) + ";\n"
    Path(sys.argv[3]).write_text(output)


if __name__ == "__main__":
    main()
