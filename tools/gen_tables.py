r"""gen_tables.py -- tables DualSense.dll needs to read the game's textures, written to DualSense\tables.h:

  - the BC7 partition tables (2 and 3 subsets) and anchor indices, recovered from Pillow's BC7 decoder: blocks made
    to show which subset each pixel is in, and which pixel's index has a bit less;
  - the game's 2D thin texture layouts (XG mode 14) for the sizes its icons use: each layout is XOR-linear in the
    element coordinate bits (checked against the game's own xg_x.dll), so a table of 2 x log2(size) offsets is it.

    python -I gen_tables.py OUT.h GAME_FOLDER
"""
import io
import os
import struct
import sys

from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'xg'))
import xg  # noqa: E402


def dds(blocks16):
    hdr = bytearray(128)
    struct.pack_into('<4sIIIIIII', hdr, 0, b'DDS ', 124, 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000, 4, 4, 16, 0, 1)
    struct.pack_into('<II4s', hdr, 76, 32, 0x4, b'DX10')
    struct.pack_into('<I', hdr, 108, 0x1000)
    return Image.open(io.BytesIO(bytes(hdr) + struct.pack('<IIIII', 98, 3, 0, 1, 0) + blocks16)).convert('RGBA')


class Bits:
    def __init__(self):
        self.v, self.n = 0, 0

    def put(self, val, bits):
        self.v |= (val & ((1 << bits) - 1)) << self.n
        self.n += bits

    def bytes(self):
        assert self.n == 128, self.n
        return self.v.to_bytes(16, 'little')


def mode1_block(part, idx, anchor1):
    """mode 1: 2 subsets; subset 0 endpoints black, subset 1 endpoints white; indices idx[16] (3 bits)"""
    b = Bits()
    b.put(0b10, 2)  # mode 1
    b.put(part, 6)
    for _ in range(3):  # R, G, B: E0 E1 of subset 0, E0 E1 of subset 1 (6 bits)
        for e in (0, 0, 63, 63):
            b.put(e, 6)
    b.put(0, 1)
    b.put(1, 1)  # shared p-bits
    for i in range(16):
        b.put(idx[i], 2 if i in (0, anchor1) else 3)
    return b.bytes()


def mode2_block(part, idx, a1, a2):
    """mode 2: 3 subsets, endpoints 0 / 15 / 31 (5 bits), indices 2 bits"""
    b = Bits()
    b.put(0b100, 3)
    b.put(part, 6)
    for _ in range(3):
        for e in (0, 0, 15, 15, 31, 31):
            b.put(e, 5)
    for i in range(16):
        b.put(idx[i], 1 if i in (0, a1, a2) else 2)
    return b.bytes()


def partitions():
    p2, a2 = [], []
    for part in range(64):
        im = dds(mode1_block(part, [0] * 16, 15))
        sub = [1 if im.getpixel((i % 4, i // 4))[0] > 128 else 0 for i in range(16)]
        p2.append(sub)
        a2.append(None)
    return p2, a2


import random


def _x7(v7):
    return (v7 << 1) | (v7 >> 6)


def anchors2(p2):
    """anchor of subset 1: the only pixel for which, under several index patterns, the block decodes as written"""
    w3 = [0, 9, 18, 27, 37, 46, 55, 64]
    res = []
    for part in range(64):
        sub = p2[part]
        cands = set(k for k in range(16) if sub[k] == 1)
        rnd = random.Random(part)
        for _ in range(12):
            idx = [rnd.randrange(8) for _ in range(16)]
            ok = set()
            for k in cands:
                ii = list(idx)
                ii[0] &= 3
                ii[k] &= 3
                b = Bits()
                b.put(0b10, 2)
                b.put(part, 6)
                for _ in range(3):
                    for e in (0, 63, 0, 63):
                        b.put(e, 6)
                b.put(0, 1)
                b.put(1, 1)
                for i in range(16):
                    b.put(ii[i], 2 if i in (0, k) else 3)
                im = dds(b.bytes())
                got = [im.getpixel((i % 4, i // 4))[0] for i in range(16)]
                exp = []
                for i in range(16):
                    pb = sub[i]
                    c0, c1 = _x7(0 << 1 | pb), _x7(63 << 1 | pb)
                    w = w3[ii[i]]
                    exp.append(((64 - w) * c0 + w * c1 + 32) >> 6)
                if got == exp:
                    ok.add(k)
            cands &= ok
            if len(cands) == 1:
                break
        assert len(cands) == 1, (part, cands)
        res.append(cands.pop())
    return res


def anchors3(p3):
    w2 = [0, 21, 43, 64]
    res = []
    for part in range(64):
        sub = p3[part]
        cands = set((k1, k2) for k1 in range(16) for k2 in range(16) if sub[k1] == 1 and sub[k2] == 2)
        rnd = random.Random(1000 + part)
        for _ in range(12):
            idx = [rnd.randrange(4) for _ in range(16)]
            ok = set()
            for (k1, k2) in cands:
                ii = list(idx)
                for k in (0, k1, k2):
                    ii[k] &= 1
                b = Bits()
                b.put(0b100, 3)
                b.put(part, 6)
                for _ in range(3):
                    for e in (0, 31, 0, 31, 0, 31):
                        b.put(e, 5)
                for i in range(16):
                    b.put(ii[i], 1 if i in (0, k1, k2) else 2)
                im = dds(b.bytes())
                got = [im.getpixel((i % 4, i // 4))[0] for i in range(16)]
                c0, c1 = 0, (31 << 3) | (31 >> 2)
                exp = [((64 - w2[ii[i]]) * c0 + w2[ii[i]] * c1 + 32) >> 6 for i in range(16)]
                if got == exp:
                    ok.add((k1, k2))
            cands &= ok
            if len(cands) == 1:
                break
        assert len(cands) == 1, (part, cands)
        res.append(cands.pop())
    return res


def partitions3():
    p3 = []
    for part in range(64):
        im = dds(mode2_block(part, [0] * 16, 14, 15))  # all indices 0: where the anchors are does not matter
        sub = []
        for i in range(16):
            r = im.getpixel((i % 4, i // 4))[0]
            sub.append(0 if r < 40 else 1 if r < 200 else 2)
        p3.append(sub)
    return p3


def main():
    out, game = sys.argv[1], sys.argv[2]
    p2, _ = partitions()
    a2 = anchors2(p2)
    p3 = partitions3()
    a3 = anchors3(p3)
    assert all(a is not None for a in a2), a2
    assert all(a is not None for a in a3), a3
    xg.load(game)
    lines = ['/* tables.h -- made by tools\\gen_tables.py: BC7 partitions and anchors (from Pillow\'s BC7 decoder) and the',
             '   game\'s 2D thin layouts (from its xg_x.dll). Do not edit. */', '#pragma once', '']
    lines.append('static const unsigned char kBc7P2[64][16] = {')
    lines += ['    {%s},' % ', '.join(map(str, r)) for r in p2]
    lines.append('};')
    lines.append('static const unsigned char kBc7P3[64][16] = {')
    lines += ['    {%s},' % ', '.join(map(str, r)) for r in p3]
    lines.append('};')
    lines.append('static const unsigned char kBc7A2[64] = {%s};' % ', '.join(map(str, a2)))
    lines.append('static const unsigned char kBc7A3[64][2] = {%s};' % ', '.join('{%d, %d}' % a for a in a3))
    # 2D thin (XG 14) layouts: element offsets for x bits and y bits
    lines.append('')
    lines.append('/* 2D thin layout: offset(x, y) = XOR of xbit[i] for each set bit i of x, and of ybit[i] for y (bytes) */')
    lines.append('typedef struct { unsigned short ew, eh, esize, size_log; unsigned int xbit[10], ybit[10]; } Layout2D;')
    lines.append('static const Layout2D kLayouts2D[] = {')
    for w in (64, 128, 256, 512):
        for fmt, es in ((xg.BC7, 16), (xg.RGBA8, 4)):
            L = xg.Layout(w, w, fmt, 14)
            n = L.ew.bit_length() - 1
            bx = [L.offsets[0][1 << i] for i in range(n)] + [0] * (10 - n)
            by = [L.offsets[1 << i][0] for i in range(n)] + [0] * (10 - n)
            lines.append('    {%d, %d, %d, %d, {%s}, {%s}},' % (L.ew, L.eh, es, n, ', '.join(map(str, bx)), ', '.join(map(str, by))))
    lines.append('};')
    open(out, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('wrote', out)


if __name__ == '__main__':
    main()
