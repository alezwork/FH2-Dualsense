/* ui.h -- "Trigger feedback" On/Off in the game's controller settings (Settings > Controls): the DualSense's
   adaptive triggers and the Xbox controller's impulse triggers.
   The game's UI is XAML bound to C++ view models; a view model type lists its properties once, when it is first used
   (FH2 1.0.0.64: 0xD012E0 adds eight descriptors to the controller options type with 0x155EE30). This adds a ninth,
   TriggerFeedback, made like the game's InvertCameraLookBack descriptor (0xD121F0: base init 0x143EB80, the value
   type 0x15C8B20(0), a getter thunk that boxes an int with 0x4702D0, a setter thunk that reads it at +0x28), with
   our getter and setter: the value lives in mods\DualSense.ini [Triggers] Enabled. icons.h puts the switch into the
   XAML (A_ControllerOptions.xaml, after the camera look-back switch). Everything is found by its code around the
   string "InvertCameraLookBack", not by fixed addresses. */
#pragma once

typedef void *(__fastcall *BoxIntFn)(void **holder, const int *value);
typedef void(__fastcall *BaseInitFn)(void *desc);
typedef void *(__fastcall *ValueTypeFn)(int);
typedef void(__fastcall *AddPropFn)(void *type, void *pair);
typedef void(__fastcall *ClassInitFn)(void);
static BoxIntFn g_boxInt;
static BaseInitFn g_descInit;
static ValueTypeFn g_valueType;
static AddPropFn g_addProp;
static void *g_descVtable, *g_vmType;
static ClassInitFn g_classInitOrig;
#ifdef _MSC_VER
static __declspec(align(16)) BYTE g_desc[0x60];
#else
static BYTE g_desc[0x60] __attribute__((aligned(16)));
#endif
static void *g_pair[2];
static volatile LONG g_propAdded;

static void *__fastcall ui_get(void **out, void *x, void *obj)
{
    int v = g_trigOn ? 1 : 0;
    void *holder = NULL;
    (void)x, (void)obj;
    g_boxInt(&holder, &v);
    *out = holder;
    return out;
}

static void __fastcall ui_set(void *x, void *obj, BYTE *boxed)
{
    (void)x, (void)obj;
    int v = *(int *)(boxed + 0x28) != 0;
    if (v == g_trigOn)
        return;
    InterlockedExchange(&g_trigOn, v);
    WritePrivateProfileStringW(L"Triggers", L"Enabled", v ? L"1" : L"0", g_modIni);
    InterlockedExchange(&g_outDirty, 1);
    if (g_wake)
        SetEvent(g_wake);
    fh2_say("trigger feedback %s (the game's controller settings)", v ? "on" : "off");
}

static void __fastcall ui_class_init(void)
{
    g_classInitOrig();
    if (InterlockedExchange(&g_propAdded, 1))
        return;
    void *vt = g_valueType(0);
    g_descInit(g_desc);
    *(void **)(g_desc + 0x28) = (void *)ui_get;
    *(void **)(g_desc + 0x30) = (void *)ui_set;
    *(void **)(g_desc + 0x38) = NULL;
    *(const char **)(g_desc + 0x40) = "TriggerFeedback";
    *(void **)(g_desc + 0x48) = vt;
    g_desc[0x50] &= 0xF0;
    *(void **)g_desc = g_descVtable;
    g_pair[0] = g_desc, g_pair[1] = NULL;
    g_addProp(g_vmType, g_pair);
    fh2_say("trigger feedback: the switch is in the game's controller settings");
}

/* the game's image (mapped by the engine at its preferred base) */
typedef struct {
    BYTE *base, *text, *textEnd, *rdata, *rdataEnd;
    RUNTIME_FUNCTION *pdata;
    DWORD npdata;
} GameImage;

static int game_image(GameImage *g)
{
    BYTE *b = (BYTE *)0x140000000ull;
    MEMORY_BASIC_INFORMATION m;
    ZeroMemory(g, sizeof *g);
    if (!VirtualQuery(b, &m, sizeof m) || m.State != MEM_COMMIT || ((IMAGE_DOS_HEADER *)b)->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return 0;
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    g->base = b;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, s++) {
        if (!memcmp(s->Name, ".text", 6))
            g->text = b + s->VirtualAddress, g->textEnd = g->text + s->Misc.VirtualSize;
        else if (!memcmp(s->Name, ".rdata", 7))
            g->rdata = b + s->VirtualAddress, g->rdataEnd = g->rdata + s->Misc.VirtualSize;
    }
    IMAGE_DATA_DIRECTORY d = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    g->pdata = (RUNTIME_FUNCTION *)(b + d.VirtualAddress);
    g->npdata = d.Size / sizeof(RUNTIME_FUNCTION);
    return g->text && g->rdata && g->npdata;
}

static BYTE *game_func(GameImage *g, BYTE *p)
{
    DWORD rva = (DWORD)(p - g->base), lo = 0, hi = g->npdata;
    while (lo < hi) {
        DWORD mid = (lo + hi) / 2;
        if (g->pdata[mid].EndAddress <= rva)
            lo = mid + 1;
        else if (g->pdata[mid].BeginAddress > rva)
            hi = mid;
        else
            return g->base + g->pdata[mid].BeginAddress;
    }
    return NULL;
}

static BYTE *rel32(BYTE *insn, int dispAt, int len) { return insn + len + *(INT32 *)(insn + dispAt); }

static int ui_install(void)
{
    GameImage g;
    static const char kName[] = "InvertCameraLookBack";
    g_trigOn = fh2_ini_int(L"Triggers", L"Enabled", 1) != 0;
    if (!game_image(&g)) {
        fh2_say("trigger feedback: the game's image is not where it should be -- no switch in its settings");
        return 0;
    }
    BYTE *str = NULL;
    for (BYTE *p = g.rdata + 1; p + sizeof kName <= g.rdataEnd; p++)
        if (*p == 'I' && p[-1] == 0 && !memcmp(p, kName, sizeof kName)) {
            str = p;
            break;
        }
    /* the descriptor's init: lea rax,[name] with its known neighbours */
    BYTE *L = NULL;
    for (BYTE *p = g.text; str && p + 7 <= g.textEnd && !L; p++)
        if (p[0] == 0x48 && p[1] == 0x8D && p[2] == 0x05 && rel32(p, 3, 7) == str && p - 0x40 >= g.text &&
            p[-0x40] == 0x33 && p[-0x3F] == 0xC9 && p[-0x3E] == 0xE8 && !memcmp(p - 0x36, "\x48\x8D\x3D", 3) &&
            p[-0x2C] == 0xE8 && !memcmp(p - 0x27, "\x48\x8D\x05", 3) && !memcmp(p - 0x19, "\x48\x8D\x05", 3) &&
            !memcmp(p + 0x1C, "\x48\x8D\x05", 3))
            L = p;
    BYTE *descFn = L ? game_func(&g, L) : NULL;
    if (!descFn) {
        fh2_say("trigger feedback: the game's settings code is not as expected -- no switch in its settings");
        return 0;
    }
    g_valueType = (ValueTypeFn)rel32(L - 0x3E, 1, 5);
    g_descInit = (BaseInitFn)rel32(L - 0x2C, 1, 5);
    BYTE *getter = rel32(L - 0x27, 3, 7);
    g_descVtable = rel32(L + 0x1C, 3, 7);
    /* the type's property list: the call of descFn followed by AddProperty(type, &pair) */
    BYTE *C = NULL;
    for (BYTE *p = g.text; p + 0x26 <= g.textEnd && !C; p++)
        if (p[0] == 0xE8 && rel32(p, 1, 5) == descFn && !memcmp(p + 5, "\x48\x89\x05", 3) &&
            !memcmp(p + 0x13, "\x48\x8D\x15", 3) && !memcmp(p + 0x1A, "\x48\x8D\x0D", 3) && p[0x21] == 0xE8)
            C = p;
    BYTE *classInit = C ? game_func(&g, C) : NULL;
    /* the getter thunk's second call boxes the int */
    int calls = 0;
    for (BYTE *p = getter; p < getter + 0xA0 && calls < 2; p++)
        if (*p == 0xE8 && ++calls == 2)
            g_boxInt = (BoxIntFn)rel32(p, 1, 5);
    static const BYTE kProlog[18] = {0x40, 0x53, 0x48, 0x81, 0xEC, 0xD0, 0x00, 0x00, 0x00,
                                     0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF};
    if (!classInit || !g_boxInt || memcmp(classInit, kProlog, sizeof kProlog)) {
        fh2_say("trigger feedback: the game's settings code is not as expected (property list %p, box %p) -- no switch",
                classInit, (void *)g_boxInt);
        return 0;
    }
    g_vmType = rel32(C + 0x1A, 3, 7);
    g_addProp = (AddPropFn)rel32(C + 0x21, 1, 5);
    /* the property list function: its first 18 bytes moved to a trampoline, a jump to ours in their place */
    BYTE *tramp = (BYTE *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp)
        return 0;
    memcpy(tramp, classInit, 18);
    tramp[18] = 0xFF, tramp[19] = 0x25, *(DWORD *)(tramp + 20) = 0;
    *(BYTE **)(tramp + 24) = classInit + 18;
    g_classInitOrig = (ClassInitFn)tramp;
    DWORD old;
    if (!VirtualProtect(classInit, 14, PAGE_EXECUTE_READWRITE, &old))
        return 0;
    classInit[0] = 0xFF, classInit[1] = 0x25, *(DWORD *)(classInit + 2) = 0;
    *(void **)(classInit + 6) = (void *)ui_class_init;
    VirtualProtect(classInit, 14, old, &old);
    FlushInstructionCache(GetCurrentProcess(), classInit, 14);
    fh2_say("trigger feedback: %s; the settings switch hooked (game+0x%lX, its list game+0x%lX)", g_trigOn ? "on" : "off",
            (DWORD)(descFn - g.base), (DWORD)(classInit - g.base));
    return 1;
}
