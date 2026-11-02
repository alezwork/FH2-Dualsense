/* icons.h -- the game's button icons follow the device in use: the DualSense (PlayStation icons), the keyboard (its
   keys) or an Xbox controller (the game's own). [Icons] Mode=Auto; or always one of them.
   Which device: the one that gave the last real input -- a button, a trigger pressed past a quarter, a stick pushed
   past a third (not a drifting stick), a key; only while the game is in front. An Xbox pad is read through XInput
   for that alone; one moving together with the DualSense is the DualSense through Steam Input or DS4Windows. A switch
   waits 0.6 s after the one before (no flicker when two are used at once).
   How: the icon archives the game is given are the look in use (files.h); and the game reads its icons once, so a
   switch also writes the other look over the icons already in the game's memory. The engine uploads a texture again
   when its memory changes (it seals texture memory read-only and unseals it on a write: its write tracking), so
   the new look is on screen at once. The icons are found by their bytes (every look of every icon is unique,
   files.h), by a search of the game's memory (10 GB: half a minute) on a thread of its own at the lowest priority:
   20 s after the game has read its icons, then every minute where icons were found, every 10 minutes all of it. A
   switch only writes the places known, at once; the search puts the look in use into any copy it finds. */
#pragma once

static ULONGLONG g_iconT0;
static volatile ULONGLONG g_padInputAt; /* the last real use of an XInput pad (GetTickCount64) */

/* ---- the icons in the game's memory ---- */
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
    int tex, gen; /* gen: the search that saw it last */
} Found;
static Found *g_found;
static int g_nfound, g_capfound, g_scanGen;
static void *g_allocs[256]; /* the allocations icons were found in (the searches between the whole ones) */
static int g_nallocs;
typedef struct {
    ULONGLONG key;
    int tex, set;
} KeySlot;
static KeySlot g_keys[8192];
static SRWLOCK g_memLock = SRWLOCK_INIT; /* g_found, and one writer of the icons at a time */

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

/* the icons: the stored entries of the archives whose looks differ */
static void tex_build(void)
{
    int cap = 0;
    for (int i = 0; i < 8192; i++)
        g_keys[i].tex = -1;
    for (int i = 0; i < 3; i++) {
        IconArch *f = &g_iconArch[i];
        if (!f->set[0] || !f->set[1] || !f->set[2])
            continue;
        const BYTE *z = f->set[0];
        ZEnt *e = NULL;
        int n = zip_parse(z, f->len, &e);
        for (int k = 0; k < n; k++) {
            DWORD off = e[k].dataOff, cs = e[k].csize;
            if (e[k].method || cs < 0x53 + 64 || (!memcmp(f->set[0] + off, f->set[1] + off, cs) &&
                                                  !memcmp(f->set[0] + off, f->set[2] + off, cs)))
                continue;
            DWORD texLen = *(const DWORD *)(z + off + 0x20);
            if (texLen < 16 + 64 || 0x53 + texLen > cs)
                continue;
            if (g_ntex == cap) {
                IconTex *t = (IconTex *)(g_tex ? HeapReAlloc(GetProcessHeap(), 0, g_tex, (cap ? cap * 2 : 256) * sizeof *g_tex)
                                               : HeapAlloc(GetProcessHeap(), 0, 256 * sizeof *g_tex));
                if (!t)
                    break;
                g_tex = t, cap = cap ? cap * 2 : 256;
            }
            IconTex *t = &g_tex[g_ntex];
            ZeroMemory(t, sizeof *t);
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
    fh2_say("icons: %d icon textures to look for in the game's memory when the look changes", g_ntex);
}

/* our own copies (not the game's) */
static int is_ours(const BYTE *p)
{
    for (int i = 0; i < 3; i++)
        for (int v = 0; v < NSETS; v++)
            if (g_iconArch[i].set[v] && p >= g_iconArch[i].set[v] && p < g_iconArch[i].set[v] + g_iconArch[i].len)
                return 1;
    return 0;
}

static void found_add(BYTE *at, int tex, int gen)
{
    AcquireSRWLockExclusive(&g_memLock);
    for (int i = 0; i < g_nfound; i++)
        if (g_found[i].at == at) {
            g_found[i].tex = tex, g_found[i].gen = gen;
            ReleaseSRWLockExclusive(&g_memLock);
            return;
        }
    if (g_nfound == g_capfound) {
        int cap = g_capfound ? g_capfound * 2 : 512;
        Found *f = (Found *)(g_found ? HeapReAlloc(GetProcessHeap(), 0, g_found, cap * sizeof *g_found)
                                     : HeapAlloc(GetProcessHeap(), 0, cap * sizeof *g_found));
        if (!f) {
            ReleaseSRWLockExclusive(&g_memLock);
            return;
        }
        g_found = f, g_capfound = cap;
    }
    g_found[g_nfound].at = at, g_found[g_nfound].tex = tex, g_found[g_nfound].gen = gen;
    g_nfound++;
    ReleaseSRWLockExclusive(&g_memLock);
}

static int known_alloc(void *base)
{
    for (int i = 0; i < g_nallocs; i++)
        if (g_allocs[i] == base)
            return 1;
    return 0;
}

/* which look the memory at p holds of icon t, or -1 */
static int holds(BYTE *p, int t, BYTE *tmp)
{
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), p, tmp, g_tex[t].len, &got) || got != g_tex[t].len)
        return -1;
    /* the look now first (two looks of an icon can be the same) */
    if (!memcmp(tmp, g_tex[t].data[g_iconSet], g_tex[t].len))
        return g_iconSet;
    for (int v = 0; v < NSETS; v++)
        if (!memcmp(tmp, g_tex[t].data[v], g_tex[t].len))
            return v;
    return -1;
}

/* the game's memory searched for the icons: all of it, or (known) the allocations they were found in before */
static void mem_scan(int known)
{
    static BYTE chunk[(1 << 20) + 8];
    BYTE *tmp = (BYTE *)HeapAlloc(GetProcessHeap(), 0, 1 << 21);
    MEMORY_BASIC_INFORMATION mbi;
    ULONGLONG t0 = GetTickCount64(), bytes = 0, sealedMb = 0;
    int regions = 0, files = 0, gen = ++g_scanGen, before = g_nfound;
    if (!tmp || !g_bloom) {
        if (tmp)
            HeapFree(GetProcessHeap(), 0, tmp);
        return;
    }
    for (BYTE *a = (BYTE *)0x10000; VirtualQuery(a, &mbi, sizeof mbi) == sizeof mbi; a = (BYTE *)mbi.BaseAddress + mbi.RegionSize) {
        DWORD prot = mbi.Protect & 0xFF;
        /* writable memory, and read-only: the engine seals texture memory nobody writes to (write tracking) */
        if (mbi.State != MEM_COMMIT || (mbi.Type != MEM_PRIVATE && mbi.Type != MEM_MAPPED) || (mbi.Protect & PAGE_GUARD) ||
            (prot != PAGE_READWRITE && prot != PAGE_WRITECOPY && prot != PAGE_EXECUTE_READWRITE && prot != PAGE_READONLY))
            continue;
        if ((BYTE *)mbi.BaseAddress + mbi.RegionSize <= a)
            break;
        if (known && !known_alloc(mbi.AllocationBase))
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
                            found_add(base, t, gen);
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
                        found_add(base, t, gen);
                }
            }
        }
    }
    HeapFree(GetProcessHeap(), 0, tmp);
    /* a whole search: what it did not see is gone; the allocations of what it saw are the next searches' */
    AcquireSRWLockExclusive(&g_memLock);
    if (!known) {
        int n = 0;
        for (int i = 0; i < g_nfound; i++)
            if (g_found[i].gen == gen)
                g_found[n++] = g_found[i];
        g_nfound = n;
        g_nallocs = 0;
        for (int i = 0; i < g_nfound; i++) {
            MEMORY_BASIC_INFORMATION m;
            if (VirtualQuery(g_found[i].at, &m, sizeof m) && !known_alloc(m.AllocationBase) && g_nallocs < 256)
                g_allocs[g_nallocs++] = m.AllocationBase;
        }
    }
    int seen = 0;
    for (int i = 0; i < g_nfound; i++)
        seen += g_found[i].gen == gen;
    ReleaseSRWLockExclusive(&g_memLock);
    if (!known || g_nfound != before)
        fh2_say("icons: the game's memory searched (%s): %d regions, %llu MB (%llu MB read-only) in %.1f s -- %d icon copies "
                "(%d of them in whole .swatchbin files), %d known in all", known ? "where icons were" : "all of it", regions,
                bytes >> 20, sealedMb >> 20, (GetTickCount64() - t0) / 1000.0, seen, files, g_nfound);
    if (known)
        return;
    /* which icons the game has in memory, and how many copies of each */
    char line[900];
    int len = 0, missing = 0;
    AcquireSRWLockShared(&g_memLock);
    for (int t = 0; t < g_ntex; t++)
        g_tex[t].seen = 0;
    for (int i = 0; i < g_nfound; i++)
        g_tex[g_found[i].tex].seen++;
    ReleaseSRWLockShared(&g_memLock);
    line[0] = 0;
    for (int t = 0; t < g_ntex; t++)
        if (g_tex[t].seen && len < 820)
            len += wsprintfA(line + len, " %s%s", g_tex[t].name, g_tex[t].seen > 1 ? (g_tex[t].seen > 2 ? "(3+)" : "(2)") : "");
        else if (!g_tex[t].seen)
            missing++;
    fh2_say("icons:   in memory:%s", line);
    fh2_say("icons:   %d icons not in memory now (not loaded by the game)", missing);
}

/* Writing into texture memory the engine has sealed (read-only after a while without writes) faults; the engine's
   exception handler unseals the pages, marks them written -- the texture goes to the GPU again -- and the write goes
   on. Only memory with a write watch is the engine's; ours (called last) unprotects a page of an icon being written
   that nobody else took. */
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

/* the wanted look over every found icon that holds another one (g_memLock held) */
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

/* the look in use over the icons known in the game's memory; why: for the log (NULL: only when some changed) */
static void mem_apply(const char *why)
{
    static BYTE *tmp;
    int w = 0, a = 0, g = 0, set;
    if (!tmp)
        tmp = (BYTE *)HeapAlloc(GetProcessHeap(), 0, 1 << 21);
    if (!tmp || !g_ntex)
        return;
    double t0 = fh2_now();
    AcquireSRWLockExclusive(&g_memLock);
    LONG ours = g_ourUnseals;
    g_sealedWrites = 0;
    set = g_iconSet;
    mem_write(set, tmp, &w, &a, &g);
    ReleaseSRWLockExclusive(&g_memLock);
    if (why || w)
        fh2_say("icons: %s in the game's memory: %d icons changed in %.0f ms (%d already right, %d gone); %d of them in "
                "sealed texture memory (%ld pages unsealed by us, not by the engine)", kSetName[set], w,
                (fh2_now() - t0) * 1000, a, g, g_sealedWrites, g_ourUnseals - ours);
}

/* the game has read its icons: they are in its memory */
static int icons_read(void)
{
    for (int i = 0; i < g_nserved; i++)
        if (g_served[i].looks && g_served[i].opens)
            return 1;
    return 0;
}

/* the search, on its own thread at the lowest priority */
static DWORD WINAPI scan_thread(void *arg)
{
    (void)arg;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    while (!icons_read())
        Sleep(1000);
    Sleep(20000);
    for (;;) {
        mem_scan(0);
        mem_apply(NULL);
        for (int k = 0; k < 10; k++) {
            Sleep(60000);
            mem_scan(1);
            mem_apply(NULL);
        }
    }
}

static void icons_switch(int set, const char *why)
{
    if (!g_iconsOn || set < 0 || set >= NSETS)
        return;
    InterlockedExchange(&g_iconSet, set);
    fh2_say("icons: %s look now (%s, %.1f s)", kSetName[set], why, (GetTickCount64() - g_iconT0) / 1000.0);
    if (icons_read())
        mem_apply("switched");
}

/* ---- the devices ---- */
typedef DWORD(WINAPI *XInputGetStateFn)(DWORD, XINPUT_STATE *);
static XInputGetStateFn g_xiGet;

/* a deliberate input: a button, a trigger past a quarter, a stick past a third (a drifting stick is not one) */
static int pad_moved(const XINPUT_GAMEPAD *g)
{
    return g->wButtons || g->bLeftTrigger > 64 || g->bRightTrigger > 64 || g->sThumbLX > 11000 || g->sThumbLX < -11000 ||
           g->sThumbLY > 11000 || g->sThumbLY < -11000 || g->sThumbRX > 11000 || g->sThumbRX < -11000 ||
           g->sThumbRY > 11000 || g->sThumbRY < -11000;
}

/* the Xbox pads (XInput): their use, and whether one is there */
static int xinput_poll(int all)
{
    static DWORD packet[4], seen[4];
    static int there[4];
    int any = 0;
    if (!g_xiGet)
        return 0;
    for (DWORD i = 0; i < 4; i++) {
        XINPUT_STATE st;
        if (!there[i] && !all)
            continue; /* a slot with nothing in it is slow to ask: every 2 s only */
        there[i] = g_xiGet(i, &st) == ERROR_SUCCESS;
        if (!there[i])
            continue;
        any = 1;
        if (seen[i] && st.dwPacketNumber != packet[i] && pad_moved(&st.Gamepad))
            g_padInputAt = GetTickCount64();
        packet[i] = st.dwPacketNumber, seen[i] = 1;
    }
    return any;
}

/* the look to start with: the DualSense there, else an Xbox pad, else the keyboard */
static int icons_start_look(void)
{
    switch (S.icons) {
    case ICONS_PS:
        return LOOK_PS;
    case ICONS_KEYS:
        return g_kbLook ? LOOK_KEYS : LOOK_XBOX;
    case ICONS_XBOX:
        return LOOK_XBOX;
    }
    if (ds_present())
        return LOOK_PS;
    if (xinput_poll(1))
        return LOOK_XBOX;
    return g_kbLook ? LOOK_KEYS : LOOK_XBOX;
}

/* the device of the last real input: the DualSense, an Xbox pad, the keyboard */
static DWORD WINAPI icons_thread(void *arg)
{
    ULONGLONG lastSwitch = 0, lastAll = 0;
    (void)arg;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    for (;;) {
        Sleep(50);
        ULONGLONG now = GetTickCount64();
        if (S.icons != ICONS_AUTO) {
            int want = icons_start_look();
            if (want != g_iconSet)
                icons_switch(want, "the setting");
            Sleep(450);
            continue;
        }
        xinput_poll(now - lastAll > 2000);
        if (now - lastAll > 2000)
            lastAll = now;
        if (!fh2_foreground())
            continue;
        ULONGLONG ds = g_dsInputAt, pad = g_padInputAt, key = g_kbLook ? g_kbInputAt : 0;
        int want = g_iconSet;
        /* an Xbox pad moving with the DualSense is the DualSense through Steam Input or DS4Windows */
        if (ds && ds + 150 >= pad && ds >= key)
            want = LOOK_PS;
        else if (pad && pad > ds + 150 && pad >= key)
            want = LOOK_XBOX;
        else if (key && key > ds && key > pad)
            want = LOOK_KEYS;
        if (want != g_iconSet && now - lastSwitch > 600 && (ds > lastSwitch || pad > lastSwitch || key > lastSwitch)) {
            lastSwitch = now;
            icons_switch(want, want == LOOK_PS ? "the DualSense is in use" : want == LOOK_KEYS ? "the keyboard is in use"
                                                                                               : "an Xbox controller is in use");
        }
    }
}

/* for tests and other mods: the look now (-1 none) and a switch */
__declspec(dllexport) LONG Fh2IconLook(void) { return g_iconsOn ? g_iconSet : -1; }
/* a whole search now (tests: the game's copies are found before a switch) */
__declspec(dllexport) void Fh2ScanIcons(void)
{
    if (g_iconsOn && g_ntex) {
        mem_scan(0);
        mem_apply(NULL);
    }
}
__declspec(dllexport) void Fh2SwitchIcons(int look)
{
    if (g_iconsOn && look >= 0 && look < NSETS)
        icons_switch(look, "asked by another program");
}

/* the look to start with, before the game opens its icons (files_build has run) */
static void icons_prepare(void)
{
    HMODULE xi = LoadLibraryExW(L"xinput1_4.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    g_iconT0 = GetTickCount64();
    g_xiGet = xi ? (XInputGetStateFn)(void *)GetProcAddress(xi, "XInputGetState") : NULL;
    if (!g_iconsOn)
        return;
    g_iconSet = icons_start_look();
    fh2_say("icons: %s look to start with ([Icons] Mode=%s)", kSetName[g_iconSet],
            S.icons == ICONS_AUTO ? "Auto: the device in use" : S.icons == ICONS_PS ? "PlayStation"
                                                          : S.icons == ICONS_KEYS ? "Keyboard" : "Xbox");
}

/* the switching, on its own thread */
static void icons_start(void)
{
    HANDLE h;
    if (!g_iconsOn)
        return;
    tex_build();
    h = CreateThread(NULL, 0, icons_thread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
    h = CreateThread(NULL, 0, scan_thread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
}
