/* files.h -- the game's files the mod changes, made from the game's own and given to the game in their place, from
   the mod's own folder (DualSense\cache\ next to azimuth.exe). Nothing is written into the game's folder.
     media\UI.zip                                the controller settings page with the DualSense's section (menu.h)
     media\stripped\stringtables\<lang>.zip      MyProfile.str with the section's texts, in every language the game has
     media\ui\textures\data_bound\ControllerIcons.zip, Help_Buttons.zip, UI_Symbols.zip
                                                 the button icons and the controls screen's drawing, in three looks:
                                                 Xbox (the game's own), PlayStation (DualSense.icons) and keyboard (the
                                                 keys of [Keys], keyicons.h) -- one copy per look, cache\<look>\
     media\zipmanifest.xml                       the new directories of those archives (the game finds a zip's entries
                                                 through it, not through the zip's own end record)
   The copies are made once (the next starts only check that the game's files, this DLL and the settings that shape
   them are the same: cache\key.txt). Azimuth is given them when it opens, looks up or finds the game's: its imports of
   CreateFileW, GetFileAttributesW, GetFileAttributesExW and FindFirstFileExW go through ours first (hooks.h). A copy is
   only given while the game's file is the size it was made from; an icon archive is the copy of the look in use now
   (icons.h switches it, and the icons already in the game's memory).
   The three looks of an icon archive have the same layout: every changed icon stored as RGBA in 1D tiling, the same
   size in every look, with four spare bytes that give each look the CRC of the first -- so one zipmanifest.xml is
   right for all of them, and a look can be written over another in memory. Entries the mod does not change are
   copied as they are, compressed.
   Works with Extra Options and without it: Extra Options' mediaoverride, mediapc and mods folders still come first
   for any file they have. The files 3.0 wrote into the game's Mount\mediaoverride\ (DualSense\mediaoverride.txt)
   are taken away once, if they are still as 3.0 left them. */
#pragma once
#include "inflate.h"
#include "texture.h"
#include "png.h"
#include "keyicons.h"

#define NSETS 3
enum { LOOK_XBOX, LOOK_PS, LOOK_KEYS };
static const char *const kSetName[NSETS] = {"Xbox", "PlayStation", "keyboard"};
static const WCHAR *const kSetDir[NSETS] = {L"xbox\\", L"playstation\\", L"keyboard\\"};
static volatile LONG g_iconSet; /* the look the game gets now: LOOK_* */

typedef struct {
    char name[160];
    WORD method;
    DWORD crc, csize, usize, dataOff;
} ZEnt;

static BYTE *load_file(const WCHAR *path, DWORD *len)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    BYTE *p = NULL;
    DWORD n = 0;
    *len = 0;
    if (h == INVALID_HANDLE_VALUE)
        return NULL;
    *len = GetFileSize(h, NULL);
    if (*len != INVALID_FILE_SIZE && (p = (BYTE *)HeapAlloc(GetProcessHeap(), 0, *len + 2)) != NULL &&
        (!ReadFile(h, p, *len, &n, NULL) || n != *len)) {
        HeapFree(GetProcessHeap(), 0, p);
        p = NULL;
    }
    CloseHandle(h);
    if (p)
        p[*len] = p[*len + 1] = 0;
    return p;
}

static int save_file(const WCHAR *path, const void *data, DWORD len)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD w = 0;
    int ok;
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    ok = WriteFile(h, data, len, &w, NULL) && w == len;
    CloseHandle(h);
    return ok;
}

/* a zip's central directory (the game's: no comment, the end record last) */
static int zip_parse(const BYTE *z, DWORD len, ZEnt **out)
{
    if (len < 22 || *(const DWORD *)(z + len - 22) != 0x06054B50)
        return -1;
    WORD n = *(const WORD *)(z + len - 22 + 10);
    DWORD cd = *(const DWORD *)(z + len - 22 + 16);
    ZEnt *e = (ZEnt *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (n + 1) * sizeof *e);
    if (!e)
        return -1;
    for (WORD k = 0; k < n; k++) {
        if (cd + 46 > len || *(const DWORD *)(z + cd) != 0x02014B50)
            goto bad;
        const BYTE *c = z + cd;
        WORD nl = *(const WORD *)(c + 28), xl = *(const WORD *)(c + 30), cl = *(const WORD *)(c + 32);
        DWORD lho = *(const DWORD *)(c + 42);
        e[k].method = *(const WORD *)(c + 10);
        e[k].crc = *(const DWORD *)(c + 16);
        e[k].csize = *(const DWORD *)(c + 20);
        e[k].usize = *(const DWORD *)(c + 24);
        memcpy(e[k].name, c + 46, nl < 159 ? nl : 159);
        for (char *q = e[k].name; *q; q++)
            if (*q == '\\')
                *q = '/';
        if (lho + 30 > len)
            goto bad;
        e[k].dataOff = lho + 30 + *(const WORD *)(z + lho + 26) + *(const WORD *)(z + lho + 28);
        if (e[k].dataOff + e[k].csize > len)
            goto bad;
        cd += 46 + nl + xl + cl;
    }
    *out = e;
    return n;
bad:
    HeapFree(GetProcessHeap(), 0, e);
    return -1;
}

static BYTE *zip_read(const BYTE *z, const ZEnt *e)
{
    BYTE *b = (BYTE *)HeapAlloc(GetProcessHeap(), 0, e->usize + 1);
    if (!b)
        return NULL;
    if (e->method == 0 && e->csize == e->usize)
        memcpy(b, z + e->dataOff, e->usize);
    else if (e->method != 8 || inflate_raw(z + e->dataOff, e->csize, b, e->usize) != (long long)e->usize) {
        HeapFree(GetProcessHeap(), 0, b);
        return NULL;
    }
    b[e->usize] = 0;
    return b;
}

/* an entry of the archive to make: the game's (compressed, as it is) or ours (stored), in each look */
typedef struct {
    const char *name;
    WORD method;
    DWORD crc, usize, len;
    const BYTE *d[NSETS];
    BYTE own[NSETS]; /* d[look] is ours to free */
} ZOut;

/* the archive in one look, laid out as the game's are (each central entry with the game's extra field 0x1123: where
   its data starts); dir: the central directory's start, its size with the end record, its entries (zipmanifest.xml's) */
static BYTE *zip_build(const ZOut *o, int n, int set, DWORD *outLen, DWORD dir[3])
{
    size_t size = 22;
    for (int i = 0; i < n; i++)
        size += 30 + 46 + 8 + 2 * strlen(o[i].name) + o[i].len;
    BYTE *z = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size), *p = z;
    DWORD *offs = (DWORD *)HeapAlloc(GetProcessHeap(), 0, (n + 1) * 2 * sizeof(DWORD));
    if (!z || !offs) {
        if (z)
            HeapFree(GetProcessHeap(), 0, z);
        if (offs)
            HeapFree(GetProcessHeap(), 0, offs);
        return NULL;
    }
    for (int i = 0; i < n; i++) {
        WORD nl = (WORD)strlen(o[i].name);
        offs[2 * i] = (DWORD)(p - z);
        *(DWORD *)p = 0x04034B50, *(WORD *)(p + 4) = 10, *(WORD *)(p + 8) = o[i].method;
        *(DWORD *)(p + 14) = o[i].crc, *(DWORD *)(p + 18) = o[i].len, *(DWORD *)(p + 22) = o[i].usize;
        *(WORD *)(p + 26) = nl;
        memcpy(p + 30, o[i].name, nl);
        p += 30 + nl;
        offs[2 * i + 1] = (DWORD)(p - z);
        memcpy(p, o[i].d[set], o[i].len);
        p += o[i].len;
    }
    DWORD cd = (DWORD)(p - z);
    for (int i = 0; i < n; i++) {
        WORD nl = (WORD)strlen(o[i].name);
        *(DWORD *)p = 0x02014B50, *(WORD *)(p + 4) = 10, *(WORD *)(p + 6) = 10, *(WORD *)(p + 10) = o[i].method;
        *(DWORD *)(p + 16) = o[i].crc, *(DWORD *)(p + 20) = o[i].len, *(DWORD *)(p + 24) = o[i].usize;
        *(WORD *)(p + 28) = nl, *(WORD *)(p + 30) = 8;
        *(DWORD *)(p + 42) = offs[2 * i];
        memcpy(p + 46, o[i].name, nl);
        p += 46 + nl;
        *(WORD *)p = 0x1123, *(WORD *)(p + 2) = 4, *(DWORD *)(p + 4) = offs[2 * i + 1];
        p += 8;
    }
    DWORD cdSize = (DWORD)(p - z) - cd;
    *(DWORD *)p = 0x06054B50, *(WORD *)(p + 8) = (WORD)n, *(WORD *)(p + 10) = (WORD)n;
    *(DWORD *)(p + 12) = cdSize, *(DWORD *)(p + 16) = cd;
    p += 22;
    HeapFree(GetProcessHeap(), 0, offs);
    *outLen = (DWORD)(p - z);
    dir[0] = cd, dir[1] = cdSize + 22, dir[2] = (DWORD)n;
    return z;
}

/* ---- string tables ("LSB2": media\stripped\stringtables\<lang>.zip\<Table>.str) ----
   A 0x24-byte header ('LSB2', 0, 0x200, 0x24, 4, the offsets of its 4 sections); section 0, the only one the game
   reads: u32 its size, u32 count, (count + 1) x {u16 key id, u32 offset in characters} sorted by id (the last: 0xFFFF
   and the end), then the strings (UTF-16, each with its 0) in that order and one more 0. A key's id: h = 0xFFFF, then
   for each character h = rotl16(h ^ tolower(c), 7). Sections 1..3 (the keys' names, not read) move along. */
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
    if (!e || !mine) {
        if (e)
            HeapFree(GetProcessHeap(), 0, e);
        if (mine)
            HeapFree(GetProcessHeap(), 0, mine);
        return NULL;
    }
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
        if (out) {
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
    }
    HeapFree(GetProcessHeap(), 0, e);
    HeapFree(GetProcessHeap(), 0, mine);
    return out;
}

/* ---- the PlayStation pictures (DualSense.icons: a zip of PNGs in the sizes of the game's own icons, made by
   tools\make_pack.py; inside the DLL, or a DualSense\DualSense.icons of your own) ---- */
#include "icons_pack.h"
static BYTE *g_pack;
static DWORD g_packLen;
static ZEnt *g_packEnt;
static int g_packN;
static int g_packOwn; /* DualSense\DualSense.icons */

static BYTE *pack_png(const char *name, int *w, int *h)
{
    for (int i = 0; i < g_packN; i++)
        if (!lstrcmpiA(g_packEnt[i].name, name)) {
            BYTE *raw = zip_read(g_pack, &g_packEnt[i]);
            BYTE *img = raw ? png_decode(raw, g_packEnt[i].usize, w, h) : NULL;
            if (raw)
                HeapFree(GetProcessHeap(), 0, raw);
            return img;
        }
    return NULL;
}

static int pack_has(const char *name)
{
    for (int i = 0; i < g_packN; i++)
        if (!lstrcmpiA(g_packEnt[i].name, name))
            return 1;
    return 0;
}

static int pack_open(void)
{
    WCHAR own[MAX_PATH];
    if (g_packN > 0)
        return 1;
    lstrcpyW(own, g_dir);
    lstrcatW(own, L"DualSense.icons");
    g_pack = load_file(own, &g_packLen);
    if (g_pack && (g_packN = zip_parse(g_pack, g_packLen, &g_packEnt)) > 0) {
        g_packOwn = 1;
        fh2_say("icons: DualSense\\DualSense.icons (%d files) -- instead of the pictures inside the DLL", g_packN);
        return 1;
    }
    if (g_pack)
        HeapFree(GetProcessHeap(), 0, g_pack);
    g_pack = (BYTE *)kIconsPack, g_packLen = ICONS_PACK_LEN;
    g_packN = g_packLen > 22 ? zip_parse(g_pack, g_packLen, &g_packEnt) : -1;
    return g_packN > 0;
}

/* the controls screen's drawing in another look: the game's, its Xbox marks cleared (diagram/clear.png) and the
   PlayStation ones (diagram/ps.png, premultiplied) laid over, or the keys written where the marks were
   (diagram/slots.txt); NULL when the pack has not what it needs */
static BYTE *diagram_look(const BYTE *orig, int w, int h, int set)
{
    int cw, ch, pw, ph;
    BYTE *clear = pack_png("diagram/clear.png", &cw, &ch);
    BYTE *out = (BYTE *)HeapAlloc(GetProcessHeap(), 0, (size_t)w * h * 4);
    if (!clear || cw != w || ch != h || !out) {
        if (clear)
            HeapFree(GetProcessHeap(), 0, clear);
        if (out)
            HeapFree(GetProcessHeap(), 0, out);
        return NULL;
    }
    memcpy(out, orig, (size_t)w * h * 4);
    for (int i = 0; i < w * h; i++)
        if (clear[i * 4 + 3] > 127)
            *(DWORD *)(out + i * 4) = 0;
    HeapFree(GetProcessHeap(), 0, clear);
    if (set == LOOK_PS) {
        BYTE *ps = pack_png("diagram/ps.png", &pw, &ph);
        if (!ps || pw != w || ph != h) {
            if (ps)
                HeapFree(GetProcessHeap(), 0, ps);
            HeapFree(GetProcessHeap(), 0, out);
            return NULL;
        }
        for (int i = 0; i < w * h; i++) {
            float a = ps[i * 4 + 3] / 255.0f;
            BYTE *d = out + i * 4, *s = ps + i * 4;
            for (int c = 0; c < 4; c++)
                d[c] = (BYTE)(s[c] + d[c] * (1 - a) + 0.5f);
        }
        HeapFree(GetProcessHeap(), 0, ps);
        return out;
    }
    /* the keys, written where the marks were: "<name> <x> <y> <w> <h> <angle>" a line */
    int slots = 0;
    for (int i = 0; i < g_packN; i++) {
        if (lstrcmpiA(g_packEnt[i].name, "diagram/slots.txt"))
            continue;
        char *t = (char *)zip_read(g_pack, &g_packEnt[i]);
        for (char *line = t; line && *line;) {
            char *end = line;
            while (*end && *end != '\n')
                end++;
            char save = *end;
            *end = 0;
            char name[16];
            int k = 0;
            while (line[k] && line[k] != ' ' && k < 15)
                name[k] = line[k], k++;
            name[k] = 0;
            float v[5] = {0, 0, 0, 0, 0};
            char *p = line + k;
            for (int j = 0; j < 5; j++) {
                while (*p == ' ')
                    p++;
                v[j] = (float)atof(p);
                while (*p && *p != ' ')
                    p++;
            }
            WCHAR label[32];
            char entry[40];
            wsprintfA(entry, "Controller_%s", name);
            if (*line != ';' && name[0] && key_label_for(entry, label, 32)) {
                /* round buttons: their old mark goes too (Menu keeps its own otherwise) */
                float r = !lstrcmpiA(name, "View") || !lstrcmpiA(name, "Menu") ? 7.5f
                          : !lstrcmpiA(name, "LB") || !lstrcmpiA(name, "RB") || !lstrcmpiA(name, "LT") || !lstrcmpiA(name, "RT")
                              ? 0
                              : 10.2f;
                for (int y = (int)(v[1] - r - 1); r > 0 && y <= (int)(v[1] + r + 1); y++)
                    for (int x = (int)(v[0] - r - 1); x <= (int)(v[0] + r + 1); x++)
                        if (x >= 0 && y >= 0 && x < w && y < h &&
                            (x + 0.5f - v[0]) * (x + 0.5f - v[0]) + (y + 0.5f - v[1]) * (y + 0.5f - v[1]) <= r * r)
                            *(DWORD *)(out + ((size_t)y * w + x) * 4) = 0;
                key_slot_text(out, w, h, v[0], v[1], v[2], v[3], -v[4], label);
                slots++;
            }
            *end = save;
            line = *end ? end + 1 : end;
        }
        if (t)
            HeapFree(GetProcessHeap(), 0, t);
    }
    if (!slots) {
        HeapFree(GetProcessHeap(), 0, out);
        return NULL;
    }
    return out;
}

/* a DualSense among the HID devices now (its path names Sony's vendor and the product) */
static int ds_present(void)
{
    GUID hid;
    SP_DEVICE_INTERFACE_DATA ifd;
    int found = 0;
    HidD_GetHidGuid(&hid);
    HDEVINFO set = SetupDiGetClassDevsW(&hid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE)
        return 0;
    ifd.cbSize = sizeof ifd;
    for (DWORD i = 0; !found && SetupDiEnumDeviceInterfaces(set, NULL, &hid, i, &ifd); i++) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &ifd, NULL, 0, &need, NULL);
        SP_DEVICE_INTERFACE_DETAIL_DATA_W *det = (SP_DEVICE_INTERFACE_DETAIL_DATA_W *)HeapAlloc(GetProcessHeap(), 0, need + 16);
        if (!det)
            continue;
        det->cbSize = sizeof *det;
        if (SetupDiGetDeviceInterfaceDetailW(set, &ifd, det, need, NULL, NULL) && contains_ci(det->DevicePath, L"054c") &&
            (contains_ci(det->DevicePath, L"0ce6") || contains_ci(det->DevicePath, L"0df2")))
            found = 1;
        HeapFree(GetProcessHeap(), 0, det);
    }
    SetupDiDestroyDeviceInfoList(set);
    return found;
}

/* ================================================================================================================ */
/* the archives the mod makes                                                                                       */
#define ARCH_UI 0
#define ARCH_STR 1 /* the string tables: menu.h's kLocZip, in its order */
#define ARCH_ICONS (ARCH_STR + LOC_ZIPS)
#define NARCH (ARCH_ICONS + 3)
static const char *const kIconArch[3] = {"ControllerIcons", "Help_Buttons", "UI_Symbols"};

/* media\<rel> of archive a */
static void arch_rel(int a, char *rel, int n)
{
    if (a == ARCH_UI)
        lstrcpynA(rel, "UI.zip", n);
    else if (a < ARCH_ICONS)
        _snprintf(rel, n, "stripped\\stringtables\\%s.zip", kLocZip[a - ARCH_STR]);
    else
        _snprintf(rel, n, "ui\\textures\\data_bound\\%s.zip", kIconArch[a - ARCH_ICONS]);
    rel[n - 1] = 0;
}

typedef struct {
    WCHAR suffix[96];             /* media\<rel> */
    WCHAR file[NSETS][MAX_PATH];  /* what the game gets instead: one file, or one per look */
    DWORD size;                   /* the game's own file has this size */
    int looks;                    /* an icon archive: file[g_iconSet] */
    int state;                    /* 0 not seen, 1 given, -1 the game's file is another one */
    volatile LONG opens;
} Served;
static Served g_served[NARCH + 1];
static int g_nserved;
static SRWLOCK g_servedLock = SRWLOCK_INIT;

/* an icon archive in every look, in memory: for the icons in the game's memory (icons.h) */
typedef struct {
    BYTE *set[NSETS];
    DWORD len;
} IconArch;
static IconArch g_iconArch[3];

static WCHAR g_media[MAX_PATH]; /* <game folder>\Mount\media\ */
static WCHAR g_cache[MAX_PATH]; /* <Azimuth>\DualSense\cache\ */
static int g_iconsOn;           /* the icon archives are given (in their three looks) */
static int g_textsOk;           /* every language's MyProfile.str has the section's texts */
static int g_kbLook;            /* the keyboard look has key caps (the keyboard is on) */
static int g_filesHooks;        /* how many of Azimuth's four file functions go through the mod */

/* <folder>\Mount\media\ or <folder>\media\ with a zipmanifest.xml; 1 found (g_media) */
static int media_in(const WCHAR *folder)
{
    WCHAR f[MAX_PATH], cand[2][MAX_PATH];
    if (!folder[0] || lstrlenW(folder) >= MAX_PATH - 40)
        return 0;
    if (!(folder[1] == ':' || (folder[0] == '\\' && folder[1] == '\\'))) { /* relative to Azimuth */
        lstrcpyW(f, g_azDir);
        if (lstrlenW(f) + lstrlenW(folder) >= MAX_PATH - 40)
            return 0;
        lstrcatW(f, folder);
    } else
        lstrcpyW(f, folder);
    if (f[lstrlenW(f) - 1] != '\\')
        lstrcatW(f, L"\\");
    lstrcpyW(cand[0], f), lstrcatW(cand[0], L"Mount\\media\\");
    lstrcpyW(cand[1], f), lstrcatW(cand[1], L"media\\");
    for (int i = 0; i < 2; i++) {
        WCHAR probe[MAX_PATH];
        lstrcpyW(probe, cand[i]);
        lstrcatW(probe, L"zipmanifest.xml");
        if (GetFileAttributesW(probe) != INVALID_FILE_ATTRIBUTES) {
            lstrcpyW(g_media, cand[i]);
            return 1;
        }
    }
    return 0;
}

/* the game's folder: from Azimuth's data\azimuth.ini (FH2's section, [title-1a96545b] folder=, or any title section
   named Forza Horizon 2), else FH2\ or the game's exe's folder next to azimuth.exe */
static int find_media(void)
{
    static WCHAR sections[16384];
    WCHAR ini[MAX_PATH], folder[MAX_PATH] = L"";
    lstrcpyW(ini, g_azDir);
    lstrcatW(ini, L"data\\azimuth.ini");
    GetPrivateProfileStringW(L"title-1a96545b", L"folder", L"", folder, MAX_PATH, ini);
    if (media_in(folder))
        return 1;
    if (GetPrivateProfileSectionNamesW(sections, 16384, ini))
        for (WCHAR *s = sections; *s; s += lstrlenW(s) + 1) {
            WCHAR name[128];
            if (_wcsnicmp(s, L"title-", 6))
                continue;
            GetPrivateProfileStringW(s, L"name", L"", name, 128, ini);
            GetPrivateProfileStringW(s, L"folder", L"", folder, MAX_PATH, ini);
            if (contains_ci(name, L"Horizon 2") && media_in(folder))
                return 1;
        }
    return media_in(L"FH2") || media_in(L"Forza Horizon 2") || media_in(L".");
}

/* a file's size and time, as text for the key */
static void stamp(const WCHAR *path, char *out, int n)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &a))
        lstrcpynA(out, "missing", n);
    else
        _snprintf(out, n, "%lu %08lX%08lX", a.nFileSizeLow, a.ftLastWriteTime.dwHighDateTime, a.ftLastWriteTime.dwLowDateTime);
    out[n - 1] = 0;
}

static DWORD file_size(const WCHAR *path)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    return GetFileAttributesExW(path, GetFileExInfoStandard, &a) && !a.nFileSizeHigh ? a.nFileSizeLow : 0;
}

static char *menu_xaml(const char *text, size_t len, size_t *outLen);
static const char *const *menu_keys(int *n);
static const char *const *menu_texts(int lang);

/* the copy of archive a (look: for an icon archive) in the cache */
static void cache_path(int a, int look, WCHAR *out)
{
    char rel[160];
    WCHAR name[64];
    arch_rel(a, rel, sizeof rel);
    MultiByteToWideChar(CP_UTF8, 0, strrchr(rel, '\\') ? strrchr(rel, '\\') + 1 : rel, -1, name, 64);
    lstrcpyW(out, g_cache);
    if (a >= ARCH_ICONS)
        lstrcatW(out, kSetDir[look]);
    lstrcatW(out, name);
}

/* no two icons the same in a look: the memory switch tells them by their bytes (icons.h). A corner pixel one step
   less transparent makes a copy different, unseen. */
static struct {
    int look;
    unsigned long long hash;
} g_seen[2048];
static int g_nseen;

static void icon_unique(BYTE *px, int w, int h, int look)
{
    for (int tries = 0; tries < 250; tries++) {
        unsigned long long hsh = 1469598103934665603ull;
        for (size_t i = 0; i < (size_t)w * h * 4; i++)
            hsh = (hsh ^ px[i]) * 1099511628211ull;
        hsh ^= (unsigned long long)w << 48 ^ (unsigned long long)h << 32;
        int dup = 0;
        for (int i = 0; i < g_nseen && !dup; i++)
            dup = g_seen[i].look == look && g_seen[i].hash == hsh;
        if (!dup) {
            if (g_nseen < (int)(sizeof g_seen / sizeof g_seen[0]))
                g_seen[g_nseen].look = look, g_seen[g_nseen].hash = hsh, g_nseen++;
            return;
        }
        px[3] = (BYTE)(px[3] + 1); /* the top-left pixel, transparent in every icon */
    }
}

/* archive a made into the cache; dir: for the manifest. 1 made, 0 not needed or not made */
static int arch_make(int a, DWORD dir[3])
{
    char rel[160], base[64];
    WCHAR wrel[160], path[MAX_PATH], out[MAX_PATH];
    DWORD len = 0;
    int n, changed = 0, keysN = 0, icons = a >= ARCH_ICONS, sets = icons ? NSETS : 1;
    BYTE *z;
    ZEnt *e = NULL;
    ZOut *o;
    arch_rel(a, rel, sizeof rel);
    MultiByteToWideChar(CP_UTF8, 0, rel, -1, wrel, 160);
    lstrcpyW(path, g_media);
    lstrcatW(path, wrel);
    z = load_file(path, &len);
    n = z ? zip_parse(z, len, &e) : -1;
    if (n < 0) {
        fh2_say("files: media\\%s: not readable -- left as it is", rel);
        if (z)
            HeapFree(GetProcessHeap(), 0, z);
        return 0;
    }
    lstrcpynA(base, strrchr(rel, '\\') ? strrchr(rel, '\\') + 1 : rel, sizeof base);
    *strrchr(base, '.') = 0;
    o = (ZOut *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (n + 1) * sizeof *o);
    const char *const *keys = menu_keys(&keysN);
    for (int k = 0; o && k < n; k++) {
        ZOut *q = &o[k];
        BYTE *raw = NULL;
        q->name = e[k].name, q->method = e[k].method, q->crc = e[k].crc, q->usize = e[k].usize, q->len = e[k].csize;
        for (int s = 0; s < NSETS; s++)
            q->d[s] = z + e[k].dataOff;
        if (a == ARCH_UI) { /* the controller settings page with the DualSense's section */
            size_t nl = 0;
            char *t;
            if (lstrcmpiA(e[k].name, "Scenes/Anthem/A_ControllerOptions.xaml"))
                continue;
            raw = zip_read(z, &e[k]);
            t = raw ? menu_xaml((char *)raw, e[k].usize, &nl) : NULL;
            if (t) {
                q->method = 0, q->usize = q->len = (DWORD)nl, q->crc = crc_update(0, (BYTE *)t, nl), q->d[0] = (BYTE *)t;
                q->own[0] = 1;
                changed++;
            } else
                fh2_say("files: the controller settings page is not as expected -- no DualSense section in it");
        } else if (!icons) { /* a string table: the section's texts in its language */
            size_t nl = 0;
            int added = -1;
            char *t;
            if (lstrcmpiA(e[k].name, "MyProfile.str"))
                continue;
            raw = zip_read(z, &e[k]);
            t = raw ? loc_add((char *)raw, e[k].usize, &nl, keys, menu_texts(kLocLang[a - ARCH_STR]), keysN, &added) : NULL;
            if (added < 0)
                fh2_say("files: media\\%s: MyProfile.str is not as expected -- the section's texts not in it", rel);
            if (t) {
                q->method = 0, q->usize = q->len = (DWORD)nl, q->crc = crc_update(0, (BYTE *)t, nl), q->d[0] = (BYTE *)t;
                q->own[0] = 1;
                changed++;
            }
        } else { /* a picture: the PlayStation look from the pack (the drawing: made from the game's), the keys */
            char png[240], nm[160];
            int diagram = !lstrcmpiA(base, "UI_Symbols") && !lstrcmpiA(e[k].name, "Help_Controller.swatchbin");
            int w = 0, h = 0, hasPs, hasKb;
            WCHAR label[32];
            lstrcpynA(nm, e[k].name, 160);
            if (strrchr(nm, '.'))
                *strrchr(nm, '.') = 0;
            wsprintfA(png, "ps/%s/%s.png", base, nm);
            hasPs = diagram || pack_has(png);
            hasKb = g_kbLook && (diagram || (lstrcmpiA(base, "UI_Symbols") && key_label_for(e[k].name, label, 32)));
            if (!hasPs && !hasKb)
                continue;
            raw = zip_read(z, &e[k]);
            BYTE *xb = raw ? tex_decode(raw, e[k].usize, &w, &h) : NULL;
            if (!xb) {
                fh2_say("icons: %s %s: not a picture this DLL reads -- left as it is", base, e[k].name);
                HeapFree(GetProcessHeap(), 0, raw);
                continue;
            }
            BYTE *rgba[NSETS] = {xb, NULL, NULL};
            if (diagram) {
                rgba[LOOK_PS] = diagram_look(xb, w, h, LOOK_PS);
                if (hasKb)
                    rgba[LOOK_KEYS] = diagram_look(xb, w, h, LOOK_KEYS);
            } else {
                int pw = 0, ph = 0;
                if (hasPs && (rgba[LOOK_PS] = pack_png(png, &pw, &ph)) != NULL && (pw != w || ph != h)) {
                    fh2_say("icons: %s is %dx%d, the game's %dx%d -- not used", png, pw, ph, w, h);
                    HeapFree(GetProcessHeap(), 0, rgba[LOOK_PS]);
                    rgba[LOOK_PS] = NULL;
                }
                if (hasKb)
                    rgba[LOOK_KEYS] = key_icon(w, h, label, !lstrcmpiA(base, "Help_Buttons"));
            }
            if (!rgba[LOOK_PS] && !rgba[LOOK_KEYS]) {
                HeapFree(GetProcessHeap(), 0, xb);
                HeapFree(GetProcessHeap(), 0, raw);
                continue;
            }
            for (int s = 0; s < NSETS; s++)
                if (rgba[s])
                    icon_unique(rgba[s], w, h, s);
            /* every look as RGBA 1D tiling with 16 spare bytes; the CRC of the first one for all */
            size_t lens[NSETS] = {0, 0, 0};
            BYTE *enc[NSETS] = {NULL, NULL, NULL};
            for (int s = 0; s < NSETS; s++)
                enc[s] = tex_encode(raw, w, h, rgba[s] ? rgba[s] : xb, 16, &lens[s]);
            if (enc[0] && enc[1] && enc[2] && lens[0] == lens[1] && lens[1] == lens[2]) {
                q->method = 0, q->usize = q->len = (DWORD)lens[0];
                q->crc = crc_update(0, enc[0], lens[0]);
                for (int s = 0; s < NSETS; s++) {
                    if (s)
                        crc_forge(enc[s], lens[s], q->crc);
                    q->d[s] = enc[s], q->own[s] = 1;
                }
                changed++;
            } else
                for (int s = 0; s < NSETS; s++)
                    if (enc[s])
                        HeapFree(GetProcessHeap(), 0, enc[s]);
            for (int s = 0; s < NSETS; s++)
                if (rgba[s])
                    HeapFree(GetProcessHeap(), 0, rgba[s]);
        }
        if (raw)
            HeapFree(GetProcessHeap(), 0, raw);
    }
    int ok = 0;
    if (o && changed) {
        DWORD l[NSETS] = {0, 0, 0}, d[NSETS][3];
        BYTE *made[NSETS] = {NULL, NULL, NULL};
        ok = 1;
        for (int s = 0; s < sets && ok; s++)
            ok = (made[s] = zip_build(o, n, s, &l[s], d[s])) != NULL && l[s] == l[0] && !memcmp(d[s], d[0], sizeof d[0]);
        for (int s = 0; s < sets && ok; s++) {
            cache_path(a, s, out);
            ok = save_file(out, made[s], l[s]);
        }
        if (ok) {
            memcpy(dir, d[0], sizeof d[0]);
            if (icons) { /* kept for the memory switch */
                IconArch *ia = &g_iconArch[a - ARCH_ICONS];
                for (int s = 0; s < NSETS; s++)
                    ia->set[s] = made[s], made[s] = NULL;
                ia->len = l[0];
            }
            fh2_say("files: media\\%s: %d of %d entries changed, %lu bytes%s", rel, changed, n, l[0],
                    icons ? " (each look)" : "");
        } else
            fh2_say("files: media\\%s: the copy could not be made or written", rel);
        for (int s = 0; s < NSETS; s++)
            if (made[s])
                HeapFree(GetProcessHeap(), 0, made[s]);
    }
    for (int k = 0; o && k < n; k++)
        for (int s = 0; s < NSETS; s++)
            if (o[k].own[s])
                HeapFree(GetProcessHeap(), 0, (void *)o[k].d[s]);
    if (o)
        HeapFree(GetProcessHeap(), 0, o);
    HeapFree(GetProcessHeap(), 0, e);
    HeapFree(GetProcessHeap(), 0, z);
    return ok;
}

/* the game's zipmanifest.xml with the new directories of the archives made */
static int manifest_make(DWORD dirs[][3], const int *made)
{
    WCHAR path[MAX_PATH], out[MAX_PATH];
    DWORD len = 0;
    char *t, *text;
    size_t tl;
    lstrcpyW(path, g_media);
    lstrcatW(path, L"zipmanifest.xml");
    t = (char *)load_file(path, &len);
    if (!t)
        return 0;
    tl = len;
    text = (char *)HeapAlloc(GetProcessHeap(), 0, len + 64 * NARCH + 64);
    if (!text) {
        HeapFree(GetProcessHeap(), 0, t);
        return 0;
    }
    memcpy(text, t, len + 1);
    HeapFree(GetProcessHeap(), 0, t);
    for (int a = 0; a < NARCH; a++) {
        char key[200], low[160], line[600], tail[256], *at = NULL, *end, *ds, *de;
        int keep;
        size_t ll, old;
        if (!made[a])
            continue;
        arch_rel(a, low, sizeof low);
        CharLowerA(low);
        wsprintfA(key, "path=\"game:\\media\\%s\"", low);
        for (char *p = text; *p && !at; p++)
            if (!_strnicmp(p, key, strlen(key)))
                at = p;
        end = at ? strchr(at, '>') : NULL;
        ds = at ? strstr(at, "dirstart=\"") : NULL, de = at ? strstr(at, "direntries=\"") : NULL;
        if (!end || !ds || ds > end || !de || de > end) {
            HeapFree(GetProcessHeap(), 0, text);
            fh2_say("files: zipmanifest.xml has no %s", low);
            return 0;
        }
        de = strchr(de + 12, '"');
        keep = (int)(ds - at);
        lstrcpynA(tail, de + 1, (int)(end - de) < 255 ? (int)(end - de) : 255);
        memcpy(line, at, keep);
        wsprintfA(line + keep, "dirstart=\"%lu\" dirsize=\"%lu\" direntries=\"%lu\"%s", dirs[a][0], dirs[a][1], dirs[a][2], tail);
        ll = strlen(line), old = (size_t)(end - at);
        memmove(at + ll, end, tl - (size_t)(end - text) + 1);
        memcpy(at, line, ll);
        tl = tl - old + ll;
    }
    lstrcpyW(out, g_cache);
    lstrcatW(out, L"zipmanifest.xml");
    int ok = save_file(out, text, (DWORD)tl);
    HeapFree(GetProcessHeap(), 0, text);
    return ok;
}

/* one file the game gets in place of its own (looks: an icon archive, one copy per look) */
static void serve(const char *rel, int a)
{
    Served *s;
    WCHAR wrel[160], path[MAX_PATH];
    int there = 1;
    if (g_nserved >= (int)(sizeof g_served / sizeof g_served[0]))
        return;
    s = &g_served[g_nserved];
    ZeroMemory(s, sizeof *s);
    MultiByteToWideChar(CP_UTF8, 0, rel, -1, wrel, 160);
    lstrcpyW(s->suffix, L"media\\");
    lstrcatW(s->suffix, wrel);
    lstrcpyW(path, g_media);
    lstrcatW(path, wrel);
    s->size = file_size(path);
    s->looks = a >= ARCH_ICONS;
    for (int l = 0; l < (s->looks ? NSETS : 1); l++) {
        if (a < 0)
            lstrcpyW(s->file[l], g_cache), lstrcatW(s->file[l], L"zipmanifest.xml");
        else
            cache_path(a, l, s->file[l]);
        there &= GetFileAttributesW(s->file[l]) != INVALID_FILE_ATTRIBUTES;
    }
    if (s->size && there)
        g_nserved++;
}

/* the icon archives' looks into memory from the cache (made before: icons.h needs them) */
static int icons_load(void)
{
    for (int i = 0; i < 3; i++) {
        IconArch *ia = &g_iconArch[i];
        for (int s = 0; s < NSETS; s++) {
            WCHAR p[MAX_PATH];
            DWORD l = 0;
            if (ia->set[s])
                continue;
            cache_path(ARCH_ICONS + i, s, p);
            ia->set[s] = load_file(p, &l);
            if (!ia->set[s] || (s && l != ia->len))
                return 0;
            ia->len = l;
        }
    }
    return 1;
}

/* the keys of [Keys] and the keyboard's switch, as they shape the keyboard look */
static DWORD keys_hash(void)
{
    static WCHAR buf[8192];
    DWORD n = GetPrivateProfileSectionW(L"Keys", buf, 8192, g_ini);
    return crc_update((DWORD)g_kbLook, (const BYTE *)buf, n * sizeof(WCHAR));
}

/* ---- 3.0's files in the game's Mount\mediaoverride\ (DualSense\mediaoverride.txt: "<size> <crc32> <path>" a line):
   away, if they are still as 3.0 left them, with the folders they leave empty ---- */
static void legacy_cleanup(void)
{
    WCHAR list[MAX_PATH], over[MAX_PATH];
    DWORD len = 0;
    char *t, *p;
    int gone = 0, kept = 0;
    lstrcpyW(list, g_dir);
    lstrcatW(list, L"mediaoverride.txt");
    t = (char *)load_file(list, &len);
    if (!t)
        return;
    lstrcpyW(over, g_media);
    over[lstrlenW(over) - 6] = 0; /* <...>\media\ -> <...>\ */
    lstrcatW(over, L"mediaoverride\\");
    for (p = t; p && *p;) {
        char *end = strchr(p, '\n'), *q;
        if (end)
            *end = 0;
        DWORD size = strtoul(p, &q, 10), crc = strtoul(q, &q, 16), fl = 0;
        while (*q == ' ')
            q++;
        for (char *r = q + lstrlenA(q); r > q && (r[-1] == '\r' || r[-1] == ' '); r--)
            r[-1] = 0;
        if (*q && !strstr(q, "..") && !strchr(q, ':') && *q != '\\' && *q != '/') { /* below mediaoverride\ only */
            WCHAR w[200], f[MAX_PATH];
            BYTE *b;
            MultiByteToWideChar(CP_UTF8, 0, q, -1, w, 200);
            lstrcpyW(f, over);
            if (lstrlenW(f) + lstrlenW(w) < MAX_PATH)
                lstrcatW(f, w);
            b = load_file(f, &fl);
            if (b && fl == size && crc_update(0, b, fl) == crc && DeleteFileW(f)) {
                gone++;
                for (int k = lstrlenW(f); k > lstrlenW(over); k--)
                    if (f[k] == '\\') {
                        f[k] = 0;
                        if (!RemoveDirectoryW(f)) /* not empty */
                            break;
                    }
            } else if (b)
                kept++;
            if (b)
                HeapFree(GetProcessHeap(), 0, b);
        }
        p = end ? end + 1 : NULL;
    }
    HeapFree(GetProcessHeap(), 0, t);
    DeleteFileW(list);
    over[lstrlenW(over) - 1] = 0;
    RemoveDirectoryW(over); /* when 3.0 made it and nothing else is in it */
    ds_note("files: the files 3.0 had written into the game's mediaoverride folder taken away (%d; %d changed since, "
            "left there)", gone, kept);
}

/* all of it, once (the mod's start, or Azimuth's first file call, whichever comes first) */
static void files_build(void)
{
    char key[8192], st[64], rel[160];
    WCHAR keyPath[MAX_PATH], path[MAX_PATH], wrel[160];
    DWORD dirs[NARCH][3], klen = 0;
    int made[NARCH], any = 0, have = 1;
    double t0 = fh2_now();
    crc_init();
    if (!find_media()) {
        ds_note("files: the game's folder is not in Azimuth's data\\azimuth.ini -- the game's own menus and icons");
        return;
    }
    fh2_say_w("files: the game's media: ", g_media);
    legacy_cleanup();
    lstrcpyW(g_cache, g_dir);
    lstrcatW(g_cache, L"cache\\");
    CreateDirectoryW(g_cache, NULL);
    for (int s = 0; s < NSETS; s++) {
        lstrcpyW(path, g_cache);
        lstrcatW(path, kSetDir[s]);
        CreateDirectoryW(path, NULL);
    }
    g_kbLook = g_kbOn;
    g_iconsOn = S.icons != ICONS_XBOX;
    if (g_iconsOn && !pack_open())
        ds_note("icons: no PlayStation pictures in this build (and no DualSense\\DualSense.icons) -- the keyboard's and "
                "the game's own only");
    if (g_iconsOn && g_packN <= 0 && !g_kbLook)
        g_iconsOn = 0;
    /* the key: this build, the pictures, the keys, and the game's files the copies are made from */
    {
        WCHAR self[MAX_PATH];
        char me[64] = "?";
        if (GetModuleFileNameW((HMODULE)&__ImageBase, self, MAX_PATH))
            stamp(self, me, sizeof me);
        _snprintf(key, sizeof key, "DualSense %s, this DLL %s; icons %d, pictures %s %lu, keys %08lX\r\n", DS_VERSION, me,
                  g_iconsOn, g_packOwn ? "own" : "built-in", g_packN > 0 ? g_packLen : 0, keys_hash());
    }
    for (int a = -1; a < NARCH; a++) {
        if (a >= ARCH_ICONS && !g_iconsOn)
            break;
        if (a < 0)
            lstrcpyA(rel, "zipmanifest.xml");
        else
            arch_rel(a, rel, sizeof rel);
        MultiByteToWideChar(CP_UTF8, 0, rel, -1, wrel, 160);
        lstrcpyW(path, g_media);
        lstrcatW(path, wrel);
        stamp(path, st, sizeof st);
        size_t kl = strlen(key);
        _snprintf(key + kl, sizeof key - kl, "%s %s\r\n", rel, st);
    }
    key[sizeof key - 1] = 0;
    lstrcpyW(keyPath, g_cache);
    lstrcatW(keyPath, L"key.txt");
    {
        char *old = (char *)load_file(keyPath, &klen);
        have = old && klen == strlen(key) && !memcmp(old, key, klen);
        if (old)
            HeapFree(GetProcessHeap(), 0, old);
    }
    if (have && g_iconsOn && !icons_load()) /* a look's copy gone: all made again */
        have = 0;
    ZeroMemory(made, sizeof made);
    if (!have) {
        DeleteFileW(keyPath);
        for (int a = 0; a < NARCH; a++)
            if (a < ARCH_ICONS || g_iconsOn)
                any |= made[a] = arch_make(a, dirs[a]);
        if (!any || !manifest_make(dirs, made)) {
            ds_note("files: the game's files could not be made (see above) -- the game's own menus and icons");
            return;
        }
        save_file(keyPath, key, (DWORD)strlen(key));
    }
    /* the copies there (made now or before) are given to the game */
    for (int a = 0; a < NARCH; a++) {
        if (a >= ARCH_ICONS && !g_iconsOn)
            continue;
        if (!have && !made[a])
            continue;
        arch_rel(a, rel, sizeof rel);
        serve(rel, a);
    }
    serve("zipmanifest.xml", -1);
    g_menuOn = g_textsOk = 0;
    int iconFiles = 0;
    for (int i = 0; i < g_nserved; i++) {
        if (!lstrcmpiW(g_served[i].suffix, L"media\\UI.zip"))
            g_menuOn = 1;
        if (!wcsncmp(g_served[i].suffix, L"media\\stripped\\", 15))
            g_textsOk++;
        iconFiles += g_served[i].looks;
    }
    g_textsOk = g_textsOk == LOC_ZIPS;
    if (!iconFiles)
        g_iconsOn = 0;
    ds_note("files: %s in %.2f s, from DualSense\\cache\\ (the game's folder is not written to): the DualSense's "
            "settings in the game's controller settings %s%s; button icons %s",
            have ? "the copies made before, checked" : "made", fh2_now() - t0, g_menuOn ? "on" : "OFF",
            g_menuOn && !g_textsOk ? " (some languages without its texts)" : "",
            g_iconsOn ? "in three looks (Xbox, PlayStation, keyboard)" : "the game's own");
}

/* ================================================================================================================ */
/* giving them to the game: Azimuth's file functions                                                                */
typedef HANDLE(WINAPI *CreateFileWFn)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef DWORD(WINAPI *GetFileAttributesWFn)(LPCWSTR);
typedef BOOL(WINAPI *GetFileAttributesExWFn)(LPCWSTR, GET_FILEEX_INFO_LEVELS, LPVOID);
typedef HANDLE(WINAPI *FindFirstFileExWFn)(LPCWSTR, FINDEX_INFO_LEVELS, LPVOID, FINDEX_SEARCH_OPS, LPVOID, DWORD);
static CreateFileWFn g_CreateFileW;
static GetFileAttributesWFn g_GetFileAttributesW;
static GetFileAttributesExWFn g_GetFileAttributesExW;
static FindFirstFileExWFn g_FindFirstFileExW;
static void mod_init_once(void); /* DualSense.c: the settings and files_build, once */

/* path ends with "\<suffix>" (any case, / or \) */
static int ends_with(const WCHAR *path, const WCHAR *suffix)
{
    int n = lstrlenW(path), k = lstrlenW(suffix), j;
    if (n <= k || (path[n - k - 1] != '\\' && path[n - k - 1] != '/'))
        return 0;
    for (j = 0; j < k; j++) {
        WCHAR a = path[n - k + j], b = suffix[j];
        if (a == '/')
            a = '\\';
        if (a >= 'A' && a <= 'Z')
            a += 32;
        if (b >= 'A' && b <= 'Z')
            b += 32;
        if (a != b)
            return 0;
    }
    return 1;
}

/* the copy for this path (the look now for an icon archive), or NULL */
static const WCHAR *served_match(const WCHAR *path, Served **out)
{
    if (!path || !g_nserved)
        return NULL;
    for (int i = 0; i < g_nserved; i++) {
        Served *s = &g_served[i];
        if (s->state < 0 || !ends_with(path, s->suffix))
            continue;
        if (!s->state) {
            WIN32_FILE_ATTRIBUTE_DATA a;
            int ok = g_GetFileAttributesExW(path, GetFileExInfoStandard, &a) && !a.nFileSizeHigh && a.nFileSizeLow == s->size;
            AcquireSRWLockExclusive(&g_servedLock);
            if (!s->state) {
                s->state = ok ? 1 : -1;
                if (!ok)
                    fh2_say_w("files: the game's file is not the one the copy was made from -- the game's own: ", s->suffix);
            }
            ReleaseSRWLockExclusive(&g_servedLock);
        }
        if (s->state > 0) {
            if (out)
                *out = s;
            return s->file[s->looks ? g_iconSet : 0];
        }
    }
    return NULL;
}

static HANDLE WINAPI hk_CreateFileW(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp,
                                    DWORD flags, HANDLE tmpl)
{
    Served *s = NULL;
    const WCHAR *f;
    mod_init_once();
    f = disp == OPEN_EXISTING && !(access & (GENERIC_WRITE | GENERIC_ALL)) ? served_match(name, &s) : NULL;
    if (!f)
        return g_CreateFileW(name, access, share, sa, disp, flags, tmpl);
    if (!wcsncmp(s->suffix, L"media\\stripped\\stringtables\\", 28)) /* the game's own language: its texts */
        for (int i = 0; i < LOC_ZIPS; i++) {
            WCHAR z[16];
            MultiByteToWideChar(CP_UTF8, 0, kLocZip[i], -1, z, 16);
            lstrcatW(z, L".zip");
            if (!lstrcmpiW(s->suffix + 28, z) && g_lang != kLocLang[i]) {
                g_lang = kLocLang[i];
                fh2_say_w("menu: the game's language, from the texts it reads: ", z);
            }
        }
    if (InterlockedIncrement(&s->opens) <= 2)
        fh2_say_w(s->looks ? (g_iconSet == LOOK_PS ? "files: the game gets the mod's (PlayStation look) "
                              : g_iconSet == LOOK_KEYS ? "files: the game gets the mod's (keyboard look) "
                                                        : "files: the game gets the mod's (Xbox look) ")
                           : "files: the game gets the mod's ",
                  s->suffix);
    return g_CreateFileW(f, access, share | FILE_SHARE_READ, sa, disp, flags, tmpl);
}

static DWORD WINAPI hk_GetFileAttributesW(LPCWSTR name)
{
    mod_init_once();
    const WCHAR *f = served_match(name, NULL);
    return g_GetFileAttributesW(f ? f : name);
}

static BOOL WINAPI hk_GetFileAttributesExW(LPCWSTR name, GET_FILEEX_INFO_LEVELS level, LPVOID info)
{
    mod_init_once();
    const WCHAR *f = served_match(name, NULL);
    return g_GetFileAttributesExW(f ? f : name, level, info);
}

static HANDLE WINAPI hk_FindFirstFileExW(LPCWSTR name, FINDEX_INFO_LEVELS level, LPVOID data, FINDEX_SEARCH_OPS op,
                                         LPVOID filter, DWORD flags)
{
    mod_init_once();
    const WCHAR *f = served_match(name, NULL);
    return g_FindFirstFileExW(f ? f : name, level, data, op, filter, flags);
}

/* from the start (DllMain): Azimuth's imports of the four go through ours */
static void files_install(void)
{
    HMODULE az = GetModuleHandleW(NULL);
    void *a, *b, *c, *d;
    g_CreateFileW = CreateFileW, g_GetFileAttributesW = GetFileAttributesW;
    g_GetFileAttributesExW = GetFileAttributesExW, g_FindFirstFileExW = FindFirstFileExW;
    if ((c = iat_hook(az, "KERNEL32.dll", "GetFileAttributesExW", (void *)hk_GetFileAttributesExW)) != NULL)
        g_GetFileAttributesExW = (GetFileAttributesExWFn)c;
    if ((b = iat_hook(az, "KERNEL32.dll", "GetFileAttributesW", (void *)hk_GetFileAttributesW)) != NULL)
        g_GetFileAttributesW = (GetFileAttributesWFn)b;
    if ((d = iat_hook(az, "KERNEL32.dll", "FindFirstFileExW", (void *)hk_FindFirstFileExW)) != NULL)
        g_FindFirstFileExW = (FindFirstFileExWFn)d;
    if ((a = iat_hook(az, "KERNEL32.dll", "CreateFileW", (void *)hk_CreateFileW)) != NULL)
        g_CreateFileW = (CreateFileWFn)a;
    g_filesHooks = (a != NULL) + (b != NULL) + (c != NULL) + (d != NULL);
}
