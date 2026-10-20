/* fx.h -- short haptic effects, as the PlayStation 5's own: the menus' ticks and clicks (from the game's interface
   sounds, sounds.h), the game's events (skills, the tombola's drum, landings, crashes, gear changes), the tests on the
   settings page (Settings > Controls). A voice is a few milliseconds to a few seconds of a waveform made from its time alone (no state, so
   any thread can render it at any moment):
   - HD haptics (USB): the stream renders every voice sample by sample on the controller's two actuators;
   - rumble emulation (Bluetooth, or HD haptics off): the controller thread turns the voices' envelopes into the
     motors' levels (a tick: a 35 ms pulse of the light motor, a thump: the heavy one). */
#pragma once

enum { FX_TICK, FX_THUMP, FX_BUZZ, FX_SWEEP, FX_NOISE, FX_RATCHET, FX_TOMBOLA };

typedef struct {
    int kind;
    double at;   /* when it starts (fh2_now) */
    float len;   /* how long it lasts from then, s */
    float amp;   /* 0..1 (the strengths already in it) */
    float hz;    /* its tone */
    float hz1;   /* sweeps: the tone at the end; buzzes: a beat (0 = none); ratchets: ticks a second */
    float decay; /* ticks, thumps, noise: how fast it fades, s */
    float pan;   /* -1 the left grip .. 0 both .. 1 the right grip */
    UINT32 id;   /* a loop: the game's sound that stops it (sounds.h), else 0 */
    UINT32 seed;
    int started; /* the HD haptics stream has begun it */
} FxVoice;

#define FX_VOICES 64
#define FX_TAU 6.2831853f
#define FX_PI 3.14159265f
static FxVoice g_fx[FX_VOICES];
static SRWLOCK g_fxLock = SRWLOCK_INIT;
static volatile double g_fxUntil; /* the last voice ends then: till then the controller thread sends more often */
static UINT32 g_fxSeed = 0x9E3779B9u;

/* -1..1 from an integer */
static float fx_hash(UINT32 x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return (float)(x & 0xFFFFFF) / 8388607.5f - 1.0f;
}

/* smooth noise: random values `rate` times a second, eased between (its energy below about `rate` Hz) */
static float fx_vnoise(double t, float rate, UINT32 seed)
{
    double p = t * rate, i = floor(p);
    float f = (float)(p - i);
    UINT32 k = (UINT32)(INT64)i;
    f = f * f * (3 - 2 * f);
    return fx_hash(k * 2654435761u + seed) * (1 - f) + fx_hash((k + 1) * 2654435761u + seed) * f;
}

/* a tone struck at t = 0 and fading: the actuator's tick (high, a few ms) or thump (low, tens of ms) */
static float fx_ping(float t, float hz, float decay)
{
    if (t < 0 || t > decay * 8)
        return 0;
    return (t < 0.0004f ? t / 0.0004f : 1.0f) * expf(-t / decay) * sinf(FX_TAU * hz * t);
}

/* in and out over `ramp` seconds */
static float fx_edge(float t, float len, float ramp)
{
    float e = 1;
    if (t < ramp)
        e = t / ramp;
    if (len - t < ramp)
        e = fminf(e, (len - t) / ramp);
    return e < 0 ? 0 : e;
}

/* The tombola (A_Tombola.xaml): its drum of prizes moves 40040 px in 7 s, slowing down (PowerEase, EaseOut, power 2),
   then bounces (7.07, 7.14, 7.24 s); its spin sound starts 0.7 s in. One tick for every prize passing (about 500 px
   apart): about 20 a second at first, the last ones far apart, a clunk as it stops. */
#define TOMBOLA_TRAVEL 40040.0f
#define TOMBOLA_PITCH 500.0f
#define TOMBOLA_SPIN 7.0f
#define TOMBOLA_SOUND_AT 0.7f

/* the drum's time of the last prize passing before drum time d (-1: none yet) */
static float tombola_last_tick(float d)
{
    if (d <= 0)
        return -1;
    if (d > TOMBOLA_SPIN)
        d = TOMBOLA_SPIN;
    float u = 1 - d / TOMBOLA_SPIN;
    int k = (int)(TOMBOLA_TRAVEL * (1 - u * u) / TOMBOLA_PITCH);
    if (k <= 0)
        return -1;
    float r = 1 - k * TOMBOLA_PITCH / TOMBOLA_TRAVEL;
    return TOMBOLA_SPIN * (1 - sqrtf(r > 0 ? r : 0));
}

/* the voice's waveform t seconds in */
static float fx_wave(const FxVoice *v, float t)
{
    switch (v->kind) {
    case FX_TICK:
    case FX_THUMP:
        return fx_ping(t, v->hz, v->decay);
    case FX_BUZZ: {
        float s = sinf(FX_TAU * v->hz * t);
        if (v->hz1 > 0)
            s *= 0.55f + 0.45f * sinf(FX_TAU * v->hz1 * t);
        return s * fx_edge(t, v->len, 0.006f);
    }
    case FX_SWEEP: {
        float ph = FX_TAU * (v->hz * t + (v->hz1 - v->hz) * t * t / (2 * v->len));
        return sinf(ph) * sinf(FX_PI * t / v->len);
    }
    case FX_NOISE:
        return fx_vnoise(t, v->hz, v->seed) * (t < 0.002f ? t / 0.002f : 1.0f) * expf(-t / v->decay);
    case FX_RATCHET: {
        float p = 1 / v->hz1;
        return fx_ping(fmodf(t, p), v->hz, v->decay) * (t > v->len - 0.05f ? fx_edge(t, v->len, 0.05f) : 1.0f);
    }
    case FX_TOMBOLA: {
        float d = t + TOMBOLA_SOUND_AT, s = 0;
        if (d < TOMBOLA_SPIN + 0.05f) {
            float tk = tombola_last_tick(d);
            if (tk >= 0) /* the ticks a little crisper as it slows down */
                s = fx_ping(d - tk, v->hz, v->decay) * (0.8f + 0.35f * d / TOMBOLA_SPIN);
        }
        /* it stops: a clunk, then the bounce */
        s += 1.3f * fx_ping(d - TOMBOLA_SPIN, 58, 0.045f) + 0.55f * fx_ping(d - 7.07f, 150, 0.01f) +
             0.35f * fx_ping(d - 7.14f, 150, 0.008f) + 0.25f * fx_ping(d - 7.24f, 150, 0.008f);
        return s;
    }
    }
    return 0;
}

/* the voice's envelope t seconds in, for the rumble emulation: 0..1, and which motor (1 = the heavy one) */
static float fx_env(const FxVoice *v, float t, int *heavy)
{
    *heavy = v->hz < 100;
    switch (v->kind) {
    case FX_TICK:
        return t < 0.035f ? 1.0f : 0.0f; /* the motors need a pulse this long to be felt */
    case FX_THUMP:
        return expf(-t / fmaxf(v->decay, 0.05f));
    case FX_NOISE:
        return expf(-t / fmaxf(v->decay, 0.04f));
    case FX_BUZZ:
        return v->hz1 > 0 ? (sinf(FX_TAU * v->hz1 * t) > -0.3f ? 1.0f : 0.25f) : 1.0f;
    case FX_SWEEP:
        return sinf(FX_PI * t / v->len);
    case FX_RATCHET: {
        float p = 1 / v->hz1;
        *heavy = 0;
        return p < 0.07f ? 0.55f : (fmodf(t, p) < 0.03f ? 1.0f : 0.0f);
    }
    case FX_TOMBOLA: {
        float d = t + TOMBOLA_SOUND_AT, tk;
        *heavy = 0;
        if (d >= TOMBOLA_SPIN) {
            *heavy = 1;
            return d < TOMBOLA_SPIN + 0.12f ? 1.0f : 0.0f;
        }
        tk = tombola_last_tick(d);
        return tk >= 0 && d - tk < 0.03f ? 1.0f : 0.0f;
    }
    }
    return 0;
}

/* a voice (from any thread): `delay` s from now. The game's vibration strength scales it; nothing without the
   controller. Ticks and thumps last until they have faded. */
static void fx_add(int kind, float delay, float amp, float hz, float hz1, float len, float decay, float pan, UINT32 id)
{
    FxVoice v;
    double now = fh2_now(), end;
    int k = 0;
    amp *= S.strength;
    if (amp < 0.01f || !g_connected || !S.haptics)
        return;
    if (amp > 1.5f)
        amp = 1.5f;
    ZeroMemory(&v, sizeof v);
    v.kind = kind, v.at = now + delay, v.amp = amp, v.hz = hz, v.hz1 = hz1, v.decay = decay, v.pan = pan, v.id = id;
    v.len = len;
    if (kind == FX_TICK || kind == FX_THUMP)
        v.len = fmaxf(decay * 8, 0.04f);
    else if (kind == FX_NOISE)
        v.len = fmaxf(len, decay * 6);
    if (v.len <= 0 || (kind == FX_RATCHET && hz1 <= 0))
        return;
    end = v.at + v.len;
    AcquireSRWLockExclusive(&g_fxLock);
    v.seed = g_fxSeed = g_fxSeed * 1664525u + 1013904223u;
    /* a free place, or the one ending first */
    for (int i = 1; i < FX_VOICES; i++)
        if (g_fx[i].at + g_fx[i].len < g_fx[k].at + g_fx[k].len)
            k = i;
    g_fx[k] = v;
    if (end > g_fxUntil)
        g_fxUntil = end;
    ReleaseSRWLockExclusive(&g_fxLock);
    if (g_wake)
        SetEvent(g_wake);
}

/* the game stopped a loop (a counter ticking up): its voices end now */
static void fx_stop(UINT32 id)
{
    double now = fh2_now();
    if (!id)
        return;
    AcquireSRWLockExclusive(&g_fxLock);
    for (int i = 0; i < FX_VOICES; i++)
        if (g_fx[i].id == id && g_fx[i].at + g_fx[i].len > now)
            g_fx[i].len = (float)fmax(now - g_fx[i].at, 0.0);
    ReleaseSRWLockExclusive(&g_fxLock);
}

/* all voices off (the controller went, the settings changed) */
static void fx_clear(void)
{
    AcquireSRWLockExclusive(&g_fxLock);
    ZeroMemory(g_fx, sizeof g_fx);
    g_fxUntil = 0;
    ReleaseSRWLockExclusive(&g_fxLock);
}

static int fx_busy(double now) { return now < g_fxUntil; }

/* the voices sounding in [t0, t1). stream: the HD haptics' next samples, which play a little later than now (what
   is queued plays first) -- a voice that should have begun before them begins with them instead, whole (a tick lasts
   a few ms: cut, it would be lost) */
static int fx_snapshot(FxVoice *out, double t0, double t1, int stream)
{
    int n = 0;
    if (stream) {
        AcquireSRWLockExclusive(&g_fxLock);
        for (int i = 0; i < FX_VOICES; i++) {
            FxVoice *v = &g_fx[i];
            if (v->len <= 0 || v->started || v->at >= t1)
                continue;
            if (v->at < t0 && t0 - v->at < 0.15) {
                v->at = t0;
                if (t0 + v->len > g_fxUntil)
                    g_fxUntil = t0 + v->len;
            }
            v->started = 1;
        }
        ReleaseSRWLockExclusive(&g_fxLock);
    }
    AcquireSRWLockShared(&g_fxLock);
    for (int i = 0; i < FX_VOICES; i++)
        if (g_fx[i].len > 0 && g_fx[i].at < t1 && g_fx[i].at + g_fx[i].len > t0)
            out[n++] = g_fx[i];
    ReleaseSRWLockShared(&g_fxLock);
    return n;
}

/* HD haptics: the voices added into the two grips' samples; t0 is when the first sample plays */
static void fx_render(float *left, float *right, UINT n, double t0, double dt)
{
    FxVoice v[FX_VOICES];
    int count = fx_snapshot(v, t0, t0 + n * dt, 1);
    for (int i = 0; i < count; i++) {
        float gl = v[i].pan > 0 ? 1 - v[i].pan : 1, gr = v[i].pan < 0 ? 1 + v[i].pan : 1;
        for (UINT f = 0; f < n; f++) {
            float t = (float)(t0 + f * dt - v[i].at);
            if (t < 0 || t >= v[i].len)
                continue;
            float s = fx_wave(&v[i], t) * v[i].amp;
            left[f] += s * gl;
            right[f] += s * gr;
        }
    }
}

/* the rumble emulation: the voices' levels on the motors now (the largest of each) */
static void fx_rumble(double now, float *lo, float *hi)
{
    FxVoice v[FX_VOICES];
    int count = fx_snapshot(v, now, now + 0.001, 0);
    for (int i = 0; i < count; i++) {
        int heavy;
        float t = (float)(now - v[i].at), e;
        if (t < 0 || t >= v[i].len)
            continue;
        e = fx_env(&v[i], t, &heavy) * v[i].amp;
        if (v[i].kind == FX_TICK || v[i].kind == FX_RATCHET || v[i].kind == FX_TOMBOLA)
            e = fminf(e * 1.5f, 1.0f); /* a motor's short pulse is felt less than the actuator's tick */
        if (heavy)
            *lo = fmaxf(*lo, e);
        else
            *hi = fmaxf(*hi, e);
    }
}
