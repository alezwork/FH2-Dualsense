/* azimuth-tls.dll -- the mod loader for Forza Horizon 2 on Azimuth.

   Azimuth loads data\bin\azimuth-tls.dll (its "native static-TLS helper") once it has mapped the game and put its
   own patches in, before any game code runs (0.9.5: 0x8F640). It wants of that DLL only:
     - a static-TLS block the game's TLS data fits in (the game's template: 0x150 zero bytes) -- Windows gives every
       thread one when the DLL loads, and the engine hands its index to the game;
     - the exports azimuth_tls_index (that index, filled in by Windows) and azimuth_tls_attached (the thread that
       loaded it: the engine checks it is its own).
   This DLL is that, and then loads every mods\*.dll next to azimuth.exe, in name order: the moment they load is the
   right one to patch the game. A name starting with "_" is skipped (a mod switched off). Its log: mods\loader.log.
   No C runtime: built /NODEFAULTLIB, entry DllMain. */
#include <windows.h>

__declspec(dllexport) DWORD azimuth_tls_index = TLS_OUT_OF_INDEXES;
__declspec(dllexport) DWORD azimuth_tls_attached;

/* the static-TLS template (zeros, as the engine's own helper has it), and the directory the linker looks for */
#pragma section(".tls", read, write)
__declspec(allocate(".tls")) char g_tlsTemplate[0x150] = {0};
static PIMAGE_TLS_CALLBACK g_tlsCallbacks[1] = {0};
#pragma section(".rdata$T", read)
__declspec(allocate(".rdata$T")) const IMAGE_TLS_DIRECTORY64 _tls_used = {
    (ULONGLONG)(ULONG_PTR)g_tlsTemplate, (ULONGLONG)(ULONG_PTR)(g_tlsTemplate + sizeof g_tlsTemplate),
    (ULONGLONG)(ULONG_PTR)&azimuth_tls_index, (ULONGLONG)(ULONG_PTR)g_tlsCallbacks, 0, 0x00500000 /* align 16 */};

extern IMAGE_DOS_HEADER __ImageBase; /* this DLL */
extern IMAGE_DOS_HEADER __ImageBase; /* this DLL */
static HANDLE g_log;

static void say(const WCHAR *a, const WCHAR *b, const WCHAR *c)
{
    char line[1024];
    WCHAR w[1024];
    DWORD n;
    if (!g_log)
        return;
    w[0] = 0;
    lstrcpynW(w, a, 1024);
    if (b && lstrlenW(w) + lstrlenW(b) < 1000)
        lstrcatW(w, b);
    if (c && lstrlenW(w) + lstrlenW(c) < 1000)
        lstrcatW(w, c);
    lstrcatW(w, L"\r\n");
    n = WideCharToMultiByte(CP_UTF8, 0, w, -1, line, sizeof line, NULL, NULL);
    if (n)
        WriteFile(g_log, line, n - 1, &n, NULL);
}

static void number(WCHAR *out, DWORD v)
{
    WCHAR t[16];
    int i = 0, j = 0;
    do {
        t[i++] = (WCHAR)('0' + v % 10);
        v /= 10;
    } while (v);
    while (i)
        out[j++] = t[--i];
    out[j] = 0;
}

static void load_mods(void)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH], self[MAX_PATH];
    WIN32_FIND_DATAW fd;
    DWORD n = GetModuleFileNameW(NULL, dir, MAX_PATH);
    while (n && dir[n - 1] != '\\' && dir[n - 1] != '/')
        n--;
    dir[n] = 0;
    if (n + 48 >= MAX_PATH)
        return;
    lstrcatW(dir, L"mods\\"); /* <the engine's folder>\mods\ */
    lstrcpyW(path, dir);
    lstrcatW(path, L"loader.log");
    g_log = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_log == INVALID_HANDLE_VALUE)
        g_log = NULL;
    say(L"FH2 mod loader 1.0 (azimuth-tls.dll): the game is mapped, loading ", dir, L"*.dll");
    GetModuleFileNameW((HMODULE)&__ImageBase, self, MAX_PATH);
    lstrcpyW(path, dir);
    lstrcatW(path, L"*.dll");
    HANDLE f = FindFirstFileW(path, &fd);
    if (f == INVALID_HANDLE_VALUE) {
        say(L"no mods", NULL, NULL);
        return;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        if (fd.cFileName[0] == '_') {
            say(L"skipped (switched off): ", fd.cFileName, NULL);
            continue;
        }
        if (lstrlenW(dir) + lstrlenW(fd.cFileName) >= MAX_PATH)
            continue;
        lstrcpyW(path, dir);
        lstrcatW(path, fd.cFileName);
        if (!lstrcmpiW(path, self))
            continue;
        HMODULE m = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!m) {
            WCHAR code[16];
            number(code, GetLastError());
            say(L"FAILED to load ", fd.cFileName, NULL);
            say(L"  error ", code, NULL);
            continue;
        }
        say(L"loaded ", fd.cFileName, NULL);
        /* the ExtraOptions mod says how it went */
        typedef const char *(*StatusFn)(void);
        StatusFn status = (StatusFn)GetProcAddress(m, "Fh2TweaksStatus");
        if (status) {
            WCHAR w[512];
            if (MultiByteToWideChar(CP_UTF8, 0, status(), -1, w, 512))
                say(L"  ExtraOptions: ", w, NULL);
        }
    } while (FindNextFileW(f, &fd));
    FindClose(f);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)inst;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        azimuth_tls_attached = GetCurrentThreadId();
        load_mods();
        if (g_log)
            CloseHandle(g_log);
        g_log = NULL;
    }
    return TRUE;
}
