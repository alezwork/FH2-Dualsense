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
    float spin, brake;              /* the haptics' slips, smoothed: the most spinning wheel, the front wheels braking */
    double shiftAt, hitAt;          /* the last gear change, the last crash */
    float hit;                      /* the crash's strength 0..1 (it fades: car_hit) */
    int hooked;
} Car;

/* the log: what came in, to tune by (the most of each over 10 seconds of driving) */
static struct {
    double from;
    float rpm, spin, brake, front, rear, bump, car, world, object, hit;
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
            "bumps %.2f, hits car %.1f world %.1f object %.3f; %d gear changes, %d crashes (the hardest %.2f)",
            g_carLog.frames, g_carLog.rpm, g_carLog.spin, g_carLog.brake, g_carLog.front, g_carLog.rear, g_carLog.bump,
            g_carLog.car, g_carLog.world, g_carLog.object, g_carLog.shifts, g_carLog.hits, g_carLog.hit);
    ZeroMemory(&g_carLog, sizeof g_carLog);
    g_carLog.from = now;
}

/* A crash's strength 0..1 from its impulse, on a log scale between "felt" and "a big one" -- measured in the game:
   cars 10..400 (a nudge is ~20, a hard hit ~300), walls 25..375, objects 0.8..30 (fences, signs, bushes come every
   frame at 0.1..5, a pole ~20). It fades in 90 ms; a new one counts only when stronger than what is left of the last,
   so scraping along a wall or through bushes stays a light rumble, not a row of full jolts. */
#define CAR_HIT_FADE 0.09f

static float hit_level(float v, float v0, float v1) { return v <= v0 ? 0 : fminf(logf(v / v0) / logf(v1 / v0), 1); }

static float car_hit(double now) { return Car.hitAt ? Car.hit * expf(-(float)(now - Car.hitAt) / CAR_HIT_FADE) : 0; }

/* the game's own rumble for crashes (media\ControllerRumble.xml) wants impulses of 1 (cars), 1 (walls), 0.015
   (objects) for full rumble -- every brush with a bush shook the pad at full. Its curves (three points: x, y at +0,
   +4, +0x10, +0x14, +0x20, +0x24; in the controller's copy at +0x2F0: cars 0x100 / 0x140 left / right motor, walls
   0x210 / 0x250, objects 0x320 / 0x360) are set to the measured scale instead, at [Vibration] Collisions. */
static void car_tune(BYTE *ctrl)
{
    static const struct {
        DWORD off;
        float x0, x1, x2, y1, y2; /* y1, y2: of the left (heavy) motor; the right one gets 0.6 of it */
    } kCurves[] = {{0x100, 10, 80, 400, 0.45f, 1.0f}, {0x210, 25, 120, 375, 0.45f, 1.0f}, {0x320, 0.8f, 4, 30, 0.4f, 1.0f}};
    float *obj = (float *)(ctrl + 0x2F0 + 0x320);
    if (obj[0] == 0.8f && obj[8] == 30.0f) /* ours already */
        return;
    if (obj[0] != 0.01f || obj[8] != 0.03f) { /* not the game's own tuning: left as it is */
        static int told;
        if (!told++)
            fh2_say("car: the game's crash rumble is not as expected -- left as it is");
        return;
    }
    for (int i = 0; i < 3; i++)
        for (int side = 0; side < 2; side++) {
            float *c = (float *)(ctrl + 0x2F0 + kCurves[i].off + side * 0x40), k = S.collisions * (side ? 0.6f : 1.0f);
            c[0] = kCurves[i].x0, c[1] = 0;
            c[4] = kCurves[i].x1, c[5] = kCurves[i].y1 * k;
            c[8] = kCurves[i].x2, c[9] = kCurves[i].y2 * k;
        }
    static int said;
    if (!said++)
        fh2_say("car: the game's crash rumble set to the real impulse scale, %d%%", (int)(S.collisions * 100 + 0.5f));
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
        float hit = fmaxf(fmaxf(hit_level(in->hitCar, 10, 400), hit_level(in->hitWorld, 25, 375)),
                          hit_level(in->hitObject, 0.8f, 30));
        hit = powf(hit, 1.3f); /* the small ones softer still */
        if (hit > 0.05f && hit > car_hit(now) * 1.15f) {
            Car.hit = hit;
            Car.hitAt = now;
            g_carLog.hits++;
            g_carLog.hit = maxf(g_carLog.hit, hit);
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
        car_tune(ctrl);
        if (g_wake)
            SetEvent(g_wake);
    }
    g_carRumble(ctrl + 0x2E0, dt, in, (BYTE)(ctrl[6] - 2) <= 1);
}

/* the slips, smoothed: up in 50 ms, down in 150 ms -- one frame's spike (a kerb, a landing) is not a lock-up */
static float follow(float v, float target, float dt, float up, float down)
{
    float tau = target > v ? up : down;
    return v + (target - v) * (1 - expf(-dt / tau));
}

static void __fastcall hk_car_haptics(BYTE *ctrl, float dt, const float *in)
{
    if (in) {
        float d = dt > 0 && dt < 0.2f ? dt : 0.016f;
        Car.spin = follow(Car.spin, in[0], d, 0.05f, 0.15f);
        Car.brake = follow(Car.brake, in[1], d, 0.05f, 0.15f);
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
