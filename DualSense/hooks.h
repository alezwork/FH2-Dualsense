/* hooks.h -- the game in memory, Extra Options (when it is there), and the mod's hooks into the game and Azimuth.
   Azimuth maps the game's exe (forza_durango_release_final.exe) into its own process when Play is pressed, then
   marks its code executable. The mod knows it by its export directory's name ("forza_durango..."), not by its build
   date or size: any build of it is taken, and every place the mod hooks is found by its code (car.h, sounds.h,
   menu.h) and checked before use -- what is not as expected is logged and left out.
   Forza Horizon 2 Extra Options (winmm.dll next to azimuth.exe) is not needed. When it is there, it patches the game
   right as it is mapped, but only if every place it patches is as on disk; its export Fh2TweaksStatus() tells how far
   it is, so then the mod hooks no game code before it says it is done. Without it (or when it made no patches) the
   mod hooks the game as soon as its code is executable.
   Hooks into game code are MinHook's (third_party\minhook, BSD 2-clause). Of Azimuth only its import table entries
   of four file functions go through the mod (files.h); its code is not changed. */
#pragma once
#include "../third_party/minhook/include/MinHook.h"

static BYTE *g_game; /* the game's image, once mapped */

static int readable(const void *p, SIZE_T n)
{
    MEMORY_BASIC_INFORMATION m;
    if (VirtualQuery(p, &m, sizeof m) != sizeof m || m.State != MEM_COMMIT || (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
        return 0;
    return (const BYTE *)p + n <= (const BYTE *)m.BaseAddress + m.RegionSize;
}

/* FH2's own, not another Forza's on the same engine (Horizon 3's exe has the same name): its online services' name,
   "Services.FH2Release", in its read-only data. 1 yes, 0 no, -1 not readable yet (still being mapped) */
static int fh2_marker(const BYTE *b, const IMAGE_NT_HEADERS64 *nt)
{
    static const char kMark[] = "Services.FH2Release";
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, s++) {
        const BYTE *p = b + s->VirtualAddress, *end = p + s->Misc.VirtualSize;
        if (memcmp(s->Name, ".rdata", 7))
            continue;
        if (!readable(p, s->Misc.VirtualSize))
            return -1;
        for (end -= sizeof kMark - 1; p < end; p++)
            if (*p == 'S' && !memcmp(p, kMark, sizeof kMark - 1))
                return 1;
    }
    return 0;
}

/* the game's image: a 64-bit image whose export directory is named "forza_durango...", with FH2's marker */
static int is_game(const BYTE *b)
{
    static const BYTE *other[8]; /* images that are not FH2 (searched once) */
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)b;
    const IMAGE_NT_HEADERS64 *nt;
    IMAGE_DATA_DIRECTORY ed;
    DWORD name;
    int fh2;
    if (!readable(b, 0x1000) || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 64 || dos->e_lfanew > 0x800)
        return 0;
    nt = (const IMAGE_NT_HEADERS64 *)(b + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt->OptionalHeader.SizeOfImage < 0x1000000)
        return 0;
    ed = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!ed.VirtualAddress || ed.VirtualAddress >= nt->OptionalHeader.SizeOfImage || !readable(b + ed.VirtualAddress, 40))
        return 0;
    name = ((const IMAGE_EXPORT_DIRECTORY *)(b + ed.VirtualAddress))->Name;
    if (!name || name >= nt->OptionalHeader.SizeOfImage || !readable(b + name, 14) ||
        _strnicmp((const char *)(b + name), "forza_durango", 13))
        return 0;
    for (int i = 0; i < 8; i++)
        if (other[i] == b)
            return 0;
    if ((fh2 = fh2_marker(b, nt)) == 0) {
        for (int i = 0; i < 8; i++)
            if (!other[i]) {
                other[i] = b;
                break;
            }
        ds_note("the game in memory is another Forza, not Horizon 2 -- the mod does not hook it (the controller and the "
                "keyboard still work as a gamepad)");
    }
    return fh2 > 0;
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

/* the game's build, for the log */
static DWORD game_stamp(void)
{
    return ((IMAGE_NT_HEADERS64 *)(g_game + ((IMAGE_DOS_HEADER *)g_game)->e_lfanew))->FileHeader.TimeDateStamp;
}

/* ---- Extra Options (optional) ----
   Fh2TweaksStatus() returns its state, a text it rewrites as it goes: "Not initialized", "Waiting for Azimuth..."
   until the game is there; then "OK: ..." once its patches are in, else "REFUSED: ...", "DISABLED: ..." or
   "INACTIVE: ..." when it made none. */
typedef const char *(*EoStatusFn)(void);
static EoStatusFn g_eoStatus;

/* Extra Options among the process's DLLs (the winmm.dll that has its export); 1 when it is there */
static int eo_find(void)
{
    HMODULE w = GetModuleHandleW(L"winmm.dll");
    return w && (g_eoStatus = (EoStatusFn)(void *)GetProcAddress(w, "Fh2TweaksStatus")) != NULL;
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
    if (!strncmp(state, "OK", 2))
        return 1;
    if (!strncmp(state, "REFUSED", 7) || !strncmp(state, "DISABLED", 8) || !strncmp(state, "INACTIVE", 8))
        return -1;
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

/* the game mapped, its code executable, Extra Options (when it is there) done with it: the game's base. Waits as long
   as Azimuth's launcher is open (Play not pressed yet). */
static BYTE *wait_for_game(void)
{
    char state[160] = "";
    ULONGLONG seen;
    int eo = g_eoStatus != NULL, done = 0;
    for (ULONGLONG n = 0;; n++) {
        if (eo) {
            eo_state(state, sizeof state);
            if ((done = eo_done(state)) != 0)
                break;
        } else if ((n & 3) == 0 && find_game()) /* the whole address space every 200 ms at most */
            break;
        Sleep(50);
    }
    if (eo && done < 0)
        ds_note("Extra Options made no patches (\"%s\") -- the mod hooks the game on its own", state);
    if (eo && done > 0)
        Sleep(250); /* after its "OK" it only puts the pages' protection back */
    seen = GetTickCount64();
    while (!find_game() && GetTickCount64() - seen < 15000)
        Sleep(50);
    if (!g_game) {
        ds_note("the game is not in memory where it should be -- the mod does not touch the game (no car data, menu "
                "haptics or settings rows); the controller and the keyboard still work");
        return NULL;
    }
    seen = GetTickCount64();
    while (!game_code_executable() && GetTickCount64() - seen < 15000)
        Sleep(10);
    if (!eo)
        Sleep(500); /* Azimuth's own fix-ups of the image, right after the protection */
    fh2_say("the game (build %08lX) is at %p; its code is ready (Extra Options: %s%s%s)", game_stamp(), (void *)g_game,
            eo ? "\"" : "not there", eo ? state : "", eo ? "\"" : "");
    return g_game;
}

/* ---- Azimuth's imports: one entry of its import table pointed at ours; the one there before (the system's, or
   another mod's) returned, NULL when it does not import it ---- */
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
            prev = InterlockedExchangePointer((void *volatile *)&slots->u1.Function, detour);
            VirtualProtect(&slots->u1.Function, 8, old, &old);
            return prev;
        }
    }
    return NULL;
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
