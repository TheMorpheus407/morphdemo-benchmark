#!/usr/bin/env python3
"""exact_size.py -- turn a linked ELF into an exactly sized executable.

Steps
  1. optional section removal (.comment, .note.*)
  2. cut the empty slots the linker leaves at the end of the dynamic table
  3. drop the section header table (the kernel does not need it)
  4. pad the file to the requested size

The result stays a valid, runnable native ELF: the program headers still
describe every loadable segment, only the optional section directory and
trailing slack are removed.
"""
import os
import struct
import sys

PT_LOAD = 1
PT_DYNAMIC = 2
DT_NULL = 0
DT_VERNEED = 0x6FFFFFFE
DT_VERNEEDNUM = 0x6FFFFFFF
DT_VERSYM = 0x6FFFFFF0


def strip_sections(data):
    """Remove the section header table so the file only carries what the
    loader needs.  e_shoff/e_shnum/e_shstrndx are patched in place.

    The section name table goes with it: it is only ever read through the
    section headers, and in this layout it sits right in front of them.
    """
    if data[:4] != b"\x7fELF":
        raise SystemExit("not an ELF file")
    data = bytearray(data)
    e_shoff, = struct.unpack_from("<Q", data, 0x28)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", data, 0x3A)
    if e_shoff and e_shnum:
        cut = e_shoff
        if e_shstrndx < e_shnum:
            sh = e_shoff + e_shstrndx * e_shentsize
            name_off, = struct.unpack_from("<Q", data, sh + 24)
            name_size, = struct.unpack_from("<Q", data, sh + 32)
            if name_off and name_size and name_off + name_size <= e_shoff:
                gap = data[name_off + name_size:e_shoff]
                if not any(gap):
                    cut = name_off
        # keep the loadable content, drop the directory
        data = data[:cut]
        struct.pack_into("<Q", data, 0x28, 0)          # e_shoff
        struct.pack_into("<HHH", data, 0x3A, 0, 0, 0)  # entsize, num, strndx
    return bytes(data)


def trim_dynamic(data):
    """Cut the unused tail of the dynamic table.

    The linker reserves a few entries more than it fills in.  When the dynamic
    segment is the last thing in the file those empty slots can simply be
    dropped, together with the matching part of the load segment.
    """
    if data[:4] != b"\x7fELF":
        return data, 0
    data = bytearray(data)
    e_phoff, = struct.unpack_from("<Q", data, 0x20)
    e_phentsize, e_phnum = struct.unpack_from("<HH", data, 0x36)

    dyn = None
    loads = []
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, = struct.unpack_from("<I", data, off)
        p_offset, p_vaddr = struct.unpack_from("<QQ", data, off + 8)
        p_filesz, p_memsz = struct.unpack_from("<QQ", data, off + 32)
        if p_type == PT_DYNAMIC:
            dyn = (off, p_offset, p_filesz)
        elif p_type == PT_LOAD:
            loads.append((off, p_offset, p_filesz, p_memsz))
    if dyn is None:
        return bytes(data), 0

    off, d_offset, d_filesz = dyn
    entries = []
    for i in range(d_filesz // 16):
        tag, val = struct.unpack_from("<QQ", data, d_offset + i * 16)
        if tag == DT_NULL:
            break
        entries.append((tag, val))
    if not entries:
        return bytes(data), 0

    # The version requirement table is optional: without it the loader simply
    # binds every symbol to its default version, which is what we want.
    kept = [e for e in entries if e[0] not in (DT_VERNEED, DT_VERNEEDNUM, DT_VERSYM)]
    used = (len(kept) + 1) * 16
    if used >= d_filesz:
        return bytes(data), 0
    if d_offset + d_filesz != len(data):
        # the dynamic table is not the last thing in the file, leave it alone
        return bytes(data), 0

    for i, (tag, val) in enumerate(kept):
        struct.pack_into("<QQ", data, d_offset + i * 16, tag, val)
    struct.pack_into("<QQ", data, d_offset + len(kept) * 16, DT_NULL, 0)

    # the load segment that carries the table ends with it
    for loff, l_offset, l_filesz, l_memsz in loads:
        if l_offset <= d_offset < l_offset + l_filesz:
            struct.pack_into("<Q", data, off + 32, used)          # p_filesz
            struct.pack_into("<Q", data, loff + 32, l_filesz - (d_filesz - used))
            break

    return bytes(data[:d_offset + used]), d_filesz - used


def pad(data, size):
    if len(data) > size:
        raise SystemExit("file is %d bytes, larger than the target %d" % (len(data), size))
    # A short run of zero bytes is inert padding: it sits behind the last
    # loadable segment and is never mapped.
    return data + b"\0" * (size - len(data))


def main():
    if len(sys.argv) < 4:
        raise SystemExit("usage: exact_size.py <in> <out> <size> | s <in> <out>")
    if sys.argv[1] == "s":
        # strip only: no padding, no trimming, used before packing
        with open(sys.argv[2], "rb") as f:
            data = f.read()
        with open(sys.argv[3], "wb") as f:
            f.write(strip_sections(data))
        return
    src, dst, size = sys.argv[1], sys.argv[2], int(sys.argv[3])
    with open(src, "rb") as f:
        data = f.read()
    data = strip_sections(data)
    data, cut = trim_dynamic(data)
    data = pad(data, size)
    with open(dst, "wb") as f:
        f.write(data)
    os.chmod(dst, 0o755)
    print("%s: %d bytes%s" % (dst, len(data),
                              " (dynamic table trimmed by %d)" % cut if cut else ""))


if __name__ == "__main__":
    main()
