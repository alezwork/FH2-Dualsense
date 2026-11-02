r"""check.py -- the mod's copies of the game's files (DualSense\cache\), against the game's own:
  every copy a valid zip whose entries' CRCs are right, the entries the game's names in the game's order;
  zipmanifest.xml's directory for each copy where the copy has it;
  the three looks of each icon archive the same size and layout, differing only in pictures;
  the pictures of each look written out as PNG into cache\png\<look>\ to look at.

    python -I check.py CACHE MEDIA
"""
import os
import re
import struct
import sys
import zipfile
import zlib

import numpy as np
from PIL import Image

LOOKS = ('xbox', 'playstation', 'keyboard')
ICONS = ('ControllerIcons', 'Help_Buttons', 'UI_Symbols')
fails = 0


def check(ok, what):
    global fails
    print(('ok: ' if ok else 'FAIL: ') + what)
    fails += not ok


def morton(x, y):
    return (x & 1) | (y & 1) << 1 | (x >> 1 & 1) << 2 | (y >> 1 & 1) << 3 | (x >> 2 & 1) << 4 | (y >> 2 & 1) << 5


def decode_rgba_1d(sw):
    """a .swatchbin the mod wrote (RGBA8, 1D thin) -> an RGBA image, or None"""
    if sw[:4] != b'burG' or not sw[0x4F] & 0x80 or sw[0x52] >> 3 != 9:
        return None
    w, h = struct.unpack_from('<HH', sw, 0x40)
    aw = (w + 7) & ~7
    out = np.zeros((h, w, 4), np.uint8)
    d = sw[0x53:]
    for y in range(h):
        for x in range(w):
            o = (((y >> 3) * (aw >> 3) + (x >> 3)) * 64 + morton(x & 7, y & 7)) * 4
            out[y, x] = list(d[o:o + 4])
    # premultiplied -> straight, for viewing
    a = out[..., 3:4].astype(np.float32)
    rgb = np.where(a > 0, out[..., :3] * 255.0 / np.maximum(a, 1), 0)
    return Image.fromarray(np.concatenate([np.clip(rgb, 0, 255).astype(np.uint8), out[..., 3:4]], 2), 'RGBA')


def zip_ok(path):
    with zipfile.ZipFile(path) as z:
        bad = z.testzip()
        return bad is None, [i.filename for i in z.infolist()]


def main():
    cache, media = sys.argv[1], sys.argv[2]
    manifest = open(os.path.join(cache, 'zipmanifest.xml'), encoding='utf-8-sig').read()

    def dir_of(rel):
        m = re.search(r'path="game:\\media\\%s"[^>]*dirstart="(\d+)" dirsize="(\d+)" direntries="(\d+)"'
                      % re.escape(rel.lower()), manifest)
        return tuple(map(int, m.groups())) if m else None

    def end_record(data):
        n, size, start = struct.unpack_from('<HII', data, len(data) - 22 + 10)
        return start, size + 22, n

    copies = [('UI.zip', 'UI.zip')] + [(f'stripped\\stringtables\\{f}', f) for f in sorted(os.listdir(cache))
                                       if f.endswith('.zip') and f != 'UI.zip']
    for rel, name in copies:
        p = os.path.join(cache, name)
        ok, names = zip_ok(p)
        with zipfile.ZipFile(os.path.join(media, rel)) as g:
            same = names == [i.filename for i in g.infolist()]
        data = open(p, 'rb').read()
        check(ok and same and dir_of(rel) == end_record(data), f'{rel}: a valid zip, the game\'s entries, the manifest right')
    with zipfile.ZipFile(os.path.join(cache, 'UI.zip')) as z:
        x = z.read('Scenes/Anthem/A_ControllerOptions.xaml').decode('utf-8', 'replace')
        check('DS_Selected' in x, 'UI.zip: the controller settings page has the DualSense\'s section')
    with zipfile.ZipFile(os.path.join(cache, 'RU.zip')) as z:
        s = z.read('MyProfile.str')
        check('Автоматические иконки кнопок'.encode('utf-16-le') in s, 'RU.zip: MyProfile.str has the Russian texts')

    for arch in ICONS:
        rel = f'ui\\textures\\data_bound\\{arch}.zip'
        datas = [open(os.path.join(cache, look, arch + '.zip'), 'rb').read() for look in LOOKS]
        check(len(set(map(len, datas))) == 1, f'{arch}: the three looks the same size ({len(datas[0])} bytes)')
        for look, d in zip(LOOKS, datas):
            ok, names = zip_ok(os.path.join(cache, look, arch + '.zip'))
            check(ok and dir_of(rel) == end_record(d), f'{arch} ({look}): a valid zip, the manifest right')
        z = [zipfile.ZipFile(os.path.join(cache, look, arch + '.zip')) for look in LOOKS]
        cds = [d[end_record(d)[0]:] for d in datas]
        check(cds[0] == cds[1] == cds[2], f'{arch}: the same central directory in every look (the same CRCs)')
        changed = {look: 0 for look in LOOKS}
        for info in z[0].infolist():
            parts = [zz.read(info.filename) for zz in z]
            if parts[0] == parts[1] == parts[2]:
                continue
            for look, part in zip(LOOKS, parts):
                img = decode_rgba_1d(part)
                if img is None:
                    continue
                if part != parts[0] or look == 'xbox':
                    changed[look] += part != parts[0]
                    out = os.path.join(cache, 'png', look, arch)
                    os.makedirs(out, exist_ok=True)
                    img.save(os.path.join(out, os.path.basename(info.filename).replace('.swatchbin', '.png')))
        print(f'   {arch}: pictures that differ from the Xbox look: PlayStation {changed["playstation"]}, keyboard '
              f'{changed["keyboard"]}')
        check(changed['playstation'] > 0 or changed['keyboard'] > 0, f'{arch}: other looks have pictures')
    print('PASSED' if not fails else f'FAILED ({fails})')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
