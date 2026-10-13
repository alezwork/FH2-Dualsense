/* car.h -- what the car does, from the game's own controller feedback code.
   Each frame the player's car (FH2 1.0.0.64: 0x2FD740) fills in what its controller's rumble and impulse triggers
   are made from, and hands it over through two small thunks:
     0x10768A0  add rcx,0x10D0 / jmp haptics                       (controller, dt, &{spin slip, brake slip, bumps})
     0x10768B0  r9b = (BYTE)(controller[6]-2) <= 1 / add rcx,0x2E0 / jmp rumble    (controller, dt, &CarIn)
   The slips are normalised by the game: 1 = the tyre's grip limit, more = sliding / locking / spinning (the trigger
   curves of media\ControllerHaptics.xml start at 1.5). The engine revs: 0 at idle, 1 at the redline. The thunks'
   first 12 bytes become a jump to ours, which keeps the values and calls the game's function itself. Found by their
   bytes (the rumble thunk is unique, the haptics one 16 bytes before it). Without them (another build of the game):
   the triggers use the game's impulse trigger levels as before. */
#pragma once

typedef struct {
    float rpm;            /* 0x00 engine revs, 0 idle .. 1 redline */
    float gearRpm;        /* 0x04 */
    float hitCar;         /* 0x08 collision impulses (this frame) */
    float hitWorld;       /* 0x0C */
    float hitObject;      /* 0x10 */
    float slip;           /* 0x14 tyres' slip, weighted by load (1 = the grip limit) */
    float slipFront;      /* 0x18 */
    float slipRear;       /* 0x1C */
    float air;            /* 0x20 */
    float bump;           /* 0x24 surface bumpiness x speed (0 standing .. full from 50 mph) */
    float extra[2];       /* 0x28 rumble added as it is */
    BYTE sputter, shift;  /* 0x30, 0x31: the engine sputters; a gear change is going on */
    BYTE backfire;        /* 0x32 */
} CarIn;

typedef void(__fastcall *CarRumbleFn)(void *rumble, float dt, const CarIn *in, BYTE wheel);
typedef void(__fastcall *CarHapticsFn)(void *haptics, float dt, const float *in);
static CarRumbleFn g_carRumble;
static CarHapticsFn g_carHaptics;

/* the latest values; written by the game's thread, read by ours (a torn read is harmless) */
static volatile struct {
    double at;                      /* fh2_now() of the last frame; 0 = never */
    float rpm, slipFront, slipRear, bump;
    float spin, brake;              /* the haptics' slips: the most spinning wheel, the front wheels braking */
    double shiftAt, hitAt;          /* the last gear change, the last crash */
    float hit;                      /* its strength 0..1 */
    int hooked;
} Car;

/* the log: what came in, to tune by (the most of each over 10 seconds of driving) */
static struct {
    double from;
    float rpm, spin, brake, front, rear, bump, car, world, object;
    int shifts, hits, frames;
} g_carLog;

static float maxf(float a, float b) { return a > b ? a : b; }

static void car_log(double now)
{
    g_carLog.frames++;
    if (!g_carLog.from) {
        g_carLog.from = now;
        return;
    }
    if (now - g_carLog.from < 10)
        return;
    fh2_say("car, the most in 10 s (%d frames): revs %.2f, spin %.2f, brake slip %.2f, slip front %.2f rear %.2f, "
            "bumps %.2f, hits car %.1f world %.1f object %.3f; %d gear changes, %d crashes",
            g_carLog.frames, g_carLog.rpm, g_carLog.spin, g_carLog.brake, g_carLog.front, g_carLog.rear, g_carLog.bump,
            g_carLog.car, g_carLog.world, g_carLog.object, g_carLog.shifts, g_carLog.hits);
    ZeroMemory(&g_carLog, sizeof g_carLog);
    g_carLog.from = now;
}

static void __fastcall hk_car_rumble(BYTE *ctrl, float dt, const CarIn *in)
{
    static BYTE shiftWas;
    double now = fh2_now();
    if (in) {
        if (!Car.at)
            fh2_say("car: the game's feedback data comes in (revs %.2f, slip %.2f)", in->rpm, in->slip);
        Car.rpm = in->rpm;
        Car.slipFront = in->slipFront;
        Car.slipRear = in->slipRear;
        Car.bump = in->bump;
        if (in->shift && !shiftWas) {
            Car.shiftAt = now;
            g_carLog.shifts++;
        }
        shiftWas = in->shift;
        /* a crash: the impulse scales of media\ControllerRumble.xml (car 1..500, world 20..375, object 0.01..0.03) */
        float hit = maxf(maxf((in->hitCar - 8) / 250, (in->hitWorld - 25) / 250), (in->hitObject - 0.012f) / 0.02f);
        if (hit > 0) {
            Car.hit = hit > 1 ? 1 : hit;
            Car.hitAt = now;
            g_carLog.hits++;
        }
        g_carLog.rpm = maxf(g_carLog.rpm, in->rpm);
        g_carLog.front = maxf(g_carLog.front, in->slipFront);
        g_carLog.rear = maxf(g_carLog.rear, in->slipRear);
        g_carLog.bump = maxf(g_carLog.bump, in->bump);
        g_carLog.car = maxf(g_carLog.car, in->hitCar);
        g_carLog.world = maxf(g_carLog.world, in->hitWorld);
        g_carLog.object = maxf(g_carLog.object, in->hitObject);
        Car.at = now;
        car_log(now);
        if (g_wake)
            SetEvent(g_wake);
    }
    g_carRumble(ctrl + 0x2E0, dt, in, (BYTE)(ctrl[6] - 2) <= 1);
}

static void __fastcall hk_car_haptics(BYTE *ctrl, float dt, const float *in)
{
    if (in) {
        Car.spin = in[0];
        Car.brake = in[1];
        g_carLog.spin = maxf(g_carLog.spin, in[0]);
        g_carLog.brake = maxf(g_carLog.brake, in[1]);
    }
    g_carHaptics(ctrl + 0x10D0, dt, in);
}

/* the thunk's first 12 bytes: mov rax, hook / jmp rax */
static int car_patch(BYTE *at, void *hook)
{
    DWORD old;
    if (!VirtualProtect(at, 12, PAGE_EXECUTE_READWRITE, &old))
        return 0;
    at[0] = 0x48, at[1] = 0xB8;
    *(void **)(at + 2) = hook;
    at[10] = 0xFF, at[11] = 0xE0;
    VirtualProtect(at, 12, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 12);
    return 1;
}

static int car_install(void)
{
    static const BYTE kRumble[20] = {0x0F, 0xB6, 0x41, 0x06, 0x2C, 0x02, 0x3C, 0x01, 0x41, 0x0F,
                                     0x96, 0xC1, 0x48, 0x81, 0xC1, 0xE0, 0x02, 0x00, 0x00, 0xE9};
    static const BYTE kHaptics[8] = {0x48, 0x81, 0xC1, 0xD0, 0x10, 0x00, 0x00, 0xE9};
    static const BYTE kRumbleFn[19] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x56, 0x48, 0x81,
                                       0xEC, 0xA0, 0x00, 0x00, 0x00, 0x80, 0x79, 0x0C, 0x00};
    static const BYTE kHapticsFn[13] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9, 0x48, 0x83, 0xC1, 0x10};
    GameImage g;
    BYTE *r = NULL;
    int n = 0;
    if (!game_image(&g))
        return 0;
    for (BYTE *p = g.text + 16; p + 24 <= g.textEnd; p++)
        if (*p == 0x0F && !memcmp(p, kRumble, sizeof kRumble))
            r = p, n++;
    BYTE *h = r ? r - 16 : NULL;
    BYTE *rf = r ? rel32(r + 19, 1, 5) : NULL, *hf = h ? rel32(h + 7, 1, 5) : NULL;
    if (n != 1 || memcmp(h, kHaptics, sizeof kHaptics) || rf < g.text || rf >= g.textEnd || hf < g.text ||
        hf >= g.textEnd || memcmp(rf, kRumbleFn, sizeof kRumbleFn) || memcmp(hf, kHapticsFn, sizeof kHapticsFn)) {
        fh2_say("car: the game's feedback code is not as expected (%d found) -- the triggers use the game's impulse "
                "trigger levels only", n);
        return 0;
    }
    g_carRumble = (CarRumbleFn)rf;
    g_carHaptics = (CarHapticsFn)hf;
    if (!car_patch(h, (void *)hk_car_haptics) || !car_patch(r, (void *)hk_car_rumble))
        return 0;
    Car.hooked = 1;
    fh2_say("car: the game's feedback data hooked (game+0x%lX, game+0x%lX)", (DWORD)(r - g.base), (DWORD)(h - g.base));
    return 1;
}

/* driving: the car's feedback came in a moment ago (the game skips it in its menus, paused, loading) */
static int car_driving(double now) { return Car.hooked && Car.at && now - Car.at < 0.35; }
