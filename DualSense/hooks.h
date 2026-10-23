/* hooks.h -- Extra Options, the game in memory, and the mod's hooks into it.
   The mod works on top of Forza Horizon 2 Extra Options (winmm.dll next to azimuth.exe) and not without it: Extra
   Options gives the game the mod's files (its media overrides, files.h), and it patches the game before anyone else.
   Azimuth maps the game's exe (forza_durango_release_final.exe, 24 November 2015) into its own process when Play is
   pressed, at the exe's preferred base, then loads its azimuth-tls.dll (its own, as it is: Azimuth checks its hash);
   right then Extra Options patches the game, but only if every place it patches is as on disk. Its export
   Fh2TweaksStatus() tells how far it is, so the mod hooks no game code before it says "OK". Hooks into game code are
   MinHook's (third_party\minhook, BSD 2-clause); nothing of Azimuth itself is hooked. */
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

/* ---- Extra Options ----
   Fh2TweaksStatus() (0.2.98) returns its state, a text it rewrites as it goes: "Not initialized", "Waiting for
   Azimuth to load its original TLS helper." until the game is there; then "OK: requested tweaks applied in memory."
   (or "OK: all requested tweaks already match...") once its patches are in, else "REFUSED: ...", "DISABLED: ..." or
   "INACTIVE: ..." when it made none. */
typedef const char *(*EoStatusFn)(void);
static EoStatusFn g_eoStatus;
static WCHAR g_eoIni[MAX_PATH]; /* its FH2_ExtraOptions.ini */

/* Extra Options among the process's DLLs (the winmm.dll that has its export); 1 when it is there */
static int eo_find(void)
{
    HMODULE w = GetModuleHandleW(L"winmm.dll");
    DWORD n;
    if (!w || !(g_eoStatus = (EoStatusFn)(void *)GetProcAddress(w, "Fh2TweaksStatus")))
        return 0;
    n = GetModuleFileNameW(w, g_eoIni, MAX_PATH);
    while (n && g_eoIni[n - 1] != '\\' && g_eoIni[n - 1] != '/')
        n--;
    g_eoIni[n] = 0;
    if (n && n + 24 < MAX_PATH)
        lstrcatW(g_eoIni, L"FH2_ExtraOptions.ini");
    else
        g_eoIni[0] = 0;
    return 1;
}

/* its state now (a copy: it may be rewritten while read) */
static void eo_state(char *out, int n)
{
    const char *s = g_eoStatus ? g_eoStatus() : NULL;
    int i = 0;
    for (; s && i < n - 1 && s[i] && s[i] != '\r' && s[i] != '\n'; i++)
        out[i] = s[i];
    out[i] = 0;
}

/* 1: its patches are in; -1: it made none (it refused, is switched off, or this is not FH2); 0: not yet */
static int eo_done(const char *state)
{
    if (!strncmp(state, "OK:", 3))
        return 1;
    if (!strncmp(state, "REFUSED:", 8) || !strncmp(state, "DISABLED:", 9) || !strncmp(state, "INACTIVE:", 9))
        return -1;
    return 0;
}

/* one of its ini's switches (1 when its ini is not found: its own default) */
static int eo_setting(const WCHAR *sec, const WCHAR *key)
{
    return g_eoIni[0] ? (int)GetPrivateProfileIntW(sec, key, 1, g_eoIni) : 1;
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

/* Extra Options' patches in, the game mapped, its code executable: the game's base; NULL when Extra Options made no
   patches (then the mod leaves the game alone) */
static BYTE *wait_for_game(void)
{
    char state[160];
    ULONGLONG seen;
    int done;
    for (;;) {
        eo_state(state, sizeof state);
        if ((done = eo_done(state)) != 0)
            break;
        Sleep(50);
    }
    if (done < 0) {
        ds_note("Extra Options made no patches (\"%s\") -- the mod does not touch the game: no car data, menu haptics "
                "or settings page this time", state);
        return NULL;
    }
    Sleep(250); /* after its "OK" it only puts the pages' protection back */
    seen = GetTickCount64();
    while (!find_game() && GetTickCount64() - seen < 10000)
        Sleep(50);
    if (!g_game) {
        ds_note("Extra Options is done, but the game is not where it should be -- the mod does not touch the game");
        return NULL;
    }
    seen = GetTickCount64();
    while (!game_code_executable() && GetTickCount64() - seen < 10000)
        Sleep(10);
    fh2_say("the game is at game+0 = %p; its code is ready (Extra Options: \"%s\")", (void *)g_game, state);
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
