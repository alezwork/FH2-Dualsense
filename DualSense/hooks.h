/* hooks.h -- the game in memory, and the mod's hooks into it and into Azimuth.
   Azimuth maps the game's exe (forza_durango_release_final.exe, 24 November 2015) into its own process when Play is
   pressed, at the exe's preferred base, and marks its code executable a moment later. Extra Options (winmm.dll), when
   it is there, patches the game right then, but only if every place it patches is as on disk; so the mod waits for it
   to say it is done (its log, FH2_ExtraOptions.log, written at that moment) before hooking any game code -- the mod
   neither needs nor uses it, it only keeps out of its way. Hooks into game code are MinHook's (third_party\minhook,
   BSD 2-clause); the file functions of Azimuth itself are hooked in its import table, from the start. */
#pragma once
#include "../third_party/minhook/include/MinHook.h"

#define GAME_TIMESTAMP 0x565501EDu
#define GAME_SIZE 0x03A18000u

static BYTE *g_game; /* the game's image, once mapped */

static int readable(const void *p, SIZE_T n)
{
    MEMORY_BASIC_INFORMATION m;
    if (VirtualQuery(p, &m, sizeof m) != sizeof m || m.State != MEM_COMMIT || (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
        return 0;
    return (const BYTE *)p + n <= (const BYTE *)m.BaseAddress + m.RegionSize;
}

static int is_game(const BYTE *b)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)b;
    const IMAGE_NT_HEADERS64 *nt;
    if (!readable(b, 0x1000) || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 64 || dos->e_lfanew > 0x800)
        return 0;
    nt = (const IMAGE_NT_HEADERS64 *)(b + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
           nt->FileHeader.TimeDateStamp == GAME_TIMESTAMP && nt->OptionalHeader.SizeOfImage == GAME_SIZE;
}

/* the game's image, if it is mapped (at its preferred base, else anywhere) */
static BYTE *find_game(void)
{
    SYSTEM_INFO si;
    MEMORY_BASIC_INFORMATION m;
    ULONG_PTR a, last;
    if (g_game)
        return g_game;
    if (is_game((const BYTE *)0x140000000ull))
        return g_game = (BYTE *)0x140000000ull;
    GetSystemInfo(&si);
    a = (ULONG_PTR)si.lpMinimumApplicationAddress, last = (ULONG_PTR)si.lpMaximumApplicationAddress;
    while (a < last && VirtualQuery((void *)a, &m, sizeof m) == sizeof m) {
        ULONG_PTR next = (ULONG_PTR)m.BaseAddress + m.RegionSize;
        if (m.State == MEM_COMMIT && m.BaseAddress == m.AllocationBase && m.RegionSize >= 0x1000 &&
            !(m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && is_game((const BYTE *)m.BaseAddress))
            return g_game = (BYTE *)m.BaseAddress;
        if (next <= a)
            break;
        a = next;
    }
    return NULL;
}

/* Extra Options: 1 when it is not there or has said it is done this launch (its log rewritten since the process
   started, with its status on the second line), else 0 */
static int extra_options_done(char *why, int whyLen)
{
    WCHAR path[MAX_PATH], sys[MAX_PATH];
    HMODULE w = GetModuleHandleW(L"winmm.dll");
    FILETIME created, x, k, u, written;
    char text[4096];
    DWORD got = 0, n;
    HANDLE f;
    if (!w || !(n = GetModuleFileNameW(w, path, MAX_PATH)) || n >= MAX_PATH)
        return lstrcpynA(why, "no Extra Options", whyLen), 1;
    GetSystemDirectoryW(sys, MAX_PATH);
    while (n && path[n - 1] != '\\')
        n--;
    path[n ? n - 1 : 0] = 0;
    if (!lstrcmpiW(path, sys))
        return lstrcpynA(why, "no Extra Options", whyLen), 1;
    lstrcatW(path, L"\\FH2_ExtraOptions.log");
    f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    GetProcessTimes(GetCurrentProcess(), &created, &x, &k, &u);
    text[0] = 0;
    if (GetFileTime(f, NULL, NULL, &written) && CompareFileTime(&written, &created) >= 0 &&
        ReadFile(f, text, sizeof text - 1, &got, NULL))
        text[got] = 0;
    CloseHandle(f);
    if (strstr(text, "\nAZIMUTH: winmm proxy initialized"))
        return lstrcpynA(why, "Extra Options has made its patches", whyLen), 1;
    char *nl = strchr(text, '\n');
    if (nl && (!strncmp(nl + 1, "OK:", 3)))
        return lstrcpynA(why, "Extra Options has made its patches", whyLen), 1;
    if (nl && (!strncmp(nl + 1, "REFUSED:", 8) || !strncmp(nl + 1, "DISABLED:", 9)))
        return lstrcpynA(why, "Extra Options made no patches", whyLen), 1;
    return 0;
}

/* every section of the game that is code on disk is executable here */
static int game_code_executable(void)
{
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(g_game + ((IMAGE_DOS_HEADER *)g_game)->e_lfanew);
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, s++) {
        MEMORY_BASIC_INFORMATION m;
        if (!(s->Characteristics & IMAGE_SCN_MEM_EXECUTE))
            continue;
        if (VirtualQuery(g_game + s->VirtualAddress, &m, sizeof m) != sizeof m ||
            !(m.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            return 0;
    }
    return 1;
}

/* the game mapped, Extra Options done (or not there), its code executable: the game's base */
static BYTE *wait_for_game(void)
{
    char why[96] = "";
    ULONGLONG seen;
    while (!find_game())
        Sleep(50);
    seen = GetTickCount64();
    while (!extra_options_done(why, sizeof why)) {
        if (GetTickCount64() - seen > 20000) {
            lstrcpyA(why, "Extra Options did not say it was done within 20 s -- going ahead");
            break;
        }
        Sleep(50);
    }
    seen = GetTickCount64();
    while (!game_code_executable() && GetTickCount64() - seen < 10000)
        Sleep(10);
    fh2_say("the game is at game+0 = %p; its code is ready (%s)", (void *)g_game, why);
    return g_game;
}

/* ---- MinHook ---- */
static int mh_ready(void)
{
    static int state; /* 0 not yet, 1 ok, -1 failed */
    if (!state)
        state = MH_Initialize() == MH_OK ? 1 : -1;
    return state > 0;
}

/* a function hooked: detour instead of it, *original to go on into it; 1 on success */
static int mh_hook(const char *name, void *target, void *detour, void **original)
{
    MH_STATUS st;
    if (!mh_ready())
        return 0;
    st = MH_CreateHook(target, detour, original);
    if (st == MH_OK)
        st = MH_EnableHook(target);
    if (st != MH_OK) {
        fh2_say("hook %s at game+0x%lX: MinHook says %s", name, (DWORD)((BYTE *)target - g_game), MH_StatusToString(st));
        MH_RemoveHook(target);
        return 0;
    }
    return 1;
}

/* an import of a module (Azimuth's own) pointed at ours: the function it called before (another mod's hook, or the
   system's), NULL when the module does not import it */
static void *iat_hook(HMODULE mod, const char *dll, const char *func, void *detour)
{
    BYTE *b = (BYTE *)mod;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress)
        return NULL;
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(b + dir.VirtualAddress); d->Name; d++) {
        if (lstrcmpiA((const char *)(b + d->Name), dll))
            continue;
        IMAGE_THUNK_DATA64 *names = (IMAGE_THUNK_DATA64 *)(b + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        IMAGE_THUNK_DATA64 *slots = (IMAGE_THUNK_DATA64 *)(b + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++) {
            DWORD old;
            void *prev;
            if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal) ||
                strcmp((const char *)((IMAGE_IMPORT_BY_NAME *)(b + names->u1.AddressOfData))->Name, func))
                continue;
            if (!VirtualProtect(&slots->u1.Function, 8, PAGE_READWRITE, &old))
                return NULL;
            prev = (void *)(ULONG_PTR)slots->u1.Function;
            slots->u1.Function = (ULONGLONG)(ULONG_PTR)detour;
            VirtualProtect(&slots->u1.Function, 8, old, &old);
            return prev;
        }
    }
    return NULL;
}

/* executable memory within 2 GB of `around` (for a 5-byte jump from there) */
static BYTE *alloc_near(const BYTE *around, SIZE_T size)
{
    SYSTEM_INFO si;
    ULONG_PTR gran, lo, hi;
    GetSystemInfo(&si);
    gran = si.dwAllocationGranularity;
    lo = (ULONG_PTR)around > 0x70000000 ? (ULONG_PTR)around - 0x70000000 : gran;
    hi = (ULONG_PTR)around + 0x70000000;
    for (ULONG_PTR a = ((ULONG_PTR)around & ~(gran - 1)) - gran; a > lo; a -= gran) {
        BYTE *p = (BYTE *)VirtualAlloc((void *)a, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (p)
            return p;
    }
    for (ULONG_PTR a = ((ULONG_PTR)around & ~(gran - 1)) + gran; a < hi; a += gran) {
        BYTE *p = (BYTE *)VirtualAlloc((void *)a, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (p)
            return p;
    }
    return NULL;
}

/* bytes written over game code (which has to hold `expect` first) */
static int code_patch(BYTE *at, const BYTE *expect, const BYTE *bytes, SIZE_T n)
{
    DWORD old;
    if (memcmp(at, expect, n) || !VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old))
        return 0;
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, n);
    return 1;
}
