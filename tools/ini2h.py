r"""ini2h.py IN.ini OUT.h -- the ini as the C string kDefaultIni, which the DLL writes when its ini is missing.
Every line of the ini is one C string; quotes, backslashes, '?' and non-ASCII as octal escapes (never longer than 3)."""
import os
import sys


def lit(line):
    out = []
    for b in line.encode('utf-8'):
        out.append(chr(b) if 32 <= b < 127 and b not in (34, 92, 63) else '\\%03o' % b)
    return '"' + ''.join(out) + '\\r\\n"'


def main():
    src, dst = sys.argv[1], sys.argv[2]
    text = open(src, encoding='utf-8').read().replace('\r\n', '\n').rstrip('\n')
    lines = ['/* made by tools\\ini2h.py from %s -- do not edit */' % os.path.basename(src), '#pragma once',
             'static const char kDefaultIni[] =']
    lines += ['    ' + lit(l) for l in text.split('\n')]
    lines[-1] += ';'
    open(dst, 'w', newline='\n').write('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
