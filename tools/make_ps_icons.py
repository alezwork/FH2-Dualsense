r"""make_ps_icons.py -- PlayStation button icons for Forza Horizon 2 (and Fast & Furious), for the DualSense mod.

    python -I make_ps_icons.py OUTDIR MOUNT [MOUNT...]

Reads the game's button icons (media\ui\textures\data_bound\ControllerIcons.zip, Help_Buttons.zip), draws the
PlayStation ones in the same style (Cross/Circle/Square/Triangle, L1/R1/L2/R2, L/R sticks, L3/R3, Options, the
touchpad, the four-key D-pad) and writes into OUTDIR, for each archive, two variants with the very same layout:
  <name>.ps.zip, <name>.xbox.zip   every icon stored uncompressed as RGBA (PlayStation / the game's own, decoded);
                                   only the icons' pixels differ, so DualSense.dll can switch the game's open archive
                                   from one to the other in place (the game read its directory once, at the start)
  zipmanifest-<size>.xml           each game's media\zipmanifest.xml with the new archives' directories
  Icons.txt                        for DualSense.dll: what it serves instead of which game file
  preview.png                      both sets side by side

The game's formats (found 2026-10-13, FH2 1.0.0.64):
- .swatchbin = "burG" container; texture header at 0x28 ("HCXT"); width/height at 0x40/0x42 (and again at 0x46/0x48),
  mips 0x4C/0x4D, format byte 0x4F (0x80 = RGBA8, else BC7), tile code at 0x52 (>>3), data at 0x53; sizes at 0x0C
  (file), 0x20 and 0x24 (data). The tile code is not XG's enum: code 8 = XG 8 (linear), 9 = XG 13 (1D thin), 23 = XG 14
  (2D thin) -- checked with the game's own xg_x.dll (tools\xg). Pixels are premultiplied by alpha.
- the archives: zips whose central directory entries carry an extra field 0x1123 = the offset of the entry's data;
  media\zipmanifest.xml gives each archive's directory offset, size (+22, the end record) and entry count. The game
  also reads stored (uncompressed) entries.
"""
import io
import os
import re
import struct
import sys
import zipfile
import zlib

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'xg'))
import xg  # noqa: E402

SS = 8  # supersampling: drawn 8x, reduced by area (a crisp one-pixel edge, like the game's own)
FONT = r'C:\Windows\Fonts\bahnschrift.ttf'
XG_MODE = {8: 8, 9: 13, 23: 14}

DARK = (28, 28, 28, 255)
RING = (214, 214, 214, 255)
LIGHT = (240, 240, 240, 255)
DIM = (92, 92, 92, 255)
HELP_DISC = (186, 186, 186, 255)
HELP_LIGHT = (234, 234, 234, 255)
HELP_INK = (22, 22, 22, 255)
HELP_DIM = (124, 124, 124, 255)
# the PlayStation colours
FACE = {'cross': (124, 172, 255, 255), 'circle': (255, 96, 96, 255), 'square': (247, 128, 214, 255),
        'triangle': (56, 214, 166, 255)}


# ---- swatchbin --------------------------------------------------------------------------------------------------
def swatch_size(d):
    return struct.unpack_from('<HH', d, 0x40)


def swatch_decode(d):
    """-> RGBA image (premultiplied, as stored), any of the icon formats"""
    w, h = swatch_size(d)
    fmt, code = d[0x4F], d[0x52] >> 3
    rgba = bool(fmt & 0x80)
    L = xg.Layout(w, h, xg.RGBA8 if rgba else xg.BC7, XG_MODE[code])
    lin = L.untile(d[0x53:])
    if rgba:
        return Image.frombytes('RGBA', (L.ew, L.eh), lin).crop((0, 0, w, h))
    hdr = bytearray(128)
    struct.pack_into('<4sIIIIIII', hdr, 0, b'DDS ', 124, 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000, L.eh * 4, L.ew * 4, len(lin), 0, 1)
    struct.pack_into('<II4s', hdr, 76, 32, 0x4, b'DX10')
    struct.pack_into('<I', hdr, 108, 0x1000)
    dds = bytes(hdr) + struct.pack('<IIIII', 98, 3, 0, 1, 0) + lin
    return Image.open(io.BytesIO(dds)).convert('RGBA').crop((0, 0, w, h))


_layouts = {}


def swatch_rgba(template, img, pad=16):
    """img (premultiplied RGBA) -> a .swatchbin: the template's header, RGBA8, 1D thin, one mip, pad spare bytes"""
    w, h = img.size
    aw, ah = (w + 7) // 8 * 8, (h + 7) // 8 * 8  # 1D thin: whole 8x8 micro tiles, the size the game's XG expects
    key = (aw, ah)
    if key not in _layouts:
        _layouts[key] = xg.Layout(aw, ah, xg.RGBA8, 13)
    L = _layouts[key]
    size = max(max(r) for r in L.offsets) + 4
    data = bytearray(size + pad)
    src = img.tobytes()
    for y in range(h):
        for x in range(w):
            o = L.offsets[y][x]
            data[o:o + 4] = src[(y * w + x) * 4:(y * w + x) * 4 + 4]
    hdr = bytearray(template[:0x53])
    struct.pack_into('<I', hdr, 0x0C, 0x53 + len(data))
    struct.pack_into('<II', hdr, 0x20, len(data), len(data))
    struct.pack_into('<6H', hdr, 0x40, w, h, 1, w, h, 1)
    hdr[0x4C] = hdr[0x4D] = 1
    hdr[0x4F] = 0xAD
    hdr[0x52] = 9 << 3
    return bytes(hdr) + bytes(data)


def unpremultiply(img):
    px = bytearray(img.tobytes())
    for i in range(0, len(px), 4):
        a = px[i + 3]
        if a and a < 255:
            for k in range(3):
                px[i + k] = min(255, px[i + k] * 255 // a)
    return Image.frombytes('RGBA', img.size, bytes(px))


def premultiply(img):
    r, g, b, a = img.split()
    from PIL import ImageChops
    return Image.merge('RGBA', [ImageChops.multiply(c, a) for c in (r, g, b)] + [a])


# ---- CRC32: four bytes that make data's CRC a chosen one (both variants of an icon keep the CRC the game read) ----
_T = [0] * 256
for _i in range(256):
    _c = _i
    for _ in range(8):
        _c = 0xEDB88320 ^ (_c >> 1) if _c & 1 else _c >> 1
    _T[_i] = _c
_INV = {(_T[i] >> 24): i for i in range(256)}


def forge_crc(data, target):
    """data's last 4 bytes replaced so that crc32(data) == target"""
    data = bytearray(data)
    crc = zlib.crc32(bytes(data[:-4])) ^ 0xFFFFFFFF  # the register before the last 4 bytes
    # run the register backwards from the target through 4 bytes
    reg = target ^ 0xFFFFFFFF
    idx = []
    for _ in range(4):
        i = _INV[reg >> 24]
        idx.append(i)
        reg = ((reg ^ _T[i]) << 8) & 0xFFFFFFFF
    idx.reverse()
    # the forward run: choose each byte so the table index is the one needed
    for k in range(4):
        b = (crc ^ idx[k]) & 0xFF
        data[len(data) - 4 + k] = b
        crc = _T[(crc ^ b) & 0xFF] ^ (crc >> 8)
    assert zlib.crc32(bytes(data)) == target
    return bytes(data)


# ---- drawing (supersampled; then reduced by area and premultiplied) ---------------------------------------------
def font(px):
    f = ImageFont.truetype(FONT, max(1, int(px)))
    try:
        f.set_variation_by_name('Bold')
    except Exception:
        pass
    return f


class Pad:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.img = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img)
        self.s = min(w, h) * SS

    def C(self):
        return self.w * SS / 2, self.h * SS / 2

    def disc(self, r, fill):
        cx, cy = self.C()
        self.d.ellipse((cx - r * self.s, cy - r * self.s, cx + r * self.s, cy + r * self.s), fill=fill)

    def ring(self, r, width, color):
        cx, cy = self.C()
        wp = max(SS, round(width * self.s / SS) * SS)  # a whole number of pixels: no grey half-pixel ring
        self.d.ellipse((cx - r * self.s, cy - r * self.s, cx + r * self.s, cy + r * self.s), outline=color, width=int(wp))

    def line(self, a, b, width, color):
        cx, cy = self.C()
        p = (cx + a[0] * self.s, cy + a[1] * self.s)
        q = (cx + b[0] * self.s, cy + b[1] * self.s)
        wpx = width * self.s
        self.d.line((p, q), fill=color, width=int(wpx))
        for x, y in (p, q):
            self.d.ellipse((x - wpx / 2, y - wpx / 2, x + wpx / 2, y + wpx / 2), fill=color)

    def poly(self, pts, fill):
        cx, cy = self.C()
        self.d.polygon([(cx + x * self.s, cy + y * self.s) for x, y in pts], fill=fill)

    def rrect(self, x0, y0, x1, y1, radius, fill=None, outline=None, width=0):
        self.d.rounded_rectangle((x0 * self.w * SS, y0 * self.h * SS, x1 * self.w * SS, y1 * self.h * SS),
                                 radius=radius * self.s, fill=fill, outline=outline,
                                 width=int(max(SS, round(width * self.s / SS) * SS)) if outline else 0)

    def text(self, t, cx, cy, size, color):
        self.d.text((cx * self.w * SS, cy * self.h * SS), t, font=font(size * self.s), fill=color, anchor='mm')

    def done(self):
        return premultiply(self.img.resize((self.w, self.h), Image.BOX))


def face_glyph(p, kind, g, width, color, bg):
    if kind == 'cross':
        k = g * 0.70
        p.line((-k, -k), (k, k), width, color)
        p.line((-k, k), (k, -k), width, color)
    elif kind == 'circle':
        p.ring(g * 0.82, width, color)
    elif kind == 'square':
        k = g * 0.72
        cx, cy = p.C()
        wp = width * p.s
        p.d.rectangle((cx - k * p.s, cy - k * p.s, cx + k * p.s, cy + k * p.s), fill=color)
        p.d.rectangle((cx - k * p.s + wp, cy - k * p.s + wp, cx + k * p.s - wp, cy + k * p.s - wp), fill=bg)
    elif kind == 'triangle':
        r = g * 0.98
        outer = [(0, -r), (r * 0.866, r * 0.5), (-r * 0.866, r * 0.5)]
        ri = r - width * 2.0
        inner = [(0, -ri), (ri * 0.866, ri * 0.5), (-ri * 0.866, ri * 0.5)]
        dy = r * 0.10
        p.poly([(x, y + dy) for x, y in outer], color)
        p.poly([(x, y + dy) for x, y in inner], bg)


def arrows(p, which, r, size, color):
    for dname, (dx, dy) in (('L', (-1, 0)), ('R', (1, 0)), ('U', (0, -1)), ('D', (0, 1))):
        if dname not in which:
            continue
        tip = (dx * (r + size * 0.6), dy * (r + size * 0.6))
        base = (dx * (r - size * 0.4), dy * (r - size * 0.4))
        side = (-dy * size * 0.75, dx * size * 0.75)
        p.poly([tip, (base[0] + side[0], base[1] + side[1]), (base[0] - side[0], base[1] - side[1])], color)


def dpad(p, lit, on, off, scale):
    """the PlayStation D-pad: four separate keys, each pointed towards the centre"""
    w, inner, outer = 0.19 * scale, 0.085 * scale, 0.39 * scale
    for dname, (dx, dy) in (('U', (0, -1)), ('D', (0, 1)), ('L', (-1, 0)), ('R', (1, 0))):
        def pt(a, b):
            return (dx * a - dy * b, dy * a + dx * b)
        pts = [pt(outer, -w / 2), pt(outer, w / 2), pt(inner + w * 0.45, w / 2), pt(inner, 0), pt(inner + w * 0.45, -w / 2)]
        p.poly(pts, on if (lit == 'ALL' or dname in lit) else off)


def options_glyph(p, scale, color, width):
    """Options: three lines, the middle one shorter (as printed on the controller)"""
    for y, half in ((-0.13, 0.17), (0.0, 0.17), (0.13, 0.17)):
        p.line((-half * scale, y * scale), (half * scale, y * scale), width, color)


def touchpad_glyph(p, scale, color):
    """the touchpad: a wide panel, rounded at the bottom"""
    cx, cy = p.C()
    s = p.s
    p.d.rounded_rectangle((cx - 0.31 * scale * s, cy - 0.17 * scale * s, cx + 0.31 * scale * s, cy + 0.17 * scale * s),
                          radius=0.09 * scale * s, fill=color)
    p.d.rectangle((cx - 0.31 * scale * s, cy - 0.17 * scale * s, cx + 0.31 * scale * s, cy - 0.05 * scale * s), fill=color)


# ---- the icons --------------------------------------------------------------------------------------------------
STICK_ARROWS = {'': '', 'Left': 'L', 'Right': 'R', 'Up': 'U', 'Down': 'D', 'LeftRight': 'LR', 'UpDown': 'UD'}
DPAD_LIT = {'DPad': 'ALL', 'DPadUp': 'U', 'DPadDown': 'D', 'DPadLeft': 'L', 'DPadRight': 'R', 'DPadUpDown': 'UD',
            'DPadLeftRight': 'LR'}
FACE_OF = {'A': 'cross', 'B': 'circle', 'X': 'square', 'Y': 'triangle'}


def controller_icon(name, w, h):
    """ControllerIcons.zip (32, 64, 128 px): a dark disc and a light ring, the face buttons in their colours"""
    base = name.split('/')[-1].replace('.swatchbin', '')
    if not base.startswith('Controller_'):
        return None
    key = base[len('Controller_'):]
    p = Pad(w, h)
    rw = 2.0 / 32  # the game's ring: 2 px at 32
    if key in FACE_OF:
        col = FACE[FACE_OF[key]]
        p.disc(0.47, DARK)
        p.ring(0.47, rw, col)
        face_glyph(p, FACE_OF[key], 0.30, 2.3 / 32, col, DARK)
    elif key in ('LB', 'RB'):
        p.rrect(0.06, 0.22, 0.94, 0.78, 0.14, fill=DARK, outline=RING, width=rw)
        p.text(key[0] + '1', 0.5, 0.52, 0.40, LIGHT)
    elif key in ('LT', 'RT'):
        p.rrect(0.25, 0.05, 0.75, 0.95, 0.13, fill=DARK, outline=RING, width=rw)
        p.text(key[0] + '2', 0.5, 0.42, 0.32, LIGHT)
    elif key.upper() == 'MENU':
        p.disc(0.47, DARK)
        p.ring(0.47, rw, RING)
        options_glyph(p, 1.0, LIGHT, 2.2 / 32)
    elif key.upper() == 'VIEW':
        p.disc(0.47, DARK)
        p.ring(0.47, rw, RING)
        touchpad_glyph(p, 1.0, LIGHT)
    elif key in DPAD_LIT:
        p.disc(0.47, DARK)
        p.ring(0.47, rw, RING)
        dpad(p, DPAD_LIT[key], LIGHT, DIM, 1.0)
    elif key in ('L3', 'R3'):
        p.disc(0.47, DARK)
        p.ring(0.47, rw, RING)
        arrows(p, 'LRUD', 0.355, 0.07, RING)
        p.text(key, 0.5, 0.52, 0.29, LIGHT)
    elif key.startswith('LeftThumb') or key.startswith('RightThumb'):
        side = 'L' if key.startswith('Left') else 'R'
        suffix = key.split('_', 1)[1] if '_' in key else ''
        p.disc(0.47, DARK)
        p.ring(0.47, rw, RING)
        arrows(p, STICK_ARROWS.get(suffix, ''), 0.355, 0.07, LIGHT)
        p.text(side, 0.5, 0.53, 0.40, LIGHT)
    else:
        return None
    return p.done()


def help_icon(name, w, h):
    """Help_Buttons.zip (22 / 46 px): a grey disc, a dark symbol"""
    base = name.replace('.swatchbin', '')
    p = Pad(w, h)
    m = re.match(r'Button_([ABXY])(_Dark|_Lg|_Light)?$', base)
    if m:
        kind = FACE_OF[m.group(1)]
        disc, ink = HELP_DISC, HELP_INK
        if m.group(2) == '_Dark':
            disc, ink = (16, 16, 16, 255), (240, 240, 240, 255)
        elif m.group(2) == '_Light':
            disc = HELP_LIGHT
        p.disc(0.48, disc)
        face_glyph(p, kind, 0.29, 2.2 / 22, ink, disc)
        return p.done()
    m = re.match(r'Button_(LB|RB|LT|RT)$', base)
    if m:
        k = m.group(1)
        p.rrect(0.04, 0.12, 0.96, 0.88, 0.2, fill=HELP_DISC)
        p.text(k[0] + ('1' if k[1] == 'B' else '2'), 0.5, 0.53, 0.60, HELP_INK)
        return p.done()
    if base in ('Button_Back', 'Button_Back_Lg'):
        p.disc(0.48, HELP_DISC)
        touchpad_glyph(p, 1.05, HELP_INK)
        return p.done()
    if base == 'Button_Start':
        p.disc(0.48, HELP_DISC)
        options_glyph(p, 1.05, HELP_INK, 2.0 / 22)
        return p.done()
    m = re.match(r'DPad_(OFF|ON|U|D|L|R|UD|LR)$', base)
    if m:
        lit = {'OFF': '', 'ON': 'ALL'}.get(m.group(1), m.group(1))
        p.disc(0.48, HELP_DISC)
        dpad(p, lit, HELP_INK, HELP_DIM, 1.08)
        return p.done()
    return None  # Thumbstick_L/R already say L and R; Button_Glow_Lg is a glow


# ---- the archives -----------------------------------------------------------------------------------------------
def build_variants(path, draw):
    """-> (ps zip, xbox zip, directory (offset, size+22, entries), [(name, ps image, xbox image)])"""
    raw = open(path, 'rb').read()
    zf = zipfile.ZipFile(io.BytesIO(raw))
    parts = []  # (name, (ps bytes, xbox bytes, crc, method, usize))
    made = []
    for info in zf.infolist():
        old = zf.read(info)
        w, h = swatch_size(old)
        img = draw(info.filename, w, h)
        if img is None:
            fnl, exl = struct.unpack_from('<HH', raw, info.header_offset + 26)
            start = info.header_offset + 30 + fnl + exl
            comp = raw[start:start + info.compress_size]
            parts.append((info.filename, comp, comp, info.CRC, 8, info.file_size))
            continue
        xbox_img = swatch_decode(old)
        ps = swatch_rgba(old, img)
        xbox = swatch_rgba(old, xbox_img)
        crc = zlib.crc32(ps)
        xbox = forge_crc(xbox, crc)
        parts.append((info.filename, ps, xbox, crc, 0, len(ps)))
        made.append((info.filename, img, xbox_img))
    outs = []
    for v in (1, 2):
        out, cd = bytearray(), bytearray()
        for name, a, b, crc, method, usize in parts:
            data = a if v == 1 else b
            nb = name.encode('ascii')
            off = len(out)
            out += struct.pack('<IHHHHHIIIHH', 0x04034B50, 10, 0, method, 0, 0, crc, len(data), usize, len(nb), 0) + nb
            data_off = len(out)
            out += data
            cd += struct.pack('<IHHHHHHIIIHHHHHII', 0x02014B50, 10, 10, 0, method, 0, 0, crc, len(data), usize, len(nb),
                              8, 0, 0, 0, 0, off) + nb + struct.pack('<HHI', 0x1123, 4, data_off)
        n = len(parts)
        cd_off = len(out)
        out += cd
        out += struct.pack('<IHHHHIIH', 0x06054B50, 0, 0, n, n, len(cd), cd_off, 0)
        z = zipfile.ZipFile(io.BytesIO(bytes(out)))
        for info in z.infolist():
            z.read(info)  # reads back, CRCs right
        outs.append((bytes(out), (cd_off, len(cd) + 22, n)))
    assert len(outs[0][0]) == len(outs[1][0]) and outs[0][1] == outs[1][1]
    return outs[0][0], outs[1][0], outs[0][1], made


def main():
    outdir, mounts = sys.argv[1], sys.argv[2:]
    os.makedirs(outdir, exist_ok=True)
    xg.load(os.path.dirname(os.path.normpath(mounts[0])))
    rel = {'ControllerIcons': ('media\\ui\\textures\\data_bound\\controllericons.zip', controller_icon),
           'Help_Buttons': ('media\\ui\\textures\\data_bound\\help_buttons.zip', help_icon)}
    index, preview, dirs, sizes = [], [], {}, {}
    for name, (suffix, draw) in rel.items():
        src = os.path.join(mounts[0], 'media', 'ui', 'textures', 'data_bound', name + '.zip')
        ps, xbox, d, made = build_variants(src, draw)
        dirs[suffix] = d
        sizes[suffix] = os.path.getsize(src)
        open(os.path.join(outdir, name + '.ps.zip'), 'wb').write(ps)
        open(os.path.join(outdir, name + '.xbox.zip'), 'wb').write(xbox)
        index.append('%s|%d|%s.zip|%s.ps.zip|%s.xbox.zip' % (suffix, sizes[suffix], name, name, name))
        preview += made
        print('%s: %d icons in both sets, %d bytes each (the game\'s: %d)' % (name, len(made), len(ps), sizes[suffix]))
    for m in mounts:
        for name, (suffix, _) in rel.items():
            src = os.path.join(m, 'media', 'ui', 'textures', 'data_bound', name + '.zip')
            if os.path.getsize(src) != sizes[suffix]:
                sys.exit('%s differs from the first game\'s: make its icons separately' % src)
        mp = os.path.join(m, 'media', 'zipmanifest.xml')
        text = open(mp, 'rb').read()
        for suffix, (start, size, entries) in dirs.items():
            key = ('game:\\' + suffix).encode()
            pat = re.compile(rb'(path="' + re.escape(key) + rb'"[^>]*?dirstart=")\d+(" dirsize=")\d+(" direntries=")\d+(")', re.I)
            text, k = pat.subn(lambda mm: mm.group(1) + b'%d' % start + mm.group(2) + b'%d' % size + mm.group(3) +
                               b'%d' % entries + mm.group(4), text)
            if k != 1:
                sys.exit('%s: the line of %s not found' % (mp, suffix))
        name = 'zipmanifest-%d.xml' % os.path.getsize(mp)
        open(os.path.join(outdir, name), 'wb').write(text)
        index.append('media\\zipmanifest.xml|%d|%s' % (os.path.getsize(mp), name))
        print('%s: %s' % (mp, name))
    open(os.path.join(outdir, 'Icons.txt'), 'w', newline='\r\n').write(
        '; DualSense.dll serves these instead of the game\'s files:\n'
        '; path suffix|the original\'s size|the file the game reads|PlayStation set|Xbox set (same layout)\n' +
        '\n'.join(index) + '\n')
    # preview: PlayStation and Xbox, on the game's dark grey, at 2x
    cell, cols = 92, 12
    rows = (len(preview) + cols - 1) // cols
    sheet = Image.new('RGBA', (cols * cell, rows * cell * 2), (52, 52, 52, 255))
    for i, (n, ps, xb) in enumerate(preview):
        for k, img in enumerate((ps, xb)):
            sc = 2 if max(img.size) <= 46 else 1 if max(img.size) <= 64 else 0.625
            im = unpremultiply(img).resize((int(img.width * sc), int(img.height * sc)), Image.NEAREST if sc >= 1 else Image.BOX)
            x, y = (i % cols) * cell, (i // cols) * cell * 2 + k * cell
            sheet.alpha_composite(im, (x + 4, y + 4))
    sheet.save(os.path.join(outdir, 'preview.png'))


if __name__ == '__main__':
    main()
