"""xg.py -- the game's own Xbox One texture layout library (xg_x.dll, from its EmbeddedXvd) on a PC, through ctypes.

The 2014 XGTextureAddressComputer has 8 methods (found by disassembly): 0 AddRef, 1 Release, 2 GetResourceLayout,
3 GetMipLevelOffsetBytes?, 4 GetTexelElementOffsetBytes(Plane, Mip, X, Y, ZOrSlice, Sample), 5 CopyIntoSubresource,
6 CopyFromSubresource, 7 GetResourceTiling. Only 4 is used: the byte offset of every element, which is all that tiling
and untiling need. xg_x.dll imports kernelx.dll; our stub (kernelx.c) is loaded first and answers for it.
"""
import ctypes as C
import os

HERE = os.path.dirname(os.path.abspath(__file__))
_k32 = C.WinDLL('kernel32', use_last_error=True)
_k32.LoadLibraryExW.restype = C.c_void_p
_k32.LoadLibraryExW.argtypes = [C.c_wchar_p, C.c_void_p, C.c_uint32]
_k32.GetProcAddress.restype = C.c_void_p
_k32.GetProcAddress.argtypes = [C.c_void_p, C.c_char_p]
_create = None

BC7 = 98
RGBA8 = 28


def load(game_folder):
    """game_folder: the folder with EmbeddedXvd (or Mount\\era\\EmbeddedXvd)"""
    global _create
    if _create:
        return
    if not _k32.LoadLibraryExW(os.path.join(HERE, 'kernelx.dll'), None, 0):
        raise OSError('kernelx.dll stub: build tools\\xg\\build.cmd')
    for rel in (r'EmbeddedXvd\Windows\System32\xg_x.dll', r'Mount\era\EmbeddedXvd\Windows\System32\xg_x.dll'):
        p = os.path.join(game_folder, rel)
        if os.path.exists(p):
            h = _k32.LoadLibraryExW(p, None, 0)
            if h:
                _create = C.CFUNCTYPE(C.c_long, C.c_void_p, C.POINTER(C.c_void_p))(
                    _k32.GetProcAddress(h, b'XGCreateTexture2DComputer'))
                return
    raise OSError('xg_x.dll not found in ' + game_folder)


class Layout:
    """element offsets of one 2D texture's top mip: offsets[y][x] (elements: pixels, or 4x4 blocks for BC)"""

    def __init__(self, w, h, fmt, tile, mips=1):
        desc = (C.c_uint32 * 16)(w, h, mips, 1, fmt, 1, 0, 0, 8, 0, 0, 0, 0, tile, 0, 0)
        p = C.c_void_p()
        hr = _create(desc, C.byref(p))
        if hr:
            raise OSError('XGCreateTexture2DComputer: 0x%08X' % (hr & 0xFFFFFFFF))
        vt = C.cast(C.cast(p, C.POINTER(C.c_void_p))[0], C.POINTER(C.c_void_p))
        off = C.CFUNCTYPE(C.c_uint64, C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint64, C.c_uint32, C.c_uint32,
                          C.c_uint32)(vt[4])
        release = C.CFUNCTYPE(C.c_uint32, C.c_void_p)(vt[1])
        self.bc = fmt not in (RGBA8,)
        self.esize = 16 if self.bc else 4
        step = 4 if self.bc else 1
        self.ew, self.eh = (w + step - 1) // step, (h + step - 1) // step
        self.offsets = [[off(p, 0, 0, x, y, 0, 0) for x in range(self.ew)] for y in range(self.eh)]  # elements: blocks for BC
        release(p)

    def untile(self, data):
        out = bytearray(self.ew * self.eh * self.esize)
        for y in range(self.eh):
            for x in range(self.ew):
                o = self.offsets[y][x]
                out[(y * self.ew + x) * self.esize:(y * self.ew + x + 1) * self.esize] = data[o:o + self.esize]
        return bytes(out)
