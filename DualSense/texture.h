/* texture.h -- the game's .swatchbin textures (FH2 1.0.0.64, found 2026-10-13):
     "burG" container, header 0x53 bytes; width/height at 0x40/0x42 (again at 0x46/0x48), mips 0x4C/0x4D, format byte
     0x4F (0x80 set: RGBA8, else BC7), tile code at 0x52 (>>3: 8 linear aligned to 8, 9 1D thin, 23 2D thin), data at
     0x53, sizes at 0x0C (file) and 0x20/0x24 (data). Pixels premultiplied by alpha.
   1D thin: 8x8-element micro tiles in rows, Morton order inside (x0 y0 x1 y1 x2 y2). 2D thin: XOR-linear in the
   coordinate bits (tables.h, from the game's xg_x.dll). */
#pragma once
#include "bc7.h"

static unsigned tex_morton(unsigned x, unsigned y)
{
    return (x & 1) | (y & 1) << 1 | (x >> 1 & 1) << 2 | (y >> 1 & 1) << 3 | (x >> 2 & 1) << 4 | (y >> 2 & 1) << 5;
}

/* where element (x, y) of a texture of ew x eh elements of esize bytes is, in the tile code's layout; -1 unknown */
static long long tex_offset(int code, unsigned ew, unsigned eh, unsigned esize, unsigned x, unsigned y)
{
    unsigned aw = (ew + 7) & ~7u;
    (void)eh;
    if (code == 9)
        return (long long)(((y >> 3) * (aw >> 3) + (x >> 3)) * 64 + tex_morton(x & 7, y & 7)) * esize;
    if (code == 8)
        return (long long)(y * aw + x) * esize;
    if (code == 23)
        for (unsigned i = 0; i < sizeof kLayouts2D / sizeof kLayouts2D[0]; i++) {
            const Layout2D *L = &kLayouts2D[i];
            if (L->ew != ew || L->eh != eh || L->esize != esize)
                continue;
            unsigned o = 0;
            for (unsigned b = 0; b < L->size_log; b++) {
                if (x >> b & 1)
                    o ^= L->xbit[b];
                if (y >> b & 1)
                    o ^= L->ybit[b];
            }
            return o;
        }
    return -1;
}

/* a .swatchbin -> its top mip as RGBA (premultiplied, as stored); NULL when the layout is not one we know */
static unsigned char *tex_decode(const unsigned char *sw, size_t len, int *pw, int *ph)
{
    if (len < 0x53 || memcmp(sw, "burG", 4))
        return NULL;
    unsigned w = *(const unsigned short *)(sw + 0x40), h = *(const unsigned short *)(sw + 0x42);
    int rgba = (sw[0x4F] & 0x80) != 0, code = sw[0x52] >> 3;
    const unsigned char *data = sw + 0x53;
    size_t dlen = len - 0x53;
    unsigned char *out = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, (size_t)w * h * 4);
    if (!out)
        return NULL;
    if (rgba) {
        for (unsigned y = 0; y < h; y++)
            for (unsigned x = 0; x < w; x++) {
                long long o = tex_offset(code, w, h, 4, x, y);
                if (o < 0 || (size_t)o + 4 > dlen)
                    goto fail;
                memcpy(out + ((size_t)y * w + x) * 4, data + o, 4);
            }
    } else {
        unsigned bw = (w + 3) / 4, bh = (h + 3) / 4;
        unsigned char px[64];
        for (unsigned by = 0; by < bh; by++)
            for (unsigned bx = 0; bx < bw; bx++) {
                long long o = tex_offset(code, bw, bh, 16, bx, by);
                if (o < 0 || (size_t)o + 16 > dlen)
                    goto fail;
                bc7_block(data + o, px);
                for (unsigned yy = 0; yy < 4; yy++)
                    for (unsigned xx = 0; xx < 4; xx++) {
                        unsigned X = bx * 4 + xx, Y = by * 4 + yy;
                        if (X < w && Y < h)
                            memcpy(out + ((size_t)Y * w + X) * 4, px + yy * 16 + xx * 4, 4);
                    }
            }
    }
    *pw = (int)w, *ph = (int)h;
    return out;
fail:
    HeapFree(GetProcessHeap(), 0, out);
    return NULL;
}

/* RGBA (premultiplied) -> a .swatchbin like the template: RGBA8, 1D thin, one mip, pad spare bytes at the end */
static unsigned char *tex_encode(const unsigned char *templ, int w, int h, const unsigned char *rgba, int pad, size_t *outLen)
{
    unsigned aw = ((unsigned)w + 7) & ~7u, ah = ((unsigned)h + 7) & ~7u;
    size_t dlen = (size_t)aw * ah * 4 + pad;
    unsigned char *sw = (unsigned char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x53 + dlen);
    if (!sw)
        return NULL;
    memcpy(sw, templ, 0x53);
    *(unsigned *)(sw + 0x0C) = (unsigned)(0x53 + dlen);
    *(unsigned *)(sw + 0x20) = (unsigned)dlen;
    *(unsigned *)(sw + 0x24) = (unsigned)dlen;
    unsigned short dims[6] = {(unsigned short)w, (unsigned short)h, 1, (unsigned short)w, (unsigned short)h, 1};
    memcpy(sw + 0x40, dims, sizeof dims);
    sw[0x4C] = sw[0x4D] = 1;
    sw[0x4F] = 0xAD;
    sw[0x52] = 9 << 3;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            memcpy(sw + 0x53 + tex_offset(9, aw, ah, 4, (unsigned)x, (unsigned)y), rgba + ((size_t)y * w + x) * 4, 4);
    *outLen = 0x53 + dlen;
    return sw;
}

/* ---- CRC-32 (the zip one), and four bytes that make a buffer's CRC a chosen one ---- */
static unsigned g_crc[256];

static void crc_init(void)
{
    for (unsigned i = 0; i < 256; i++) {
        unsigned c = i;
        for (int k = 0; k < 8; k++)
            c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        g_crc[i] = c;
    }
}

static unsigned crc_update(unsigned crc, const unsigned char *p, size_t n)
{
    crc = ~crc;
    while (n--)
        crc = g_crc[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

/* the last 4 bytes of buf[len] chosen so that crc(buf) == target */
static void crc_forge(unsigned char *buf, size_t len, unsigned target)
{
    unsigned reg = ~crc_update(0, buf, len - 4); /* the register before the last 4 bytes */
    unsigned want = ~target, idx[4];
    /* backwards from the wanted register: which table entries the 4 steps used */
    for (int k = 3; k >= 0; k--) {
        int i;
        for (i = 0; i < 256 && (g_crc[i] >> 24) != (want >> 24); i++)
            ;
        idx[k] = (unsigned)i;
        want = ((want ^ g_crc[i]) << 8) & 0xFFFFFFFFu;
    }
    for (int k = 0; k < 4; k++) {
        unsigned char b = (unsigned char)((reg ^ idx[k]) & 0xFF);
        buf[len - 4 + k] = b;
        reg = g_crc[(reg ^ b) & 0xFF] ^ (reg >> 8);
    }
}
