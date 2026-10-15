/* fh2mod.h -- what the Forza Horizon 2 mods on Azimuth share (one header, static functions, no C runtime needs).

   A mod is a DLL in Azimuth\mods\, loaded by our azimuth-tls.dll once the engine has mapped the game and before any
   game code runs (Source\loader\modloader.c). The mods here hook the engine, not the game:

   - XInput. The engine (azimuth::input) gives the game Windows.Xbox.Input over desktop XInput: one function finds
     xinput1_4.dll once and keeps {module, XInputGetState, XInputSetState} in a table; every poll and every rumble
     goes through that table. fh2_xinput_table() finds it by the code that fills it (the string "XInputGetState",
     the lea of it, the store after GetProcAddress) and calls that function, so the table is filled before we put
     our functions in. Each mod chains: it keeps the pointer it replaced and calls it. Works on 0.9.5-0.9.7.
   - Gamepad::SetVibration(this, GamepadVibration*): the game's four levels (left/right motor, left/right impulse
     trigger). The engine passes the motors to XInputSetState and drops the triggers. fh2_hook_set_vibration()
     finds it by its loads of the four floats and swaps its vtable slot.

   Every mod logs to mods\<name>.log and reads mods\<name>.ini. */
#pragma once
#include <windows.h>
#include <xinput.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef XINPUT_GAMEPAD_GUIDE
#define XINPUT_GAMEPAD_GUIDE 0x0400
#endif

/* ---- the mod's files: mods\<name>.log, mods\<name>.ini ---- */
static WCHAR g_modDir[MAX_PATH]; /* <engine>\mods\ */
static WCHAR g_modIni[MAX_PATH];
static HANDLE g_modLog;
static CRITICAL_SECTION g_modLogLock;

static void fh2_say(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    int k;
    DWORD n;
    if (!g_modLog)
        return;
    va_start(ap, fmt);
    k = _vsnprintf(line, sizeof line - 3, fmt, ap);
    va_end(ap);
    n = k < 0 ? (DWORD)sizeof line - 3 : (DWORD)k;
    line[n++] = '\r';
    line[n++] = '\n';
    EnterCriticalSection(&g_modLogLock);
    WriteFile(g_modLog, line, n, &n, NULL);
    LeaveCriticalSection(&g_modLogLock);
}

/* wide text into the log as UTF-8 */
static void fh2_say_w(const char *prefix, const WCHAR *w)
{
    char u[MAX_PATH * 3];
    if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof u, NULL, NULL))
        u[0] = 0;
    fh2_say("%s%s", prefix, u);
}

static void fh2_mod_open(const WCHAR *name)
{
    WCHAR log[MAX_PATH];
    SYSTEMTIME t;
    DWORD n = GetModuleFileNameW(NULL, g_modDir, MAX_PATH);
    while (n && g_modDir[n - 1] != '\\' && g_modDir[n - 1] != '/')
        n--;
    g_modDir[n] = 0;
    lstrcatW(g_modDir, L"mods\\");
    lstrcpyW(g_modIni, g_modDir);
    lstrcatW(g_modIni, name);
    lstrcatW(g_modIni, L".ini");
    lstrcpyW(log, g_modDir);
    lstrcatW(log, name);
    lstrcatW(log, L".log");
    InitializeCriticalSection(&g_modLogLock);
    g_modLog = CreateFileW(log, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_modLog == INVALID_HANDLE_VALUE)
        g_modLog = NULL;
    GetLocalTime(&t);
    {
        char u[64];
        WideCharToMultiByte(CP_UTF8, 0, name, -1, u, sizeof u, NULL, NULL);
        fh2_say("%s: %04d-%02d-%02d %02d:%02d:%02d", u, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    }
}

/* the mod's ini with its defaults (text inside the DLL), when there is none yet: dropping the DLL is enough */
static void fh2_ini_default(const char *text)
{
    if (GetFileAttributesW(g_modIni) != INVALID_FILE_ATTRIBUTES)
        return;
    HANDLE h = CreateFileW(g_modIni, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD n;
    if (h == INVALID_HANDLE_VALUE)
        return;
    WriteFile(h, text, (DWORD)strlen(text), &n, NULL);
    CloseHandle(h);
    fh2_say("its settings file made with the defaults");
}

static int fh2_ini_int(const WCHAR *sec, const WCHAR *key, int def)
{
    return (int)GetPrivateProfileIntW(sec, key, def, g_modIni);
}

static void fh2_ini_str(const WCHAR *sec, const WCHAR *key, const WCHAR *def, WCHAR *out, DWORD n)
{
    GetPrivateProfileStringW(sec, key, def, out, n, g_modIni);
    /* a comment after the value: "1 ; text" */
    for (WCHAR *p = out; *p; p++)
        if (*p == ';') {
            *p = 0;
            break;
        }
    for (int k = lstrlenW(out); k && (out[k - 1] == ' ' || out[k - 1] == '\t'); k--)
        out[k - 1] = 0;
}

/* the game's folder (<it>\Mount\media\ has its files) and the language asked for, from the engine's data\azimuth.ini
   (UTF-16): Azimuth 0.9.9 keeps the game's folder in its title's section ([launcher] game=<id>, [title-<id>]
   folder=), 0.9.5..0.9.7 in [launcher] game_folder=; a relative folder is from the engine's. lang: "" for the
   system's. 0: not there. */
static int fh2_game_folder(WCHAR folder[MAX_PATH], WCHAR *lang, DWORD langLen)
{
    WCHAR dir[MAX_PATH], ini[MAX_PATH], id[32], sec[48], f[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, dir, MAX_PATH);
    while (n && dir[n - 1] != '\\' && dir[n - 1] != '/')
        n--;
    dir[n] = 0;
    if (n + 20 >= MAX_PATH)
        return 0;
    lstrcpyW(ini, dir);
    lstrcatW(ini, L"data\\azimuth.ini");
    GetPrivateProfileStringW(L"launcher", L"game", L"1a96545b", id, 32, ini); /* Forza Horizon 2's id */
    lstrcpyW(sec, L"title-");
    lstrcatW(sec, id);
    if (!GetPrivateProfileStringW(sec, L"folder", L"", f, MAX_PATH, ini) &&
        !GetPrivateProfileStringW(L"launcher", L"game_folder", L"", f, MAX_PATH, ini))
        return 0;
    if (lang)
        GetPrivateProfileStringW(L"launcher", L"language", L"", lang, langLen, ini);
    if (!(f[1] == ':' || (f[0] == '\\' && f[1] == '\\'))) {
        if (lstrlenW(dir) + lstrlenW(f) >= MAX_PATH)
            return 0;
        lstrcpyW(ini, dir);
        lstrcatW(ini, f);
        lstrcpyW(f, ini);
    }
    n = GetFullPathNameW(f, MAX_PATH, folder, NULL);
    return n && n < MAX_PATH - 20;
}

/* a number with a fraction ("3.5"); def when missing */
static float fh2_ini_float(const WCHAR *sec, const WCHAR *key, float def)
{
    WCHAR s[64];
    float v = 0, frac = 0.1f;
    int neg = 0, any = 0, dot = 0;
    fh2_ini_str(sec, key, L"", s, 64);
    for (WCHAR *p = s; *p; p++) {
        if (*p == ' ')
            continue;
        if (*p == '-' && !any)
            neg = 1;
        else if (*p == '.' || *p == ',')
            dot = 1;
        else if (*p >= '0' && *p <= '9') {
            any = 1;
            if (dot) {
                v += (*p - '0') * frac;
                frac *= 0.1f;
            } else
                v = v * 10 + (*p - '0');
        } else
            break;
    }
    return any ? (neg ? -v : v) : def;
}

/* "255, 96, 0" -> r g b */
static void fh2_ini_rgb(const WCHAR *sec, const WCHAR *key, const WCHAR *def, BYTE rgb[3])
{
    WCHAR s[64];
    int v[3] = {0, 0, 0}, k = 0, any = 0;
    fh2_ini_str(sec, key, def, s, 64);
    for (WCHAR *p = s;; p++) {
        if (*p >= '0' && *p <= '9') {
            v[k] = v[k] * 10 + (*p - '0');
            any = 1;
        } else if (any && k < 2 && (*p == ',' || *p == ' ')) {
            k++;
            any = 0;
            while (p[1] == ' ' || p[1] == ',')
                p++;
        } else if (!*p)
            break;
    }
    for (k = 0; k < 3; k++)
        rgb[k] = (BYTE)(v[k] > 255 ? 255 : v[k]);
}

/* ---- the engine image ---- */
typedef struct {
    BYTE *base;
    BYTE *text, *textEnd;
    BYTE *rdata, *rdataEnd;
} Fh2Engine;

static int fh2_engine(Fh2Engine *e)
{
    BYTE *b = (BYTE *)GetModuleHandleW(NULL);
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    ZeroMemory(e, sizeof *e);
    e->base = b;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, s++) {
        if (!memcmp(s->Name, ".text", 6)) {
            e->text = b + s->VirtualAddress;
            e->textEnd = e->text + s->Misc.VirtualSize;
        } else if (!memcmp(s->Name, ".rdata", 7)) {
            e->rdata = b + s->VirtualAddress;
            e->rdataEnd = e->rdata + s->Misc.VirtualSize;
        }
    }
    return e->text && e->rdata;
}

/* bytes; mask '?' = any byte */
static BYTE *fh2_find(BYTE *from, BYTE *to, const BYTE *pat, const char *mask, SIZE_T len)
{
    for (BYTE *p = from; p + len <= to; p++) {
        SIZE_T k = 0;
        while (k < len && ((mask && mask[k] == '?') || p[k] == pat[k]))
            k++;
        if (k == len)
            return p;
    }
    return NULL;
}

/* the zero-terminated ASCII string, at its start (a zero byte before it) */
static BYTE *fh2_find_string(Fh2Engine *e, const char *s)
{
    SIZE_T n = lstrlenA(s) + 1;
    for (BYTE *p = e->rdata; (p = fh2_find(p, e->rdataEnd, (const BYTE *)s, NULL, n)) != NULL; p++)
        if (p > e->rdata && p[-1] == 0)
            return p;
    return NULL;
}

/* lea r64, [rip+disp32] (48/4C 8D, mod 00, rm 101) pointing at target */
static BYTE *fh2_find_lea(Fh2Engine *e, BYTE *target)
{
    for (BYTE *p = e->text; p + 7 <= e->textEnd; p++)
        if ((p[0] == 0x48 || p[0] == 0x4C) && p[1] == 0x8D && (p[2] & 0xC7) == 0x05 &&
            p + 7 + *(INT32 *)(p + 3) == target)
            return p;
    return NULL;
}

/* the first "mov [rip+disp32], rax" (48 89 05) within n bytes after p: the address it stores to */
static void **fh2_store_after(BYTE *p, int n)
{
    for (int k = 0; k < n; k++)
        if (p[k] == 0x48 && p[k + 1] == 0x89 && p[k + 2] == 0x05)
            return (void **)(p + k + 7 + *(INT32 *)(p + k + 3));
    return NULL;
}

/* ---- XInput: the engine's table {xinput1_4 module, XInputGetState, XInputSetState} ---- */
typedef DWORD(WINAPI *Fh2GetStateFn)(DWORD user, XINPUT_STATE *state);
typedef DWORD(WINAPI *Fh2SetStateFn)(DWORD user, XINPUT_VIBRATION *vib);

static void **fh2_xinput_table(Fh2Engine *e)
{
    BYTE *s = fh2_find_string(e, "XInputGetState");
    BYTE *lea = s ? fh2_find_lea(e, s) : NULL;
    void **getSlot = lea ? fh2_store_after(lea, 40) : NULL;
    if (!getSlot) {
        fh2_say("XInput: the engine's XInput table not found (string %p, lea %p) -- not this engine", s, lea);
        return NULL;
    }
    /* the function that fills it: called once here, so the table is complete before it is changed */
    DWORD64 imageBase = 0;
    RUNTIME_FUNCTION *rf = RtlLookupFunctionEntry((DWORD64)lea, &imageBase, NULL);
    if (!rf) {
        fh2_say("XInput: no function around the table's code -- not this engine");
        return NULL;
    }
    typedef void **(*InitFn)(void);
    void **table = ((InitFn)(imageBase + rf->BeginAddress))();
    if (table + 1 != getSlot) {
        fh2_say("XInput: the table's function returned %p, the table is at %p -- not this engine", table, getSlot - 1);
        return NULL;
    }
    fh2_say("XInput: the engine's table at engine+0x%X: xinput1_4 %p, XInputGetState %p, XInputSetState %p",
            (DWORD)((BYTE *)table - e->base), table[0], table[1], table[2]);
    return table;
}

/* ---- Gamepad::SetVibration(this, const float levels[4]) ---- */
typedef HRESULT(__fastcall *Fh2SetVibrationFn)(void *self, const float *levels);

static Fh2SetVibrationFn fh2_hook_set_vibration(Fh2Engine *e, Fh2SetVibrationFn hook)
{
    /* movss xmm9,[rdx] ; movss [rsp+28h],xmm9 ; ... [rdx+4] -> [rsp+2Ch] ; [rdx+8] -> [rsp+30h] ; [rdx+0Ch] -> [rsp+34h] */
    static const BYTE pat[] = {0xF3, 0x44, 0x0F, 0x10, 0x0A, 0xF3, 0x44, 0x0F, 0x11, 0x4C, 0x24, 0x28,
                               0xF3, 0x44, 0x0F, 0x10, 0x52, 0x04, 0xF3, 0x44, 0x0F, 0x11, 0x54, 0x24, 0x2C,
                               0xF3, 0x44, 0x0F, 0x10, 0x5A, 0x08, 0xF3, 0x44, 0x0F, 0x11, 0x5C, 0x24, 0x30,
                               0xF3, 0x44, 0x0F, 0x10, 0x62, 0x0C, 0xF3, 0x44, 0x0F, 0x11, 0x64, 0x24, 0x34};
    BYTE *p = fh2_find(e->text, e->textEnd, pat, NULL, sizeof pat);
    if (!p || fh2_find(p + 1, e->textEnd, pat, NULL, sizeof pat)) {
        fh2_say("SetVibration: %s -- the impulse triggers stay off", p ? "found twice" : "not found");
        return NULL;
    }
    DWORD64 imageBase = 0;
    RUNTIME_FUNCTION *rf = RtlLookupFunctionEntry((DWORD64)p, &imageBase, NULL);
    if (!rf)
        return NULL;
    BYTE *fn = (BYTE *)(imageBase + rf->BeginAddress);
    BYTE **slot = NULL;
    int slots = 0;
    for (BYTE **q = (BYTE **)(((ULONG_PTR)e->rdata + 7) & ~(ULONG_PTR)7); (BYTE *)(q + 1) <= e->rdataEnd; q++)
        if (*q == fn) {
            slot = q;
            slots++;
        }
    if (slots != 1) {
        fh2_say("SetVibration: engine+0x%X, %d vtable slots point at it -- the impulse triggers stay off",
                (DWORD)(fn - e->base), slots);
        return NULL;
    }
    DWORD old;
    if (!VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old))
        return NULL;
    Fh2SetVibrationFn orig = (Fh2SetVibrationFn)InterlockedExchangePointer((void **)slot, (void *)hook);
    VirtualProtect(slot, sizeof *slot, old, &old);
    fh2_say("SetVibration: engine+0x%X (vtable slot engine+0x%X) hooked: the game's impulse trigger levels come here",
            (DWORD)(fn - e->base), (DWORD)((BYTE *)slot - e->base));
    return orig;
}

/* ---- an import of the engine (its IAT slot): our function in, the one that was there back (chains with other
   mods and fh2core, which hook the same imports) ---- */
static void *fh2_hook_import(Fh2Engine *e, const char *dll, const char *fn, void *hook)
{
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(e->base + ((IMAGE_DOS_HEADER *)e->base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress)
        return NULL;
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(e->base + dir.VirtualAddress); d->Name; d++) {
        if (lstrcmpiA((const char *)(e->base + d->Name), dll))
            continue;
        IMAGE_THUNK_DATA64 *names = (IMAGE_THUNK_DATA64 *)(e->base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        IMAGE_THUNK_DATA64 *slots = (IMAGE_THUNK_DATA64 *)(e->base + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++) {
            if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))
                continue;
            IMAGE_IMPORT_BY_NAME *ibn = (IMAGE_IMPORT_BY_NAME *)(e->base + names->u1.AddressOfData);
            if (lstrcmpA((const char *)ibn->Name, fn))
                continue;
            DWORD old;
            if (!VirtualProtect(&slots->u1.Function, 8, PAGE_READWRITE, &old))
                return NULL;
            void *prev = InterlockedExchangePointer((void **)&slots->u1.Function, hook);
            VirtualProtect(&slots->u1.Function, 8, old, &old);
            return prev;
        }
    }
    return NULL;
}

/* ---- the game window has the keyboard ---- */
static int fh2_foreground(void)
{
    DWORD pid = 0;
    HWND w = GetForegroundWindow();
    if (!w)
        return 0;
    GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

static double fh2_now(void)
{
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart)
        QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}

static SHORT fh2_axis(float v) /* -1..1 -> XInput stick */
{
    if (v > 1)
        v = 1;
    if (v < -1)
        v = -1;
    return (SHORT)(v >= 0 ? v * 32767.0f : v * 32768.0f);
}

/* two readings into one: buttons or'ed, sticks and triggers the larger deflection */
static void fh2_merge(XINPUT_GAMEPAD *into, const XINPUT_GAMEPAD *g)
{
    into->wButtons |= g->wButtons;
    if (g->bLeftTrigger > into->bLeftTrigger)
        into->bLeftTrigger = g->bLeftTrigger;
    if (g->bRightTrigger > into->bRightTrigger)
        into->bRightTrigger = g->bRightTrigger;
#define FH2_AXIS(a)                                                                                                    \
    if ((g->a < 0 ? -(int)g->a : g->a) > (into->a < 0 ? -(int)into->a : into->a))                                      \
        into->a = g->a;
    FH2_AXIS(sThumbLX)
    FH2_AXIS(sThumbLY)
    FH2_AXIS(sThumbRX)
    FH2_AXIS(sThumbRY)
#undef FH2_AXIS
}

/* ---- the real XInput pads behind the replaced table entries ----
   The engine uses one gamepad: the first XInput user that answers. A mod shows the game one pad, user 0, made of
   every real pad (users 0-3) and its own device; users 1-3 then answer "not connected". Chained mods stay right:
   the outer one asks the inner one, which already hides users 1-3. Absent users are asked again once a second. */
typedef struct {
    Fh2GetStateFn get;
    Fh2SetStateFn set;
    double retry[4];
    LONG present[4];
} Fh2Pads;

/* every present pad merged into *out; returns how many answered; *packet = the sum of their packet numbers */
static int fh2_pads_get(Fh2Pads *p, XINPUT_GAMEPAD *out, DWORD *packet)
{
    int n = 0;
    double now = 0;
    ZeroMemory(out, sizeof *out);
    *packet = 0;
    if (!p->get)
        return 0;
    for (DWORD u = 0; u < 4; u++) {
        XINPUT_STATE s;
        if (!p->present[u]) {
            if (!now)
                now = fh2_now();
            if (now < p->retry[u])
                continue;
        }
        ZeroMemory(&s, sizeof s);
        if (p->get(u, &s) == ERROR_SUCCESS) {
            p->present[u] = 1;
            fh2_merge(out, &s.Gamepad);
            *packet += s.dwPacketNumber;
            n++;
        } else {
            p->present[u] = 0;
            p->retry[u] = (now ? now : fh2_now()) + 1.0;
        }
    }
    return n;
}

static void fh2_pads_set(Fh2Pads *p, XINPUT_VIBRATION *v)
{
    if (!p->set)
        return;
    for (DWORD u = 0; u < 4; u++)
        if (p->present[u])
            p->set(u, v);
}
