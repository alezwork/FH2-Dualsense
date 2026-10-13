r"""make_pack.py -- DualSense.icons, the icon file that goes next to DualSense.dll (a zip of PNG pictures):

  ps/<archive>/<entry>.png   the PlayStation look of each of the game's button icons (ControllerIcons.zip,
                             Help_Buttons.zip), the size of the game's own; RGBA, any picture of that size will do
  diagram/clear.png          512 x 512: where the controls screen's drawing (UI_Symbols\Help_Controller) loses the
                             Xbox marks (the coloured letters, LB/RB/LT/RT, the View mark, the Xbox logo)
  diagram/ps.png             512 x 512: the PlayStation marks drawn in their place
  diagram/slots.txt          where the marks are, for the keyboard look (the DLL writes the keys there)
  readme.txt

The Xbox look is always the game's own; the keyboard look the DLL draws from mods\Keyboard.ini.

    python -I make_pack.py OUT.icons MOUNT [GAME_FOLDER]   (GAME_FOLDER with EmbeddedXvd, for the game's drawing)
"""
import io
import math
import os
import struct
import sys
import zipfile

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'xg'))
import make_ps_icons as icons  # noqa: E402
import xg  # noqa: E402

ARCHIVES = (('ControllerIcons', icons.controller_icon), ('Help_Buttons', icons.help_icon))
SS = 4
# the controls screen's drawing (512 x 512): the marks, measured on the game's own
FACE = {'Y': ((377.5, 207.5), 'triangle'), 'B': ((409.0, 240.2), 'circle'), 'X': ((345.5, 239.2), 'square'),
        'A': ((378.0, 269.8), 'cross')}
BUMPERS = {'LT': ((134.5, 144.0), -21, (124, 135, 146, 154)), 'LB': ((171.0, 155.5), 0, (162, 149, 181, 162)),
           'RB': ((345.5, 155.5), 0, (336, 149, 356, 162)), 'RT': ((380.0, 144.0), 21, (369, 135, 391, 154))}
VIEW = (227.5, 249.7)
MENU = (288.6, 249.5)
LOGO = (256.5, 190.2)


def png(img):
    b = io.BytesIO()
    img.save(b, 'PNG', optimize=True)
    return b.getvalue()


def components(mask):
    """4-connected components of a boolean array: list of (ys, xs)"""
    h, w = mask.shape
    seen = np.zeros_like(mask)
    out = []
    for y0, x0 in zip(*np.nonzero(mask)):
        if seen[y0, x0]:
            continue
        stack, ys, xs = [(y0, x0)], [], []
        seen[y0, x0] = True
        while stack:
            y, x = stack.pop()
            ys.append(y)
            xs.append(x)
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                yy, xx = y + dy, x + dx
                if 0 <= yy < h and 0 <= xx < w and mask[yy, xx] and not seen[yy, xx]:
                    seen[yy, xx] = True
                    stack.append((yy, xx))
        out.append((np.array(ys), np.array(xs)))
    return out


def diagram_mask(orig):
    """what to clear on the game's drawing: the letters, the bumper labels, the View mark, the logo"""
    a = np.array(icons.unpremultiply(orig)).astype(int)
    alpha = a[..., 3]
    rgb = a[..., :3]
    sat = (rgb.max(axis=2) - rgb.min(axis=2)) > 50
    mask = np.zeros(alpha.shape, bool)
    # the coloured letters
    for (cx, cy), _ in FACE.values():
        yy, xx = np.mgrid[0:512, 0:512]
        inside = (xx - cx) ** 2 + (yy - cy) ** 2 <= 10.2 ** 2  # inside the button's ring
        mask |= inside & sat & (alpha > 10)
    # the bumper labels and the View mark: small parts wholly inside their box (the outlines run out of it)
    ink = alpha > 24
    boxes = [b[2] for b in BUMPERS.values()]
    boxes.append((int(VIEW[0] - 8), int(VIEW[1] - 8), int(VIEW[0] + 9), int(VIEW[1] + 9)))
    boxes.append((int(LOGO[0] - 22), int(LOGO[1] - 22), int(LOGO[0] + 23), int(LOGO[1] + 23)))
    for x0, y0, x1, y1 in boxes:
        sub = np.zeros_like(ink)
        sub[y0 - 3:y1 + 3, x0 - 3:x1 + 3] = ink[y0 - 3:y1 + 3, x0 - 3:x1 + 3]
        for ys, xs in components(sub):
            if xs.min() >= x0 and xs.max() < x1 and ys.min() >= y0 and ys.max() < y1:
                mask[ys, xs] = True
    # a pixel round it, for the anti-aliased edge
    grown = mask.copy()
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            grown |= np.roll(np.roll(mask, dy, 0), dx, 1)
    m = np.zeros((512, 512, 4), np.uint8)
    m[grown] = (255, 255, 255, 255)
    return Image.fromarray(m, 'RGBA')


def diagram_ps():
    """the PlayStation marks, straight alpha"""
    img = Image.new('RGBA', (512 * SS, 512 * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    def P(x, y):
        return (x * SS, y * SS)
    W = (255, 255, 255, 255)
    for name, ((cx, cy), kind) in FACE.items():
        col = tuple(icons.FACE[kind])
        g, w = 8.6, 2.7
        if kind == 'cross':
            k = g * 0.72
            for s in (1, -1):
                d.line([P(cx - k, cy - s * k), P(cx + k, cy + s * k)], fill=col, width=int(w * SS))
                for ex, ey in ((cx - k, cy - s * k), (cx + k, cy + s * k)):
                    r = w / 2
                    d.ellipse(P(ex - r, ey - r) + P(ex + r, ey + r), fill=col)
        elif kind == 'circle':
            r = g * 0.84
            d.ellipse(P(cx - r, cy - r) + P(cx + r, cy + r), outline=col, width=int(w * SS))
        elif kind == 'square':
            k = g * 0.74
            d.rectangle(P(cx - k, cy - k) + P(cx + k, cy + k), outline=col, width=int(w * SS))
        else:
            r = g * 1.02
            dy = r * 0.10
            outer = [P(cx, cy - r + dy), P(cx + r * 0.866, cy + r * 0.5 + dy), P(cx - r * 0.866, cy + r * 0.5 + dy)]
            ri = r - w * 2
            inner = [P(cx, cy - ri + dy), P(cx + ri * 0.866, cy + ri * 0.5 + dy), P(cx - ri * 0.866, cy + ri * 0.5 + dy)]
            d.polygon(outer, fill=col)
            d.polygon(inner, fill=(0, 0, 0, 0))
    for name, ((cx, cy), angle, _) in BUMPERS.items():
        label = {'LT': 'L2', 'LB': 'L1', 'RB': 'R1', 'RT': 'R2'}[name]
        t = Image.new('RGBA', (40 * SS, 20 * SS), (0, 0, 0, 0))
        ImageDraw.Draw(t).text((20 * SS, 10 * SS), label, font=icons.font(10.5 * SS), fill=W, anchor='mm')
        t = t.rotate(-angle, resample=Image.BICUBIC, expand=False)
        img.alpha_composite(t, (int(cx * SS - 20 * SS), int(cy * SS - 10 * SS)))
    # View: the touchpad
    cx, cy = VIEW
    d.rounded_rectangle(P(cx - 6.2, cy - 3.6) + P(cx + 6.2, cy + 3.6), radius=1.6 * SS, fill=W)
    # the PS button in place of the logo: a ring
    cx, cy = LOGO
    d.ellipse(P(cx - 13, cy - 13) + P(cx + 13, cy + 13), outline=W, width=int(2.4 * SS))
    return img.resize((512, 512), Image.BOX)


def main():
    out, mount = sys.argv[1], sys.argv[2]
    game = sys.argv[3] if len(sys.argv) > 3 else os.path.dirname(os.path.normpath(mount))
    xg.load(game)
    media = os.path.join(mount, 'media', 'ui', 'textures', 'data_bound')
    z = zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED)
    n = 0
    for arc, draw in ARCHIVES:
        src = zipfile.ZipFile(os.path.join(media, arc + '.zip'))
        for name in src.namelist():
            w, h = struct.unpack_from('<HH', src.read(name), 0x40)
            img = draw(name, w, h)
            if img is None:
                continue
            z.writestr('ps/%s/%s' % (arc, name.replace('.swatchbin', '.png')), png(icons.unpremultiply(img)))
            n += 1
    us = zipfile.ZipFile(os.path.join(media, 'UI_Symbols.zip'))
    orig = icons.swatch_decode(us.read('Help_Controller.swatchbin'))
    z.writestr('diagram/clear.png', png(diagram_mask(orig)))
    z.writestr('diagram/ps.png', png(diagram_ps()))
    slots = ['; the controls screen drawing: where each mark is (centre x y, width height, angle) -- the keyboard look',
             '; writes the key there']
    for name, ((cx, cy), _) in FACE.items():
        slots.append('%s %.1f %.1f 20 16 0' % (name, cx, cy))
    for name, ((cx, cy), angle, _) in BUMPERS.items():
        slots.append('%s %.1f %.1f 30 11 %d' % (name, cx, cy, angle))
    slots.append('View %.1f %.1f 18 10 0' % VIEW)
    slots.append('Menu %.1f %.1f 18 10 0' % MENU)
    z.writestr('diagram/slots.txt', '\r\n'.join(slots) + '\r\n')
    z.writestr('readme.txt', (
        'DualSense.icons -- the PlayStation button icons for the DualSense mod (a zip; keep the name).\r\n'
        'ps\\<archive>\\<name>.png: change any picture, keep its size (RGBA). diagram\\: the controls screen.\r\n'
        'The Xbox icons are the game\'s own; the keyboard ones DualSense.dll draws from mods\\Keyboard.ini.\r\n'))
    z.close()
    print('%s: %d icons + the controls screen drawing, %d KB' % (out, n, os.path.getsize(out) >> 10))
    # a preview of the drawing, both ways
    m = np.array(diagram_mask(orig))[..., 3] > 0
    base = np.array(icons.unpremultiply(orig))
    base[m] = 0
    pv = Image.new('RGBA', (1024, 512), (70, 80, 90, 255))
    pv.alpha_composite(icons.unpremultiply(orig), (0, 0))
    ps = Image.fromarray(base, 'RGBA')
    ps.alpha_composite(diagram_ps())
    pv.alpha_composite(ps, (512, 0))
    pv.save(os.path.splitext(out)[0] + '.preview.png')


if __name__ == '__main__':
    main()
