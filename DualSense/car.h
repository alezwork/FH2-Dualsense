/* car.h -- what the car does, from the game's own controller feedback code.
   Each frame the player's car (FH2 1.0.0.64: 0x2FD740) fills in what its controller's rumble and impulse triggers
   are made from, and hands it over through two small thunks:
     0x10768A0  add rcx,0x10D0 / jmp haptics                       (controller, dt, &{spin slip, brake slip, bumps})
     0x10768B0  r9b = (BYTE)(controller[6]-2) <= 1 / add rcx,0x2E0 / jmp rumble    (controller, dt, &CarIn)
   The slips are normalised by the game: 1 = the tyre's grip limit, more = sliding / locking / spinning (the trigger
   curves of media\ControllerHaptics.xml start at 1.5). The engine revs: 0 at idle, 1 at the redline. Both thunks
   are hooked through the platform (hook_function, once Extra Options has patched the game): ours keeps the values
   and goes on into the thunk. Found by their bytes (the rumble thunk is unique, the haptics one 16 bytes before it).
   Without them: the triggers use the game's impulse trigger levels (wgi.h).
   The road under each wheel: the same code goes over the four wheels (0, 1 front left and right, 2, 3 rear) for
   their surfaces' bumpiness -- a wheel's contact (or none: in the air) and in it the surface's number in the game's
   table of surfaces (media\physics\surfaceTypes.xml: records of 0x1F0 bytes, the name first). An instruction hook
   there (API 7) reads the number of every wheel on the ground and the car's speed (m/s, in xmm12), so the vibration
   and the triggers can tell gravel from grass from a kerb, and a jump from a landing.
   ABS: the game's assist is not read from the game -- the brakes show it: a front wheel locking under firm braking
   means no ABS; hard stops at the grip limit without locking mean ABS (abs_learn). */
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

typedef void(__fastcall *CarRumbleFn)(BYTE *ctrl, float dt, const CarIn *in);
typedef void(__fastcall *CarHapticsFn)(BYTE *ctrl, float dt, const float *in);
static CarRumbleFn g_carRumble;   /* the thunks, as the platform hands them on */
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

/* the road under the wheels (surface_commit, each frame) */
enum { SURF_NONE = -1, SURF_TARMAC, SURF_KERB, SURF_GRAVEL, SURF_DIRT, SURF_GRASS, SURF_SAND, SURF_SNOW, SURF_COBBLE,
       SURF_METAL, SURF_BUMPY, SURF_KINDS };
static const char *const kSurfName[SURF_KINDS] = {"tarmac", "kerb", "gravel",  "dirt",  "grass",
                                                  "sand",   "snow", "cobbles", "metal", "bumpy"};
static volatile struct {
    signed char cls[4]; /* each wheel's surface, SURF_NONE: in the air */
    float speed;        /* m/s */
    double at;          /* fh2_now() of the frame */
    int air;            /* all four wheels off the ground */
    int lifted;         /* this flight's take-off felt */
    double airSince, landAt;
    float land;         /* the landing's strength 0..1 */
    int hooked;
} Surf;
static volatile struct {
    BYTE idx[4], seen; /* this frame: the wheels on the ground (bits) and their surfaces' numbers */
    float speed;
} g_wheelsNow;
static BYTE *g_surfGlobal; /* the game's global: its table of surfaces (a std::vector of records at +8) */

/* ABS, as the brakes show it (abs_learn) */
static struct {
    int on;         /* presumed on (the game's default) until a wheel locks */
    float lockT;    /* how long a front wheel has been locked */
    float limitT;   /* this stop: how long at the grip limit */
    int lockedNow;  /* this stop: a wheel locked */
    int cleanStops; /* hard stops at the limit without locking, since the last lock */
} g_abs = {1};

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

/* a surface's kind from its name (surfaceTypes.xml) */
static int surf_classify(const char *n)
{
#define SURF_IS(p) (!strncmp(n, p, sizeof(p) - 1))
    if (SURF_IS("RumbleStrip") || SURF_IS("SpeedBump"))
        return SURF_KERB;
    if (SURF_IS("Gravel") || SURF_IS("Rally_Gravel"))
        return SURF_GRAVEL;
    if (SURF_IS("SandyDirt") || SURF_IS("Dirt") || SURF_IS("VineyardDirt") || SURF_IS("Quicksand") ||
        SURF_IS("Rally_Dirt") || SURF_IS("Rally_Ash"))
        return SURF_DIRT;
    if (SURF_IS("Sand"))
        return SURF_SAND;
    if (SURF_IS("Grasscrete") || SURF_IS("Latticecrete") || SURF_IS("Cobblestone") || SURF_IS("Brick"))
        return SURF_COBBLE;
    if (strstr(n, "Grass") || SURF_IS("Astroturf") || SURF_IS("CornCrop"))
        return SURF_GRASS;
    if (SURF_IS("Snow"))
        return SURF_SNOW;
    if (SURF_IS("MetalGrate") || SURF_IS("MetalPlateBumpy"))
        return SURF_METAL;
    if (SURF_IS("BumpyAsphalt") || SURF_IS("BumpyConcrete") || SURF_IS("TarPatch") || SURF_IS("TungstenAsphaltHeavy"))
        return SURF_BUMPY;
#undef SURF_IS
    return SURF_TARMAC;
}

/* the kind of the game's surface number idx (once per number; the log says which is which) */
static int surf_kind(BYTE idx)
{
    static signed char kind[256];
    static BYTE *cached;
    BYTE *t = g_surfGlobal ? *(BYTE **)g_surfGlobal : NULL, *b, *e;
    int k = SURF_TARMAC;
    if (!t)
        return SURF_TARMAC;
    b = *(BYTE **)(t + 8), e = *(BYTE **)(t + 0x10);
    if (b != cached) { /* the table (re)loaded */
        memset(kind, 0x7F, sizeof kind);
        cached = b;
    }
    if (kind[idx] != 0x7F)
        return kind[idx];
    if (b && e > b && (SIZE_T)(e - b) / 0x1F0 > idx) {
        const char *n = (const char *)(b + (SIZE_T)idx * 0x1F0);
        int ok = n[0] != 0;
        for (int i = 0; i < 0x40 && n[i]; i++)
            if (n[i] < 32 || n[i] > 126)
                ok = 0;
        if (ok) {
            k = surf_classify(n);
            fh2_say("surface %u: %.63s -> %s", idx, n, kSurfName[k]);
        }
    }
    kind[idx] = (signed char)k;
    return k;
}

/* the instruction hook, for each wheel on the ground: rbx = the wheel, rax = its contact, [rax+1] its surface */
static void hk_wheel_surface(FH2Context *c, void *user)
{
    unsigned w = (unsigned)c->rbx;
    const BYTE *contact = (const BYTE *)c->rax;
    (void)user;
    if (w < 4 && contact) {
        g_wheelsNow.idx[w] = contact[1];
        g_wheelsNow.seen |= (BYTE)(1u << w);
    }
    g_wheelsNow.speed = c->xmm[12].f[0];
}

/* how much the road is felt at this speed, 0..1 (full from 90 km/h) */
static float surf_speed_level(float speed) { return speed < 1.5f ? 0 : powf(fminf((speed - 1.5f) / 23.5f, 1.0f), 0.6f); }

/* the frame's wheels: their surfaces; a jump, a landing */
static void surface_commit(double now)
{
    BYTE seen = g_wheelsNow.seen;
    int on = 0;
    g_wheelsNow.seen = 0;
    for (int w = 0; w < 4; w++) {
        Surf.cls[w] = (signed char)((seen >> w & 1) ? surf_kind(g_wheelsNow.idx[w]) : SURF_NONE);
        on += seen >> w & 1;
    }
    Surf.speed = g_wheelsNow.speed;
    if (!on && Surf.speed > 3) {
        if (!Surf.air)
            Surf.air = 1, Surf.lifted = 0, Surf.airSince = now;
        else if (!Surf.lifted && now - Surf.airSince > 0.12) { /* a jump, not a crest: the road falls away */
            Surf.lifted = 1;
            fx_add(FX_SWEEP, 0, 0.22f * S.events, 110, 45, 0.16f, 0, 0, 0);
        }
    } else if (on >= 2 && Surf.air) {
        float t = (float)(now - Surf.airSince);
        Surf.air = 0;
        if (t > 0.25f && t < 8) { /* a landing: the longer the flight, the harder */
            float k = fminf(0.3f + 0.3f * t, 1.0f) * (0.6f + 0.4f * surf_speed_level(Surf.speed));
            Surf.land = k, Surf.landAt = now;
            fx_add(FX_THUMP, 0, 0.95f * k * S.events, 42, 0, 0, 0.11f, 0, 0);
            fx_add(FX_NOISE, 0, 0.4f * k * S.events, 240, 0, 0, 0.03f, 0, 0);
        }
    } else if (on)
        Surf.air = 0;
    Surf.at = now;
}

/* the surface under one side (0 left: wheels 0, 2; 1 right: 1, 3): a kerb under either, else the front wheel's */
static int surf_side(int side)
{
    int a = Surf.cls[side], b = Surf.cls[side + 2];
    if (a == SURF_KERB || b == SURF_KERB)
        return SURF_KERB;
    return a >= 0 ? a : b;
}

/* the surface the car is on (the triggers): a kerb under any wheel, else the rougher side's */
static int surf_main(void)
{
    int l = surf_side(0), r = surf_side(1);
    if (l == SURF_KERB || r == SURF_KERB)
        return SURF_KERB;
    return l > r ? l : r;
}

/* ABS from the brakes (the game's thread, each frame): raw = the front tyres' braking slip */
static void abs_learn(float raw, float dt)
{
    int l2 = g_dsPad.bLeftTrigger;
    if (l2 > 150 && raw >= 3.6f) {
        g_abs.lockT += dt;
        if (g_abs.lockT > 0.15f) {
            g_abs.lockedNow = 1;
            g_abs.cleanStops = 0;
            if (g_abs.on) {
                g_abs.on = 0;
                fh2_say("brakes: a front wheel locked -- no ABS: the brake trigger shows lock-ups");
            }
        }
    } else
        g_abs.lockT = 0;
    if (l2 > 200 && raw >= 1.1f && raw < 3.0f)
        g_abs.limitT += dt;
    if (l2 < 60) { /* a stop is over */
        if (g_abs.limitT > 0.8f && !g_abs.lockedNow && !g_abs.on && ++g_abs.cleanStops >= 3) {
            g_abs.on = 1;
            fh2_say("brakes: three hard stops at the grip limit without locking -- ABS: the brake trigger chatters");
        }
        g_abs.limitT = 0;
        g_abs.lockedNow = 0;
    }
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
            fx_add(FX_THUMP, 0, 0.5f * S.carFeel, 85, 0, 0, 0.03f, 0, 0); /* the gearbox's knock */
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
            if (hit > 0.15f) { /* the impact itself, sharp, with the game's rumble */
                fx_add(FX_THUMP, 0, 1.5f * hit * S.collisions, 55, 0, 0, 0.08f, 0, 0);
                fx_add(FX_NOISE, 0, 0.9f * hit * S.collisions, 320, 0, 0, 0.04f, 0, 0);
            }
        }
        g_carLog.rpm = maxf(g_carLog.rpm, in->rpm);
        g_carLog.front = maxf(g_carLog.front, in->slipFront);
        g_carLog.rear = maxf(g_carLog.rear, in->slipRear);
        g_carLog.bump = maxf(g_carLog.bump, in->bump);
        g_carLog.car = maxf(g_carLog.car, in->hitCar);
        g_carLog.world = maxf(g_carLog.world, in->hitWorld);
        g_carLog.object = maxf(g_carLog.object, in->hitObject);
        Car.at = now;
        if (Surf.hooked)
            surface_commit(now);
        car_log(now);
        car_tune(ctrl);
        if (g_wake)
            SetEvent(g_wake);
    }
    g_carRumble(ctrl, dt, in);
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
        abs_learn(in[1], d);
        g_carLog.spin = maxf(g_carLog.spin, in[0]);
        g_carLog.brake = maxf(g_carLog.brake, in[1]);
    }
    g_carHaptics(ctrl, dt, in);
}

/* at the game's code (base: from wait_for_code): both thunks through the platform */
static int car_install(BYTE *base)
{
    static const BYTE kRumble[20] = {0x0F, 0xB6, 0x41, 0x06, 0x2C, 0x02, 0x3C, 0x01, 0x41, 0x0F,
                                     0x96, 0xC1, 0x48, 0x81, 0xC1, 0xE0, 0x02, 0x00, 0x00, 0xE9};
    static const BYTE kHaptics[8] = {0x48, 0x81, 0xC1, 0xD0, 0x10, 0x00, 0x00, 0xE9};
    static const BYTE kRumbleFn[19] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x56, 0x48, 0x81,
                                       0xEC, 0xA0, 0x00, 0x00, 0x00, 0x80, 0x79, 0x0C, 0x00};
    static const BYTE kHapticsFn[13] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9, 0x48, 0x83, 0xC1, 0x10};
    Image g;
    BYTE *r = NULL;
    int n = 0;
    if (!image_of(base, &g))
        return 0;
    for (BYTE *p = g.text + 16; p + 24 <= g.textEnd; p++)
        if (*p == 0x0F && !memcmp(p, kRumble, sizeof kRumble))
            r = p, n++;
    BYTE *h = r ? r - 16 : NULL;
    BYTE *rf = r ? rel32(r + 19, 1, 5) : NULL, *hf = h ? rel32(h + 7, 1, 5) : NULL;
    if (n != 1 || memcmp(h, kHaptics, sizeof kHaptics) || !in_range(rf, g.text, g.textEnd) ||
        !in_range(hf, g.text, g.textEnd) || memcmp(rf, kRumbleFn, sizeof kRumbleFn) || memcmp(hf, kHapticsFn, sizeof kHapticsFn)) {
        ds_note("car: the game's feedback code is not as expected (%d found) -- the triggers use the game's impulse "
                "trigger levels only", n);
        return 0;
    }
    FH2Hook *a = g_api->hook_function(g_mod, "car feedback: rumble thunk", r, (void *)hk_car_rumble, (void **)&g_carRumble);
    FH2Hook *b = g_api->hook_function(g_mod, "car feedback: haptics thunk", h, (void *)hk_car_haptics, (void **)&g_carHaptics);
    if (!a || !b || !g_api->enable_hooks(g_mod, a) || !g_api->enable_hooks(g_mod, b)) {
        ds_note("car: the platform did not hook the game's feedback code (see its log) -- the triggers use the game's "
                "impulse trigger levels only");
        if (a)
            g_api->disable_hooks(g_mod, a);
        if (b)
            g_api->disable_hooks(g_mod, b);
        return 0;
    }
    Car.hooked = 1;
    fh2_say("car: the game's feedback data hooked (game+0x%lX, game+0x%lX)", (DWORD)(r - g.base), (DWORD)(h - g.base));
    return 1;
}

/* the wheels' surfaces: an instruction hook in the car's feedback code (API 7), where it reads a wheel's surface */
static int surface_install(BYTE *base)
{
    static const char kLoop[] = "FF 90 E0 04 00 00 48 85 C0 74 ?? 0F B6 50 01 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? C5 F8 2F "
                                "45 94";
    static const BYTE kRumbleOf[] = {0x8B, 0xC2, 0x48, 0x69, 0xD0, 0xF0, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x41, 0x08};
    Image g;
    int n = 0;
    BYTE *p, *fn;
    FH2Hook *h;
    if (g_api->api_version < 7 || !Car.hooked) {
        ds_note("surfaces: %s -- no road surfaces", Car.hooked ? "the platform is too old (2.6.0 or newer)" : "no car data");
        return 0;
    }
    if (!image_of(base, &g))
        return 0;
    p = find_unique(g.text, g.textEnd, kLoop, &n);
    fn = p ? rel32(p + 22, 1, 5) : NULL;
    if (!p || !in_range(fn, g.text, g.textEnd) || memcmp(fn, kRumbleOf, sizeof kRumbleOf)) {
        ds_note("surfaces: the game's wheel code is not as expected (%d found) -- no road surfaces", n);
        return 0;
    }
    g_surfGlobal = rel32(p + 15, 3, 7);
    if (!in_range(g_surfGlobal, g.data, g.dataEnd)) {
        ds_note("surfaces: the game's surface table is not where expected -- no road surfaces");
        return 0;
    }
    h = g_api->hook_instruction(g_mod, "car feedback: a wheel's surface", p + 11, hk_wheel_surface, NULL);
    if (!h || !g_api->enable_hooks(g_mod, h)) {
        if (h)
            g_api->disable_hooks(g_mod, h);
        ds_note("surfaces: the platform did not hook the game's wheel code (see its log) -- no road surfaces");
        return 0;
    }
    Surf.hooked = 1;
    fh2_say("surfaces: the wheels' surfaces hooked (game+0x%lX, the table at game+0x%lX)", (DWORD)(p + 11 - g.base),
            (DWORD)(g_surfGlobal - g.base));
    return 1;
}

/* driving: the car's feedback came in a moment ago (the game skips it in its menus, paused, loading) */
static int car_driving(double now) { return Car.hooked && Car.at && now - Car.at < 0.35; }
