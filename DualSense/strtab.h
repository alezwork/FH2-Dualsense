/* strtab.h -- our labels in the game's own string tables, in every language it has (media\stripped\stringtables\
   <LANG>.zip\<Table>.str, "LSB2"): the XAML then says {local:Loc Table.Key} as the game's own rows do, and the game
   shows the text of the language it runs in, whatever picked it (the system, Azimuth, ExtraOptions).
   A table: a 0x24-byte header ('LSB2', 0, 0x200, 0x24, 4, the offsets of its 4 sections); section 0 -- the only one
   the game reads (FH2 1.0.0.64: 0x1246F77) -- u32 its size, u32 count, (count + 1) x {u16 key id, u32 offset in
   characters} sorted by id (the last: 0xFFFF and the end), then the strings (UTF-16, each with its 0) in that order
   and one more 0. A key's id: h = 0xFFFF, then for each character h = rotl16(h ^ tolower(c), 7). Sections 1..3 (the
   keys' names, not read) stay as they are, moved along. */
#pragma once

/* the game's languages: media\stripped\stringtables\<this>.zip (cz and HU have English text, as the game's own) */
static const char *const kLocLang[] = {"EN", "GB", "DE", "FR", "IT", "ES", "MX", "br", "PL",
                                       "nl", "RU", "JP", "KO", "CHT", "cz", "HU", "DEV", "LOC"};
#define LOC_LANGS ((int)(sizeof kLocLang / sizeof kLocLang[0]))

static WORD loc_id(const char *key)
{
    WORD h = 0xFFFF;
    for (; *key; key++) {
        BYTE c = (BYTE)*key;
        if (c >= 'A' && c <= 'Z')
            c += 32;
        h ^= c;
        h = (WORD)((h << 7) | (h >> 9));
    }
    return h;
}

typedef struct {
    WORD id;
    const WCHAR *s; /* with its 0 */
    DWORD n;        /* characters, the 0 too */
} LocEntry;

/* the table with the keys added (texts UTF-8), or NULL; *added: how many went in -- 0: all there already, -1: not a
   table, or a key's id is another key's */
static char *loc_add(const char *str, size_t len, size_t *outLen, const char *const *keys, const char *const *texts, int n,
                     int *added)
{
    const BYTE *b = (const BYTE *)str;
    *added = -1;
    if (len < 0x24 + 8 || *(const DWORD *)b != 0x3242534C)
        return NULL;
    DWORD s0 = *(const DWORD *)(b + 0x14), size, count;
    if (s0 < 0x24 || s0 + 8 > len)
        return NULL;
    size = *(const DWORD *)(b + s0), count = *(const DWORD *)(b + s0 + 4);
    DWORD pool = s0 + 8 + 6 * (count + 1);
    if (s0 + size > len || pool > s0 + size || count > 60000)
        return NULL;
    DWORD poolChars = (s0 + size - pool) / 2;
    const WCHAR *ps = (const WCHAR *)(b + pool);
    LocEntry *e = (LocEntry *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (count + n) * sizeof *e);
    WCHAR *mine = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (size_t)n * 512 * sizeof(WCHAR));
    if (!e || !mine)
        return NULL;
    DWORD m = 0;
    for (DWORD i = 0; i < count; i++) {
        const BYTE *r = b + s0 + 8 + 6 * i;
        DWORD off = *(const DWORD *)(r + 2), k = off;
        while (k < poolChars && ps[k])
            k++;
        if (k >= poolChars) {
            HeapFree(GetProcessHeap(), 0, e);
            HeapFree(GetProcessHeap(), 0, mine);
            return NULL;
        }
        e[m].id = *(const WORD *)r, e[m].s = ps + off, e[m].n = k + 1 - off;
        m++;
    }
    *added = 0;
    for (int j = 0; j < n; j++) {
        WORD id = loc_id(keys[j]);
        WCHAR *w = mine + j * 512;
        int wn = MultiByteToWideChar(CP_UTF8, 0, texts[j], -1, w, 512);
        DWORD at = 0;
        while (at < m && e[at].id < id)
            at++;
        if (id == 0xFFFF || wn <= 0 || (at < m && e[at].id == id && (e[at].n != (DWORD)wn || memcmp(e[at].s, w, wn * 2)))) {
            HeapFree(GetProcessHeap(), 0, e);
            HeapFree(GetProcessHeap(), 0, mine);
            *added = -1;
            return NULL; /* taken by another key */
        }
        if (at < m && e[at].id == id)
            continue; /* there already */
        memmove(&e[at + 1], &e[at], (m - at) * sizeof *e);
        e[at].id = id, e[at].s = w, e[at].n = (DWORD)wn;
        m++;
        (*added)++;
    }
    char *out = NULL;
    if (*added) {
        DWORD chars = 1;
        for (DWORD i = 0; i < m; i++)
            chars += e[i].n;
        DWORD nsize = 8 + 6 * (m + 1) + 2 * chars;
        LONG delta = (LONG)nsize - (LONG)size;
        *outLen = len + delta;
        out = (char *)HeapAlloc(GetProcessHeap(), 0, *outLen + 2);
        BYTE *o = (BYTE *)out;
        memcpy(o, b, s0);
        for (int k = 5; k < 9; k++) {
            DWORD *sec = (DWORD *)(o + 4 * k);
            if (*sec > s0)
                *sec += delta;
        }
        BYTE *q = o + s0;
        *(DWORD *)q = nsize, *(DWORD *)(q + 4) = m;
        q += 8;
        DWORD off = 0;
        for (DWORD i = 0; i < m; i++, q += 6) {
            *(WORD *)q = e[i].id, *(DWORD *)(q + 2) = off;
            off += e[i].n;
        }
        *(WORD *)q = 0xFFFF, *(DWORD *)(q + 2) = off;
        q += 6;
        for (DWORD i = 0; i < m; i++) {
            memcpy(q, e[i].s, e[i].n * 2);
            q += e[i].n * 2;
        }
        *(WCHAR *)q = 0;
        q += 2;
        memcpy(q, b + s0 + size, len - (s0 + size));
        out[*outLen] = out[*outLen + 1] = 0;
    }
    HeapFree(GetProcessHeap(), 0, e);
    HeapFree(GetProcessHeap(), 0, mine);
    return out;
}
