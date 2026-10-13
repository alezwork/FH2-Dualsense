/* png.h -- reading the PNG pictures in DualSense.icons: 8-bit RGBA (or RGB / grey / grey+alpha), not interlaced.
   Returns RGBA premultiplied by alpha, as the game keeps its textures. */
#pragma once
#include "inflate.h"

static unsigned png_be32(const unsigned char *p) { return (unsigned)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

static int png_paeth(int a, int b, int c)
{
    int p = a + b - c, pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p, pc = p > c ? p - c : c - p;
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

static unsigned char *png_decode(const unsigned char *png, size_t len, int *pw, int *ph)
{
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (len < 33 || memcmp(png, sig, 8))
        return NULL;
    unsigned w = png_be32(png + 16), h = png_be32(png + 20);
    int depth = png[24], type = png[25], interlace = png[28];
    int ch = type == 6 ? 4 : type == 2 ? 3 : type == 4 ? 2 : type == 0 ? 1 : 0;
    if (!ch || depth != 8 || interlace || w > 4096 || h > 4096)
        return NULL;
    /* the IDAT chunks together */
    size_t zlen = 0;
    for (size_t p = 8; p + 12 <= len;) {
        unsigned n = png_be32(png + p);
        if (!memcmp(png + p + 4, "IDAT", 4))
            zlen += n;
        p += 12 + n;
    }
    unsigned char *z = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, zlen + 1);
    size_t stride = (size_t)w * ch, rawLen = (stride + 1) * h;
    unsigned char *raw = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, rawLen + 1);
    unsigned char *out = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, (size_t)w * h * 4 + 1);
    if (!z || !raw || !out)
        goto fail;
    zlen = 0;
    for (size_t p = 8; p + 12 <= len;) {
        unsigned n = png_be32(png + p);
        if (!memcmp(png + p + 4, "IDAT", 4) && p + 12 + n <= len) {
            memcpy(z + zlen, png + p + 8, n);
            zlen += n;
        }
        p += 12 + n;
    }
    /* zlib: 2 bytes of header, the deflate stream, the Adler-32 */
    if (zlen < 6 || inflate_raw(z + 2, zlen - 2, raw, rawLen) != (long long)rawLen)
        goto fail;
    for (unsigned y = 0; y < h; y++) {
        unsigned char *row = raw + y * (stride + 1), f = row[0];
        unsigned char *cur = row + 1, *prev = y ? raw + (y - 1) * (stride + 1) + 1 : NULL;
        for (size_t i = 0; i < stride; i++) {
            int a = i >= (size_t)ch ? cur[i - ch] : 0, b = prev ? prev[i] : 0, c = prev && i >= (size_t)ch ? prev[i - ch] : 0;
            switch (f) {
            case 1: cur[i] = (unsigned char)(cur[i] + a); break;
            case 2: cur[i] = (unsigned char)(cur[i] + b); break;
            case 3: cur[i] = (unsigned char)(cur[i] + ((a + b) >> 1)); break;
            case 4: cur[i] = (unsigned char)(cur[i] + png_paeth(a, b, c)); break;
            }
        }
        for (unsigned x = 0; x < w; x++) {
            unsigned char *s = cur + (size_t)x * ch, *d = out + ((size_t)y * w + x) * 4;
            unsigned r = s[0], g = ch >= 3 ? s[1] : s[0], bl = ch >= 3 ? s[2] : s[0], al = ch == 4 ? s[3] : ch == 2 ? s[1] : 255;
            d[0] = (unsigned char)((r * al + 127) / 255);
            d[1] = (unsigned char)((g * al + 127) / 255);
            d[2] = (unsigned char)((bl * al + 127) / 255);
            d[3] = (unsigned char)al;
        }
    }
    HeapFree(GetProcessHeap(), 0, z);
    HeapFree(GetProcessHeap(), 0, raw);
    *pw = (int)w, *ph = (int)h;
    return out;
fail:
    if (z)
        HeapFree(GetProcessHeap(), 0, z);
    if (raw)
        HeapFree(GetProcessHeap(), 0, raw);
    if (out)
        HeapFree(GetProcessHeap(), 0, out);
    return NULL;
}
