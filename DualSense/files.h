/* files.h -- the game's files the mod changes, made here and given to the game in place of its own:
     media\UI.zip                                the controller settings page with the DualSense's section (menu.h)
     media\stripped\stringtables\<lang>.zip      MyProfile.str with the section's texts, in every language the game has
     media\ui\textures\data_bound\ControllerIcons.zip, Help_Buttons.zip, UI_Symbols.zip
                                                 the button icons and the controller drawing with the PlayStation
                                                 pictures ([Icons] PlayStation)
     media\zipmanifest.xml                       the new directories of those archives (the game finds a zip's entries
                                                 through it, not through the zip's own end record)
   The game's own files are left as they are. The copies are made into DualSense\cache\ next to azimuth.exe once (the
   next starts only check that the game's files and the settings that shape them are the same: cache\key.txt), and
   Azimuth is given them when it opens, looks up or lists the game's: its imports of CreateFileW, GetFileAttributesW,
   GetFileAttributesExW and FindFirstFileExW go through ours first (hooks.h). A copy is only given while the game's
   file is the size it was made from. The string table the game opens tells the game's language (menu.h's status).
   Entries the mod adds or changes are stored uncompressed; the rest are copied as they are, compressed. */
#pragma once
#include "inflate.h"
#include "texture.h"
#include "png.h"

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

/* an entry of the archive to make: the game's (compressed, as it is) or ours (stored) */
typedef struct {
    const char *name;
    WORD method;
    DWORD crc, usize, len;
    const BYTE *data;
    int ours; /* data is ours to free */
} ZOut;

/* the archive, laid out as the game's are (each central entry with the game's extra field 0x1123: where its data
   starts); dir: the central directory's start, its size with the end record, its entries (zipmanifest.xml's) */
static BYTE *zip_build(const ZOut *o, int n, DWORD *outLen, DWORD dir[3])
{
    size_t size = 22;
    for (int i = 0; i < n; i++)
        size += 30 + 46 + 8 + 2 * strlen(o[i].name) + o[i].len;
    BYTE *z = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size), *p = z;
    DWORD *offs = (DWORD *)HeapAlloc(GetProcessHeap(), 0, (n + 1) * 2 * sizeof(DWORD));
    if (!z || !offs)
        return NULL;
    for (int i = 0; i < n; i++) {
        WORD nl = (WORD)strlen(o[i].name);
        offs[2 * i] = (DWORD)(p - z);
        *(DWORD *)p = 0x04034B50, *(WORD *)(p + 4) = 10, *(WORD *)(p + 8) = o[i].method;
        *(DWORD *)(p + 14) = o[i].crc, *(DWORD *)(p + 18) = o[i].len, *(DWORD *)(p + 22) = o[i].usize;
        *(WORD *)(p + 26) = nl;
        memcpy(p + 30, o[i].name, nl);
        p += 30 + nl;
        offs[2 * i + 1] = (DWORD)(p - z);
        memcpy(p, o[i].data, o[i].len);
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

/* the controls screen's drawing with the PlayStation marks: the game's, its Xbox marks cleared (diagram/clear.png)
   and the PlayStation ones (diagram/ps.png, premultiplied) laid over */
static BYTE *diagram_ps(const BYTE *orig, int w, int h)
{
    int cw, ch, pw, ph;
    BYTE *clear = pack_png("diagram/clear.png", &cw, &ch), *ps = pack_png("diagram/ps.png", &pw, &ph);
    BYTE *out = (BYTE *)HeapAlloc(GetProcessHeap(), 0, (size_t)w * h * 4);
    if (!clear || !ps || cw != w || ch != h || pw != w || ph != h || !out) {
        if (clear)
            HeapFree(GetProcessHeap(), 0, clear);
        if (ps)
            HeapFree(GetProcessHeap(), 0, ps);
        if (out)
            HeapFree(GetProcessHeap(), 0, out);
        return NULL;
    }
    memcpy(out, orig, (size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) {
        if (clear[i * 4 + 3] > 127)
            *(DWORD *)(out + i * 4) = 0;
        float a = ps[i * 4 + 3] / 255.0f;
        BYTE *d = out + i * 4, *s = ps + i * 4;
        for (int c = 0; c < 4; c++)
            d[c] = (BYTE)(s[c] + d[c] * (1 - a) + 0.5f);
    }
    HeapFree(GetProcessHeap(), 0, clear);
    HeapFree(GetProcessHeap(), 0, ps);
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
    WCHAR suffix[96];     /* media\<rel> */
    WCHAR file[MAX_PATH]; /* what the game gets instead */
    DWORD size;           /* the game's own file has this size */
    int state;            /* 0 not seen, 1 given, -1 the game's file is another one */
} Served;
static Served g_served[NARCH + 1];
static int g_nserved;
static SRWLOCK g_servedLock = SRWLOCK_INIT;
static WCHAR g_media[MAX_PATH]; /* <game folder>\Mount\media\ */
static WCHAR g_cache[MAX_PATH]; /* <Azimuth>\DualSense\cache\ */
static int g_iconsOn;           /* the PlayStation pictures this time */
static int g_textsOk;           /* every language's MyProfile.str has the section's texts */

/* the game's folder, from Azimuth's data\azimuth.ini ([title-1a96545b] folder=, else FH2 next to azimuth.exe) */
static int find_media(void)
{
    WCHAR ini[MAX_PATH], folder[MAX_PATH] = L"", cand[3][MAX_PATH];
    lstrcpyW(ini, g_azDir);
    lstrcatW(ini, L"data\\azimuth.ini");
    GetPrivateProfileStringW(L"title-1a96545b", L"folder", L"", folder, MAX_PATH, ini);
    if (folder[0] && !(folder[1] == ':' || (folder[0] == '\\' && folder[1] == '\\'))) { /* relative to Azimuth */
        WCHAR t[MAX_PATH];
        lstrcpyW(t, g_azDir);
        if (lstrlenW(t) + lstrlenW(folder) < MAX_PATH - 1)
            lstrcatW(t, folder);
        lstrcpyW(folder, t);
    }
    int n = 0;
    if (folder[0] && lstrlenW(folder) < MAX_PATH - 32) {
        if (folder[lstrlenW(folder) - 1] != '\\')
            lstrcatW(folder, L"\\");
        lstrcpyW(cand[n], folder), lstrcatW(cand[n++], L"Mount\\media\\");
        lstrcpyW(cand[n], folder), lstrcatW(cand[n++], L"media\\");
    }
    lstrcpyW(cand[n], g_azDir), lstrcatW(cand[n++], L"FH2\\Mount\\media\\");
    for (int i = 0; i < n; i++) {
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

/* archive a made into the cache; dir: for the manifest. 1 made, 0 not needed or not made */
static int arch_make(int a, DWORD dir[3])
{
    char rel[160], base[64];
    WCHAR wrel[160], path[MAX_PATH], out[MAX_PATH];
    DWORD len = 0;
    int n, changed = 0, keysN = 0;
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
        q->data = z + e[k].dataOff;
        if (a == ARCH_UI) { /* the controller settings page with the DualSense's section */
            size_t nl = 0;
            char *t;
            if (lstrcmpiA(e[k].name, "Scenes/Anthem/A_ControllerOptions.xaml"))
                continue;
            raw = zip_read(z, &e[k]);
            t = raw ? menu_xaml((char *)raw, e[k].usize, &nl) : NULL;
            if (t) {
                q->method = 0, q->usize = q->len = (DWORD)nl, q->crc = crc32(0, (BYTE *)t, nl), q->data = (BYTE *)t, q->ours = 1;
                changed++;
            } else
                fh2_say("files: the controller settings page is not as expected -- no DualSense section in it");
        } else if (a < ARCH_ICONS) { /* a string table: the section's texts in its language */
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
                q->method = 0, q->usize = q->len = (DWORD)nl, q->crc = crc32(0, (BYTE *)t, nl), q->data = (BYTE *)t, q->ours = 1;
                changed++;
            }
        } else { /* the icons: the PlayStation pictures */
            char png[240], nm[160];
            int diagram = !lstrcmpiA(base, "UI_Symbols") && !lstrcmpiA(e[k].name, "Help_Controller.swatchbin");
            int w = 0, h = 0, pw = 0, ph = 0;
            BYTE *xb, *ps = NULL, *enc = NULL;
            size_t el = 0;
            lstrcpynA(nm, e[k].name, 160);
            if (strrchr(nm, '.'))
                *strrchr(nm, '.') = 0;
            wsprintfA(png, "ps/%s/%s.png", base, nm);
            if (!diagram && !pack_has(png))
                continue;
            raw = zip_read(z, &e[k]);
            xb = raw ? tex_decode(raw, e[k].usize, &w, &h) : NULL;
            if (xb)
                ps = diagram ? diagram_ps(xb, w, h) : pack_png(png, &pw, &ph);
            if (ps && !diagram && (pw != w || ph != h)) {
                fh2_say("icons: %s is %dx%d, the game's %dx%d -- not used", png, pw, ph, w, h);
                HeapFree(GetProcessHeap(), 0, ps);
                ps = NULL;
            }
            enc = ps ? tex_encode(raw, w, h, ps, 0, &el) : NULL;
            if (enc) {
                q->method = 0, q->usize = q->len = (DWORD)el, q->crc = crc32(0, enc, el), q->data = enc, q->ours = 1;
                changed++;
            } else
                fh2_say("icons: %s %s: %s", base, e[k].name, !xb ? "not a picture this DLL reads" : "no picture");
            if (ps)
                HeapFree(GetProcessHeap(), 0, ps);
            if (xb)
                HeapFree(GetProcessHeap(), 0, xb);
        }
        if (raw)
            HeapFree(GetProcessHeap(), 0, raw);
    }
    int ok = 0;
    if (o && changed) {
        DWORD ol = 0;
        BYTE *made = zip_build(o, n, &ol, dir);
        lstrcpyW(out, g_cache);
        lstrcatW(out, wrel[0] && wcsrchr(wrel, '\\') ? wcsrchr(wrel, '\\') + 1 : wrel);
        ok = made && save_file(out, made, ol);
        if (made)
            HeapFree(GetProcessHeap(), 0, made);
        if (!ok)
            fh2_say("files: media\\%s: the copy could not be written", rel);
        else if (a == ARCH_UI || a >= ARCH_ICONS)
            fh2_say("files: media\\%s: %d of %d entries changed, %lu bytes", rel, changed, n, ol);
    }
    for (int k = 0; o && k < n; k++)
        if (o[k].ours)
            HeapFree(GetProcessHeap(), 0, (void *)o[k].data);
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
    if (!text)
        return 0;
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

/* one file the game gets in place of its own */
static void serve(const char *rel, const WCHAR *cacheName)
{
    Served *s;
    WCHAR wrel[160], path[MAX_PATH];
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
    lstrcpyW(s->file, g_cache);
    lstrcatW(s->file, cacheName);
    if (s->size && GetFileAttributesW(s->file) != INVALID_FILE_ATTRIBUTES)
        g_nserved++;
}

/* all of it, once (the mod's start, or the game's first file open, whichever comes first) */
static BOOL CALLBACK files_build(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    char key[8192], st[64], rel[160];
    WCHAR keyPath[MAX_PATH], path[MAX_PATH], wrel[160];
    DWORD dirs[NARCH][3], klen = 0;
    int made[NARCH], any = 0, have = 1;
    double t0 = fh2_now();
    (void)once, (void)param, (void)ctx;
    if (!find_media()) {
        ds_note("files: the game's folder is not in Azimuth's data\\azimuth.ini -- the game's own menus and icons");
        return TRUE;
    }
    fh2_say_w("files: the game's media: ", g_media);
    lstrcpyW(g_cache, g_dir);
    lstrcatW(g_cache, L"cache\\");
    CreateDirectoryW(g_cache, NULL);
    g_iconsOn = S.icons == 1 || (S.icons == 2 && ds_present());
    if (g_iconsOn && !pack_open()) {
        ds_note("icons: no PlayStation pictures in this build (and no DualSense\\DualSense.icons) -- the game's own icons");
        g_iconsOn = 0;
    }
    /* the key: this build, the icons, and the game's files the copies are made from */
    {
        WCHAR self[MAX_PATH];
        char me[64] = "?";
        if (GetModuleFileNameW((HMODULE)&__ImageBase, self, MAX_PATH))
            stamp(self, me, sizeof me);
        _snprintf(key, sizeof key, "DualSense %s, this DLL %s; icons %d %s %lu\r\n", DS_VERSION, me, g_iconsOn,
                  g_packOwn ? "own" : "built-in", g_iconsOn ? g_packLen : 0);
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
    ZeroMemory(made, sizeof made);
    if (!have) {
        DeleteFileW(keyPath);
        for (int a = 0; a < NARCH; a++)
            if (a < ARCH_ICONS || g_iconsOn)
                any |= made[a] = arch_make(a, dirs[a]);
        if (!any || !manifest_make(dirs, made)) {
            ds_note("files: the game's files could not be made (see above) -- the game's own menus and icons");
            return TRUE;
        }
        save_file(keyPath, key, (DWORD)strlen(key));
    }
    /* the copies there (made now or before) are given to the game */
    for (int a = 0; a < NARCH; a++) {
        WCHAR name[64];
        if (a >= ARCH_ICONS && !g_iconsOn)
            continue;
        arch_rel(a, rel, sizeof rel);
        MultiByteToWideChar(CP_UTF8, 0, strrchr(rel, '\\') ? strrchr(rel, '\\') + 1 : rel, -1, name, 64);
        if (!have && !made[a])
            continue;
        serve(rel, name);
    }
    serve("zipmanifest.xml", L"zipmanifest.xml");
    g_menuOn = g_textsOk = 0;
    for (int i = 0; i < g_nserved; i++) {
        if (!lstrcmpiW(g_served[i].suffix, L"media\\UI.zip"))
            g_menuOn = 1;
        if (!wcsncmp(g_served[i].suffix, L"media\\stripped\\", 15))
            g_textsOk++;
    }
    g_textsOk = g_textsOk == LOC_ZIPS;
    ds_note("files: %s in %.2f s: the DualSense's settings in the game's controller settings %s%s; PlayStation icons %s",
            have ? "the copies made before, checked" : "made", fh2_now() - t0, g_menuOn ? "on" : "OFF",
            g_menuOn && !g_textsOk ? " (some languages without its texts)" : "", g_iconsOn ? "on" : "off");
    return TRUE;
}

/* ---- giving them to the game: Azimuth's file functions ---- */
typedef HANDLE(WINAPI *CreateFileWFn)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef DWORD(WINAPI *GetFileAttributesWFn)(LPCWSTR);
typedef BOOL(WINAPI *GetFileAttributesExWFn)(LPCWSTR, GET_FILEEX_INFO_LEVELS, LPVOID);
typedef HANDLE(WINAPI *FindFirstFileExWFn)(LPCWSTR, FINDEX_INFO_LEVELS, LPVOID, FINDEX_SEARCH_OPS, LPVOID, DWORD);
static CreateFileWFn g_CreateFileW;
static GetFileAttributesWFn g_GetFileAttributesW;
static GetFileAttributesExWFn g_GetFileAttributesExW;
static FindFirstFileExWFn g_FindFirstFileExW;
static INIT_ONCE g_filesOnce = INIT_ONCE_STATIC_INIT;

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

static Served *served_match(const WCHAR *path)
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
        if (s->state > 0)
            return s;
    }
    return NULL;
}

static void note_language(const WCHAR *path);

static HANDLE WINAPI hk_CreateFileW(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp,
                                    DWORD flags, HANDLE tmpl)
{
    Served *s;
    InitOnceExecuteOnce(&g_filesOnce, files_build, NULL, NULL);
    if (name)
        note_language(name);
    s = disp == OPEN_EXISTING && !(access & (GENERIC_WRITE | GENERIC_ALL)) ? served_match(name) : NULL;
    if (!s)
        return g_CreateFileW(name, access, share, sa, disp, flags, tmpl);
    fh2_say_w("files: the game gets the mod's ", s->suffix);
    return g_CreateFileW(s->file, access, share | FILE_SHARE_READ, sa, disp, flags, tmpl);
}

static DWORD WINAPI hk_GetFileAttributesW(LPCWSTR name)
{
    InitOnceExecuteOnce(&g_filesOnce, files_build, NULL, NULL);
    Served *s = served_match(name);
    return g_GetFileAttributesW(s ? s->file : name);
}

static BOOL WINAPI hk_GetFileAttributesExW(LPCWSTR name, GET_FILEEX_INFO_LEVELS level, LPVOID info)
{
    InitOnceExecuteOnce(&g_filesOnce, files_build, NULL, NULL);
    Served *s = served_match(name);
    return g_GetFileAttributesExW(s ? s->file : name, level, info);
}

static HANDLE WINAPI hk_FindFirstFileExW(LPCWSTR name, FINDEX_INFO_LEVELS level, LPVOID data, FINDEX_SEARCH_OPS op,
                                         LPVOID filter, DWORD flags)
{
    InitOnceExecuteOnce(&g_filesOnce, files_build, NULL, NULL);
    Served *s = served_match(name);
    return g_FindFirstFileExW(s ? s->file : name, level, data, op, filter, flags);
}

/* from the start: Azimuth's imports of the four go through ours */
static void files_install(void)
{
    HMODULE az = GetModuleHandleW(NULL);
    void *a, *b, *c, *d;
    g_GetFileAttributesExW = GetFileAttributesExW; /* served_match's, until the hook gives the one below ours */
    a = iat_hook(az, "KERNEL32.dll", "CreateFileW", (void *)hk_CreateFileW);
    if (a)
        g_CreateFileW = (CreateFileWFn)a;
    b = iat_hook(az, "KERNEL32.dll", "GetFileAttributesW", (void *)hk_GetFileAttributesW);
    if (b)
        g_GetFileAttributesW = (GetFileAttributesWFn)b;
    c = iat_hook(az, "KERNEL32.dll", "GetFileAttributesExW", (void *)hk_GetFileAttributesExW);
    if (c)
        g_GetFileAttributesExW = (GetFileAttributesExWFn)c;
    d = iat_hook(az, "KERNEL32.dll", "FindFirstFileExW", (void *)hk_FindFirstFileExW);
    if (d)
        g_FindFirstFileExW = (FindFirstFileExWFn)d;
    if (!g_CreateFileW)
        g_CreateFileW = CreateFileW;
    if (!g_GetFileAttributesW)
        g_GetFileAttributesW = GetFileAttributesW;
    if (!g_FindFirstFileExW)
        g_FindFirstFileExW = FindFirstFileExW;
    if (!a)
        ds_note("files: azimuth.exe does not import CreateFileW -- the game's own menus and icons");
    else
        fh2_say("files: Azimuth's file functions go through the mod (%s%s%s)", b ? "attributes " : "", c ? "attributes-ex " : "",
                d ? "find" : "");
}
