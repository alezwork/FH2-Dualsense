r"""bin2h.py -- a file as a C array in a header (DualSense.icons inside DualSense.dll).

    python bin2h.py FILE HEADER

kIconsPack[] holds the bytes, ICONS_PACK_LEN their count. Without the file: an empty pack (the DLL then has no
PlayStation pictures of its own, and takes mods\DualSense.icons when there).
"""
import os
import sys


def main():
    src, out = sys.argv[1], sys.argv[2]
    data = open(src, 'rb').read() if os.path.exists(src) else b''
    lines = ['/* %s -- made by tools\\bin2h.py from %s; do not edit */' % (os.path.basename(out), os.path.basename(src)),
             '#define ICONS_PACK_LEN %du' % len(data),
             'static const unsigned char kIconsPack[%d] = {' % max(len(data), 1)]
    for i in range(0, len(data), 24):
        lines.append(','.join(str(b) for b in data[i:i + 24]) + ',')
    if not data:
        lines.append('0')
    lines.append('};')
    text = '\n'.join(lines) + '\n'
    if not os.path.exists(out) or open(out, encoding='ascii').read() != text:
        open(out, 'w', encoding='ascii', newline='\n').write(text)
    print('%s: %d bytes' % (out, len(data)))


if __name__ == '__main__':
    main()
