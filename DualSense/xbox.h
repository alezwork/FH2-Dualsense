/* xbox.h -- the Xbox controller's impulse triggers: the small rumble motors in LT and RT of Xbox One and Series
   controllers. XInput has no way to them; Windows.Gaming.Input has (Gamepad.Vibration: the two big motors and the
   two trigger motors), so while it sees an Xbox pad this sends the game's rumble there too (not through XInput, or
   the two would fight over the big motors) and adds the triggers:
     LT  from the front tyres' grip limit under braking: pulses like ABS at work, a steady buzz once they lock;
     RT  wheel spin, stronger the more they spin; knocks on the rev limiter;
     a gear change kicks the trigger pressed, a crash both -- from the car's data (car.h), as the DualSense's triggers;
     without it the game's own Xbox One impulse trigger levels.
   On/Off with the DualSense's adaptive triggers: "Trigger feedback" in the game's controller settings;
   [ImpulseTriggers] in DualSense.ini.
   Over USB and the Xbox Wireless Adapter; over Bluetooth only where Windows passes the trigger motors on.
   WinRT from C: the few interfaces needed are declared here by their vtables. */
#pragma once

typedef struct {
    double leftMotor, rightMotor, leftTrigger, rightTrigger;
} WgiVibration;

typedef struct WgiPad WgiPad;
typedef struct {
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(WgiPad *, REFIID, void **);
    ULONG(STDMETHODCALLTYPE *AddRef)(WgiPad *);
    ULONG(STDMETHODCALLTYPE *Release)(WgiPad *);
    void *GetIids, *GetRuntimeClassName, *GetTrustLevel;
    HRESULT(STDMETHODCALLTYPE *get_Vibration)(WgiPad *, WgiVibration *);
    HRESULT(STDMETHODCALLTYPE *put_Vibration)(WgiPad *, WgiVibration);
} WgiPadVtbl;
struct WgiPad {
    const WgiPadVtbl *v;
};

typedef struct WgiList WgiList; /* IVectorView<Gamepad> */
typedef struct {
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(WgiList *, REFIID, void **);
    ULONG(STDMETHODCALLTYPE *AddRef)(WgiList *);
    ULONG(STDMETHODCALLTYPE *Release)(WgiList *);
    void *GetIids, *GetRuntimeClassName, *GetTrustLevel;
    HRESULT(STDMETHODCALLTYPE *GetAt)(WgiList *, UINT, WgiPad **);
    HRESULT(STDMETHODCALLTYPE *get_Size)(WgiList *, UINT *);
} WgiListVtbl;
struct WgiList {
    const WgiListVtbl *v;
};

typedef struct WgiStatics WgiStatics; /* IGamepadStatics */
typedef struct {
    HRESULT(STDMETHODCALLTYPE *QueryInterface)(WgiStatics *, REFIID, void **);
    ULONG(STDMETHODCALLTYPE *AddRef)(WgiStatics *);
    ULONG(STDMETHODCALLTYPE *Release)(WgiStatics *);
    void *GetIids, *GetRuntimeClassName, *GetTrustLevel;
    void *add_GamepadAdded, *remove_GamepadAdded, *add_GamepadRemoved, *remove_GamepadRemoved;
    HRESULT(STDMETHODCALLTYPE *get_Gamepads)(WgiStatics *, WgiList **);
} WgiStaticsVtbl;
struct WgiStatics {
    const WgiStaticsVtbl *v;
};

static const GUID kIidGamepadStatics = {0x8BBCE529, 0xD49C, 0x39E9, {0x95, 0x60, 0xE4, 0x7D, 0xDE, 0x96, 0xB7, 0xC8}};
static const GUID kIidGamepad = {0xBC7BB43C, 0x0A69, 0x3903, {0x9E, 0x9D, 0xA5, 0x0F, 0x86, 0xA4, 0x5D, 0xE5}};

typedef HRESULT(WINAPI *RoInitializeFn)(int);
typedef HRESULT(WINAPI *RoGetActivationFactoryFn)(void *cls, REFIID iid, void **factory);
typedef HRESULT(WINAPI *WindowsCreateStringFn)(const WCHAR *s, UINT32 n, void **out);
typedef HRESULT(WINAPI *WindowsDeleteStringFn)(void *s);

static struct {
    float strength;
    int on;
} X_S;

static struct {
    int lockOn, spinOn;
    double lockSince, spinSince;
} X;

/* the trigger motors now, 0..1: [0] LT, [1] RT */
static void impulse_make(double now, float out[2])
{
    BYTE lt = g_xpadLT, rt = g_xpadRT;
    float game[2];
    out[0] = out[1] = 0;
    if (!X_S.on || !g_trigOn || !fh2_foreground())
        return;
    AcquireSRWLockShared(&g_lock);
    game[0] = g_trigger[0], game[1] = g_trigger[1];
    ReleaseSRWLockShared(&g_lock);
    if (!Car.hooked) { /* the game's own impulse triggers, as on the Xbox One */
        out[0] = game[0], out[1] = game[1];
        return;
    }
    if (!car_driving(now)) {
        X.lockOn = X.spinOn = 0;
        return;
    }
    /* a crash: both, by its strength, fading */
    float hit = S.crashJolt ? car_hit(now) : 0;
    if (hit > 0.08f) {
        out[0] = out[1] = 0.7f * hit;
        return;
    }
    float knock = S.gearKick && (lt > 24 || rt > 24) ? shift_knock(now) : 0;
    int kickL = knock > 0.05f && lt > rt;
    /* LT: past the grip limit a soft ABS pulse (11 Hz), into a steady buzz growing as the wheels lock */
    float sb = Car.brake;
    if (kickL)
        out[0] = 0.4f * knock;
    else if (latch(&X.lockOn, &X.lockSince, sb, S.brakeFrom - 0.1f, S.brakeFrom, 0.1, now) && lt > 10) {
        float k = slip_level(sb, S.brakeFrom, BRAKE_FULL);
        out[0] = 0.12f + 0.6f * k;
        if (k < 0.6f)
            out[0] *= 0.65f + 0.35f * sinf((float)(now * 2 * 3.14159265 * 11));
    }
    /* RT: wheel spin, growing; the rev limiter knocks */
    float sa = Car.spin;
    if (knock > 0.05f && !kickL)
        out[1] = 0.4f * knock;
    else if (latch(&X.spinOn, &X.spinSince, sa, S.spinFrom - 0.1f, S.spinFrom, 0.1, now) && rt > 16)
        out[1] = 0.1f + 0.45f * slip_level(sa, S.spinFrom, SPIN_FULL);
    else if (S.revLimiter && Car.rpm >= 0.975f && rt > 140)
        out[1] = 0.4f * (0.5f + 0.5f * sinf((float)(now * 2 * 3.14159265 * 8)));
}

static DWORD WINAPI xbox_thread(void *arg)
{
    HMODULE combase = LoadLibraryW(L"combase.dll");
    RoInitializeFn roInit = (RoInitializeFn)GetProcAddress(combase, "RoInitialize");
    RoGetActivationFactoryFn roFactory = (RoGetActivationFactoryFn)GetProcAddress(combase, "RoGetActivationFactory");
    WindowsCreateStringFn hsNew = (WindowsCreateStringFn)GetProcAddress(combase, "WindowsCreateString");
    WindowsDeleteStringFn hsDel = (WindowsDeleteStringFn)GetProcAddress(combase, "WindowsDeleteString");
    static const WCHAR kClass[] = L"Windows.Gaming.Input.Gamepad";
    WgiStatics *statics = NULL;
    WgiPad *pads[8];
    UINT npads = 0;
    void *cls = NULL;
    double nextList = 0, lastSent = 0, lastTick = 0;
    float soft[2] = {0, 0};
    WgiVibration last = {-1, -1, -1, -1};
    (void)arg;
    if (!roInit || !roFactory || !hsNew || !hsDel) {
        fh2_say("xbox: no Windows Runtime -- no impulse triggers (the rumble goes through XInput)");
        return 0;
    }
    roInit(1); /* multithreaded */
    if (FAILED(hsNew(kClass, (UINT32)(sizeof kClass / sizeof kClass[0] - 1), &cls)) ||
        FAILED(roFactory(cls, &kIidGamepadStatics, (void **)&statics)) || !statics) {
        fh2_say("xbox: Windows.Gaming.Input is not there -- no impulse triggers (the rumble goes through XInput)");
        if (cls)
            hsDel(cls);
        return 0;
    }
    hsDel(cls);
    for (;;) {
        double now = fh2_now();
        /* the pads, again every second (Windows finds them a moment after the first ask, and they come and go) */
        if (now >= nextList) {
            WgiList *list = NULL;
            UINT n = 0, had = npads;
            nextList = now + 1.0;
            for (UINT i = 0; i < npads; i++)
                pads[i]->v->Release(pads[i]);
            npads = 0;
            if (SUCCEEDED(statics->v->get_Gamepads(statics, &list)) && list) {
                list->v->get_Size(list, &n);
                for (UINT i = 0; i < n && npads < 8; i++) {
                    WgiPad *item = NULL, *pad = NULL;
                    if (SUCCEEDED(list->v->GetAt(list, i, &item)) && item) {
                        if (SUCCEEDED(item->v->QueryInterface(item, &kIidGamepad, (void **)&pad)) && pad)
                            pads[npads++] = pad;
                        item->v->Release(item);
                    }
                }
                list->v->Release(list);
            }
            if (npads != had) {
                fh2_say("xbox: %u Xbox pad%s through Windows.Gaming.Input%s", npads, npads == 1 ? "" : "s",
                        npads ? " -- rumble and impulse triggers" : "");
                last.leftMotor = -1; /* send again */
            }
            InterlockedExchange(&g_xboxPads, (LONG)npads);
        }
        /* the vibration: the game's two motors (with the car's gear changes and rev limiter), the triggers */
        WgiVibration v;
        float trig[2];
        WORD motor[2];
        AcquireSRWLockShared(&g_lock);
        motor[0] = g_motor[0], motor[1] = g_motor[1];
        ReleaseSRWLockShared(&g_lock);
        float lo = motor[0] / 65535.0f * S.strength, hi = motor[1] / 65535.0f * S.strength;
        int driving = car_driving(now) && fh2_foreground();
        if (driving && S.carFeel > 0) {
            hi = fmaxf(hi, S.carFeel * 0.5f * shift_knock(now));
            if (Car.rpm >= 0.975f && g_xpadRT > 128 && ((int)(now * 16) & 1))
                hi = fmaxf(hi, S.carFeel * 0.4f);
        }
        impulse_make(now, trig);
        /* the trigger motors without hard edges: up in 20 ms, down in 60 ms */
        float dt = lastTick ? (float)(now - lastTick) : 0.01f;
        lastTick = now;
        for (int i = 0; i < 2; i++)
            soft[i] = follow(soft[i], trig[i], dt > 0.1f ? 0.1f : dt, 0.02f, 0.06f);
        v.leftMotor = clampf(lo, 0, 1);
        v.rightMotor = clampf(hi, 0, 1);
        v.leftTrigger = soft[0] < 0.01f ? 0 : clampf(soft[0] * X_S.strength, 0, 1);
        v.rightTrigger = soft[1] < 0.01f ? 0 : clampf(soft[1] * X_S.strength, 0, 1);
        if (npads && (memcmp(&v, &last, sizeof v) || now - lastSent > 1.0)) {
            for (UINT i = 0; i < npads; i++)
                pads[i]->v->put_Vibration(pads[i], v);
            last = v;
            lastSent = now;
        }
        WaitForSingleObject(g_xwake, npads ? (driving ? 10 : 30) : 250);
    }
}

static void xbox_install(void)
{
    X_S.on = fh2_ini_int(L"ImpulseTriggers", L"Enabled", 1);
    X_S.strength = clampi(fh2_ini_int(L"ImpulseTriggers", L"Strength", 100), 0, 100) / 100.0f;
    if (!X_S.on) {
        fh2_say("xbox: impulse triggers off ([ImpulseTriggers] Enabled=0) -- the rumble goes through XInput");
        return;
    }
    g_xwake = CreateEventW(NULL, FALSE, FALSE, NULL);
    HANDLE h = CreateThread(NULL, 0, xbox_thread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
    fh2_say("xbox: impulse triggers on, strength %d%%", (int)(X_S.strength * 100 + 0.5f));
}
