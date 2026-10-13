/* keyicons.h -- the keyboard look of the game's button icons: a key cap with the key the Keyboard mod gives that button
   (mods\Keyboard.ini [Keys], or its defaults), drawn here with GDI at 4x and reduced (anti-aliased), premultiplied. */
#pragma once

/* the Keyboard mod's controls and their defaults (keep in step with Keyboard.c) */
static const struct {
    const char *xbox;    /* the game's icon name part: A, LB, LeftThumb_Left, ... */
    const WCHAR *key;    /* Keyboard.ini [Keys] */
    const WCHAR *def;
    const WCHAR *key2;   /* a second control shown with it (sticks: both directions), or NULL */
    const WCHAR *def2;
} kKeyOf[] = {
    {"A", L"A", L"Space, Enter", NULL, NULL},
    {"B", L"B", L"E, Backspace", NULL, NULL},
    {"X", L"X", L"Q", NULL, NULL},
    {"Y", L"Y", L"R", NULL, NULL},
    {"LB", L"LB", L"LShift", NULL, NULL},
    {"RB", L"RB", L"C", NULL, NULL},
    {"LT", L"Brake", L"S", NULL, NULL},
    {"RT", L"Throttle", L"W", NULL, NULL},
    {"L3", L"LeftStickClick", L"F", NULL, NULL},
    {"R3", L"RightStickClick", L"V", NULL, NULL},
    {"MENU", L"Menu", L"Escape", NULL, NULL},
    {"VIEW", L"View", L"Tab", NULL, NULL},
    {"START", L"Menu", L"Escape", NULL, NULL},
    {"BACK", L"View", L"Tab", NULL, NULL},
    {"DPADUP", L"DPadUp", L"Up", NULL, NULL},
    {"DPADDOWN", L"DPadDown", L"Down", NULL, NULL},
    {"DPADLEFT", L"DPadLeft", L"Left", NULL, NULL},
    {"DPADRIGHT", L"DPadRight", L"Right", NULL, NULL},
    {"DPADUPDOWN", L"DPadUp", L"Up", L"DPadDown", L"Down"},
    {"DPADLEFTRIGHT", L"DPadLeft", L"Left", L"DPadRight", L"Right"},
    {"DPAD", L"DPadUp", L"Up", L"DPadDown", L"Down"}, /* all four: key_label_for adds left and right */
    {"LEFTTHUMB", L"SteerLeft", L"A", L"SteerRight", L"D"},
    {"LEFTTHUMB_LEFT", L"SteerLeft", L"A", NULL, NULL},
    {"LEFTTHUMB_RIGHT", L"SteerRight", L"D", NULL, NULL},
    {"LEFTTHUMB_LEFTRIGHT", L"SteerLeft", L"A", L"SteerRight", L"D"},
    {"LEFTTHUMB_UP", L"StickUp", L"", NULL, NULL},
    {"LEFTTHUMB_DOWN", L"StickDown", L"", NULL, NULL},
    {"LEFTTHUMB_UPDOWN", L"StickUp", L"", L"StickDown", L""},
    {"RIGHTTHUMB", L"LookLeft", L"J", L"LookRight", L"L"},
    {"RIGHTTHUMB_LEFT", L"LookLeft", L"J", NULL, NULL},
    {"RIGHTTHUMB_RIGHT", L"LookRight", L"L", NULL, NULL},
    {"RIGHTTHUMB_LEFTRIGHT", L"LookLeft", L"J", L"LookRight", L"L"},
    {"RIGHTTHUMB_UP", L"LookUp", L"I", NULL, NULL},
    {"RIGHTTHUMB_DOWN", L"LookDown", L"K", NULL, NULL},
    {"RIGHTTHUMB_UPDOWN", L"LookUp", L"I", L"LookDown", L"K"},
    {"THUMBSTICK_L", L"SteerLeft", L"A", L"SteerRight", L"D"},
    {"THUMBSTICK_R", L"LookLeft", L"J", L"LookRight", L"L"},
};

static WCHAR g_kbIni[MAX_PATH];

/* the first key bound to a control, as a short label (SPACE, ESC, an arrow); empty when none */
static void key_label(const WCHAR *control, const WCHAR *def, WCHAR *out, int n)
{
    static const struct {
        const WCHAR *name, *label;
    } kShort[] = {{L"Space", L"SPACE"}, {L"Enter", L"ENTER"}, {L"Return", L"ENTER"}, {L"Backspace", L"BKSP"},
                  {L"Tab", L"TAB"}, {L"Escape", L"ESC"}, {L"Esc", L"ESC"}, {L"Shift", L"SHIFT"}, {L"LShift", L"SHIFT"},
                  {L"RShift", L"SHIFT"}, {L"Ctrl", L"CTRL"}, {L"LCtrl", L"CTRL"}, {L"RCtrl", L"CTRL"}, {L"Alt", L"ALT"},
                  {L"LAlt", L"ALT"}, {L"RAlt", L"ALT"}, {L"Up", L"\x2191"}, {L"Down", L"\x2193"}, {L"Left", L"\x2190"},
                  {L"Right", L"\x2192"}, {L"PageUp", L"PGUP"}, {L"PageDown", L"PGDN"}, {L"Home", L"HOME"},
                  {L"End", L"END"}, {L"Insert", L"INS"}, {L"Delete", L"DEL"}, {L"CapsLock", L"CAPS"},
                  {L"Mouse1", L"LMB"}, {L"Mouse2", L"RMB"}, {L"Mouse3", L"MMB"}, {L"Mouse4", L"M4"}, {L"Mouse5", L"M5"},
                  {L"Comma", L","}, {L"Period", L"."}, {L"Minus", L"-"}, {L"Equals", L"="}, {L"Semicolon", L";"},
                  {L"Slash", L"/"}, {L"Backquote", L"`"}, {L"LBracket", L"["}, {L"RBracket", L"]"}, {L"Backslash", L"\\"},
                  {L"Quote", L"'"}, {L"NumpadPlus", L"NUM+"}, {L"NumpadMinus", L"NUM-"}, {L"NumpadStar", L"NUM*"},
                  {L"NumpadSlash", L"NUM/"}, {L"NumpadDot", L"NUM."}, {L"NumpadEnter", L"ENTER"}};
    WCHAR list[128], name[32];
    int k = 0;
    GetPrivateProfileStringW(L"Keys", control, def, list, 128, g_kbIni);
    while (list[k] == ' ')
        k++;
    int j = 0;
    while (list[k] && list[k] != ',' && list[k] != ';' && j < 31)
        name[j++] = list[k++];
    while (j && name[j - 1] == ' ')
        j--;
    name[j] = 0;
    out[0] = 0;
    if (!j)
        return;
    for (int i = 0; i < (int)(sizeof kShort / sizeof kShort[0]); i++)
        if (CompareStringOrdinal(name, -1, kShort[i].name, -1, TRUE) == CSTR_EQUAL) {
            lstrcpynW(out, kShort[i].label, n);
            return;
        }
    if (j == 7 && CompareStringOrdinal(name, 6, L"Numpad", 6, TRUE) == CSTR_EQUAL) {
        wsprintfW(out, L"NUM%c", name[6]);
        return;
    }
    lstrcpynW(out, name, n);
    CharUpperW(out);
}

/* the label for one of the game's icon names (Controller_LeftThumb_Left, Button_A_Lg, DPad_U ...); 0 = none */
static int key_label_for(const char *entry, WCHAR *out, int n)
{
    char base[96], key[96];
    const char *s = strrchr(entry, '/');
    lstrcpynA(base, s ? s + 1 : entry, 96);
    char *dot = strrchr(base, '.');
    if (dot)
        *dot = 0;
    key[0] = 0;
    if (!_strnicmp(base, "Controller_", 11))
        lstrcpyA(key, base + 11);
    else if (!_strnicmp(base, "Button_", 7)) {
        lstrcpyA(key, base + 7);
        char *u = strchr(key, '_'); /* _Lg, _Dark, _Light */
        if (u)
            *u = 0;
    } else if (!_strnicmp(base, "DPad_", 5)) {
        static const char *const m[][2] = {{"U", "DPADUP"}, {"D", "DPADDOWN"}, {"L", "DPADLEFT"}, {"R", "DPADRIGHT"},
                                           {"UD", "DPADUPDOWN"}, {"LR", "DPADLEFTRIGHT"}, {"ON", "DPAD"}, {"OFF", "DPAD"}};
        for (int i = 0; i < 8; i++)
            if (!lstrcmpiA(base + 5, m[i][0]))
                lstrcpyA(key, m[i][1]);
    } else if (!_strnicmp(base, "Thumbstick_", 11))
        wsprintfA(key, "THUMBSTICK_%s", base + 11);
    if (!key[0])
        return 0;
    CharUpperA(key);
    for (int i = 0; i < (int)(sizeof kKeyOf / sizeof kKeyOf[0]); i++) {
        if (lstrcmpiA(kKeyOf[i].xbox, key))
            continue;
        WCHAR a[24], b[24];
        key_label(kKeyOf[i].key, kKeyOf[i].def, a, 24);
        b[0] = 0;
        if (kKeyOf[i].key2)
            key_label(kKeyOf[i].key2, kKeyOf[i].def2, b, 24);
        if (!a[0] && !b[0])
            return 0;
        if (!lstrcmpiA(key, "DPAD")) { /* the whole D-pad: all four keys */
            WCHAR c2[24], d2[24];
            key_label(L"DPadLeft", L"Left", c2, 24);
            key_label(L"DPadRight", L"Right", d2, 24);
            wsprintfW(out, L"%s%s%s%s", a, b, c2, d2);
        } else if (a[0] && b[0])
            wsprintfW(out, L"%s %s", a, b);
        else
            lstrcpynW(out, a[0] ? a : b, n);
        return 1;
    }
    return 0;
}

/* ---- drawing: coverage masks with GDI at 4x ---- */
#define KS 4

typedef struct {
    HDC dc;
    HBITMAP bmp;
    unsigned char *bits; /* BGRA, top-down */
    int w, h;            /* at 4x */
} Canvas;

static int canvas_open(Canvas *c, int w, int h)
{
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w * KS;
    bi.bmiHeader.biHeight = -h * KS;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    c->w = w * KS, c->h = h * KS;
    c->dc = CreateCompatibleDC(NULL);
    c->bmp = CreateDIBSection(c->dc, &bi, DIB_RGB_COLORS, (void **)&c->bits, NULL, 0);
    if (!c->dc || !c->bmp)
        return 0;
    SelectObject(c->dc, c->bmp);
    return 1;
}

static void canvas_clear(Canvas *c)
{
    GdiFlush();
    memset(c->bits, 0, (size_t)c->w * c->h * 4);
}

static void canvas_close(Canvas *c)
{
    if (c->bmp)
        DeleteObject(c->bmp);
    if (c->dc)
        DeleteDC(c->dc);
}

/* the canvas (white on black) reduced to w x h coverage 0..1 */
static void canvas_mask(Canvas *c, float *cov)
{
    int w = c->w / KS, h = c->h / KS;
    GdiFlush();
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            unsigned s = 0;
            for (int yy = 0; yy < KS; yy++)
                for (int xx = 0; xx < KS; xx++)
                    s += c->bits[((size_t)(y * KS + yy) * c->w + x * KS + xx) * 4 + 1];
            cov[y * w + x] = s / (255.0f * KS * KS);
        }
}

static void canvas_rrect(Canvas *c, float x0, float y0, float x1, float y1, float r, int white)
{
    HBRUSH br = (HBRUSH)GetStockObject(white ? WHITE_BRUSH : BLACK_BRUSH);
    SelectObject(c->dc, br);
    SelectObject(c->dc, GetStockObject(NULL_PEN));
    RoundRect(c->dc, (int)(x0 * KS), (int)(y0 * KS), (int)(x1 * KS) + 1, (int)(y1 * KS) + 1, (int)(2 * r * KS), (int)(2 * r * KS));
}

/* text fitted into the box (x0 y0 x1 y1, in 1x pixels), white */
static void canvas_text(Canvas *c, const WCHAR *t, float x0, float y0, float x1, float y1)
{
    float bw = (x1 - x0) * KS, bh = (y1 - y0) * KS;
    int size = (int)(bh * 0.92f);
    HFONT f = NULL;
    SIZE ext = {0, 0};
    for (; size > 6; size = size * 9 / 10) {
        if (f)
            DeleteObject(f);
        f = CreateFontW(-size, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                        ANTIALIASED_QUALITY, VARIABLE_PITCH, L"Bahnschrift");
        SelectObject(c->dc, f);
        GetTextExtentPoint32W(c->dc, t, lstrlenW(t), &ext);
        if (ext.cx <= bw)
            break;
    }
    SetTextColor(c->dc, RGB(255, 255, 255));
    SetBkMode(c->dc, TRANSPARENT);
    TEXTMETRICW tm;
    GetTextMetricsW(c->dc, &tm);
    int x = (int)(x0 * KS + (bw - ext.cx) / 2), y = (int)(y0 * KS + (bh - (tm.tmAscent - tm.tmInternalLeading)) / 2 - tm.tmInternalLeading);
    TextOutW(c->dc, x, y, t, lstrlenW(t));
    if (f)
        DeleteObject(f);
}

/* src over dst (premultiplied), a colour at coverage */
static void px_over(unsigned char *d, float cov, unsigned char r, unsigned char g, unsigned char b)
{
    if (cov <= 0)
        return;
    if (cov > 1)
        cov = 1;
    float k = 1 - cov;
    d[0] = (unsigned char)(r * cov + d[0] * k + 0.5f);
    d[1] = (unsigned char)(g * cov + d[1] * k + 0.5f);
    d[2] = (unsigned char)(b * cov + d[2] * k + 0.5f);
    d[3] = (unsigned char)(255 * cov + d[3] * k + 0.5f);
}

/* a key cap icon w x h: style 0 dark with a light rim (the controller icons), 1 grey (the help buttons) */
static unsigned char *key_icon(int w, int h, const WCHAR *label, int style)
{
    Canvas c = {0};
    unsigned char *out = (unsigned char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size_t)w * h * 4);
    float *fill = (float *)HeapAlloc(GetProcessHeap(), 0, sizeof(float) * w * h * 3), *rim = fill + w * h, *ink = rim + w * h;
    if (!out || !fill || !canvas_open(&c, w, h)) {
        canvas_close(&c);
        return NULL;
    }
    float s = (float)(w < h ? w : h), m = s * 0.05f, r = s * 0.2f, b = style ? 0 : s / 16.0f;
    if (b < 1.2f && !style)
        b = 1.2f;
    canvas_clear(&c);
    canvas_rrect(&c, m, m, w - m, h - m, r, 1);
    canvas_mask(&c, fill);
    canvas_clear(&c);
    if (b > 0) {
        canvas_rrect(&c, m, m, w - m, h - m, r, 1);
        canvas_rrect(&c, m + b, m + b, w - m - b, h - m - b, r - b, 0);
    }
    canvas_mask(&c, rim);
    canvas_clear(&c);
    float pad = s * 0.17f;
    /* small icons: the long names shortened */
    static const WCHAR *const kShorter[][2] = {{L"SPACE", L"SPC"}, {L"ENTER", L"ENT"}, {L"SHIFT", L"SHF"}, {L"BKSP", L"BS"},
                                              {L"CAPS", L"CPS"}, {L"HOME", L"HM"}, {L"PGUP", L"PU"}, {L"PGDN", L"PD"}};
    for (int i = 0; s < 40 && i < (int)(sizeof kShorter / sizeof kShorter[0]); i++)
        if (!lstrcmpW(label, kShorter[i][0]))
            label = kShorter[i][1];
    canvas_text(&c, label, pad, h * 0.24f, w - pad, h * 0.76f);
    canvas_mask(&c, ink);
    canvas_close(&c);
    for (int i = 0; i < w * h; i++) {
        unsigned char *d = out + i * 4;
        if (style == 0) {
            px_over(d, fill[i], 28, 28, 28);
            px_over(d, rim[i], 214, 214, 214);
            px_over(d, ink[i] * fill[i], 240, 240, 240);
        } else {
            px_over(d, fill[i], 186, 186, 186);
            px_over(d, ink[i] * fill[i], 22, 22, 22);
        }
    }
    HeapFree(GetProcessHeap(), 0, fill);
    return out;
}

/* white text into a 512 x 512 picture at a slot (centre, box, angle in degrees, positive = anticlockwise) */
static void key_slot_text(unsigned char *img, int iw, int ih, float cx, float cy, float bw, float bh, float angle, const WCHAR *t)
{
    int w = (int)bw + 4, h = (int)bh + 4;
    Canvas c = {0};
    float *ink = (float *)HeapAlloc(GetProcessHeap(), 0, sizeof(float) * w * h);
    if (!ink || !canvas_open(&c, w, h)) {
        canvas_close(&c);
        return;
    }
    canvas_clear(&c);
    canvas_text(&c, t, 2, 2, w - 2, h - 2);
    canvas_mask(&c, ink);
    canvas_close(&c);
    /* placed by inverse rotation, sampled bilinearly */
    float a = angle * 3.14159265f / 180.0f, ca = cosf(a), sa = sinf(a);
    int R = (int)(sqrtf((float)(w * w + h * h)) / 2) + 2;
    for (int y = (int)cy - R; y <= (int)cy + R; y++)
        for (int x = (int)cx - R; x <= (int)cx + R; x++) {
            if (x < 0 || y < 0 || x >= iw || y >= ih)
                continue;
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float u = dx * ca - dy * sa + w / 2.0f - 0.5f, v = dx * sa + dy * ca + h / 2.0f - 0.5f;
            int u0 = (int)floorf(u), v0 = (int)floorf(v);
            float fu = u - u0, fv = v - v0, cov = 0;
            for (int k = 0; k < 4; k++) {
                int uu = u0 + (k & 1), vv = v0 + (k >> 1);
                float wgt = ((k & 1) ? fu : 1 - fu) * ((k >> 1) ? fv : 1 - fv);
                if (uu >= 0 && vv >= 0 && uu < w && vv < h)
                    cov += ink[vv * w + uu] * wgt;
            }
            px_over(img + ((size_t)y * iw + x) * 4, cov, 255, 255, 255);
        }
    HeapFree(GetProcessHeap(), 0, ink);
}
