#!/usr/bin/env python3
"""sstrip.py — strip ELF section headers and everything after the last
allocated segment content (symtab/strtab/section headers).  The kernel only
needs the program headers, so the file can be truncated at the end of the
last PT_LOAD's file image.

usage: sstrip.py <input> <output>
"""
import struct
import sys

data = open(sys.argv[1], "rb").read()
if data[:4] != b"\x7fELF" or data[4] != 2:
    sys.exit("sstrip: not an ELF64 file")

e_phoff = struct.unpack_from("<Q", data, 0x20)[0]
e_phentsize = struct.unpack_from("<H", data, 0x36)[0]
e_phnum = struct.unpack_from("<H", data, 0x38)[0]

end = 0
for i in range(e_phnum):
    off = e_phoff + i * e_phentsize
    p_type, p_flags = struct.unpack_from("<II", data, off)
    p_offset, p_vaddr, p_paddr, p_filesz = struct.unpack_from("<QQQQ", data, off + 8)
    if p_type == 1:  # PT_LOAD
        end = max(end, p_offset + p_filesz)

# zero the section header fields
data = bytearray(data)
struct.pack_into("<Q", data, 0x28, 0)   # e_shoff = 0
struct.pack_into("<H", data, 0x3C, 0)   # e_shnum = 0
struct.pack_into("<H", data, 0x3E, 0)   # e_shstrndx = 0

with open(sys.argv[2], "wb") as f:
    f.write(bytes(data[:end]))
print(f"sstrip: {sys.argv[1]} ({len(open(sys.argv[1],'rb').read())} B) -> "
      f"{sys.argv[2]} ({end} B)", file=sys.stderr)
