/* bc7.h -- BC7 block decoding (all 8 modes), for the game's own icons (the Xbox set). Tables: tables.h. */
#pragma once
#include "tables.h"

typedef struct {
    const unsigned char *b;
    int pos;
} Bc7Bits;

static unsigned bc7_get(Bc7Bits *r, int n)
{
    unsigned v = 0;
    for (int i = 0; i < n; i++, r->pos++)
        v |= (unsigned)((r->b[r->pos >> 3] >> (r->pos & 7)) & 1) << i;
    return v;
}

static unsigned char bc7_expand(unsigned v, int bits)
{
    v <<= 8 - bits;
    return (unsigned char)(v | (v >> bits));
}

static unsigned char bc7_mix(int e0, int e1, int idx, int bits)
{
    static const unsigned char w2[4] = {0, 21, 43, 64};
    static const unsigned char w3[8] = {0, 9, 18, 27, 37, 46, 55, 64};
    static const unsigned char w4[16] = {0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64};
    int w = bits == 2 ? w2[idx] : bits == 3 ? w3[idx] : w4[idx];
    return (unsigned char)(((64 - w) * e0 + w * e1 + 32) >> 6);
}

/* one 16-byte block -> 4x4 RGBA (out[row * 16 + col * 4]) */
static void bc7_block(const unsigned char *blk, unsigned char *out)
{
    /*                         NS PB RB ISB CB AB EPB SPB IB IB2 */
    static const unsigned char kModes[8][10] = {
        {3, 4, 0, 0, 4, 0, 1, 0, 3, 0}, {2, 6, 0, 0, 6, 0, 0, 1, 3, 0}, {3, 6, 0, 0, 5, 0, 0, 0, 2, 0},
        {2, 6, 0, 0, 7, 0, 1, 0, 2, 0}, {1, 0, 2, 1, 5, 6, 0, 0, 2, 3}, {1, 0, 2, 0, 7, 8, 0, 0, 2, 2},
        {1, 0, 0, 0, 7, 7, 1, 0, 4, 0}, {2, 6, 0, 0, 5, 5, 1, 0, 2, 0}};
    Bc7Bits r = {blk, 0};
    int mode = 0;
    while (mode < 8 && !bc7_get(&r, 1))
        mode++;
    if (mode == 8) {
        memset(out, 0, 64);
        return;
    }
    const unsigned char *m = kModes[mode];
    int ns = m[0], part = (int)bc7_get(&r, m[1]), rot = (int)bc7_get(&r, m[2]), isb = (int)bc7_get(&r, m[3]);
    int cb = m[4], ab = m[5], ne = ns * 2;
    unsigned ep[6][4];
    for (int c = 0; c < 3; c++)
        for (int e = 0; e < ne; e++)
            ep[e][c] = bc7_get(&r, cb);
    for (int e = 0; e < ne; e++)
        ep[e][3] = ab ? bc7_get(&r, ab) : 255;
    int cbits = cb, abits = ab;
    if (m[6]) { /* a p-bit per endpoint */
        for (int e = 0; e < ne; e++) {
            unsigned p = bc7_get(&r, 1);
            for (int c = 0; c < 3; c++)
                ep[e][c] = ep[e][c] << 1 | p;
            if (ab)
                ep[e][3] = ep[e][3] << 1 | p;
        }
        cbits++;
        if (ab)
            abits++;
    } else if (m[7]) { /* a p-bit per subset */
        for (int s = 0; s < ns; s++) {
            unsigned p = bc7_get(&r, 1);
            for (int e = 2 * s; e < 2 * s + 2; e++)
                for (int c = 0; c < 3; c++)
                    ep[e][c] = ep[e][c] << 1 | p;
        }
        cbits++;
    }
    unsigned char col[6][4];
    for (int e = 0; e < ne; e++) {
        for (int c = 0; c < 3; c++)
            col[e][c] = bc7_expand(ep[e][c], cbits);
        col[e][3] = ab ? bc7_expand(ep[e][3], abits) : 255;
    }
    /* subsets and anchors */
    unsigned char sub[16];
    int anchor[3] = {0, 0, 0};
    for (int i = 0; i < 16; i++)
        sub[i] = ns == 2 ? kBc7P2[part][i] : ns == 3 ? kBc7P3[part][i] : 0;
    if (ns == 2)
        anchor[1] = kBc7A2[part];
    else if (ns == 3)
        anchor[1] = kBc7A3[part][0], anchor[2] = kBc7A3[part][1];
    /* indices */
    unsigned idx1[16], idx2[16];
    int ib = m[8], ib2 = m[9];
    for (int i = 0; i < 16; i++) {
        int isAnchor = i == anchor[sub[i]];
        idx1[i] = bc7_get(&r, ib - isAnchor);
    }
    if (ib2)
        for (int i = 0; i < 16; i++)
            idx2[i] = bc7_get(&r, ib2 - (i == 0));
    for (int i = 0; i < 16; i++) {
        int s = sub[i];
        unsigned char *px = out + i * 4;
        const unsigned char *e0 = col[2 * s], *e1 = col[2 * s + 1];
        if (!ib2) {
            for (int c = 0; c < 4; c++)
                px[c] = bc7_mix(e0[c], e1[c], (int)idx1[i], ib);
        } else {
            /* modes 4 and 5: colour and alpha indices apart; mode 4's selector swaps them */
            int ci = (int)idx1[i], ai = (int)idx2[i], cbi = ib, abi = ib2;
            if (isb) {
                int t = ci;
                ci = ai, ai = t;
                cbi = ib2, abi = ib;
            }
            for (int c = 0; c < 3; c++)
                px[c] = bc7_mix(e0[c], e1[c], ci, cbi);
            px[3] = bc7_mix(e0[3], e1[3], ai, abi);
        }
        if (rot) { /* the alpha channel swapped with R, G or B */
            unsigned char t = px[3];
            px[3] = px[rot - 1];
            px[rot - 1] = t;
        }
    }
}
