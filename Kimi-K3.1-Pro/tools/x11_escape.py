import ctypes as C
import sys

lib = C.CDLL('/run/current-system/sw/share/nix-ld/lib/libX11.so.6')

class XKeyEvent(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send_event', C.c_int),
                ('display', C.c_void_p), ('window', C.c_ulong), ('root', C.c_ulong),
                ('subwindow', C.c_ulong), ('time', C.c_ulong), ('x', C.c_int),
                ('y', C.c_int), ('x_root', C.c_int), ('y_root', C.c_int),
                ('state', C.c_uint), ('keycode', C.c_uint), ('same_screen', C.c_int)]

class XEvent(C.Union):
    _fields_ = [('key', XKeyEvent), ('pad', C.c_long * 24)]

lib.XOpenDisplay.argtypes = [C.c_char_p]
lib.XOpenDisplay.restype = C.c_void_p
lib.XDefaultRootWindow.argtypes = [C.c_void_p]
lib.XDefaultRootWindow.restype = C.c_ulong
lib.XGetInputFocus.argtypes = [C.c_void_p, C.POINTER(C.c_ulong), C.POINTER(C.c_int)]
lib.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
lib.XKeysymToKeycode.restype = C.c_ubyte
lib.XQueryTree.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong),
                           C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ulong)),
                           C.POINTER(C.c_uint)]
lib.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_char_p)]
lib.XFree.argtypes = [C.c_void_p]
lib.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.POINTER(XEvent)]
lib.XFlush.argtypes = [C.c_void_p]
lib.XCloseDisplay.argtypes = [C.c_void_p]

display = lib.XOpenDisplay(None)
if not display:
    raise SystemExit('cannot open X display')
root = lib.XDefaultRootWindow(display)
focus = C.c_ulong()
revert = C.c_int()
lib.XGetInputFocus(display, C.byref(focus), C.byref(revert))
window = focus.value
if len(sys.argv) > 1 and sys.argv[1] == '--find-morph':
    root_ret, parent = C.c_ulong(), C.c_ulong()
    children = C.POINTER(C.c_ulong)()
    count = C.c_uint()
    if lib.XQueryTree(display, root, C.byref(root_ret), C.byref(parent),
                      C.byref(children), C.byref(count)):
        for i in range(count.value):
            name = C.c_char_p()
            if lib.XFetchName(display, children[i], C.byref(name)) and name.value:
                if b'MORPH' in name.value:
                    window = children[i]
                lib.XFree(name)
    if children:
        lib.XFree(children)
if not window or window == 1:
    lib.XCloseDisplay(display)
    raise SystemExit('no focused demo window')
event = XEvent()
event.key.display = display
event.key.window = window
event.key.root = root
event.key.same_screen = 1
event.key.keycode = lib.XKeysymToKeycode(display, 0xff1b)
for kind, mask in ((2, 1), (3, 2)):
    event.key.type = kind
    event.key.send_event = 1
    if not lib.XSendEvent(display, window, 1, mask, C.byref(event)):
        lib.XCloseDisplay(display)
        raise SystemExit('XSendEvent failed')
lib.XFlush(display)
lib.XCloseDisplay(display)
