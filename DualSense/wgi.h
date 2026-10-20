/* wgi.h -- the DualSense as one of the PC's gamepads (Azimuth 0.9.10).
   The engine gives the game the PC's Windows.Gaming.Input gamepads (azimuth.exe 0.9.10, its pads code): it takes the
   Gamepad statics (RoGetActivationFactory "Windows.Gaming.Input.Gamepad"), registers its handlers of GamepadAdded and
   GamepadRemoved, keeps the statics, and then reads every pad it was given with GetCurrentReading and sends it the
   game's four vibration levels -- the two motors and the two impulse triggers -- with put_Vibration. Windows lists
   only Xbox-like controllers there, not the DualSense. So the DualSense is a gamepad object of ours (the
   Windows.Gaming.Input IGamepad interface: its reading and its vibration), handed to the engine's own GamepadAdded
   handler once the engine's pads are up and the controller is there, and to its GamepadRemoved handler when the
   controller goes. The engine treats it as any other gamepad: the game sees it, its player, its rumble.
   The handlers and the statics are found by the code that registers them, right after the class name. */
#pragma once

/* Windows.Gaming.Input */
typedef struct {
    UINT64 timestamp; /* microseconds */
    UINT32 buttons;
    double leftTrigger, rightTrigger, leftX, leftY, rightX, rightY;
} WgiReading;

typedef struct {
    double leftMotor, rightMotor, leftTrigger, rightTrigger; /* 0..1 */
} WgiVibration;

enum {
    WGI_MENU = 0x1, WGI_VIEW = 0x2, WGI_A = 0x4, WGI_B = 0x8, WGI_X = 0x10, WGI_Y = 0x20, WGI_UP = 0x40, WGI_DOWN = 0x80,
    WGI_LEFT = 0x100, WGI_RIGHT = 0x200, WGI_LB = 0x400, WGI_RB = 0x800, WGI_LS = 0x1000, WGI_RS = 0x2000
};

static const IID kIidUnknown = {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const IID kIidInspectable = {0xAF86E2E0, 0xB12D, 0x4C6A, {0x9C, 0x5A, 0xD7, 0xAA, 0x65, 0x10, 0x1E, 0x90}};
static const IID kIidAgile = {0x94EA2B94, 0xE9CC, 0x49E0, {0xC0, 0xFF, 0xEE, 0x64, 0xCA, 0x8F, 0x5B, 0x90}};
static const IID kIidGamepad = {0xBC7BB43C, 0x0A69, 0x3903, {0x9E, 0x9D, 0xA5, 0x0F, 0x86, 0xA4, 0x5D, 0xE5}};

typedef struct WgiPad WgiPad;
typedef struct {
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(WgiPad *, REFIID, void **);
    ULONG(STDMETHODCALLTYPE *AddRef)(WgiPad *);
    ULONG(STDMETHODCALLTYPE *Release)(WgiPad *);
    HRESULT(STDMETHODCALLTYPE *GetIids)(WgiPad *, ULONG *, IID **);
    HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)(WgiPad *, void **);
    HRESULT(STDMETHODCALLTYPE *GetTrustLevel)(WgiPad *, int *);
    HRESULT(STDMETHODCALLTYPE *get_Vibration)(WgiPad *, WgiVibration *);
    HRESULT(STDMETHODCALLTYPE *put_Vibration)(WgiPad *, const WgiVibration *); /* the struct by value: by address */
    HRESULT(STDMETHODCALLTYPE *GetCurrentReading)(WgiPad *, WgiReading *);
} WgiPadVtbl;
struct WgiPad {
    const WgiPadVtbl *v;
    volatile LONG refs;
};

static WgiVibration g_vib; /* the last the engine sent */

static HRESULT STDMETHODCALLTYPE pad_qi(WgiPad *p, REFIID iid, void **out)
{
    if (!out)
        return E_POINTER;
    if (IsEqualGUID(iid, &kIidUnknown) || IsEqualGUID(iid, &kIidInspectable) || IsEqualGUID(iid, &kIidAgile) ||
        IsEqualGUID(iid, &kIidGamepad)) {
        InterlockedIncrement(&p->refs);
        *out = p;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
/* one object for the life of the process: counted, never freed */
static ULONG STDMETHODCALLTYPE pad_addref(WgiPad *p) { return (ULONG)InterlockedIncrement(&p->refs); }
static ULONG STDMETHODCALLTYPE pad_release(WgiPad *p) { return (ULONG)InterlockedDecrement(&p->refs); }
static HRESULT STDMETHODCALLTYPE pad_iids(WgiPad *p, ULONG *n, IID **iids)
{
    (void)p;
    if (n)
        *n = 0;
    if (iids)
        *iids = NULL;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE pad_class(WgiPad *p, void **name)
{
    (void)p;
    if (name)
        *name = NULL; /* an empty HSTRING */
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE pad_trust(WgiPad *p, int *level)
{
    (void)p;
    if (level)
        *level = 0; /* BaseTrust */
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE pad_get_vibration(WgiPad *p, WgiVibration *v)
{
    (void)p;
    if (!v)
        return E_POINTER;
    AcquireSRWLockShared(&g_lock);
    *v = g_vib;
    ReleaseSRWLockShared(&g_lock);
    return S_OK;
}

static float unit(double v) { return v > 0 && v <= 1 ? (float)v : v > 1 ? 1.0f : 0.0f; }

/* the game's rumble and impulse triggers, from the engine */
static HRESULT STDMETHODCALLTYPE pad_put_vibration(WgiPad *p, const WgiVibration *v)
{
    (void)p;
    if (!v)
        return E_POINTER;
    AcquireSRWLockExclusive(&g_lock);
    g_vib = *v;
    g_motor[0] = (WORD)(unit(v->leftMotor) * 65535.0f + 0.5f);
    g_motor[1] = (WORD)(unit(v->rightMotor) * 65535.0f + 0.5f);
    g_trigger[0] = unit(v->leftTrigger);
    g_trigger[1] = unit(v->rightTrigger);
    ReleaseSRWLockExclusive(&g_lock);
    InterlockedExchange(&g_outDirty, 1);
    if (g_wake)
        SetEvent(g_wake);
    return S_OK;
}

static double axis(SHORT v) { return v >= 0 ? v / 32767.0 : v / 32768.0; }

/* the controller now, as a Windows.Gaming.Input reading */
static HRESULT STDMETHODCALLTYPE pad_reading(WgiPad *p, WgiReading *r)
{
    static const struct {
        WORD x;
        UINT32 w;
    } kButtons[14] = {{XINPUT_GAMEPAD_DPAD_UP, WGI_UP},       {XINPUT_GAMEPAD_DPAD_DOWN, WGI_DOWN},
                      {XINPUT_GAMEPAD_DPAD_LEFT, WGI_LEFT},   {XINPUT_GAMEPAD_DPAD_RIGHT, WGI_RIGHT},
                      {XINPUT_GAMEPAD_START, WGI_MENU},       {XINPUT_GAMEPAD_BACK, WGI_VIEW},
                      {XINPUT_GAMEPAD_LEFT_THUMB, WGI_LS},    {XINPUT_GAMEPAD_RIGHT_THUMB, WGI_RS},
                      {XINPUT_GAMEPAD_LEFT_SHOULDER, WGI_LB}, {XINPUT_GAMEPAD_RIGHT_SHOULDER, WGI_RB},
                      {XINPUT_GAMEPAD_A, WGI_A},              {XINPUT_GAMEPAD_B, WGI_B},
                      {XINPUT_GAMEPAD_X, WGI_X},              {XINPUT_GAMEPAD_Y, WGI_Y}};
    XINPUT_GAMEPAD g;
    (void)p;
    if (!r)
        return E_POINTER;
    AcquireSRWLockShared(&g_lock);
    g = g_dsPad;
    ReleaseSRWLockShared(&g_lock);
    ZeroMemory(r, sizeof *r);
    r->timestamp = (UINT64)(fh2_now() * 1e6);
    for (int i = 0; i < 14; i++)
        if (g.wButtons & kButtons[i].x)
            r->buttons |= kButtons[i].w;
    r->leftTrigger = g.bLeftTrigger / 255.0;
    r->rightTrigger = g.bRightTrigger / 255.0;
    r->leftX = axis(g.sThumbLX), r->leftY = axis(g.sThumbLY);
    r->rightX = axis(g.sThumbRX), r->rightY = axis(g.sThumbRY);
    return S_OK;
}

static const WgiPadVtbl kPadVtbl = {pad_qi,  pad_addref,        pad_release,       pad_iids,   pad_class,
                                    pad_trust, pad_get_vibration, pad_put_vibration, pad_reading};
static WgiPad g_pad = {&kPadVtbl, 1};

/* ---- the engine's pads ---- */
typedef HRESULT(STDMETHODCALLTYPE *WgiInvokeFn)(void *handler, void *sender, void *pad);
static BYTE *g_engAdded, *g_engRemoved; /* its handler objects (a vtable, then: 1 added, 0 removed) */
static void *volatile *g_engStatics;   /* set when its pads are up */
static int g_padIn;                     /* ours is among them */

static int wgi_handler_ok(Image *m, BYTE *h, BYTE kind)
{
    if (!in_range(h, m->data, m->dataEnd - 16))
        return 0;
    void **vt = *(void ***)h;
    return in_range(vt, m->rdata, m->rdataEnd - 32) && in_range(vt[3], m->text, m->textEnd) && h[8] == kind;
}

/* the engine's two handlers and its statics: after "lea rcx, [L"Windows.Gaming.Input.Gamepad"]", the calls of the
   statics' add_GamepadAdded (vtable +0x30) and add_GamepadRemoved (+0x40) with "lea rdx, [handler]" before each, and
   the statics stored ("mov [rip + x], rax") */
static int wgi_find_engine(BYTE *engine)
{
    static const WCHAR kClass[] = L"Windows.Gaming.Input.Gamepad";
    Image m;
    BYTE *str = NULL, *lea = NULL;
    if (!image_of(engine, &m))
        return 0;
    for (BYTE *p = m.rdata + 2; p + sizeof kClass <= m.rdataEnd && !str; p += 2)
        if (*(const WCHAR *)p == 'W' && !*(const WCHAR *)(p - 2) && !memcmp(p, kClass, sizeof kClass))
            str = p;
    for (BYTE *p = m.text; str && p + 7 <= m.textEnd && !lea; p++)
        if (p[0] == 0x48 && p[1] == 0x8D && p[2] == 0x0D && rel32(p, 3, 7) == str)
            lea = p;
    BYTE *added = NULL, *removed = NULL;
    void *volatile *statics = NULL;
    for (BYTE *p = lea; p && p < lea + 0x140 && p + 16 <= m.textEnd && !statics; p++) {
        if (p[0] == 0x48 && p[1] == 0x8D && p[2] == 0x15) {
            for (BYTE *q = p + 7; q < p + 16; q++)
                if (q[0] == 0xFF && q[1] == 0x50 && (q[2] == 0x30 || q[2] == 0x40)) {
                    if (q[2] == 0x30 && !added)
                        added = rel32(p, 3, 7);
                    else if (q[2] == 0x40 && !removed)
                        removed = rel32(p, 3, 7);
                    break;
                }
        } else if (added && removed && p[0] == 0x48 && p[1] == 0x89 && p[2] == 0x05)
            statics = (void *volatile *)rel32(p, 3, 7);
    }
    if (!statics || !in_range((void *)statics, m.data, m.dataEnd) || !wgi_handler_ok(&m, added, 1) ||
        !wgi_handler_ok(&m, removed, 0) || *(void **)added != *(void **)removed) {
        ds_note("pads: not Azimuth 0.9.10's (class name %s, its code %s, handlers %p %p, statics %p) -- the DualSense "
                "can't be given to the game", str ? "found" : "NOT found", lea ? "found" : "NOT found", (void *)added,
                (void *)removed, (void *)statics);
        return 0;
    }
    g_engAdded = added, g_engRemoved = removed, g_engStatics = statics;
    fh2_say("pads: the engine's GamepadAdded/Removed handlers at engine+0x%lX/0x%lX, its statics at engine+0x%lX",
            (DWORD)(added - m.base), (DWORD)(removed - m.base), (DWORD)((BYTE *)statics - m.base));
    return 1;
}

/* the controller's thread: ours to the engine once its pads are up and the controller is there; away when it goes */
static void wgi_update(int connected)
{
    BYTE *h = connected ? g_engAdded : g_engRemoved;
    if (!h || connected == g_padIn || (connected && !*g_engStatics))
        return;
    HRESULT hr = ((WgiInvokeFn)(*(void ***)h)[3])(h, NULL, &g_pad);
    g_padIn = connected;
    ds_note("pads: the DualSense %s the game's gamepads (0x%08lX)", connected ? "joins" : "leaves", (unsigned long)hr);
}
