import ctypes as C
import struct
import sys
import zlib
from pathlib import Path

lib = C.CDLL('/run/current-system/sw/share/nix-ld/lib/libX11.so.6')

class XImage(C.Structure):
    _fields_ = [
        ('width', C.c_int), ('height', C.c_int), ('xoffset', C.c_int),
        ('format', C.c_int), ('data', C.POINTER(C.c_char)),
        ('byte_order', C.c_int), ('bitmap_unit', C.c_int),
        ('bitmap_bit_order', C.c_int), ('bitmap_pad', C.c_int),
        ('depth', C.c_int), ('bytes_per_line', C.c_int),
        ('bits_per_pixel', C.c_int), ('red_mask', C.c_ulong),
        ('green_mask', C.c_ulong), ('blue_mask', C.c_ulong),
    ]

lib.XOpenDisplay.argtypes = [C.c_char_p]
lib.XOpenDisplay.restype = C.c_void_p
lib.XDefaultRootWindow.argtypes = [C.c_void_p]
lib.XDefaultRootWindow.restype = C.c_ulong
lib.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                          C.c_uint, C.c_uint, C.c_ulong, C.c_int]
lib.XGetImage.restype = C.POINTER(XImage)
lib.XDestroyImage.argtypes = [C.POINTER(XImage)]
lib.XCloseDisplay.argtypes = [C.c_void_p]

display = lib.XOpenDisplay(None)
if not display:
    raise SystemExit('cannot open X display')
root = lib.XDefaultRootWindow(display)
image = lib.XGetImage(display, root, 0, 0, 1920, 1080, 0xffffffff, 2)
if not image:
    raise SystemExit('cannot capture root window')
item = image.contents
width, height = item.width, item.height
if item.bits_per_pixel != 32:
    raise SystemExit(f'unexpected pixel format: {item.bits_per_pixel}')
raw = C.string_at(item.data, item.bytes_per_line * height)
rgb = bytearray(width * height * 3)
for y in range(height):
    row = raw[y * item.bytes_per_line:y * item.bytes_per_line + width * 4]
    out = memoryview(rgb)[y * width * 3:(y + 1) * width * 3]
    out[0::3] = row[2::4]
    out[1::3] = row[1::4]
    out[2::3] = row[0::4]
lib.XDestroyImage(image)
lib.XCloseDisplay(display)

def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)

scanlines = b''.join(b'\0' + rgb[y * width * 3:(y + 1) * width * 3]
                     for y in range(height))
png = (b'\x89PNG\r\n\x1a\n' +
       chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
       chunk(b'IDAT', zlib.compress(scanlines, 6)) + chunk(b'IEND', b''))
Path(sys.argv[1]).write_bytes(png)
