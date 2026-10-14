/* icons.h -- the game's button icons in three looks, switched to the controller in use:
     0 Xbox       the game's own pictures
     1 PlayStation the pictures of DualSense.icons (a zip of PNGs, made by tools\make_pack.py), inside this DLL;
                  a mods\DualSense.icons of your own is taken instead
     2 keyboard   key caps drawn here from the keys in [Keys] (keyboard.h)
   At the game's first file open, its icon archives (ControllerIcons.zip, Help_Buttons.zip, UI_Symbols.zip for the
   controls screen drawing) are made again with every icon stored as RGBA in 1D tiling, in all looks with the same
   layout (and the same CRC: four spare bytes at the end of each picture), into mods\DualSense.cache\; UI.zip gets
   the "Adaptive triggers" switch; zipmanifest.xml their new directories. The game is given those files.
   The game reads its icons once and the engine uploads a texture again when its memory changes (it seals texture
   memory read-only and unseals it on a write), so a switch writes the other look over the icons in the game's
   memory, and into the files. Which look: the device that gave the last real input (DualSense, an Xbox pad, the
   keyboard). */
#pragma once
#include "inflate.h"
#include "texture.h"
#include "png.h"
#include "keyicons.h"

#define NSETS 3
static const char *const kSetName[NSETS] = {"Xbox", "PlayStation", "keyboard"};

typedef struct {
    WCHAR suffix[96];
    DWORD size;           /* the game's own file has this size */
    WCHAR file[MAX_PATH]; /* what the game gets instead */
    BYTE *set[NSETS];     /* archives: each look (NULL: a plain file, or the look is not there) */
    DWORD len;
    DWORD *runs; /* (offset, length) pairs where the looks differ */
    int nruns;
    int state; /* 0 not seen, 1 serving, -1 the game's file is another one */
    HANDLE handle[4];
    volatile LONG reads;
} IconFile;
static IconFile g_icon[8];
static int g_icons;
static SRWLOCK g_iconLock = SRWLOCK_INIT;
static volatile LONG g_iconSet = -1; /* the look in the game: 0 Xbox, 1 PlayStation, 2 keyboard */
static int g_iconStart, g_haveKb;
static ULONGLONG g_iconT0;
static INIT_ONCE g_iconOnce = INIT_ONCE_STATIC_INIT;
static WCHAR g_media[MAX_PATH]; /* <game folder>\Mount\media\ */
static int g_langRu;
static int g_uiAdaptive; /* the game knows AdaptiveTriggers (ui.h): its switch goes into the XAML */

typedef HANDLE(WINAPI *CreateFileWFn)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef DWORD(WINAPI *GetFileAttributesWFn)(LPCWSTR);
typedef BOOL(WINAPI *GetFileAttributesExWFn)(LPCWSTR, GET_FILEEX_INFO_LEVELS, LPVOID);
typedef HANDLE(WINAPI *FindFirstFileExWFn)(LPCWSTR, FINDEX_INFO_LEVELS, LPVOID, FINDEX_SEARCH_OPS, LPVOID, DWORD);
typedef BOOL(WINAPI *ReadFileFn)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
static CreateFileWFn g_CreateFileW;
static GetFileAttributesWFn g_GetFileAttributesW;
static GetFileAttributesExWFn g_GetFileAttributesExW;
static FindFirstFileExWFn g_FindFirstFileExW;
static ReadFileFn g_ReadFile;

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

/* a look into the file the game gets: all of it, or only where the looks differ */
static int write_set(IconFile *f, int set, int whole)
{
    HANDLE h = CreateFileW(f->file, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           whole ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD n;
    int ok = 1;
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    if (whole)
        ok = WriteFile(h, f->set[set], f->len, &n, NULL) && n == f->len;
    else
        for (int i = 0; i < f->nruns && ok; i++) {
            LARGE_INTEGER at;
            at.QuadPart = f->runs[2 * i];
            ok = SetFilePointerEx(h, at, NULL, FILE_BEGIN) &&
                 WriteFile(h, f->set[set] + f->runs[2 * i], f->runs[2 * i + 1], &n, NULL);
        }
    CloseHandle(h);
    return ok;
}

/* ================================================================================================================ */
/* making the files                                                                                                 */

/* the engine's data\azimuth.ini (UTF-16): game_folder (relative to the engine), language */
static int ov_find_game(void)
{
    WCHAR ini[MAX_PATH], dir[MAX_PATH], folder[MAX_PATH] = L"", lang[32] = L"";
    DWORD n = GetModuleFileNameW(NULL, dir, MAX_PATH), len = 0;
    while (n && dir[n - 1] != '\\' && dir[n - 1] != '/')
        n--;
    dir[n] = 0;
    lstrcpyW(ini, dir);
    lstrcatW(ini, L"data\\azimuth.ini");
    WCHAR *text = (WCHAR *)load_file(ini, &len);
    if (!text)
        return 0;
    text[len / 2] = 0;
    for (WCHAR *p = text; *p;) {
        WCHAR *e = p;
        while (*e && *e != '\n' && *e != '\r')
            e++;
        WCHAR save = *e;
        *e = 0;
        if (*p == 0xFEFF)
            p++;
        if (!folder[0] && CompareStringOrdinal(p, 12, L"game_folder=", 12, TRUE) == CSTR_EQUAL)
            lstrcpynW(folder, p + 12, MAX_PATH);
        if (!lang[0] && CompareStringOrdinal(p, 9, L"language=", 9, TRUE) == CSTR_EQUAL)
            lstrcpynW(lang, p + 9, 32);
        *e = save;
        p = *e ? e + 1 : e;
    }
    HeapFree(GetProcessHeap(), 0, text);
    if (!folder[0])
        return 0;
    if (folder[1] == ':' || (folder[0] == '\\' && folder[1] == '\\'))
        lstrcpyW(ini, folder);
    else {
        lstrcpyW(ini, dir);
        lstrcatW(ini, folder);
    }
    if (!GetFullPathNameW(ini, MAX_PATH, g_media, NULL))
        return 0;
    n = lstrlenW(g_media);
    if (n && g_media[n - 1] != '\\')
        lstrcatW(g_media, L"\\");
    lstrcatW(g_media, L"Mount\\media\\");
    g_langRu = lang[0] ? ((lang[0] | 32) == 'r' && (lang[1] | 32) == 'u') : PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN;
    return 1;
}

typedef struct {
    char name[160];
    WORD method;
    DWORD crc, csize, usize, dataOff;
} ZEnt;

/* a zip's central directory (the game's and ours: no comment, the end record last) */
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

typedef struct {
    const char *name;
    WORD method;
    DWORD crc, usize, len;
    const BYTE *d[NSETS];
} ZOut;

static BYTE *zip_build(const ZOut *o, int n, int set, DWORD *outLen, DWORD dir[3])
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
        *(WORD *)p = 0x1123, *(WORD *)(p + 2) = 4, *(DWORD *)(p + 4) = offs[2 * i + 1]; /* the game's: where the data is */
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

/* ---- DualSense.icons ---- */
#include "icons_pack.h" /* DualSense.icons inside the DLL (build.cmd: tools\bin2h.py) */
static BYTE *g_pack;
static DWORD g_packLen;
static ZEnt *g_packEnt;
static int g_packN;

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

/* the controls screen drawing in the other looks: the game's, with the Xbox marks cleared (diagram/clear.png) and
   the PlayStation marks (diagram/ps.png) or the keys (diagram/slots.txt) put there */
static BYTE *diagram_look(const BYTE *orig, int w, int h, int set)
{
    int cw, ch, pw, ph;
    BYTE *clear = pack_png("diagram/clear.png", &cw, &ch);
    BYTE *out = (BYTE *)HeapAlloc(GetProcessHeap(), 0, (size_t)w * h * 4);
    if (!clear || cw != w || ch != h || !out)
        return NULL;
    memcpy(out, orig, (size_t)w * h * 4);
    for (int i = 0; i < w * h; i++)
        if (clear[i * 4 + 3] > 127)
            *(DWORD *)(out + i * 4) = 0;
    HeapFree(GetProcessHeap(), 0, clear);
    if (set == 1) {
        BYTE *ps = pack_png("diagram/ps.png", &pw, &ph);
        if (!ps || pw != w || ph != h)
            return NULL;
        for (int i = 0; i < w * h; i++) {
            float a = ps[i * 4 + 3] / 255.0f;
            BYTE *d = out + i * 4, *s = ps + i * 4;
            for (int c = 0; c < 4; c++)
                d[c] = (BYTE)(s[c] + d[c] * (1 - a) + 0.5f);
        }
        HeapFree(GetProcessHeap(), 0, ps);
        return out;
    }
    /* the keys, written where the marks were */
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
                float r = !lstrcmpiA(name, "View") || !lstrcmpiA(name, "Menu") ? 7.5f : !lstrcmpiA(name, "LB") ||
                                  !lstrcmpiA(name, "RB") || !lstrcmpiA(name, "LT") || !lstrcmpiA(name, "RT") ? 0 : 10.2f;
                for (int y = (int)(v[1] - r - 1); r > 0 && y <= (int)(v[1] + r + 1); y++)
                    for (int x = (int)(v[0] - r - 1); x <= (int)(v[0] + r + 1); x++)
                        if (x >= 0 && y >= 0 && x < w && y < h && (x + 0.5f - v[0]) * (x + 0.5f - v[0]) + (y + 0.5f - v[1]) * (y + 0.5f - v[1]) <= r * r)
                            *(DWORD *)(out + ((size_t)y * w + x) * 4) = 0;
                key_slot_text(out, w, h, v[0], v[1], v[2], v[3], -v[4], label);
            }
            *end = save;
            line = *end ? end + 1 : end;
        }
        if (t)
            HeapFree(GetProcessHeap(), 0, t);
    }
    return out;
}

/* replace the only find in text (a new buffer); NULL when it is not there once */
static char *str_replace1(const char *text, size_t len, const char *find, const char *repl, size_t *outLen)
{
    size_t fl = strlen(find), rl = strlen(repl);
    const char *at = NULL;
    for (const char *p = text; p + fl <= text + len; p++)
        if (*p == *find && !memcmp(p, find, fl)) {
            if (at)
                return NULL;
            at = p;
        }
    if (!at)
        return NULL;
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, len - fl + rl + 1);
    if (!out)
        return NULL;
    size_t pre = (size_t)(at - text);
    memcpy(out, text, pre);
    memcpy(out + pre, repl, rl);
    memcpy(out + pre + rl, at + fl, len - pre - fl);
    *outLen = len - fl + rl;
    out[*outLen] = 0;
    return out;
}

/* the controller settings get "Adaptive triggers", after the camera look-back switch */
static char *xaml_switch(const char *text, size_t len, size_t *outLen)
{
    static const char kAnchor[] = "SelectedIndex=\"{Binding InvertCameraLookBack}\"";
    const char *a = strstr(text, kAnchor);
    if (!a)
        return NULL;
    const char *start = a, *end = strstr(a, "/>");
    while (start > text && *start != '<')
        start--;
    const char *ls = start;
    while (ls > text && ls[-1] != '\n')
        ls--;
    if (!end || strncmp(start, "<local:SpinnerControl", 21))
        return NULL;
    end += 2;
    char add[700], indent[64];
    int il = (int)(start - ls) < 63 ? (int)(start - ls) : 63;
    memcpy(indent, ls, il);
    indent[il] = 0;
    wsprintfA(add, "\r\n%s<local:SpinnerControl Style=\"{StaticResource A_Style_SpinnerControl_Default}\" Label=\"%s\" "
                   "SelectedIndex=\"{Binding AdaptiveTriggers}\" ItemsSource=\"{Binding OnOffOptions}\"/>",
              indent, g_langRu ? "\xD0\x90\xD0\x94\xD0\x90\xD0\x9F\xD0\xA2\xD0\x98\xD0\x92\xD0\x9D\xD0\xAB\xD0\x95 \xD0\x9A\xD0\xA3\xD0\xA0\xD0\x9A\xD0\x98"
                               : "ADAPTIVE TRIGGERS");
    size_t al = strlen(add), pre = (size_t)(end - text);
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, len + al + 1);
    if (!out)
        return NULL;
    memcpy(out, text, pre);
    memcpy(out + pre, add, al);
    memcpy(out + pre + al, end, len - pre);
    *outLen = len + al;
    out[*outLen] = 0;
    return out;
}

static struct {
    int look;
    unsigned long long hash;
} g_seen[1024];
static int g_nseen;

static const char *const kArchives[] = {"ui\\textures\\data_bound\\ControllerIcons.zip", "ui\\textures\\data_bound\\Help_Buttons.zip",
                                        "ui\\textures\\data_bound\\UI_Symbols.zip", "UI.zip"};

/* one archive: every look (or the XAML change), the file written, served as media\<rel> */
static int ov_archive(int which, const WCHAR *cache, DWORD dirOut[3])
{
    const char *rel = kArchives[which];
    WCHAR path[MAX_PATH], wrel[160];
    DWORD len = 0;
    MultiByteToWideChar(CP_UTF8, 0, rel, -1, wrel, 160);
    lstrcpyW(path, g_media);
    lstrcatW(path, wrel);
    BYTE *z = load_file(path, &len);
    ZEnt *e = NULL;
    int n = z ? zip_parse(z, len, &e) : -1;
    if (n < 0) {
        fh2_say("icons: %s: not readable", rel);
        return 0;
    }
    const char *arcName = strrchr(rel, '\\') ? strrchr(rel, '\\') + 1 : rel;
    char arcBase[64];
    lstrcpynA(arcBase, arcName, 64);
    if (strrchr(arcBase, '.'))
        *strrchr(arcBase, '.') = 0;
    ZOut *o = (ZOut *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (n + 1) * sizeof *o);
    int looks = 0, changed = 0;
    for (int k = 0; k < n; k++) {
        ZOut *q = &o[k];
        q->name = e[k].name, q->method = e[k].method, q->crc = e[k].crc, q->usize = e[k].usize, q->len = e[k].csize;
        for (int s = 0; s < NSETS; s++)
            q->d[s] = z + e[k].dataOff;
        if (which == 3) { /* UI.zip: the switch in the controller settings */
            if (!g_uiAdaptive || lstrcmpiA(e[k].name, "Scenes/Anthem/A_ControllerOptions.xaml"))
                continue;
            BYTE *raw = zip_read(z, &e[k]);
            size_t nl = 0;
            char *t = raw ? xaml_switch((char *)raw, e[k].usize, &nl) : NULL;
            if (t) {
                q->method = 0, q->usize = q->len = (DWORD)nl, q->crc = crc_update(0, (BYTE *)t, nl);
                for (int s = 0; s < NSETS; s++)
                    q->d[s] = (BYTE *)t;
                changed++;
            } else
                fh2_say("icons: UI.zip: the controller settings are not as expected -- no Adaptive triggers switch");
            if (raw)
                HeapFree(GetProcessHeap(), 0, raw);
            continue;
        }
        /* a picture: the PlayStation look from the icon file (the drawing: made from the game's), the keyboard look */
        int isDiagram = which == 2 && !lstrcmpiA(e[k].name, "Help_Controller.swatchbin");
        char pngName[220], base[160];
        lstrcpynA(base, e[k].name, 160);
        if (strrchr(base, '.'))
            *strrchr(base, '.') = 0;
        wsprintfA(pngName, "ps/%s/%s.png", arcBase, base);
        int hasPs = 0;
        for (int i = 0; i < g_packN && !hasPs; i++)
            hasPs = !lstrcmpiA(g_packEnt[i].name, pngName);
        WCHAR label[32];
        int hasKb = g_haveKb && !isDiagram && which != 2 && key_label_for(e[k].name, label, 32);
        if (!hasPs && !hasKb && !isDiagram)
            continue;
        BYTE *raw = zip_read(z, &e[k]);
        int w = 0, h = 0;
        BYTE *xb = raw ? tex_decode(raw, e[k].usize, &w, &h) : NULL;
        if (!xb) {
            fh2_say("icons: %s %s: not a picture this DLL reads -- left as it is", rel, e[k].name);
            if (raw)
                HeapFree(GetProcessHeap(), 0, raw);
            continue;
        }
        BYTE *rgba[NSETS] = {xb, NULL, NULL};
        if (isDiagram) {
            rgba[1] = diagram_look(xb, w, h, 1);
            if (g_haveKb)
                rgba[2] = diagram_look(xb, w, h, 2);
        } else {
            int pw = 0, ph = 0;
            if (hasPs) {
                rgba[1] = pack_png(pngName, &pw, &ph);
                if (rgba[1] && (pw != w || ph != h)) {
                    fh2_say("icons: DualSense.icons %s is %dx%d, the game's %dx%d -- not used", pngName, pw, ph, w, h);
                    HeapFree(GetProcessHeap(), 0, rgba[1]);
                    rgba[1] = NULL;
                }
            }
            if (hasKb)
                rgba[2] = key_icon(w, h, label, which == 1);
        }
        /* no two icons the same in a look: the memory switch tells them by their bytes (a corner pixel at alpha
           1/255 makes a copy different, unseen) */
        for (int s2 = 0; s2 < NSETS; s2++) {
            BYTE *px = rgba[s2] ? rgba[s2] : (s2 == 0 ? xb : NULL);
            if (!px)
                continue;
            for (int tries = 0; tries < 250; tries++) {
                unsigned long long hsh = 1469598103934665603ull;
                for (size_t i = 0; i < (size_t)w * h * 4; i++)
                    hsh = (hsh ^ px[i]) * 1099511628211ull;
                hsh ^= (unsigned long long)w << 48 ^ (unsigned long long)h << 32;
                int dup = 0;
                for (int i = 0; i < g_nseen && !dup; i++)
                    dup = g_seen[i].look == s2 && g_seen[i].hash == hsh;
                if (!dup) {
                    if (g_nseen < (int)(sizeof g_seen / sizeof g_seen[0]))
                        g_seen[g_nseen].look = s2, g_seen[g_nseen].hash = hsh, g_nseen++;
                    break;
                }
                px[3] = (BYTE)(px[3] + 1); /* the top-left pixel, transparent in every icon */
            }
        }
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
                q->d[s] = enc[s];
            }
            looks++;
        } else
            for (int s = 0; s < NSETS; s++)
                if (enc[s])
                    HeapFree(GetProcessHeap(), 0, enc[s]);
        for (int s = 0; s < NSETS; s++)
            if (rgba[s])
                HeapFree(GetProcessHeap(), 0, rgba[s]);
        HeapFree(GetProcessHeap(), 0, raw);
    }
    DWORD l[NSETS], d[NSETS][3];
    BYTE *s[NSETS] = {NULL, NULL, NULL};
    for (int k = 0; k < NSETS && (which != 3 || changed); k++)
        s[k] = zip_build(o, n, k, &l[k], d[k]);
    /* the pictures made for it are in the archives now: freed (no stray copies of them for the memory search) */
    for (int k = 0; k < n; k++)
        for (int v = 0; v < NSETS; v++) {
            BYTE *q = (BYTE *)o[k].d[v];
            int again = q >= z && q < z + len;
            for (int u = 0; u < v && !again; u++)
                again = o[k].d[u] == q;
            if (!again)
                HeapFree(GetProcessHeap(), 0, q);
        }
    HeapFree(GetProcessHeap(), 0, o);
    HeapFree(GetProcessHeap(), 0, z);
    if (which == 3 && !changed) {
        HeapFree(GetProcessHeap(), 0, e);
        return 0;
    }
    if (!s[0] || !s[1] || !s[2] || l[0] != l[1] || l[1] != l[2]) {
        fh2_say("icons: %s: could not be made", rel);
        return 0;
    }
    memcpy(dirOut, d[0], sizeof d[0]);
    IconFile *f = &g_icon[g_icons];
    ZeroMemory(f, sizeof *f);
    lstrcpyW(f->suffix, L"media\\");
    lstrcatW(f->suffix, wrel);
    f->size = len;
    lstrcpyW(f->file, cache);
    {
        WCHAR wb[64];
        MultiByteToWideChar(CP_UTF8, 0, arcName, -1, wb, 64);
        lstrcatW(f->file, wb);
    }
    f->len = l[0];
    for (int k = 0; k < NSETS; k++)
        f->set[k] = s[k];
    if (!memcmp(s[0], s[1], l[0]) && !memcmp(s[0], s[2], l[0])) {
        int ok = write_set(f, 0, 1);
        for (int k = 0; k < NSETS; k++)
            f->set[k] = NULL; /* a plain file */
        if (!ok)
            return 0;
    } else {
        f->runs = (DWORD *)HeapAlloc(GetProcessHeap(), 0, 2 * sizeof(DWORD) * 8192);
        for (DWORD i = 0; f->runs && i < l[0];) {
            if (s[0][i] == s[1][i] && s[0][i] == s[2][i]) {
                i++;
                continue;
            }
            DWORD a0 = i, last = i;
            while (i < l[0] && i - last < 256) {
                if (s[0][i] != s[1][i] || s[0][i] != s[2][i])
                    last = i;
                i++;
            }
            if (f->nruns < 8192) {
                f->runs[2 * f->nruns] = a0;
                f->runs[2 * f->nruns + 1] = last + 1 - a0;
                f->nruns++;
            }
        }
        if (!write_set(f, g_iconStart, 1))
            return 0;
    }
    g_icons++;
    fh2_say("icons: %s: %d pictures in every look, %d XAML changes, %lu bytes", rel, looks, changed, l[0]);
    HeapFree(GetProcessHeap(), 0, e);
    return 1;
}

/* the game's zipmanifest.xml with the new archives' directories */
static int ov_manifest(const WCHAR *cache, DWORD dirs[][3], const int *made)
{
    WCHAR path[MAX_PATH], out[MAX_PATH];
    DWORD len = 0;
    lstrcpyW(path, g_media);
    lstrcatW(path, L"zipmanifest.xml");
    char *t = (char *)load_file(path, &len);
    if (!t)
        return 0;
    size_t tl = len;
    char *text = (char *)HeapAlloc(GetProcessHeap(), 0, len + 1024);
    memcpy(text, t, len + 1);
    for (int a = 0; a < 4; a++) {
        if (!made[a])
            continue;
        char key[200], low[160];
        lstrcpyA(low, kArchives[a]);
        CharLowerA(low);
        wsprintfA(key, "path=\"game:\\media\\%s\"", low);
        char *at = NULL;
        for (char *p = text; *p && !at; p++)
            if (!_strnicmp(p, key, strlen(key)))
                at = p;
        char *end = at ? strchr(at, '>') : NULL;
        char *ds = at ? strstr(at, "dirstart=\"") : NULL, *de = at ? strstr(at, "direntries=\"") : NULL;
        if (!end || !ds || ds > end || !de || de > end)
            return 0;
        de = strchr(de + 12, '"');
        char line[600], tail[256];
        int keep = (int)(ds - at);
        lstrcpynA(tail, de + 1, (int)(end - de));
        memcpy(line, at, keep);
        wsprintfA(line + keep, "dirstart=\"%lu\" dirsize=\"%lu\" direntries=\"%lu\"%s", dirs[a][0], dirs[a][1], dirs[a][2], tail);
        size_t ll = strlen(line), old = (size_t)(end - at);
        memmove(at + ll, end, tl - (size_t)(end - text) + 1);
        memcpy(at, line, ll);
        tl = tl - old + ll;
    }
    lstrcpyW(out, cache);
    lstrcatW(out, L"zipmanifest.xml");
    HANDLE h = CreateFileW(out, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD w = 0;
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    WriteFile(h, text, (DWORD)tl, &w, NULL);
    CloseHandle(h);
    IconFile *f = &g_icon[g_icons++];
    ZeroMemory(f, sizeof *f);
    lstrcpyW(f->suffix, L"media\\zipmanifest.xml");
    f->size = len;
    lstrcpyW(f->file, out);
    HeapFree(GetProcessHeap(), 0, t);
    HeapFree(GetProcessHeap(), 0, text);
    return w == tl;
}

static void tex_build(void);
static DWORD WINAPI icons_thread(void *arg);

/* all of it, once, at the game's first file open (not under the loader lock) */
static BOOL CALLBACK ov_build(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    WCHAR cache[MAX_PATH], packPath[MAX_PATH];
    DWORD dirs[4][3];
    int made[4] = {0, 0, 0, 0};
    (void)once, (void)param, (void)ctx;
    ULONGLONG t0 = GetTickCount64();
    crc_init();
    if (!ov_find_game()) {
        fh2_say("icons: the game folder is not in data\\azimuth.ini -- the game's own icons");
        return TRUE;
    }
    /* the PlayStation pictures: a mods\\DualSense.icons of your own when there, else the ones inside this DLL */
    lstrcpyW(packPath, g_modDir);
    lstrcatW(packPath, L"DualSense.icons");
    g_pack = load_file(packPath, &g_packLen);
    if (g_pack)
        fh2_say("icons: the PlayStation pictures from mods\\DualSense.icons");
    else
        g_pack = (BYTE *)kIconsPack, g_packLen = ICONS_PACK_LEN;
    g_packN = g_packLen ? zip_parse(g_pack, g_packLen, &g_packEnt) : -1;
    if (g_packN < 0) {
        g_packN = 0;
        fh2_say("icons: no PlayStation pictures -- the PlayStation look is the Xbox one");
    }
    lstrcpyW(cache, g_modDir);
    lstrcatW(cache, L"DualSense.cache\\");
    CreateDirectoryW(cache, NULL);
    for (int a = 0; a < 4; a++)
        made[a] = ov_archive(a, cache, dirs[a]);
    if ((made[0] || made[1] || made[2] || made[3]) && !ov_manifest(cache, dirs, made)) {
        fh2_say("icons: zipmanifest.xml could not be changed -- the game's own files");
        g_icons = 0;
        return TRUE;
    }
    InterlockedExchange(&g_iconSet, g_iconStart);
    fh2_say("icons: made in %.2f s; %s look to start%s", (GetTickCount64() - t0) / 1000.0, kSetName[g_iconStart],
            S.icons == 2 ? ", then the look of the controller in use" : "");
    tex_build();
    if (S.icons == 2) {
        HANDLE t = CreateThread(NULL, 0, icons_thread, NULL, 0, NULL);
        if (t)
            CloseHandle(t);
    }
    return TRUE;
}

/* ================================================================================================================ */
/* serving them                                                                                                     */
static IconFile *icon_match(const WCHAR *path)
{
    int n;
    if (!path || !g_icons)
        return NULL;
    n = lstrlenW(path);
    for (int i = 0; i < g_icons; i++) {
        IconFile *f = &g_icon[i];
        int k = lstrlenW(f->suffix), j;
        if (n <= k || (path[n - k - 1] != '\\' && path[n - k - 1] != '/') || f->state < 0)
            continue;
        for (j = 0; j < k; j++) {
            WCHAR a = path[n - k + j], b = f->suffix[j];
            if (a == '/')
                a = '\\';
            if (a >= 'A' && a <= 'Z')
                a += 32;
            if (b >= 'A' && b <= 'Z')
                b += 32;
            if (a != b)
                break;
        }
        if (j < k)
            continue;
        if (!f->state) {
            WIN32_FILE_ATTRIBUTE_DATA a;
            int ok = g_GetFileAttributesExW(path, GetFileExInfoStandard, &a) && !a.nFileSizeHigh && a.nFileSizeLow == f->size;
            AcquireSRWLockExclusive(&g_iconLock);
            if (!f->state) {
                f->state = ok ? 1 : -1;
                if (!ok)
                    fh2_say_w("icons: the game's file is not the one expected, left as it is: ", f->suffix);
            }
            ReleaseSRWLockExclusive(&g_iconLock);
        }
        if (f->state > 0)
            return f;
    }
    return NULL;
}

static IconFile *icon_of_handle(HANDLE h)
{
    if (!h)
        return NULL;
    for (int i = 0; i < g_icons; i++)
        for (int k = 0; k < 4; k++)
            if (g_icon[i].handle[k] == h)
                return &g_icon[i];
    return NULL;
}

static HANDLE WINAPI hk_CreateFileW(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp,
                                    DWORD flags, HANDLE tmpl)
{
    InitOnceExecuteOnce(&g_iconOnce, ov_build, NULL, NULL);
    IconFile *f = disp == OPEN_EXISTING && !(access & (GENERIC_WRITE | GENERIC_ALL)) ? icon_match(name) : NULL;
    if (!f)
        return g_CreateFileW(name, access, share, sa, disp, flags, tmpl);
    /* the archives: another look is written into them while the game has them open */
    HANDLE h = g_CreateFileW(f->file, access, share | FILE_SHARE_READ | FILE_SHARE_WRITE, sa, disp, flags, tmpl);
    if (h != INVALID_HANDLE_VALUE && f->set[0])
        for (int k = 0; k < 4; k++)
            if (!f->handle[k]) {
                f->handle[k] = h;
                break;
            }
    fh2_say("icons: the game opened %ls (%.1f s)", f->suffix, (GetTickCount64() - g_iconT0) / 1000.0);
    return h;
}

static BOOL WINAPI hk_ReadFile(HANDLE h, LPVOID buf, DWORD n, LPDWORD got, LPOVERLAPPED ov)
{
    IconFile *f = g_icons ? icon_of_handle(h) : NULL;
    if (f) {
        LONG k = InterlockedIncrement(&f->reads);
        if (k == 1 || k % 200 == 0)
            fh2_say("icons: the game reads %ls (read #%ld, %.1f s)", f->suffix, k, (GetTickCount64() - g_iconT0) / 1000.0);
    }
    return g_ReadFile(h, buf, n, got, ov);
}

static DWORD WINAPI hk_GetFileAttributesW(LPCWSTR name)
{
    InitOnceExecuteOnce(&g_iconOnce, ov_build, NULL, NULL);
    IconFile *f = icon_match(name);
    return g_GetFileAttributesW(f ? f->file : name);
}

static BOOL WINAPI hk_GetFileAttributesExW(LPCWSTR name, GET_FILEEX_INFO_LEVELS level, LPVOID info)
{
    InitOnceExecuteOnce(&g_iconOnce, ov_build, NULL, NULL);
    IconFile *f = icon_match(name);
    return g_GetFileAttributesExW(f ? f->file : name, level, info);
}

static HANDLE WINAPI hk_FindFirstFileExW(LPCWSTR name, FINDEX_INFO_LEVELS level, LPVOID data, FINDEX_SEARCH_OPS op,
                                         LPVOID filter, DWORD flags)
{
    InitOnceExecuteOnce(&g_iconOnce, ov_build, NULL, NULL);
    IconFile *f = icon_match(name);
    return g_FindFirstFileExW(f ? f->file : name, level, data, op, filter, flags);
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
/* the icons in the game's memory                                                                                   */
typedef struct {
    const BYTE *data[NSETS];
    const BYTE *hdr; /* its .swatchbin header (the same in every look) */
    DWORD len;
    DWORD anchor[NSETS];
    ULONGLONG key[NSETS];
    char name[64];
    int seen; /* found in the last search */
} IconTex;
static IconTex *g_tex;
static int g_ntex;
static BYTE *g_bloom;
typedef struct {
    BYTE *at;
    int tex;
} Found;
static Found *g_found;
static int g_nfound, g_capfound;
typedef struct {
    ULONGLONG key;
    int tex, set;
} KeySlot;
static KeySlot g_keys[8192];

static DWORD key_hash(ULONGLONG k) { return (DWORD)((k * 0x9E3779B97F4A7C15ull) >> 44); }

/* 8 bytes of the icon unlike the rest of it: the most different bytes (an anti-aliased edge), not repeated in it */
static DWORD pick_anchor(const BYTE *d, DWORD len)
{
    DWORD best = 0;
    int bestScore = -1;
    for (DWORD o = 0; o + 8 <= len; o += 4) {
        int distinct = 0;
        for (int i = 0; i < 8; i++) {
            int seen = 0;
            for (int j = 0; j < i; j++)
                seen |= d[o + i] == d[o + j];
            distinct += !seen;
        }
        if (distinct <= bestScore)
            continue;
        ULONGLONG q = *(const ULONGLONG *)(d + o);
        int again = 0;
        for (DWORD k = 0; k + 8 <= len && !again; k += 4)
            again = k != o && *(const ULONGLONG *)(d + k) == q;
        if (again)
            continue;
        bestScore = distinct;
        best = o;
        if (distinct == 8)
            break;
    }
    return best;
}

static void key_put(ULONGLONG k, int t, int v)
{
    for (DWORD h = key_hash(k) & 8191;; h = (h + 1) & 8191)
        if (g_keys[h].tex < 0) {
            g_keys[h].key = k, g_keys[h].tex = t, g_keys[h].set = v;
            return;
        }
}

/* the icons: the stored entries whose looks differ */
static void tex_build(void)
{
    int cap = 0;
    for (int i = 0; i < 8192; i++)
        g_keys[i].tex = -1;
    for (int i = 0; i < g_icons; i++) {
        IconFile *f = &g_icon[i];
        if (!f->set[0])
            continue;
        const BYTE *z = f->set[0];
        ZEnt *e = NULL;
        int n = zip_parse(z, f->len, &e);
        for (int k = 0; k < n; k++) {
            DWORD off = e[k].dataOff, cs = e[k].csize;
            if (e[k].method || cs < 0x53 + 64 || (!memcmp(f->set[0] + off, f->set[1] + off, cs) && !memcmp(f->set[0] + off, f->set[2] + off, cs)))
                continue;
            DWORD texLen = *(const DWORD *)(z + off + 0x20);
            if (texLen < 16 + 64 || 0x53 + texLen > cs)
                continue;
            if (g_ntex == cap) {
                cap = cap ? cap * 2 : 256;
                g_tex = (IconTex *)(g_tex ? HeapReAlloc(GetProcessHeap(), 0, g_tex, cap * sizeof *g_tex)
                                          : HeapAlloc(GetProcessHeap(), 0, cap * sizeof *g_tex));
            }
            IconTex *t = &g_tex[g_ntex];
            t->len = texLen - 16;
            t->hdr = z + off;
            lstrcpynA(t->name, e[k].name, 64);
            for (int v = 0; v < NSETS; v++) {
                t->data[v] = f->set[v] + off + 0x53;
                t->anchor[v] = pick_anchor(t->data[v], t->len);
                t->key[v] = *(const ULONGLONG *)(t->data[v] + t->anchor[v]);
            }
            g_ntex++;
        }
        if (e)
            HeapFree(GetProcessHeap(), 0, e);
    }
    g_bloom = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 1 << 17);
    for (int i = 0; g_bloom && i < g_ntex && i < 2500; i++)
        for (int v = 0; v < NSETS; v++) {
            DWORD h = key_hash(g_tex[i].key[v]);
            g_bloom[h >> 3] |= (BYTE)(1 << (h & 7));
            key_put(g_tex[i].key[v], i, v);
        }
    fh2_say("icons: %d icon textures to look for in the game's memory", g_ntex);
}

static int is_ours(const BYTE *p)
{
    for (int i = 0; i < g_icons; i++)
        for (int v = 0; v < NSETS; v++)
            if (g_icon[i].set[v] && p >= g_icon[i].set[v] && p < g_icon[i].set[v] + g_icon[i].len)
                return 1;
    return 0;
}

static void found_add(BYTE *at, int tex)
{
    for (int i = 0; i < g_nfound; i++)
        if (g_found[i].at == at)
            return;
    if (g_nfound == g_capfound) {
        g_capfound = g_capfound ? g_capfound * 2 : 512;
        g_found = (Found *)(g_found ? HeapReAlloc(GetProcessHeap(), 0, g_found, g_capfound * sizeof *g_found)
                                    : HeapAlloc(GetProcessHeap(), 0, g_capfound * sizeof *g_found));
    }
    g_found[g_nfound].at = at;
    g_found[g_nfound].tex = tex;
    g_nfound++;
}

/* which look the memory at p holds of icon t, or -1 */
static int holds(BYTE *p, int t, BYTE *tmp)
{
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), p, tmp, g_tex[t].len, &got) || got != g_tex[t].len)
        return -1;
    /* the look now first (two looks of an icon can be the same) */
    if (g_iconSet >= 0 && !memcmp(tmp, g_tex[t].data[g_iconSet], g_tex[t].len))
        return g_iconSet;
    for (int v = 0; v < NSETS; v++)
        if (!memcmp(tmp, g_tex[t].data[v], g_tex[t].len))
            return v;
    return -1;
}

static void mem_scan(void)
{
    static BYTE chunk[(1 << 20) + 8];
    BYTE *tmp = (BYTE *)HeapAlloc(GetProcessHeap(), 0, 1 << 21);
    MEMORY_BASIC_INFORMATION mbi;
    ULONGLONG t0 = GetTickCount64(), bytes = 0, sealedMb = 0;
    int regions = 0, files = 0;
    if (!tmp)
        return;
    for (BYTE *a = (BYTE *)0x10000; VirtualQuery(a, &mbi, sizeof mbi) == sizeof mbi; a = (BYTE *)mbi.BaseAddress + mbi.RegionSize) {
        DWORD prot = mbi.Protect & 0xFF;
        /* writable memory, and read-only: the engine seals texture memory nobody writes to (write tracking) */
        if (mbi.State != MEM_COMMIT || (mbi.Type != MEM_PRIVATE && mbi.Type != MEM_MAPPED) || (mbi.Protect & PAGE_GUARD) ||
            (prot != PAGE_READWRITE && prot != PAGE_WRITECOPY && prot != PAGE_EXECUTE_READWRITE && prot != PAGE_READONLY))
            continue;
        regions++;
        if (prot == PAGE_READONLY)
            sealedMb += mbi.RegionSize;
        BYTE *r = (BYTE *)mbi.BaseAddress, *end = r + mbi.RegionSize;
        for (BYTE *c = r; c < end; c += 1 << 20) {
            SIZE_T want = (SIZE_T)(end - c) < (1 << 20) + 8 ? (SIZE_T)(end - c) : (1 << 20) + 8, got = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), c, chunk, want, &got) || got < 8)
                continue;
            bytes += got;
            /* whole .swatchbin files kept by the game: "burG", the header, then the pixels (not aligned) */
            for (BYTE *p = chunk; (p = (BYTE *)memchr(p, 'b', (chunk + got) - p - 3 > 0 ? (size_t)((chunk + got) - p - 3) : 0)) != NULL; p++) {
                if (p[1] != 'u' || p[2] != 'r' || p[3] != 'G' || p - chunk >= (1 << 20))
                    continue;
                BYTE hdr[0x53];
                SIZE_T hg = 0;
                if (!ReadProcessMemory(GetCurrentProcess(), c + (p - chunk), hdr, 0x53, &hg) || hg != 0x53)
                    continue;
                for (int t = 0; t < g_ntex; t++)
                    if (!memcmp(hdr, g_tex[t].hdr, 0x53)) {
                        BYTE *base = c + (p - chunk) + 0x53;
                        if (!is_ours(base) && holds(base, t, tmp) >= 0) {
                            found_add(base, t);
                            files++;
                        }
                    }
            }
            for (SIZE_T o = 0; o + 8 <= got && o < (1 << 20); o += 4) {
                ULONGLONG q = *(ULONGLONG *)(chunk + o);
                DWORD h = key_hash(q);
                if (!(g_bloom[h >> 3] & (1 << (h & 7))))
                    continue;
                for (DWORD k = h & 8191; g_keys[k].tex >= 0; k = (k + 1) & 8191) {
                    if (g_keys[k].key != q)
                        continue;
                    int t = g_keys[k].tex, v = g_keys[k].set;
                    DWORD an = g_tex[t].anchor[v];
                    if (o >= an && o - an + 64 <= got && memcmp(chunk + o - an, g_tex[t].data[v], 64))
                        continue;
                    BYTE *base = c + o - an;
                    if (!is_ours(base) && holds(base, t, tmp) == v)
                        found_add(base, t);
                }
            }
        }
    }
    HeapFree(GetProcessHeap(), 0, tmp);
    fh2_say("icons: the game's memory searched: %d regions, %llu MB (%llu MB read-only) in %.1f s -- %d icon copies found "
            "(%d of them in whole .swatchbin files)", regions, bytes >> 20, sealedMb >> 20, (GetTickCount64() - t0) / 1000.0,
            g_nfound, files);
    /* which icons the game has in memory, and how many copies of each */
    char line[900];
    int len = 0, missing = 0;
    for (int t = 0; t < g_ntex; t++)
        g_tex[t].seen = 0;
    for (int i = 0; i < g_nfound; i++)
        g_tex[g_found[i].tex].seen++;
    line[0] = 0;
    for (int t = 0; t < g_ntex; t++)
        if (g_tex[t].seen && len < 820)
            len += wsprintfA(line + len, " %s%s", g_tex[t].name, g_tex[t].seen > 1 ? (g_tex[t].seen > 2 ? "(3+)" : "(2)") : "");
        else if (!g_tex[t].seen)
            missing++;
    fh2_say("icons:   in memory:%s", line);
    fh2_say("icons:   %d icons not in memory now (not loaded by the game)", missing);
}

/* Writing into texture memory the engine has sealed (read-only after a while without writes; Azimuth 0.9.7: its
   write tracking, 0xDB820) faults; the engine's exception handler (0xDBB00) unseals the pages, marks them written --
   the texture goes to the GPU again -- and the write goes on. Only memory with a write watch is the engine's; ours
   (called last) unprotects a page of an icon being written that nobody else took. */
static volatile DWORD g_writer;
static BYTE *volatile g_wlo, *volatile g_whi;
static volatile LONG g_ourUnseals;
static PVOID g_veh;

static LONG CALLBACK icons_veh(EXCEPTION_POINTERS *x)
{
    EXCEPTION_RECORD *r = x->ExceptionRecord;
    DWORD old;
    if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || r->NumberParameters < 2 || r->ExceptionInformation[0] != 1 ||
        GetCurrentThreadId() != g_writer)
        return EXCEPTION_CONTINUE_SEARCH;
    BYTE *a = (BYTE *)r->ExceptionInformation[1];
    if (a < g_wlo || a >= g_whi || !VirtualProtect((void *)((ULONG_PTR)a & ~(ULONG_PTR)0xFFF), 1, PAGE_READWRITE, &old))
        return EXCEPTION_CONTINUE_SEARCH;
    InterlockedIncrement(&g_ourUnseals);
    return EXCEPTION_CONTINUE_EXECUTION;
}

static int write_watched(BYTE *p)
{
    PVOID pages[1];
    ULONG_PTR count = 1;
    ULONG gran;
    return GetWriteWatch(0, (void *)((ULONG_PTR)p & ~(ULONG_PTR)0xFFF), 4096, pages, &count, &gran) == 0;
}

/* one icon written: 2 = through sealed texture memory, 1 = writable memory, 0 = not (read-only, not the engine's) */
static int mem_put(BYTE *at, const BYTE *src, DWORD len)
{
    MEMORY_BASIC_INFORMATION m;
    if (!VirtualQuery(at, &m, sizeof m) || m.State != MEM_COMMIT)
        return 0;
    DWORD prot = m.Protect & 0xFF;
    int sealed = prot == PAGE_READONLY;
    if (sealed && !write_watched(at))
        return 0;
    if (!sealed && prot != PAGE_READWRITE && prot != PAGE_EXECUTE_READWRITE)
        return 0;
    if (!g_veh)
        g_veh = AddVectoredExceptionHandler(0, icons_veh);
    g_wlo = at, g_whi = at + len;
    g_writer = GetCurrentThreadId();
    memcpy(at, src, len);
    g_writer = 0;
    return sealed ? 2 : 1;
}

/* the wanted look over every found icon that holds another one */
static int g_sealedWrites;
static void mem_write(int set, BYTE *tmp, int *written, int *already, int *gone)
{
    for (int i = 0; i < g_nfound; i++) {
        Found *f = &g_found[i];
        int h = holds(f->at, f->tex, tmp);
        if (h < 0) {
            (*gone)++;
            continue;
        }
        if (h == set) {
            (*already)++;
            continue;
        }
        int w = mem_put(f->at, g_tex[f->tex].data[set], g_tex[f->tex].len);
        *written += w != 0;
        g_sealedWrites += w == 2;
    }
}

/* the places known from the last time first (at once), then the whole memory again: icons the game loaded since */
static void mem_switch(int set)
{
    BYTE *tmp = (BYTE *)HeapAlloc(GetProcessHeap(), 0, 1 << 21);
    int w1 = 0, a1 = 0, g1 = 0, w2 = 0, a2 = 0, g2 = 0;
    if (!tmp || !g_ntex)
        return;
    LONG ours = g_ourUnseals;
    g_sealedWrites = 0;
    mem_write(set, tmp, &w1, &a1, &g1);
    g_nfound = 0;
    mem_scan();
    mem_write(set, tmp, &w2, &a2, &g2);
    HeapFree(GetProcessHeap(), 0, tmp);
    fh2_say("icons: in the game's memory: %d icons changed at once, %d more after the search (%d found, %d already right); "
            "%d of them in sealed texture memory (%ld pages unsealed by us, not by the engine)",
            w1, w2, g_nfound, a2, g_sealedWrites, g_ourUnseals - ours);
    /* 3 seconds on: still the wanted look? (the game might write its own copy back) */
    Sleep(3000);
    if (g_iconSet != set)
        return;
    BYTE *t2 = (BYTE *)HeapAlloc(GetProcessHeap(), 0, 1 << 21);
    char back[600];
    int nb = 0, bl = 0;
    back[0] = 0;
    for (int i = 0; t2 && i < g_nfound; i++) {
        int h = holds(g_found[i].at, g_found[i].tex, t2);
        if (h >= 0 && h != set) {
            nb++;
            if (bl < 520)
                bl += wsprintfA(back + bl, " %s", g_tex[g_found[i].tex].name);
            mem_put(g_found[i].at, g_tex[g_found[i].tex].data[set], g_tex[g_found[i].tex].len);
        }
    }
    if (t2)
        HeapFree(GetProcessHeap(), 0, t2);
    if (nb)
        fh2_say("icons: 3 s on, %d icons had another look again (written again):%s", nb, back);
    else
        fh2_say("icons: 3 s on, all of them still right");
}

static void icons_switch(int set, const char *why)
{
    for (int i = 0; i < g_icons; i++)
        if (g_icon[i].set[0] && !write_set(&g_icon[i], set, 0))
            fh2_say_w("icons: could not write ", g_icon[i].file);
    InterlockedExchange(&g_iconSet, set);
    fh2_say("icons: %s look now (%s, %.1f s)", kSetName[set], why, (GetTickCount64() - g_iconT0) / 1000.0);
    mem_switch(set);
}

/* the device of the last real input: the DualSense, an Xbox pad, or the keyboard (ours, or an old Keyboard.dll's) */
static DWORD WINAPI icons_thread(void *arg)
{
    typedef ULONGLONG (*KbLastFn)(void);
    KbLastFn kbLast = NULL;
    ULONGLONG lastSwitch = 0;
    (void)arg;
    for (;;) {
        Sleep(150);
        if (!kbLast && g_kbBuiltin)
            kbLast = Fh2KeyboardLastInput;
        else if (!kbLast) {
            HMODULE kb = GetModuleHandleW(L"Keyboard.dll");
            if (kb)
                kbLast = (KbLastFn)GetProcAddress(kb, "Fh2KeyboardLastInput");
        }
        ULONGLONG ds = g_dsInputAt, pad = g_padInputAt, key = kbLast ? kbLast() : 0, now = GetTickCount64();
        int want = g_iconSet;
        /* an Xbox pad moving with the DualSense is the DualSense through Steam or DS4Windows */
        if (ds && ds + 150 >= pad && ds >= key)
            want = 1;
        else if (pad && pad > ds + 150 && pad >= key)
            want = 0;
        else if (key && g_haveKb && key > ds && key > pad)
            want = 2;
        if (want >= 0 && want != g_iconSet && now - lastSwitch > 800) {
            lastSwitch = now;
            icons_switch(want, want == 1 ? "the DualSense is in use" : want == 2 ? "the keyboard is in use" : "an Xbox pad is in use");
        }
    }
}

/* for other mods and tests: the look now (-1 none), and a switch */
__declspec(dllexport) LONG Fh2IconLook(void) { return g_icons ? g_iconSet : -1; }
__declspec(dllexport) void Fh2SwitchIcons(int look)
{
    if (g_icons && look >= 0 && look < NSETS)
        icons_switch(look, "asked by another program");
}

/* at load: the hooks; the files are made at the game's first file open */
static void icons_install(Fh2Engine *e)
{
    g_iconT0 = GetTickCount64();
    if (S.icons == 0) {
        fh2_say("icons: the game's own ([Icons] PlayStation=Off)");
        return;
    }
    /* the keyboard's keys: ours ([Keys] in DualSense.ini), or an old Keyboard.dll's (Keyboard.ini) */
    if (g_kbBuiltin) {
        lstrcpyW(g_kbIni, g_modIni);
        g_haveKb = 1;
    } else {
        WCHAR kb[MAX_PATH];
        lstrcpyW(g_kbIni, g_modDir);
        lstrcatW(g_kbIni, L"Keyboard.ini");
        lstrcpyW(kb, g_modDir);
        lstrcatW(kb, L"Keyboard.dll");
        g_haveKb = GetFileAttributesW(kb) != INVALID_FILE_ATTRIBUTES;
    }
    g_iconStart = S.icons == 1 || ds_present() ? 1 : 0;
    if (GetEnvironmentVariableW(L"FH2_DS_TEST", NULL, 0))
        g_uiAdaptive = 1; /* tests without the game: the XAML change too */
    g_GetFileAttributesExW = (GetFileAttributesExWFn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetFileAttributesExW");
    void *p;
    if ((p = fh2_hook_import(e, "KERNEL32.dll", "GetFileAttributesExW", (void *)hk_GetFileAttributesExW)) != NULL)
        g_GetFileAttributesExW = (GetFileAttributesExWFn)p;
    g_GetFileAttributesW = (GetFileAttributesWFn)fh2_hook_import(e, "KERNEL32.dll", "GetFileAttributesW", (void *)hk_GetFileAttributesW);
    g_FindFirstFileExW = (FindFirstFileExWFn)fh2_hook_import(e, "KERNEL32.dll", "FindFirstFileExW", (void *)hk_FindFirstFileExW);
    g_ReadFile = (ReadFileFn)fh2_hook_import(e, "KERNEL32.dll", "ReadFile", (void *)hk_ReadFile);
    g_CreateFileW = (CreateFileWFn)fh2_hook_import(e, "KERNEL32.dll", "CreateFileW", (void *)hk_CreateFileW);
    if (!g_CreateFileW) {
        fh2_say("icons: the engine's CreateFileW import not found -- the game's own icons");
        return;
    }
    fh2_say("icons: ready (keyboard look %s); the files are made when the game starts reading", g_haveKb ? "on" : "off: the keyboard is off");
}
