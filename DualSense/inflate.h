/* inflate.h -- raw deflate (RFC 1951) decoding, for the game's zip archives and the art inside DualSense.dll.
   Small and plain: canonical Huffman tables decoded symbol by symbol. */
#pragma once
#include <stddef.h>
#include <string.h>

typedef struct {
    unsigned short counts[16];  /* codes of each length */
    unsigned short symbols[320];
} InfHuff;

typedef struct {
    const unsigned char *in;
    size_t inLen, inPos;
    unsigned bitBuf, bitCnt;
    unsigned char *out;
    size_t outLen, outPos;
    int error;
} InfState;

static int inf_bits(InfState *s, int n)
{
    while (s->bitCnt < (unsigned)n) {
        if (s->inPos >= s->inLen) {
            s->error = 1;
            return 0;
        }
        s->bitBuf |= (unsigned)s->in[s->inPos++] << s->bitCnt;
        s->bitCnt += 8;
    }
    int v = (int)(s->bitBuf & ((1u << n) - 1));
    s->bitBuf >>= n;
    s->bitCnt -= n;
    return v;
}

static void inf_build(InfHuff *h, const unsigned char *lengths, int n)
{
    unsigned short offs[16];
    memset(h->counts, 0, sizeof h->counts);
    for (int i = 0; i < n; i++)
        h->counts[lengths[i]]++;
    h->counts[0] = 0;
    offs[1] = 0;
    for (int i = 1; i < 15; i++)
        offs[i + 1] = offs[i] + h->counts[i];
    for (int i = 0; i < n; i++)
        if (lengths[i])
            h->symbols[offs[lengths[i]]++] = (unsigned short)i;
}

static int inf_decode(InfState *s, const InfHuff *h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= inf_bits(s, 1);
        int count = h->counts[len];
        if (code - count < first)
            return h->symbols[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
        if (s->error)
            return -1;
    }
    s->error = 1;
    return -1;
}

static const unsigned short kInfLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                               35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const unsigned char kInfLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                               3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const unsigned short kInfDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                                                193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                                                6145, 8193, 12289, 16385, 24577};
static const unsigned char kInfDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                                6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static int inf_block(InfState *s, const InfHuff *lit, const InfHuff *dist)
{
    for (;;) {
        int sym = inf_decode(s, lit);
        if (s->error || sym < 0)
            return -1;
        if (sym < 256) {
            if (s->outPos >= s->outLen)
                return -1;
            s->out[s->outPos++] = (unsigned char)sym;
        } else if (sym == 256) {
            return 0;
        } else {
            sym -= 257;
            if (sym >= 29)
                return -1;
            int len = kInfLenBase[sym] + inf_bits(s, kInfLenExtra[sym]);
            int ds = inf_decode(s, dist);
            if (ds < 0 || ds >= 30)
                return -1;
            size_t d = kInfDistBase[ds] + (size_t)inf_bits(s, kInfDistExtra[ds]);
            if (s->error || d > s->outPos || s->outPos + (size_t)len > s->outLen)
                return -1;
            for (int i = 0; i < len; i++, s->outPos++)
                s->out[s->outPos] = s->out[s->outPos - d];
        }
    }
}

/* raw deflate in -> out; returns the bytes written, or -1 */
static long long inflate_raw(const void *in, size_t inLen, void *out, size_t outLen)
{
    InfState s;
    InfHuff lit, dist;
    memset(&s, 0, sizeof s);
    s.in = (const unsigned char *)in, s.inLen = inLen, s.out = (unsigned char *)out, s.outLen = outLen;
    int last;
    do {
        last = inf_bits(&s, 1);
        int type = inf_bits(&s, 2);
        if (s.error)
            return -1;
        if (type == 0) {
            s.bitBuf = 0, s.bitCnt = 0;
            if (s.inPos + 4 > s.inLen)
                return -1;
            unsigned len = s.in[s.inPos] | s.in[s.inPos + 1] << 8;
            s.inPos += 4;
            if (s.inPos + len > s.inLen || s.outPos + len > s.outLen)
                return -1;
            memcpy(s.out + s.outPos, s.in + s.inPos, len);
            s.inPos += len, s.outPos += len;
        } else if (type == 1) {
            unsigned char l[288];
            int i = 0;
            for (; i < 144; i++) l[i] = 8;
            for (; i < 256; i++) l[i] = 9;
            for (; i < 280; i++) l[i] = 7;
            for (; i < 288; i++) l[i] = 8;
            inf_build(&lit, l, 288);
            for (i = 0; i < 30; i++) l[i] = 5;
            inf_build(&dist, l, 30);
            if (inf_block(&s, &lit, &dist))
                return -1;
        } else if (type == 2) {
            static const unsigned char order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            unsigned char lens[320], cl[19];
            InfHuff clh;
            int hlit = inf_bits(&s, 5) + 257, hdist = inf_bits(&s, 5) + 1, hclen = inf_bits(&s, 4) + 4;
            memset(cl, 0, sizeof cl);
            for (int i = 0; i < hclen; i++)
                cl[order[i]] = (unsigned char)inf_bits(&s, 3);
            inf_build(&clh, cl, 19);
            for (int i = 0; i < hlit + hdist;) {
                int sym = inf_decode(&s, &clh);
                if (s.error || sym < 0)
                    return -1;
                if (sym < 16) {
                    lens[i++] = (unsigned char)sym;
                } else {
                    int rep, val = 0;
                    if (sym == 16) {
                        if (!i)
                            return -1;
                        val = lens[i - 1];
                        rep = 3 + inf_bits(&s, 2);
                    } else if (sym == 17)
                        rep = 3 + inf_bits(&s, 3);
                    else
                        rep = 11 + inf_bits(&s, 7);
                    if (i + rep > hlit + hdist)
                        return -1;
                    while (rep--)
                        lens[i++] = (unsigned char)val;
                }
            }
            inf_build(&lit, lens, hlit);
            inf_build(&dist, lens + hlit, hdist);
            if (inf_block(&s, &lit, &dist))
                return -1;
        } else
            return -1;
    } while (!last);
    return (long long)s.outPos;
}
