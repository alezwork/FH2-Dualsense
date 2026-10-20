/* icons.h -- the game's button icons with the PlayStation pictures ([Icons] PlayStation: On, Off, or Auto: when a
   DualSense is there as the game starts; "PlayStation button icons" on the Mods page).
   The pictures are inside this DLL (DualSense.icons: a zip of PNGs in the sizes of the game's own icons, made by
   tools\make_pack.py); a DualSense\DualSense.icons of your own is taken instead. Every icon of the game's
   ControllerIcons.zip, Help_Buttons.zip and UI_Symbols.zip (the controls screen's drawing) that has a PlayStation
   picture goes to the game through the platform (set_file: Extra Options' mediaoverride, which the game reads in
   place of the archives' own), in the game's texture format (RGBA, 1D tiling: texture.h). Off: the platform's copies
   are taken away, and the game shows its own. Made once, when the game starts: the game reads its icons then. */
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

static int pack_has(const char *name)
{
    for (int i = 0; i < g_packN; i++)
        if (!lstrcmpiA(g_packEnt[i].name, name))
            return 1;
    return 0;
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

/* the pictures: DualSense\DualSense.icons when there is one, else the ones inside the DLL */
static int pack_open(void)
{
    WCHAR own[MAX_PATH];
    lstrcpyW(own, g_dir);
    lstrcatW(own, L"DualSense.icons");
    g_pack = load_file(own, &g_packLen);
    if (g_pack && (g_packN = zip_parse(g_pack, g_packLen, &g_packEnt)) > 0) {
        fh2_say("icons: DualSense\\DualSense.icons (%d files) -- instead of the pictures inside the DLL", g_packN);
        return 1;
    }
    if (g_pack)
        HeapFree(GetProcessHeap(), 0, g_pack);
    g_pack = (BYTE *)kIconsPack, g_packLen = ICONS_PACK_LEN;
    g_packN = g_packLen > 22 ? zip_parse(g_pack, g_packLen, &g_packEnt) : -1;
    return g_packN > 0;
}

/* one archive: its icons that have a PlayStation picture, to the game; returns how many */
static int icons_archive(const char *arc)
{
    WCHAR path[MAX_PATH], wrel[96];
    DWORD len = 0;
    int made = 0, failed = 0;
    MultiByteToWideChar(CP_UTF8, 0, arc, -1, wrel, 96);
    lstrcpyW(path, g_api->game_dir);
    lstrcatW(path, L"\\media\\ui\\textures\\data_bound\\");
    lstrcatW(path, wrel);
    lstrcatW(path, L".zip");
    BYTE *z = load_file(path, &len);
    ZEnt *e = NULL;
    int n = z ? zip_parse(z, len, &e) : -1;
    if (n < 0) {
        ds_note("icons: %s.zip: not readable -- its icons stay the game's", arc);
        if (z)
            HeapFree(GetProcessHeap(), 0, z);
        return 0;
    }
    for (int k = 0; k < n; k++) {
        char png[240], base[160], media[260];
        int diagram = !lstrcmpiA(arc, "UI_Symbols") && !lstrcmpiA(e[k].name, "Help_Controller.swatchbin");
        lstrcpynA(base, e[k].name, 160);
        if (strrchr(base, '.'))
            *strrchr(base, '.') = 0;
        wsprintfA(png, "ps/%s/%s.png", arc, base);
        if (!diagram && !pack_has(png))
            continue;
        BYTE *raw = zip_read(z, &e[k]), *xb = NULL, *ps = NULL;
        int w = 0, h = 0, pw = 0, ph = 0;
        xb = raw ? tex_decode(raw, e[k].usize, &w, &h) : NULL;
        if (xb)
            ps = diagram ? diagram_ps(xb, w, h) : pack_png(png, &pw, &ph);
        if (ps && !diagram && (pw != w || ph != h)) {
            fh2_say("icons: %s is %dx%d, the game's %dx%d -- not used", png, pw, ph, w, h);
            HeapFree(GetProcessHeap(), 0, ps);
            ps = NULL;
        }
        size_t el = 0;
        BYTE *enc = ps ? tex_encode(raw, w, h, ps, 0, &el) : NULL;
        wsprintfA(media, "ui/textures/data_bound/%s/%s", arc, e[k].name);
        if (enc && g_api->set_file(g_mod, media, enc, el))
            made++;
        else {
            failed++;
            fh2_say("icons: %s %s: %s", arc, e[k].name, !xb ? "not a picture this DLL reads" : !ps ? "no picture" : !enc ? "not made" : "the platform did not take it");
        }
        if (enc)
            HeapFree(GetProcessHeap(), 0, enc);
        if (ps)
            HeapFree(GetProcessHeap(), 0, ps);
        if (xb)
            HeapFree(GetProcessHeap(), 0, xb);
        if (raw)
            HeapFree(GetProcessHeap(), 0, raw);
    }
    HeapFree(GetProcessHeap(), 0, e);
    HeapFree(GetProcessHeap(), 0, z);
    fh2_say("icons: %s.zip: %d PlayStation pictures to the game%s", arc, made, failed ? " (some not: see above)" : "");
    return made;
}

/* when the game starts (the mod's own thread): the PlayStation pictures to the game, or the platform's copies away */
static void icons_make(void)
{
    static const char *const kArchives[3] = {"ControllerIcons", "Help_Buttons", "UI_Symbols"};
    double t0 = fh2_now();
    int on = S.icons == 1 || (S.icons == 2 && ds_present());
    if (g_api->api_version < 2) {
        ds_note("icons: the platform is too old for game files (2.1.0 or newer) -- the game's own icons");
        return;
    }
    if (!on) {
        g_api->clear_file(g_mod, NULL);
        ds_note("icons: the game's own (%s)", S.icons ? "no DualSense connected as the game starts" : "[Icons] PlayStation=Off");
        return;
    }
    if (!g_api->game_dir || !g_api->game_dir[0]) {
        ds_note("icons: the platform did not find the game's folder -- the game's own icons");
        return;
    }
    if (!pack_open()) {
        ds_note("icons: no PlayStation pictures in this build (and no DualSense\\DualSense.icons) -- the game's own icons");
        return;
    }
    int n = 0;
    for (int i = 0; i < 3; i++)
        n += icons_archive(kArchives[i]);
    ds_note("icons: %d PlayStation pictures to the game in %.2f s%s", n, fh2_now() - t0,
            n ? "" : " -- none: is Extra Options there (its mediaoverride)?");
}
