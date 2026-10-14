/* camera.h -- the cockpit camera alive. The game moves its cameras by "camera physics" (media\Camera.zip,
   CameraPhysics.xml): per camera, layers that take an input (g-forces, slips, crashes, the bumps under each wheel,
   speed, landings, Kinect head tracking ...), map it through a curve (and noise, and a spring) and move or turn the
   camera by it (metres, degrees). The driver's camera (DriverCam, 64 layers) has its sideways g-force layers switched
   off, only a light dip under braking and no look into turns. At the game's start Camera.zip is made again
   (icons.h) with a DriverCam that looks into turns and leans and sways with the sideways g-forces (on layers that
   were off), dips deeper under braking and sinks back under throttle, rides the bumps harder and shakes a little at
   speed: [Camera] in DualSense.ini. Every layer is checked by its input before it is changed; anything unexpected and
   the game's own camera stays. Positive g sideways is taken as a right turn, positive yaw as a look to the right. */
#pragma once

static struct {
    int on;
    float look;  /* degrees of yaw at 1 g sideways */
    float lean;  /* 0..2: the roll and the sway */
    float pitch; /* 0..2: added to the game's dip and push back (1 = twice the game's) */
    float bumps; /* 0..3: of the game's */
    float shake; /* 0..2 */
} Cam;

static void camera_load(void)
{
    Cam.on = fh2_ini_int(L"Camera", L"Dynamic", 1);
    Cam.look = fh2_ini_float(L"Camera", L"LookIntoTurns", 8);
    Cam.lean = clampi(fh2_ini_int(L"Camera", L"Lean", 100), 0, 200) / 100.0f;
    Cam.pitch = clampi(fh2_ini_int(L"Camera", L"Pitch", 100), 0, 200) / 100.0f;
    Cam.bumps = clampi(fh2_ini_int(L"Camera", L"Bumps", 150), 0, 300) / 100.0f;
    Cam.shake = clampi(fh2_ini_int(L"Camera", L"Shake", 60), 0, 200) / 100.0f;
}

/* ---- a layer's tag: <Layer a="v" ... /> ---- */
static const char *cam_attr(const char *tag, size_t len, const char *name, size_t *vlen)
{
    char key[48];
    size_t kl = (size_t)wsprintfA(key, " %s=\"", name);
    for (size_t i = 0; i + kl <= len; i++)
        if (!memcmp(tag + i, key, kl)) {
            const char *v = tag + i + kl, *q = memchr(v, '"', len - (size_t)(v - tag));
            if (!q)
                return NULL;
            *vlen = (size_t)(q - v);
            return v;
        }
    return NULL;
}

static int cam_is(const char *tag, size_t len, const char *input)
{
    size_t n;
    const char *v = cam_attr(tag, len, "InputType", &n);
    return v && n == strlen(input) && !memcmp(v, input, n);
}

/* a new tag: the old one with its Param.x/y/z times f */
static char *cam_scale(const char *tag, size_t len, float f)
{
    static const char *const kP[3] = {"Param.x", "Param.y", "Param.z"};
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, len + 64), *o = out;
    const char *p = tag;
    for (;;) {
        const char *best = NULL;
        size_t bl = 0;
        for (int i = 0; i < 3; i++) {
            size_t n;
            const char *v = cam_attr(p, len - (size_t)(p - tag), kP[i], &n);
            if (v && (!best || v < best))
                best = v, bl = n;
        }
        if (!best)
            break;
        memcpy(o, p, (size_t)(best - p));
        o += best - p;
        o += _snprintf(o, 31, "%.6f", strtod(best, NULL) * f);
        p = best + bl;
    }
    memcpy(o, p, len - (size_t)(p - tag));
    o += len - (size_t)(p - tag);
    *o = 0;
    return out;
}

/* a new layer: input -> output through a three-point curve (mirrored: the same for negative inputs, negated),
   times (x, y, z), on a spring (k, damping) or without (k 0) */
static char *cam_layer(const char *input, const char *noise, const char *output, float cmin, float cmax, int mirrored,
                       const float curve[6], float x, float y, float z, float k, float d, float noiseFreq)
{
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, 1200);
    int n = _snprintf(out, 1199,
                      "<Layer Enabled=\"1\" LocationType=\"All\" InputType=\"%s\" NoiseType=\"%s\" OutputType=\"%s\" "
                      "InputTakeDerivative=\"0\" InputClampMin=\"%.6f\" InputClampMax=\"%.6f\" InputMagAttackDecay=\"0\" ",
                      input, noise, output, cmin, cmax);
    if (noiseFreq > 0)
        n += _snprintf(out + n, 1199 - n,
                       "InputToNoiseFreqMapping=\"Linear\" NoiseFreq=\"%.6f\" NoiseSeed=\"%.6f\" NoiseOutputRangeMin=\"-1.000000\" "
                       "NoiseOutputRangeMax=\"1.000000\" ",
                       noiseFreq, noiseFreq * 1.37f);
    n += _snprintf(out + n, 1199 - n,
                   "InputToParamMapping=\"ThreePoint\" InputToParamCurveInput0=\"%.6f\" InputToParamCurveOutput0=\"%.6f\" "
                   "InputToParamCurveInput1=\"%.6f\" InputToParamCurveOutput1=\"%.6f\" InputToParamCurveInput2=\"%.6f\" "
                   "InputToParamCurveOutput2=\"%.6f\" InputToParamCurveMirrored=\"%d\" Param.x=\"%.6f\" Param.y=\"%.6f\" "
                   "Param.z=\"%.6f\" Param.w=\"0.000000\" ",
                   curve[0], curve[1], curve[2], curve[3], curve[4], curve[5], mirrored, x, y, z);
    if (k > 0)
        _snprintf(out + n, 1199 - n, "Sprung=\"1\" SpringK=\"%.6f\" SpringDPercent=\"%.6f\"/>", k, d);
    else
        _snprintf(out + n, 1199 - n, "Sprung=\"0\"/>");
    out[1199] = 0;
    return out;
}

/* CameraPhysics.xml with the DriverCam made alive; NULL: as it is */
static char *camera_patch(const char *xml, size_t len, size_t *outLen)
{
    enum { N = 64 };
    const char *sec = strstr(xml, "<DriverCam"), *secEnd = sec ? strstr(sec, "</DriverCam>") : NULL;
    const char *tag[N];
    size_t tl[N];
    char *nw[N];
    int n = 0;
    if (!secEnd)
        return NULL;
    for (const char *p = sec; n < N && (p = strstr(p, "<Layer ")) != NULL && p < secEnd; n++) {
        const char *e = strstr(p, "/>");
        if (!e || e > secEnd)
            return NULL;
        tag[n] = p, tl[n] = (size_t)(e + 2 - p);
        p = e + 2;
    }
    /* the layers this changes, by their inputs (FH2 1.0.0.64) */
    static const struct {
        int i;
        const char *input;
    } kExpect[] = {{0, "Gs_Longitudinal"}, {1, "Gs_Longitudinal"}, {2, "Gs_Longitudinal"}, {3, "Gs_Longitudinal"},
                   {4, "Gs_Longitudinal"}, {5, "Gs_Longitudinal"}, {8, "Gs_Lateral"}, {9, "Gs_Lateral"},
                   {15, "WorldCollision_Longitudinal"}, {17, "WorldCollision_Longitudinal"},
                   {24, "CarCollision_Longitudinal"}, {33, "SurfaceBumpOffset_FL"}, {34, "SurfaceBumpOffset_FR"},
                   {35, "SurfaceBumpOffset_RR"}, {36, "SurfaceBumpOffset_RL"}, {37, "SuspensionBias_Longitudinal"},
                   {44, "SpeedMPH"}, {45, "SpeedMPH"}, {47, "SpeedMPH"}};
    if (n != N) {
        fh2_say("camera: the driver's camera has %d layers, not 64 -- the game's own", n);
        return NULL;
    }
    for (int i = 0; i < (int)(sizeof kExpect / sizeof kExpect[0]); i++)
        if (!cam_is(tag[kExpect[i].i], tl[kExpect[i].i], kExpect[i].input)) {
            fh2_say("camera: the driver's camera layer %d is not %s -- the game's own", kExpect[i].i, kExpect[i].input);
            return NULL;
        }
    ZeroMemory(nw, sizeof nw);
    /* braking and throttle: the dip forward and the push back, deeper */
    for (int i = 0; i <= 5; i++)
        nw[i] = cam_scale(tag[i], tl[i], 1 + Cam.pitch);
    /* looking into turns: yaw by the sideways g, smooth (a soft spring), one layer per side as the game's own */
    static const float kLook[6] = {0.05f, 0, 0.5f, 0.45f, 1.2f, 1};
    nw[8] = cam_layer("Gs_Lateral", "None", "CameraSpaceYPROffset", -1.5f, 0, 1, kLook, Cam.look, 0, 0, 25, 1.0f, 0);
    nw[9] = cam_layer("Gs_Lateral", "None", "CameraSpaceYPROffset", 0, 1.5f, 0, kLook, Cam.look, 0, 0, 25, 1.0f, 0);
    /* leaning: the horizon tilting with the body (roll), and the head pushed to the outside (sway) */
    static const float kLean[6] = {0.1f, 0, 0.6f, 0.55f, 1.3f, 1};
    nw[15] = cam_layer("Gs_Lateral", "None", "CarSpaceYPROffset", -1.5f, 0, 1, kLean, 0, 0, -2.5f * Cam.lean, 80, 0.85f, 0);
    nw[17] = cam_layer("Gs_Lateral", "None", "CarSpaceYPROffset", 0, 1.5f, 0, kLean, 0, 0, -2.5f * Cam.lean, 80, 0.85f, 0);
    nw[24] = cam_layer("Gs_Lateral", "None", "CarSpaceXYZOffset", -1.5f, 0, 1, kLean, -0.03f * Cam.lean, 0, 0, 60, 0.9f, 0);
    nw[37] = cam_layer("Gs_Lateral", "None", "CarSpaceXYZOffset", 0, 1.5f, 0, kLean, -0.03f * Cam.lean, 0, 0, 60, 0.9f, 0);
    /* the bumps under each wheel */
    for (int i = 33; i <= 36; i++)
        nw[i] = cam_scale(tag[i], tl[i], Cam.bumps);
    /* a light shake growing with speed (from 30 mph; the game's own, off, came only beyond 140) */
    static const float kShake[6] = {30, 0, 90, 0.45f, 200, 1};
    if (Cam.shake > 0) {
        nw[44] = cam_layer("SpeedMPH", "Simplex", "CameraSpaceXYZOffset", 0, 250, 0, kShake, 0.0015f * Cam.shake,
                           0.0035f * Cam.shake, 0.0015f * Cam.shake, 0, 0, 18);
        nw[45] = cam_layer("SpeedMPH", "Simplex", "CameraSpaceYPROffset", 0, 250, 0, kShake, 0.12f * Cam.shake,
                           0.18f * Cam.shake, 0.10f * Cam.shake, 0, 0, 9);
        char *t = cam_scale(tag[47], tl[47], Cam.shake);
        char *en = strstr(t, "Enabled=\"0\"");
        if (en)
            en[9] = '1';
        nw[47] = t;
    }
    /* the new text */
    size_t total = len + 1;
    for (int i = 0; i < N; i++)
        if (nw[i])
            total += strlen(nw[i]);
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, total), *o = out;
    const char *p = xml;
    for (int i = 0; i < N; i++) {
        memcpy(o, p, (size_t)(tag[i] - p));
        o += tag[i] - p;
        if (nw[i]) {
            size_t l = strlen(nw[i]);
            memcpy(o, nw[i], l);
            o += l;
            HeapFree(GetProcessHeap(), 0, nw[i]);
        } else {
            memcpy(o, tag[i], tl[i]);
            o += tl[i];
        }
        p = tag[i] + tl[i];
    }
    memcpy(o, p, len - (size_t)(p - xml));
    o += len - (size_t)(p - xml);
    *o = 0;
    *outLen = (size_t)(o - out);
    fh2_say("camera: the cockpit camera alive -- looks %.0f degrees into turns, lean %d%%, pitch +%d%%, bumps %d%%, "
            "shake %d%%", Cam.look, (int)(Cam.lean * 100), (int)(Cam.pitch * 100), (int)(Cam.bumps * 100),
            (int)(Cam.shake * 100));
    return out;
}
