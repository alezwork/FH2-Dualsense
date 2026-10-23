/* base.h -- what the mod's parts share: its folder next to azimuth.exe (DualSense\: DualSense.ini, DualSense.log,
   mediaoverride.txt -- the list of the game's files it has written --, an icon pack of your own), the log, the ini,
   the clock, the game's image. */
#pragma once
#include <windows.h>
#include <xinput.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define DS_VERSION "3.0.0"

extern IMAGE_DOS_HEADER __ImageBase; /* this DLL */
static WCHAR g_azDir[MAX_PATH]; /* azimuth.exe's folder, with its \ */
static WCHAR g_dir[MAX_PATH];   /* <Azimuth>\DualSense\ */
static WCHAR g_ini[MAX_PATH];
static HANDLE g_log;
static CRITICAL_SECTION g_logLock;

static void ds_line(const char *line)
{
    char buf[1100];
    DWORD n;
    if (g_log) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        int k = _snprintf(buf, sizeof buf - 3, "%02d:%02d:%02d.%03d %s", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, line);
        n = k < 0 ? (DWORD)sizeof buf - 3 : (DWORD)k;
        buf[n++] = '\r', buf[n++] = '\n';
        EnterCriticalSection(&g_logLock);
        WriteFile(g_log, buf, n, &n, NULL);
        LeaveCriticalSection(&g_logLock);
    }
}

/* a line in DualSense.log */
static void fh2_say(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    if (_vsnprintf(line, sizeof line - 1, fmt, ap) < 0)
        line[sizeof line - 1] = 0;
    va_end(ap);
    ds_line(line);
}

/* wide text into DualSense.log as UTF-8 */
static void fh2_say_w(const char *prefix, const WCHAR *w)
{
    char u[MAX_PATH * 3];
    if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof u, NULL, NULL))
        u[0] = 0;
    fh2_say("%s%s", prefix, u);
}

/* a line in DualSense.log that matters (what is on, what is not) */
static void ds_note(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    if (_vsnprintf(line, sizeof line - 1, fmt, ap) < 0)
        line[sizeof line - 1] = 0;
    va_end(ap);
    ds_line(line);
}

/* the folder (made when missing), the log (new each start), the ini with its defaults when there is none */
static void ds_files_open(const char *defaultIni)
{
    SYSTEMTIME t;
    WCHAR log[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, g_azDir, MAX_PATH);
    while (n && g_azDir[n - 1] != '\\' && g_azDir[n - 1] != '/')
        n--;
    g_azDir[n] = 0;
    if (!n || n + 40 >= MAX_PATH)
        lstrcpyW(g_azDir, L".\\");
    lstrcpyW(g_dir, g_azDir);
    lstrcatW(g_dir, L"DualSense\\");
    CreateDirectoryW(g_dir, NULL);
    lstrcpyW(g_ini, g_dir);
    lstrcatW(g_ini, L"DualSense.ini");
    lstrcpyW(log, g_dir);
    lstrcatW(log, L"DualSense.log");
    InitializeCriticalSection(&g_logLock);
    g_log = CreateFileW(log, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_log == INVALID_HANDLE_VALUE)
        g_log = NULL;
    GetLocalTime(&t);
    fh2_say("DualSense %s: %04d-%02d-%02d", DS_VERSION, t.wYear, t.wMonth, t.wDay);
    fh2_say_w("Azimuth: ", g_azDir);
    if (GetFileAttributesW(g_ini) == INVALID_FILE_ATTRIBUTES) {
        HANDLE h = CreateFileW(g_ini, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        DWORD w;
        if (h != INVALID_HANDLE_VALUE) {
            WriteFile(h, defaultIni, (DWORD)strlen(defaultIni), &w, NULL);
            CloseHandle(h);
            fh2_say("DualSense.ini made with the defaults");
        }
    }
}

static int fh2_ini_int(const WCHAR *sec, const WCHAR *key, int def) { return (int)GetPrivateProfileIntW(sec, key, def, g_ini); }

static void fh2_ini_str(const WCHAR *sec, const WCHAR *key, const WCHAR *def, WCHAR *out, DWORD n)
{
    GetPrivateProfileStringW(sec, key, def, out, n, g_ini);
    /* a comment after the value: "1 ; text" */
    for (WCHAR *p = out; *p; p++)
        if (*p == ';') {
            *p = 0;
            break;
        }
    for (int k = lstrlenW(out); k && (out[k - 1] == ' ' || out[k - 1] == '\t'); k--)
        out[k - 1] = 0;
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

/* a value back into the ini (the Mods page's changes), keeping the comment after it on the line */
static void fh2_ini_set(const WCHAR *sec, const WCHAR *key, const WCHAR *value)
{
    WritePrivateProfileStringW(sec, key, value, g_ini);
}

static void fh2_ini_set_int(const WCHAR *sec, const WCHAR *key, int value)
{
    WCHAR v[16];
    wsprintfW(v, L"%d", value);
    fh2_ini_set(sec, key, v);
}

/* the game's window has the keyboard (the engine's window is the game's) */
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

static SHORT fh2_axis(float v) /* -1..1 -> an XInput stick */
{
    if (v > 1)
        v = 1;
    if (v < -1)
        v = -1;
    return (SHORT)(v >= 0 ? v * 32767.0f : v * 32768.0f);
}

/* ---- a mapped image: the game's (hooks.h) or the engine's (azimuth.exe) ---- */
typedef struct {
    BYTE *base, *text, *textEnd, *rdata, *rdataEnd, *data, *dataEnd;
} Image;

static int image_of(BYTE *b, Image *m)
{
    ZeroMemory(m, sizeof *m);
    if (!b || ((IMAGE_DOS_HEADER *)b)->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return 0;
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    m->base = b;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, s++) {
        if (!memcmp(s->Name, ".text", 6))
            m->text = b + s->VirtualAddress, m->textEnd = m->text + s->Misc.VirtualSize;
        else if (!memcmp(s->Name, ".rdata", 7))
            m->rdata = b + s->VirtualAddress, m->rdataEnd = m->rdata + s->Misc.VirtualSize;
        else if (!memcmp(s->Name, ".data", 6))
            m->data = b + s->VirtualAddress, m->dataEnd = m->data + s->Misc.VirtualSize;
    }
    return m->text && m->rdata && m->data;
}

static BYTE *rel32(BYTE *insn, int dispAt, int len) { return insn + len + *(INT32 *)(insn + dispAt); }

/* the start of the image's function that holds p (its exception directory), or NULL */
static BYTE *func_start(BYTE *base, const BYTE *p)
{
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY d = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    RUNTIME_FUNCTION *f = (RUNTIME_FUNCTION *)(base + d.VirtualAddress);
    DWORD rva = (DWORD)(p - base), lo = 0, hi = d.Size / sizeof *f;
    while (lo < hi) {
        DWORD mid = (lo + hi) / 2;
        if (f[mid].EndAddress <= rva)
            lo = mid + 1;
        else if (f[mid].BeginAddress > rva)
            hi = mid;
        else
            return base + f[mid].BeginAddress;
    }
    return NULL;
}

static int in_range(const void *p, const BYTE *lo, const BYTE *hi) { return (const BYTE *)p >= lo && (const BYTE *)p < hi; }

static int hex_digit(char c) { return c >= '0' && c <= '9' ? c - '0' : (c | 32) >= 'a' && (c | 32) <= 'f' ? (c | 32) - 'a' + 10 : -1; }

/* the one place in [lo, hi) with these bytes ("48 8B 0D ?? ?? ?? ?? E8": ?? = any byte; the first one fixed): NULL when
   there is none or more than one (*count: how many) */
static BYTE *find_unique(BYTE *lo, BYTE *hi, const char *pattern, int *count)
{
    BYTE v[128], m[128];
    int n = 0, c = 0;
    BYTE *found = NULL;
    for (const char *p = pattern; *p && n < 128;) {
        if (*p == ' ') {
            p++;
        } else if (*p == '?') {
            v[n] = 0, m[n++] = 0;
            p += p[1] == '?' ? 2 : 1;
        } else if (hex_digit(p[0]) >= 0 && hex_digit(p[1]) >= 0) {
            v[n] = (BYTE)(hex_digit(p[0]) << 4 | hex_digit(p[1])), m[n++] = 1;
            p += 2;
        } else
            return NULL;
    }
    if (count)
        *count = 0;
    if (!n || !m[0] || !lo || hi < lo + n)
        return NULL;
    for (BYTE *q = lo; q + n <= hi; q++) {
        if (*q != v[0])
            continue;
        int i = 1;
        while (i < n && (!m[i] || q[i] == v[i]))
            i++;
        if (i == n && !c++)
            found = q;
    }
    if (count)
        *count = c;
    return c == 1 ? found : NULL;
}
