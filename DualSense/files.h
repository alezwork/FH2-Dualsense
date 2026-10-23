/* files.h -- the game's files the mod changes, given to the game through Extra Options' media overrides: the game
   reads <game folder>\Mount\mediaoverride\<path> in place of media\<path>, a zip standing for a folder of its name
   (FH2_ExtraOptions.ini: [Files] EnableMediaOverrides=1). The mod writes there
     UI\Scenes\Anthem\A_ControllerOptions.xaml          the controller settings page with the DualSense's section
                                                        (from media\UI.zip; menu.h)
     stripped\stringtables\<lang>\MyProfile.str         the section's texts, in every language the game has (from
                                                        media\stripped\stringtables\<lang>.zip)
     ui\textures\data_bound\<archive>\...\<icon>.swatchbin
                                                        the button icons and the controller drawing with the
                                                        PlayStation pictures ([Icons] PlayStation; from
                                                        ControllerIcons.zip, Help_Buttons.zip and UI_Symbols.zip)
   made from the game's own (which are left as they are) as the mod starts, before Play is pressed; the next starts
   only check that the game's files, this DLL and the settings that shape them are the same (DualSense\cache\key.txt)
   and that the files are still there. What the mod wrote is listed in DualSense\mediaoverride.txt with each file's
   size and CRC-32: a file in its place that is not the mod's (another mod's, or changed by hand) is left alone, and
   the mod's own are taken away when they are no longer wanted ([Icons] PlayStation=Off, [DualSense] Enabled=0). */
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
/* the archives the mod changes entries of                                                                          */
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

static WCHAR g_media[MAX_PATH];    /* <game folder>\Mount\media\ */
static WCHAR g_override[MAX_PATH]; /* <game folder>\Mount\mediaoverride\ (Extra Options') */
static WCHAR g_cache[MAX_PATH];    /* <Azimuth>\DualSense\cache\ */
static int g_iconsOn;              /* the PlayStation pictures this time */
static int g_textsOk;              /* every language's MyProfile.str has the section's texts */

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
            /* mediaoverride\ beside media\ (the game's game:\ is the folder of its exe) */
            lstrcpyW(g_override, g_media);
            g_override[lstrlenW(g_override) - 6] = 0;
            lstrcatW(g_override, L"mediaoverride\\");
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

static char *menu_xaml(const char *text, size_t len, size_t *outLen);
static const char *const *menu_keys(int *n);
static const char *const *menu_texts(int lang);

/* ---- what the mod wrote into mediaoverride\ (DualSense\mediaoverride.txt: "<size> <crc32> <path>" a line) ---- */
typedef struct {
    char rel[200]; /* below mediaoverride\ */
    DWORD size, crc;
} OvFile;
typedef struct {
    OvFile *f;
    int n, cap;
} OvList;
static OvList g_ovOld, g_ovNew; /* listed before; written (or kept) now */
static WCHAR g_ovList[MAX_PATH];

static int ov_add(OvList *l, const char *rel, DWORD size, DWORD crc)
{
    if (l->n == l->cap) {
        int cap = l->cap ? l->cap * 2 : 64;
        OvFile *f = (OvFile *)(l->f ? HeapReAlloc(GetProcessHeap(), 0, l->f, cap * sizeof *f)
                                    : HeapAlloc(GetProcessHeap(), 0, cap * sizeof *f));
        if (!f)
            return 0;
        l->f = f, l->cap = cap;
    }
    lstrcpynA(l->f[l->n].rel, rel, sizeof l->f[0].rel);
    l->f[l->n].size = size, l->f[l->n].crc = crc;
    l->n++;
    return 1;
}

static const OvFile *ov_find(const OvList *l, const char *rel)
{
    for (int i = 0; i < l->n; i++)
        if (!lstrcmpiA(l->f[i].rel, rel))
            return &l->f[i];
    return NULL;
}

/* how many listed files are below the folder (prefix: "stripped\stringtables\", ...) */
static int ov_count(const OvList *l, const char *prefix)
{
    int c = 0, k = lstrlenA(prefix);
    for (int i = 0; i < l->n; i++)
        c += !_strnicmp(l->f[i].rel, prefix, k);
    return c;
}

static void ov_path(const char *rel, WCHAR *out)
{
    WCHAR w[200];
    MultiByteToWideChar(CP_UTF8, 0, rel, -1, w, 200);
    lstrcpyW(out, g_override);
    if (lstrlenW(out) + lstrlenW(w) < MAX_PATH)
        lstrcatW(out, w);
}

/* the file is there as the mod left it (its size and CRC as listed) */
static int ov_intact(const OvFile *f)
{
    WCHAR p[MAX_PATH];
    DWORD len = 0;
    BYTE *b;
    int same;
    ov_path(f->rel, p);
    b = load_file(p, &len);
    same = b && len == f->size && crc32(0, b, len) == f->crc;
    if (b)
        HeapFree(GetProcessHeap(), 0, b);
    return same;
}

static void ov_load(void)
{
    DWORD len = 0;
    char *t = (char *)load_file(g_ovList, &len), *p = t;
    g_ovOld.n = 0;
    while (p && *p) {
        char *end = strchr(p, '\n'), *q;
        if (end)
            *end = 0;
        DWORD size = strtoul(p, &q, 10), crc = strtoul(q, &q, 16);
        while (*q == ' ')
            q++;
        for (char *r = q + lstrlenA(q); r > q && (r[-1] == '\r' || r[-1] == ' '); r--)
            r[-1] = 0;
        if (*q && !strstr(q, "..") && !strchr(q, ':') && *q != '\\' && *q != '/') /* below mediaoverride\ only */
            ov_add(&g_ovOld, q, size, crc);
        p = end ? end + 1 : NULL;
    }
    if (t)
        HeapFree(GetProcessHeap(), 0, t);
}

static void ov_save(const OvList *l)
{
    char *t = (char *)HeapAlloc(GetProcessHeap(), 0, (size_t)l->n * 240 + 1), *p = t;
    if (!t)
        return;
    for (int i = 0; i < l->n; i++)
        p += wsprintfA(p, "%lu %08lX %s\r\n", l->f[i].size, l->f[i].crc, l->f[i].rel);
    if (l->n)
        save_file(g_ovList, t, (DWORD)(p - t));
    else
        DeleteFileW(g_ovList);
    HeapFree(GetProcessHeap(), 0, t);
}

/* the folders down to a file's made */
static void make_dirs(const WCHAR *file)
{
    WCHAR p[MAX_PATH];
    int root = lstrlenW(g_override) - 1;
    lstrcpyW(p, file);
    for (int i = root; p[i]; i++)
        if (p[i] == '\\') {
            p[i] = 0;
            CreateDirectoryW(p, NULL);
            p[i] = '\\';
        }
}

/* one file for the game, below mediaoverride\ -- unless a file there is not the mod's; 1 when it is there now */
static int ov_put(const char *rel, const void *data, DWORD len)
{
    WCHAR p[MAX_PATH];
    const OvFile *old = ov_find(&g_ovOld, rel);
    ov_path(rel, p);
    if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES && !(old && ov_intact(old))) {
        fh2_say("files: mediaoverride\\%s is not the mod's (another mod's, or changed by hand) -- left as it is", rel);
        return 0;
    }
    make_dirs(p);
    if (!save_file(p, data, len)) {
        fh2_say("files: mediaoverride\\%s could not be written (error %lu)", rel, GetLastError());
        return 0;
    }
    return ov_add(&g_ovNew, rel, len, crc32(0, (const BYTE *)data, len));
}

/* the mod's files of before that it does not give now: away, if they are still as it left them, with the folders
   they leave empty (mediaoverride\ itself stays) */
static int ov_drop_old(void)
{
    int n = 0, root = lstrlenW(g_override) - 1;
    for (int i = 0; i < g_ovOld.n; i++) {
        WCHAR p[MAX_PATH];
        if (ov_find(&g_ovNew, g_ovOld.f[i].rel) || !ov_intact(&g_ovOld.f[i]))
            continue;
        ov_path(g_ovOld.f[i].rel, p);
        if (!DeleteFileW(p))
            continue;
        n++;
        for (int k = lstrlenW(p); k > root; k--)
            if (p[k] == '\\') {
                p[k] = 0;
                if (!RemoveDirectoryW(p)) /* not empty */
                    break;
            }
    }
    return n;
}

/* archive a's entries that the mod changes, made and given to the game; how many */
static int arch_give(int a)
{
    char rel[160], dir[160], base[64];
    WCHAR wrel[160], path[MAX_PATH];
    DWORD len = 0;
    int n, given = 0, keysN = 0;
    BYTE *z;
    ZEnt *e = NULL;
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
    lstrcpynA(dir, rel, sizeof dir);
    *strrchr(dir, '.') = 0; /* the zip as a folder */
    lstrcpynA(base, strrchr(dir, '\\') ? strrchr(dir, '\\') + 1 : dir, sizeof base);
    const char *const *keys = menu_keys(&keysN);
    for (int k = 0; k < n; k++) {
        char out[200];
        BYTE *raw = NULL;
        _snprintf(out, sizeof out - 1, "%s\\%s", dir, e[k].name);
        out[sizeof out - 1] = 0;
        for (char *q = out; *q; q++)
            if (*q == '/')
                *q = '\\';
        if (a == ARCH_UI) { /* the controller settings page with the DualSense's section */
            size_t nl = 0;
            char *t;
            if (lstrcmpiA(e[k].name, "Scenes/Anthem/A_ControllerOptions.xaml"))
                continue;
            raw = zip_read(z, &e[k]);
            t = raw ? menu_xaml((char *)raw, e[k].usize, &nl) : NULL;
            if (t) {
                given += ov_put(out, t, (DWORD)nl);
                HeapFree(GetProcessHeap(), 0, t);
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
                given += ov_put(out, t, (DWORD)nl);
                HeapFree(GetProcessHeap(), 0, t);
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
                given += ov_put(out, enc, (DWORD)el);
                HeapFree(GetProcessHeap(), 0, enc);
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
    if (a == ARCH_UI || a >= ARCH_ICONS)
        fh2_say("files: media\\%s: %d of its %d files to the game", rel, given, n);
    HeapFree(GetProcessHeap(), 0, e);
    HeapFree(GetProcessHeap(), 0, z);
    return given;
}

/* all of it, as the mod starts -- before Play is pressed, so before the game reads its menus, string tables and
   icons. on = 0 ([DualSense] Enabled=0): the mod's files are taken away, the game gets its own */
static void files_build(int on)
{
    char key[8192], st[64], rel[160];
    WCHAR keyPath[MAX_PATH], path[MAX_PATH], wrel[160];
    DWORD klen = 0;
    int have, overrides, dropped;
    double t0 = fh2_now();
    if (!find_media()) {
        ds_note("files: the game's folder is not in Azimuth's data\\azimuth.ini -- the game's own menus and icons");
        return;
    }
    fh2_say_w("files: the game's media: ", g_media);
    lstrcpyW(g_cache, g_dir);
    lstrcatW(g_cache, L"cache\\");
    CreateDirectoryW(g_cache, NULL);
    lstrcpyW(keyPath, g_cache);
    lstrcatW(keyPath, L"key.txt");
    lstrcpyW(g_ovList, g_dir);
    lstrcatW(g_ovList, L"mediaoverride.txt");
    ov_load();
    if (!on) {
        dropped = ov_drop_old();
        ov_save(&g_ovNew);
        DeleteFileW(keyPath);
        if (dropped)
            ds_note("files: the mod's %d files taken out of mediaoverride -- the game's own menus and icons", dropped);
        return;
    }
    CreateDirectoryW(g_override, NULL);
    overrides = eo_setting(L"Global", L"Enabled") && eo_setting(L"Files", L"EnableMediaOverrides");
    if (!overrides)
        ds_note("files: Extra Options' media overrides are off (FH2_ExtraOptions.ini: [Global] Enabled=1 and [Files] "
                "EnableMediaOverrides=1 needed) -- the game shows its own menus and icons");
    g_iconsOn = S.icons == 1 || (S.icons == 2 && ds_present());
    if (g_iconsOn && !pack_open()) {
        ds_note("icons: no PlayStation pictures in this build (and no DualSense\\DualSense.icons) -- the game's own icons");
        g_iconsOn = 0;
    }
    /* the key: this build, the icons, and the game's files they are made from */
    {
        WCHAR self[MAX_PATH];
        char me[64] = "?";
        if (GetModuleFileNameW((HMODULE)&__ImageBase, self, MAX_PATH))
            stamp(self, me, sizeof me);
        _snprintf(key, sizeof key, "DualSense %s, this DLL %s; icons %d %s %lu\r\n", DS_VERSION, me, g_iconsOn,
                  g_packOwn ? "own" : "built-in", g_iconsOn ? g_packLen : 0);
    }
    for (int a = 0; a < NARCH && (a < ARCH_ICONS || g_iconsOn); a++) {
        arch_rel(a, rel, sizeof rel);
        MultiByteToWideChar(CP_UTF8, 0, rel, -1, wrel, 160);
        lstrcpyW(path, g_media);
        lstrcatW(path, wrel);
        stamp(path, st, sizeof st);
        size_t kl = strlen(key);
        _snprintf(key + kl, sizeof key - kl, "%s %s\r\n", rel, st);
    }
    key[sizeof key - 1] = 0;
    {
        char *old = (char *)load_file(keyPath, &klen);
        have = old && klen == strlen(key) && !memcmp(old, key, klen) && g_ovOld.n;
        if (old)
            HeapFree(GetProcessHeap(), 0, old);
    }
    for (int i = 0; have && i < g_ovOld.n; i++)
        have = ov_intact(&g_ovOld.f[i]);
    if (have) { /* the files of before, there as they were */
        for (int i = 0; i < g_ovOld.n; i++)
            ov_add(&g_ovNew, g_ovOld.f[i].rel, g_ovOld.f[i].size, g_ovOld.f[i].crc);
        dropped = 0;
    } else {
        DeleteFileW(keyPath);
        for (int a = 0; a < NARCH; a++)
            if (a < ARCH_ICONS || g_iconsOn)
                arch_give(a);
        dropped = ov_drop_old();
        ov_save(&g_ovNew);
        save_file(keyPath, key, (DWORD)strlen(key));
    }
    if (dropped)
        fh2_say("files: %d of the mod's files of before taken out of mediaoverride", dropped);
    g_menuOn = overrides && ov_count(&g_ovNew, "UI\\Scenes\\");
    g_textsOk = ov_count(&g_ovNew, "stripped\\stringtables\\") == LOC_ZIPS;
    ds_note("files: %s in %.2f s, %d in mediaoverride: the DualSense's settings in the game's controller settings %s%s; "
            "PlayStation icons %s",
            have ? "the files of before, checked" : "made", fh2_now() - t0, g_ovNew.n, g_menuOn ? "on" : "OFF",
            g_menuOn && !g_textsOk ? " (some languages without its texts)" : "",
            !overrides || !g_iconsOn ? "off" : ov_count(&g_ovNew, "ui\\textures\\") ? "on" : "OFF (see above)");
}
