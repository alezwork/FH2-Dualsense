/* DualSense.dll -- the PlayStation 5 controller (DualSense, DualSense Edge; USB or Bluetooth, no other software
   needed) for Forza Horizon 2 on Azimuth 0.9.10, a mod of the FH2 Mod Platform (fh2-mods\DualSense.dll).

   What the game gets: the DualSense as one of the PC's gamepads (wgi.h: the engine gives the game the PC's
   Windows.Gaming.Input gamepads, and ours is one of them), with the PlayStation button icons (icons.h).
   What the controller gets from the game:
   - rumble: the game's two motors (engine revs, collisions, tyres: media\ControllerRumble.xml) -- as the DualSense's
     rumble emulation (USB/Bluetooth), or, on USB, as HD haptics: our own waveforms on the controller's two haptic
     audio channels (a low and a high tone per side, from the two motors);
   - the car itself (car.h: the game's own data for its rumble -- engine revs, the tyres' slip, collisions, gear
     changes, the surface under each wheel): an engine hum at its revs, the rev limiter, gear changes, crashes, the
     road (gravel, dirt, grass, sand, cobbles, kerbs: each its own texture, on the side it is under), jumps' landings;
   - the game's interface sounds felt (sounds.h, fx.h): a tick for every move in the menus, a click for every choice,
     the tombola's drum ticking by as it slows down, skills, combos, checkpoints, countdowns, level-ups -- as on a PS5;
   - adaptive triggers: a brake-pedal resistance on L2 and a light one on R2; with ABS, L2 pulses at the front
     tyres' grip limit as the ABS works, without it a locked wheel makes the pedal go hard; R2 vibrates with wheel
     spin and knocks on the rev limiter; gear changes knock, crashes and landings jolt, the road's texture is in the
     trigger you press; free in the menus. Without the car's data: the game's Xbox One impulse triggers
     (media\ControllerHaptics.xml, which the engine passes on with the motors) as vibration;
   - light bar: a colour, flashing with the rumble (or the revs, or the battery level); low battery blinks red;
   - player lights: a rev counter while driving (else player 1), the skill combo; the PS button shows the battery;
   - the DualSense page (page.h, MODS > Installed mods > DualSense): the controller's connection, charge, firmware,
     and tests of the vibration, the triggers and the lights;
   - mic button: gyro steering on/off (tilt the controller like a wheel), the mic light shows it;
   - touchpad: click = View, swipes = D-pad; Create = View, Options = Menu; Edge paddles/Fn as set in the ini.
   The main switches are on the game's Mods page (Pause > Settings > MODS), the rest in DualSense\DualSense.ini next
   to azimuth.exe; the log is DualSense\DualSense.log. Report layouts: Linux hid-playstation.c; trigger effects:
   Nielk1's DualSense trigger effect notes (MIT). */
#define COBJMACROS
#include "base.h"
#include <setupapi.h>
#include <hidsdi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <math.h>
#include <stdlib.h>
#include "ini_default.h"

/* the COM ids (the SDK declares them for C but does not define them) */
static const GUID kClsidDeviceEnumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
static const GUID kIidDeviceEnumerator = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
static const GUID kIidAudioClient = {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
static const GUID kIidAudioRenderClient = {0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2}};
static const PROPERTYKEY kFriendlyName = {{0xA45C254E, 0xDF1C, 0x4EFD, {0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0}}, 14};
static const GUID kSubtypeFloat = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

#define DS_VID 0x054C
#define DS_PID 0x0CE6
#define DS_EDGE_PID 0x0DF2

/* ---------------------------------------------------------------------------------------------------------------- */
/* settings                                                                                                         */
static struct {
    int enabled, swapCrossCircle, swipes, psBattery;
    WORD touchClick, create, leftPaddle, rightPaddle, leftFn, rightFn;
    int haptics; /* 0 off, 1 rumble, 2 audio, 3 auto */
    float strength, lowHz, highHz;
    float carFeel; /* the engine, the rev limiter, gear changes in the vibration: 0..1 */
    float collisions; /* the game's crash rumble on the motors, on the real impulse scale (car.h): 0..1 */
    int triggers; /* 0 off, 1 resistance only, 2 game */
    float trigStrength;
    int brakeResist, throttleResist; /* 0 off, 1 constant, 2 slope */
    int brakeStart, brakeForce0, brakeForce1, throttleStart, throttleForce;
    int brakeHz, throttleHz;
    float brakeFrom, spinFrom; /* the slip (1 = the grip limit) where the brake / throttle triggers start vibrating */
    int gearKick, revLimiter, crashJolt, freeInMenus;
    int lightMode; /* 0 off, 1 static, 2 rumble, 3 battery, 4 revs */
    int revLights;
    BYTE color[3], flash[3];
    float brightness;
    int lowBatteryBlink;
    int icons; /* 0 off, 1 on, 2 auto (a DualSense connected when the game starts) */
    int gyro;  /* 0 off, 1 toggle (mic button), 2 always */
    float gyroMaxAngle;
    int gyroInvert;
    float menus;      /* the menus' ticks and clicks: 0..1 */
    float events;     /* the game's events (skills, the tombola, landings...): 0..1 */
    float surface;    /* the road in the vibration and the triggers: 0..1 */
    int comboLights;  /* the skill combo on the player lights */
    int trigOn;       /* the Mods page's "Adaptive triggers" ([Triggers] Enabled) */
    int checkUpdates; /* the platform looks for a newer release on GitHub */
    int menuRu;       /* the Mods page in Russian */
} S;

static WORD button_of(const WCHAR *s)
{
    static const struct {
        const WCHAR *n;
        WORD b;
    } k[] = {{L"A", XINPUT_GAMEPAD_A}, {L"B", XINPUT_GAMEPAD_B}, {L"X", XINPUT_GAMEPAD_X}, {L"Y", XINPUT_GAMEPAD_Y},
             {L"LB", XINPUT_GAMEPAD_LEFT_SHOULDER}, {L"RB", XINPUT_GAMEPAD_RIGHT_SHOULDER},
             {L"LS", XINPUT_GAMEPAD_LEFT_THUMB}, {L"RS", XINPUT_GAMEPAD_RIGHT_THUMB}, {L"Menu", XINPUT_GAMEPAD_START},
             {L"View", XINPUT_GAMEPAD_BACK}, {L"DPadUp", XINPUT_GAMEPAD_DPAD_UP},
             {L"DPadDown", XINPUT_GAMEPAD_DPAD_DOWN}, {L"DPadLeft", XINPUT_GAMEPAD_DPAD_LEFT},
             {L"DPadRight", XINPUT_GAMEPAD_DPAD_RIGHT}};
    for (int i = 0; i < (int)(sizeof k / sizeof k[0]); i++)
        if (CompareStringOrdinal(s, -1, k[i].n, -1, TRUE) == CSTR_EQUAL)
            return k[i].b;
    return 0;
}

static int choice(const WCHAR *sec, const WCHAR *key, const WCHAR *def, const WCHAR *const *names, int n, int fallback)
{
    WCHAR v[64];
    fh2_ini_str(sec, key, def, v, 64);
    for (int i = 0; i < n; i++)
        if (CompareStringOrdinal(v, -1, names[i], -1, TRUE) == CSTR_EQUAL)
            return i;
    return fallback;
}

static WORD button_setting(const WCHAR *sec, const WCHAR *key, const WCHAR *def)
{
    WCHAR v[64];
    fh2_ini_str(sec, key, def, v, 64);
    return button_of(v);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void load_settings(void)
{
    static const WCHAR *const hap[] = {L"Off", L"Rumble", L"Audio", L"Auto"};
    static const WCHAR *const trg[] = {L"Off", L"ResistanceOnly", L"Game"};
    static const WCHAR *const res[] = {L"Off", L"Constant", L"Slope"};
    static const WCHAR *const light[] = {L"Off", L"Static", L"Rumble", L"Battery", L"Revs"};
    static const WCHAR *const gyro[] = {L"Off", L"Toggle", L"On"};
    WCHAR v[32];
    S.enabled = fh2_ini_int(L"DualSense", L"Enabled", 1);
    S.swapCrossCircle = fh2_ini_int(L"DualSense", L"SwapCrossCircle", 0);
    S.touchClick = button_setting(L"DualSense", L"TouchpadClick", L"View");
    S.create = button_setting(L"DualSense", L"CreateButton", L"View");
    S.swipes = fh2_ini_int(L"DualSense", L"TouchpadSwipes", 1);
    fh2_ini_str(L"DualSense", L"PSButton", L"Battery", v, 32);
    S.psBattery = CompareStringOrdinal(v, -1, L"Battery", -1, TRUE) == CSTR_EQUAL;
    S.leftPaddle = button_setting(L"DualSense", L"EdgeLeftPaddle", L"X");
    S.rightPaddle = button_setting(L"DualSense", L"EdgeRightPaddle", L"B");
    S.leftFn = button_setting(L"DualSense", L"EdgeLeftFn", L"");
    S.rightFn = button_setting(L"DualSense", L"EdgeRightFn", L"");

    S.haptics = choice(L"Vibration", L"Mode", L"Auto", hap, 4, 3);
    S.strength = clampi(fh2_ini_int(L"Vibration", L"Strength", 100), 0, 200) / 100.0f;
    S.lowHz = fh2_ini_float(L"Vibration", L"HapticsLowHz", 45);
    S.highHz = fh2_ini_float(L"Vibration", L"HapticsHighHz", 160);
    S.carFeel = clampi(fh2_ini_int(L"Vibration", L"Car", 40), 0, 100) / 100.0f;
    S.collisions = clampi(fh2_ini_int(L"Vibration", L"Collisions", 60), 0, 100) / 100.0f;
    S.menus = clampi(fh2_ini_int(L"Vibration", L"Menus", 60), 0, 100) / 100.0f;
    S.events = clampi(fh2_ini_int(L"Vibration", L"Events", 70), 0, 100) / 100.0f;
    S.surface = clampi(fh2_ini_int(L"Vibration", L"Surface", 60), 0, 100) / 100.0f;

    S.triggers = choice(L"Triggers", L"Mode", L"Game", trg, 3, 2);
    S.trigStrength = clampi(fh2_ini_int(L"Triggers", L"Strength", 100), 0, 100) / 100.0f;
    S.brakeResist = choice(L"Triggers", L"BrakeResistance", L"Slope", res, 3, 2);
    S.brakeStart = clampi(fh2_ini_int(L"Triggers", L"BrakeStart", 1), 0, 8);
    S.brakeForce0 = clampi(fh2_ini_int(L"Triggers", L"BrakeForceStart", 1), 1, 8);
    S.brakeForce1 = clampi(fh2_ini_int(L"Triggers", L"BrakeForceEnd", 6), 1, 8);
    S.throttleResist = choice(L"Triggers", L"ThrottleResistance", L"Constant", res, 3, 1);
    S.throttleStart = clampi(fh2_ini_int(L"Triggers", L"ThrottleStart", 1), 0, 9);
    S.throttleForce = clampi(fh2_ini_int(L"Triggers", L"ThrottleForce", 1), 1, 8);
    S.brakeHz = clampi(fh2_ini_int(L"Triggers", L"BrakeVibrationHz", 30), 1, 255);
    S.throttleHz = clampi(fh2_ini_int(L"Triggers", L"ThrottleVibrationHz", 40), 1, 255);
    S.brakeFrom = clampi(fh2_ini_int(L"Triggers", L"BrakeSlipStart", 120), 50, 250) / 100.0f;
    S.spinFrom = clampi(fh2_ini_int(L"Triggers", L"SpinSlipStart", 80), 30, 250) / 100.0f;
    S.gearKick = fh2_ini_int(L"Triggers", L"GearChangeKick", 1);
    S.revLimiter = fh2_ini_int(L"Triggers", L"RevLimiter", 1);
    S.crashJolt = fh2_ini_int(L"Triggers", L"CrashJolt", 1);
    S.freeInMenus = fh2_ini_int(L"Triggers", L"FreeInMenus", 1);

    S.lightMode = choice(L"Lightbar", L"Mode", L"Rumble", light, 5, 2);
    S.revLights = fh2_ini_int(L"Lightbar", L"RevLights", 1);
    fh2_ini_rgb(L"Lightbar", L"Color", L"255, 70, 0", S.color);
    fh2_ini_rgb(L"Lightbar", L"FlashColor", L"255, 255, 255", S.flash);
    S.brightness = clampi(fh2_ini_int(L"Lightbar", L"Brightness", 100), 0, 100) / 100.0f;
    S.lowBatteryBlink = fh2_ini_int(L"Lightbar", L"LowBatteryBlink", 1);
    S.comboLights = fh2_ini_int(L"Lightbar", L"ComboLights", 1);

    {
        static const WCHAR *const icons[] = {L"Off", L"On", L"Auto"};
        S.icons = choice(L"Icons", L"PlayStation", L"Auto", icons, 3, 2);
    }
    S.gyro = choice(L"Gyro", L"Steering", L"Toggle", gyro, 3, 1);
    S.gyroMaxAngle = fh2_ini_float(L"Gyro", L"MaxAngle", 40);
    if (S.gyroMaxAngle < 5)
        S.gyroMaxAngle = 5;
    S.gyroInvert = fh2_ini_int(L"Gyro", L"Invert", 0);
    S.trigOn = fh2_ini_int(L"Triggers", L"Enabled", 1) != 0;
    S.checkUpdates = fh2_ini_int(L"DualSense", L"CheckUpdates", 1) != 0;
    {
        /* the Mods page's language: Auto = the game's (Azimuth's language, else Windows') */
        static const WCHAR *const lang[] = {L"Auto", L"English", L"Russian"};
        int l = choice(L"DualSense", L"MenuLanguage", L"Auto", lang, 3, 0);
        WCHAR az[MAX_PATH], v[32] = L"";
        if (l == 0 && g_api && lstrlenW(g_api->azimuth_dir) + 24 < MAX_PATH) {
            lstrcpyW(az, g_api->azimuth_dir);
            lstrcatW(az, L"\\data\\azimuth.ini");
            GetPrivateProfileStringW(L"launcher", L"language", L"", v, 32, az);
        }
        S.menuRu = l == 2 || (l == 0 && (v[0] ? (v[0] | 32) == 'r' && (v[1] | 32) == 'u'
                                              : PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN));
    }
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* what the engine's calls and the controller thread share                                                          */
static SRWLOCK g_lock = SRWLOCK_INIT;
static volatile LONG g_connected;
static XINPUT_GAMEPAD g_dsPad; /* the controller, in the Xbox pad's terms */
static WORD g_motor[2];        /* the game's motors: left = low frequency, right = high */
static float g_trigger[2];     /* the game's impulse triggers: left, right */
static volatile LONG g_outDirty;
static HANDLE g_wake;
static volatile ULONGLONG g_dsInputAt; /* the last real use of the DualSense */

static int used(const XINPUT_GAMEPAD *g)
{
    return g->wButtons || g->bLeftTrigger > 64 || g->bRightTrigger > 64 || g->sThumbLX > 12000 || g->sThumbLX < -12000 ||
           g->sThumbLY > 12000 || g->sThumbLY < -12000 || g->sThumbRX > 12000 || g->sThumbRX < -12000 ||
           g->sThumbRY > 12000 || g->sThumbRY < -12000;
}

#include "wgi.h"
#include "fx.h"
#include "car.h"

/* ---------------------------------------------------------------------------------------------------------------- */
/* the controller                                                                                                   */
typedef struct {
    HANDLE h;
    int bt, edge;
    USHORT inLen, outLen, featLen;
    int vibrationV2;
    DWORD hw, fw;  /* the firmware report (0x20): hardware and firmware versions */
    WORD update;   /* its update version (as the PS5 shows it) */
    char built[24]; /* its build date */
    BYTE seq;
    OVERLAPPED rd, wr;
    BYTE in[600];
} Ds;

static DWORD g_crcTable[256];

static DWORD crc32(DWORD crc, const BYTE *p, SIZE_T n)
{
    crc = ~crc;
    while (n--)
        crc = g_crcTable[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

static int contains_ci(const WCHAR *s, const WCHAR *what)
{
    int n = lstrlenW(what);
    for (; *s; s++)
        if (CompareStringOrdinal(s, n, what, n, TRUE) == CSTR_EQUAL)
            return 1;
    return 0;
}

static int ds_open(Ds *d)
{
    GUID hid;
    HidD_GetHidGuid(&hid);
    HDEVINFO set = SetupDiGetClassDevsW(&hid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    SP_DEVICE_INTERFACE_DATA ifd;
    int found = 0, sony = 0;
    char why[160] = "";
    static char lastWhy[160] = "?";
    if (set == INVALID_HANDLE_VALUE)
        return 0;
    ifd.cbSize = sizeof ifd;
    for (DWORD i = 0; !found && SetupDiEnumDeviceInterfaces(set, NULL, &hid, i, &ifd); i++) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &ifd, NULL, 0, &need, NULL);
        SP_DEVICE_INTERFACE_DETAIL_DATA_W *det = (SP_DEVICE_INTERFACE_DETAIL_DATA_W *)HeapAlloc(GetProcessHeap(), 0, need + 16);
        if (!det)
            continue;
        det->cbSize = sizeof *det;
        if (SetupDiGetDeviceInterfaceDetailW(set, &ifd, det, need, NULL, NULL) && contains_ci(det->DevicePath, L"054c")) {
            HANDLE h = CreateFileW(det->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                   OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
            sony++;
            if (h == INVALID_HANDLE_VALUE)
                _snprintf(why, sizeof why - 1, "a Sony HID device does not open (error %lu)", GetLastError());
            if (h != INVALID_HANDLE_VALUE) {
                HIDD_ATTRIBUTES a;
                PHIDP_PREPARSED_DATA pp;
                HIDP_CAPS caps;
                a.Size = sizeof a;
                if (HidD_GetAttributes(h, &a) && a.VendorID == DS_VID &&
                    (a.ProductID == DS_PID || a.ProductID == DS_EDGE_PID) && HidD_GetPreparsedData(h, &pp)) {
                    if (HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS && caps.UsagePage == 1 && caps.Usage == 5 &&
                        caps.InputReportByteLength >= 64 && caps.OutputReportByteLength >= 48) {
                        ZeroMemory(d, sizeof *d);
                        d->h = h;
                        d->edge = a.ProductID == DS_EDGE_PID;
                        d->inLen = caps.InputReportByteLength;
                        d->outLen = caps.OutputReportByteLength;
                        d->featLen = caps.FeatureReportByteLength;
                        d->bt = caps.InputReportByteLength != 64;
                        found = 1;
                        fh2_say("controller: %s over %s (input %u, output %u, feature %u bytes)",
                                d->edge ? "DualSense Edge" : "DualSense", d->bt ? "Bluetooth" : "USB", d->inLen,
                                d->outLen, d->featLen);
                    }
                    HidD_FreePreparsedData(pp);
                }
                if (!found) {
                    _snprintf(why, sizeof why - 1, "a Sony HID device %04X:%04X is not a DualSense gamepad collection",
                              a.VendorID, a.ProductID);
                    CloseHandle(h);
                }
            }
        }
        HeapFree(GetProcessHeap(), 0, det);
    }
    SetupDiDestroyDeviceInfoList(set);
    if (!found) {
        /* why not, once per change: nothing connected, or something in the way (exclusive use by another program) */
        if (!sony)
            lstrcpyA(why, "no DualSense connected");
        if (lstrcmpA(why, lastWhy)) {
            lstrcpyA(lastWhy, why);
            fh2_say("controller: %s -- looking again every 2 seconds", why);
        }
        return 0;
    }
    lastWhy[0] = '?', lastWhy[1] = 0;
    d->rd.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    d->wr.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    /* the calibration report: over Bluetooth it also switches the controller to its full reports (0x31) */
    BYTE feat[600];
    if (d->featLen && d->featLen <= sizeof feat) {
        ZeroMemory(feat, sizeof feat);
        feat[0] = 0x05;
        if (!HidD_GetFeature(d->h, feat, d->featLen))
            fh2_say("controller: calibration report: error %lu", GetLastError());
        /* the firmware: rumble emulation v2 from update 0x0224 on (as Linux decides) */
        ZeroMemory(feat, sizeof feat);
        feat[0] = 0x20;
        if (HidD_GetFeature(d->h, feat, d->featLen)) {
            d->update = (WORD)(feat[44] | feat[45] << 8);
            d->hw = (DWORD)feat[24] | (DWORD)feat[25] << 8 | (DWORD)feat[26] << 16 | (DWORD)feat[27] << 24;
            d->fw = (DWORD)feat[28] | (DWORD)feat[29] << 8 | (DWORD)feat[30] << 16 | (DWORD)feat[31] << 24;
            for (int i = 0; i < 11 && feat[1 + i] >= 32 && feat[1 + i] < 127; i++)
                d->built[i] = (char)feat[1 + i], d->built[i + 1] = 0;
            d->vibrationV2 = d->update >= 0x0224;
            fh2_say("controller: firmware 0x%08lX (update %04X, built %s), hardware 0x%08lX, rumble emulation v%d", d->fw,
                    d->update, d->built, d->hw, d->vibrationV2 ? 2 : 1);
        }
    }
    return 1;
}

static void ds_close(Ds *d)
{
    if (!d->h)
        return;
    CancelIo(d->h);
    CloseHandle(d->h);
    CloseHandle(d->rd.hEvent);
    CloseHandle(d->wr.hEvent);
    d->h = NULL;
}

/* one output report: the 47 common bytes (Linux: dualsense_output_report_common) */
static int ds_write(Ds *d, const BYTE common[47])
{
    BYTE buf[600];
    DWORD n = 0, len = d->outLen;
    if (len > sizeof buf)
        return 0;
    ZeroMemory(buf, len);
    if (!d->bt) {
        buf[0] = 0x02;
        memcpy(buf + 1, common, 47);
    } else {
        BYTE seed = 0xA2;
        buf[0] = 0x31;
        buf[1] = (BYTE)((d->seq++ & 0x0F) << 4);
        buf[2] = 0x10;
        memcpy(buf + 3, common, 47);
        DWORD crc = crc32(crc32(0, &seed, 1), buf, 74);
        buf[74] = (BYTE)crc;
        buf[75] = (BYTE)(crc >> 8);
        buf[76] = (BYTE)(crc >> 16);
        buf[77] = (BYTE)(crc >> 24);
    }
    ResetEvent(d->wr.hEvent);
    if (!WriteFile(d->h, buf, len, NULL, &d->wr)) {
        if (GetLastError() != ERROR_IO_PENDING)
            return 0;
        if (WaitForSingleObject(d->wr.hEvent, 200) != WAIT_OBJECT_0) {
            CancelIoEx(d->h, &d->wr);
            GetOverlappedResult(d->h, &d->wr, &n, TRUE);
            return 0;
        }
    }
    return GetOverlappedResult(d->h, &d->wr, &n, FALSE);
}

/* ---- trigger effects (11 bytes; Nielk1's encoding) ---- */
static void trig_off(BYTE *e)
{
    ZeroMemory(e, 11);
    e[0] = 0x05;
}

static void trig_zones(BYTE *e, BYTE mode, const BYTE force[10], BYTE frequency)
{
    DWORD zones = 0;
    WORD active = 0;
    ZeroMemory(e, 11);
    for (int i = 0; i < 10; i++)
        if (force[i]) {
            zones |= (DWORD)((force[i] - 1) & 7) << (3 * i);
            active |= (WORD)(1 << i);
        }
    if (!active) {
        trig_off(e);
        return;
    }
    e[0] = mode;
    e[1] = (BYTE)active;
    e[2] = (BYTE)(active >> 8);
    e[3] = (BYTE)zones;
    e[4] = (BYTE)(zones >> 8);
    e[5] = (BYTE)(zones >> 16);
    e[6] = (BYTE)(zones >> 24);
    e[9] = frequency;
}

/* resistance: constant from start, or a slope (a brake pedal: light first, then harder) */
static void trig_resistance(BYTE *e, int kind, int start, int f0, int f1)
{
    BYTE force[10] = {0};
    if (kind == 0) {
        trig_off(e);
        return;
    }
    for (int i = start; i < 10; i++) {
        float t = kind == 2 && start < 9 ? (float)(i - start) / (9 - start) : 0;
        force[i] = (BYTE)clampi((int)(f0 + (f1 - f0) * t + 0.5f), 1, 8);
    }
    trig_zones(e, 0x21, force, 0);
}

/* vibration over the whole travel: amplitude 1..8, frequency in Hz */
static void trig_vib(BYTE *e, int a, int hz)
{
    BYTE amp[10];
    for (int i = 0; i < 10; i++)
        amp[i] = (BYTE)clampi(a, 1, 8);
    trig_zones(e, 0x26, amp, (BYTE)clampi(hz, 1, 255));
}

static void trig_vibration(BYTE *e, float lvl, int hz) { trig_vib(e, (int)(lvl * 8 + 0.5f), hz); }

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* How far into the slip the tyres are, 0..1, eased in and out: from where the feel starts to where it is full. The
   slips (car.h, smoothed; 1 = the grip limit) run 1..3 in hard braking and 3..8 locked, 0.1..1 accelerating, 1..5
   spinning (in the air far more, hence the cap). The game's own trigger curves: braking 1.5..3, spin 0.7..1. */
#define BRAKE_FULL 3.0f
#define SPIN_FULL 2.5f
static float slip_level(float slip, float from, float full)
{
    float t = clampf((slip - from) / (full > from + 0.5f ? full - from : 0.5f), 0, 1);
    return t * t * (3 - 2 * t);
}

/* a gear change: a knock fading in 50 ms */
static float shift_knock(double now)
{
    double t = now - Car.shiftAt;
    return Car.shiftAt && t >= 0 && t < 0.15 ? expf(-(float)t / 0.05f) : 0;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* HD haptics: the controller's audio endpoint (USB: 4 channels, 3 and 4 drive the left and right actuators)       */
static volatile float g_hapAmp[2]; /* targets: low (left motor), high (right motor), 0..1 */
/* every connection over USB starts a stream thread with a new generation; a thread whose generation is not the
   current one ends (the controller went away, or a newer thread took over). g_hapRunning = the generation whose
   stream is playing (0 = none: the rumble emulation carries the vibration). */
static volatile LONG g_hapGen, g_hapRunning;

/* the road in the HD haptics, one side: its texture's state */
typedef struct {
    int cls;
    float gain;
    double ph, ph2; /* kerbs, cobbles: where between two bumps; metal: the tone's phase */
    float jit;      /* cobbles: this bump's spacing */
    float y1, y2;   /* gravel, sand, snow: the grit's resonator */
    UINT32 rng;
} SurfOsc;

static float rnd01(UINT32 *r)
{
    *r = *r * 1664525u + 1013904223u;
    return (float)(*r >> 8) / 16777216.0f;
}

/* how much of a surface at this speed: kerbs are felt even slowly (but not standing on one) */
static float surf_gain(int cls, float level, float speed)
{
    return cls == SURF_KERB ? (0.4f + 0.6f * level) * fminf(speed / 3.0f, 1.0f) : level;
}

/* one side's road into out[0..n): level = how much at this speed (0..1), gain = with the strengths */
static void surf_render(SurfOsc *o, int cls, float level, float gain, float speed, float *out, UINT n, double t0, double sr,
                        UINT32 seed)
{
    const double dt = 1.0 / sr;
    const float kg = 1 - expf(-1 / (0.02f * (float)sr)); /* 20 ms in and out */
    float rhz = cls == SURF_GRAVEL ? 220.0f : cls == SURF_SAND ? 160.0f : 120.0f;
    float rr = expf(-1 / ((cls == SURF_GRAVEL ? 0.006f : 0.009f) * (float)sr)), rc = 2 * rr * cosf(FX_TAU * rhz / (float)sr);
    float sw = sinf(FX_TAU * rhz / (float)sr); /* the resonator's peak is 1 / sin(w) */
    float grit = cls == SURF_GRAVEL ? 150 + 550 * level : cls == SURF_SAND ? 30 + 90 * level : cls == SURF_SNOW ? 40 : 0;
    float target = cls > SURF_TARMAC ? gain : 0;
    if (cls != o->cls) { /* another surface: in from silence */
        if (o->gain > 0.01f) {
            target = 0; /* the old one out first */
            cls = o->cls;
        } else {
            o->cls = cls, o->y1 = o->y2 = 0, o->ph = 0;
            o->jit = 1;
        }
    }
    if (!o->rng)
        o->rng = seed;
    for (UINT f = 0; f < n; f++) {
        double t = t0 + f * dt;
        float s = 0;
        o->gain += (target - o->gain) * kg;
        if (o->gain < 0.0005f && target == 0)
            continue;
        switch (cls) {
        case SURF_KERB: { /* a bump for every stripe */
            float rate = clampf(speed / 0.95f, 3, 40);
            o->ph += rate * dt;
            if (o->ph >= 1)
                o->ph -= floor(o->ph);
            s = 1.3f * fx_ping((float)(o->ph / rate), 65, 0.016f);
            break;
        }
        case SURF_COBBLE: { /* a bump for every stone, not quite even */
            float rate = clampf(speed / 0.32f, 4, 45) * o->jit;
            o->ph += rate * dt;
            if (o->ph >= 1) {
                o->ph -= floor(o->ph);
                o->jit = 0.8f + 0.4f * rnd01(&o->rng);
            }
            s = 0.55f * fx_ping((float)(o->ph / rate), 95, 0.009f);
            break;
        }
        case SURF_GRAVEL:
        case SURF_SAND:
        case SURF_SNOW: { /* grit: stones hitting, each a ring of the resonator, and a low rumble under */
            float x = rnd01(&o->rng) < grit * (float)dt ? (rnd01(&o->rng) * 2 - 1) * sw : 0;
            float y = rc * o->y1 - rr * rr * o->y2 + x;
            o->y2 = o->y1, o->y1 = y;
            s = cls == SURF_GRAVEL ? 0.42f * y + 0.18f * fx_vnoise(t, 45, seed)
                : cls == SURF_SAND ? 0.25f * y + 0.3f * fx_vnoise(t, 60, seed)
                                   : 0.2f * y + 0.25f * fx_vnoise(t, 22, seed);
            break;
        }
        case SURF_DIRT:
            s = 0.45f * fx_vnoise(t, 42, seed) + 0.25f * fx_vnoise(t, 9, seed + 1);
            break;
        case SURF_GRASS:
            s = 0.35f * fx_vnoise(t, 28, seed) + 0.15f * fx_vnoise(t, 6, seed + 1);
            break;
        case SURF_METAL: { /* a grate: a buzz rising with the speed */
            float hz = clampf(40 + 9 * speed, 60, 240);
            o->ph2 += hz * dt;
            if (o->ph2 >= 1)
                o->ph2 -= floor(o->ph2);
            s = 0.3f * sinf(FX_TAU * (float)o->ph2) * (0.7f + 0.3f * fx_vnoise(t, 15, seed));
            break;
        }
        case SURF_BUMPY:
            s = 0.4f * fx_vnoise(t, 16, seed) + 0.2f * fx_vnoise(t, 5, seed + 1);
            break;
        }
        out[f] += s * o->gain;
    }
}

/* the rumble emulation: the road on the motors */
static void surface_rumble(double now, float *lo, float *hi)
{
    float level, g, sp;
    if (!Surf.hooked || now - Surf.at > 0.3 || S.surface <= 0)
        return;
    level = surf_speed_level(Surf.speed), g = S.surface * S.strength, sp = Surf.speed;
    switch (surf_main()) {
    case SURF_KERB:
        if (fmod(now * clampf(sp / 0.95f, 3, 14), 1.0) < 0.5)
            *lo += g * 0.6f * surf_gain(SURF_KERB, level, sp);
        break;
    case SURF_COBBLE:
        if (fmod(now * clampf(sp / 0.32f, 4, 14), 1.0) < 0.4)
            *hi += g * 0.3f * level;
        break;
    case SURF_GRAVEL:
        *hi += g * level * (0.18f + 0.1f * fabsf(fx_vnoise(now, 30, 7)));
        break;
    case SURF_SAND:
        *hi += g * level * 0.1f, *lo += g * level * 0.08f;
        break;
    case SURF_SNOW:
        *lo += g * level * 0.08f;
        break;
    case SURF_DIRT:
        *lo += g * level * (0.12f + 0.1f * fabsf(fx_vnoise(now, 8, 3)));
        break;
    case SURF_GRASS:
        *lo += g * level * 0.1f;
        break;
    case SURF_METAL:
        *hi += g * level * 0.14f;
        break;
    case SURF_BUMPY:
        *lo += g * level * 0.12f * fabsf(fx_vnoise(now, 6, 5));
        break;
    }
}

/* -1..1, gently (no hard clipping: a strong moment stays round) */
static float soft_limit(float x)
{
    if (x > 3)
        return 1;
    if (x < -3)
        return -1;
    return x * (27 + x * x) / (27 + 9 * x * x);
}

/* the DualSense's render endpoint (it shows up a moment after the controller's HID device) */
static IMMDevice *find_endpoint(IMMDeviceEnumerator *en)
{
    IMMDeviceCollection *col = NULL;
    IMMDevice *dev = NULL;
    UINT count = 0;
    if (FAILED(IMMDeviceEnumerator_EnumAudioEndpoints(en, eRender, DEVICE_STATE_ACTIVE, &col)))
        return NULL;
    IMMDeviceCollection_GetCount(col, &count);
    for (UINT i = 0; i < count && !dev; i++) {
        IMMDevice *d = NULL;
        IPropertyStore *ps = NULL;
        PROPVARIANT pv;
        if (FAILED(IMMDeviceCollection_Item(col, i, &d)))
            continue;
        PropVariantInit(&pv);
        if (SUCCEEDED(IMMDevice_OpenPropertyStore(d, STGM_READ, &ps)) &&
            SUCCEEDED(IPropertyStore_GetValue(ps, &kFriendlyName, &pv)) && pv.vt == VT_LPWSTR &&
            (contains_ci(pv.pwszVal, L"DualSense") || contains_ci(pv.pwszVal, L"Wireless Controller"))) {
            fh2_say_w("haptics: audio endpoint ", pv.pwszVal);
            dev = d;
            d = NULL;
        }
        PropVariantClear(&pv);
        if (ps)
            IPropertyStore_Release(ps);
        if (d)
            IMMDevice_Release(d);
    }
    IMMDeviceCollection_Release(col);
    return dev;
}

static DWORD WINAPI haptics_thread(void *arg)
{
    LONG gen = (LONG)(LONG_PTR)arg;
    IMMDeviceEnumerator *en = NULL;
    IMMDevice *dev = NULL;
    IAudioClient *ac = NULL;
    IAudioRenderClient *rc = NULL;
    WAVEFORMATEX *wf = NULL;
    HANDLE ev = NULL;
    UINT frames = 0;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(CoCreateInstance(&kClsidDeviceEnumerator, NULL, CLSCTX_ALL, &kIidDeviceEnumerator, (void **)&en)))
        goto done;
    for (int tries = 0; tries < 16 && g_hapGen == gen && !(dev = find_endpoint(en)); tries++)
        Sleep(500);
    if (!dev) {
        if (g_hapGen == gen)
            fh2_say("haptics: no DualSense audio endpoint -- rumble emulation instead");
        goto done;
    }
    if (FAILED(IMMDevice_Activate(dev, &kIidAudioClient, CLSCTX_ALL, NULL, (void **)&ac)) ||
        FAILED(IAudioClient_GetMixFormat(ac, &wf)))
        goto done;
    if (wf->nChannels < 4) {
        fh2_say("haptics: the endpoint has %u channels, not 4 -- rumble emulation instead", wf->nChannels);
        goto done;
    }
    int isFloat = wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                  (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                   IsEqualGUID(&((WAVEFORMATEXTENSIBLE *)wf)->SubFormat, &kSubtypeFloat));
    int bits = wf->wBitsPerSample;
    ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (FAILED(IAudioClient_Initialize(ac, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 100000, 0, wf, NULL)) ||
        FAILED(IAudioClient_SetEventHandle(ac, ev)) || FAILED(IAudioClient_GetBufferSize(ac, &frames)) ||
        FAILED(IAudioClient_GetService(ac, &kIidAudioRenderClient, (void **)&rc)) || FAILED(IAudioClient_Start(ac))) {
        fh2_say("haptics: the audio stream did not start -- rumble emulation instead");
        goto done;
    }
    fh2_say("haptics: HD haptics on, %u channels, %lu Hz, %d-bit %s, tones %.0f / %.0f Hz", wf->nChannels,
            wf->nSamplesPerSec, bits, isFloat ? "float" : "PCM", S.lowHz, S.highHz);
    InterlockedExchange(&g_hapRunning, gen);
    {
        const double tau = 6.283185307179586;
        double ph[3] = {0, 0, 0}, step[2], amp[2] = {0, 0}, eng = 0, sr = wf->nSamplesPerSec;
        double k = 1.0 - exp(-1.0 / (0.006 * sr)); /* 6 ms smoothing: no clicks */
        unsigned noise = 0x12345678u;
        float rough = 0, *side = (float *)HeapAlloc(GetProcessHeap(), 0, sizeof(float) * 2 * (frames + 16));
        SurfOsc road[2];
        ZeroMemory(road, sizeof road);
        road[0].cls = road[1].cls = SURF_NONE;
        if (!side)
            goto stop;
        step[0] = tau * S.lowHz / sr;
        step[1] = tau * S.highHz / sr;
        while (g_hapGen == gen) {
            UINT pad = 0;
            BYTE *buf;
            float *left = side, *right = side + frames;
            if (WaitForSingleObject(ev, 200) != WAIT_OBJECT_0)
                continue;
            if (FAILED(IAudioClient_GetCurrentPadding(ac, &pad)))
                break;
            UINT n = frames - pad;
            if (!n || FAILED(IAudioRenderClient_GetBuffer(rc, n, &buf)))
                continue;
            /* the car (when its data comes in): the engine as a hum at its revs (22 Hz idle .. 92 Hz redline), louder
               with revs and throttle; knocking on the rev limiter; the road under each side */
            double now = fh2_now(), t0 = now + pad / sr, engHz = 0;
            float engT = 0, level = 0, speed = 0;
            int roadCls[2] = {SURF_NONE, SURF_NONE};
            rough = 0;
            if (car_driving(now) && fh2_foreground()) {
                if (S.carFeel > 0) {
                    float rpm = clampf(Car.rpm, 0, 1.05f), thr = g_dsPad.bRightTrigger / 255.0f;
                    engHz = 22 + 70 * rpm;
                    engT = S.carFeel * 0.45f * (0.1f + 0.9f * rpm * rpm) * (0.4f + 0.6f * thr);
                    if (rpm >= 0.975f && thr > 0.5f)
                        engT = ((int)(now * 16) & 1) ? S.carFeel * 0.6f : S.carFeel * 0.05f;
                }
                if (Surf.hooked && now - Surf.at < 0.3) {
                    roadCls[0] = surf_side(0), roadCls[1] = surf_side(1);
                    speed = Surf.speed, level = surf_speed_level(speed);
                } else if (!Surf.hooked) /* without the wheels' surfaces: the game's bumpiness as noise */
                    rough = clampf(Car.bump * 1.5f, 0, 1) * 0.3f * S.carFeel / 0.4f;
            }
            ZeroMemory(side, sizeof(float) * 2 * frames);
            fx_render(left, right, n, t0, 1.0 / sr);
            for (int i = 0; i < 2; i++)
                surf_render(&road[i], roadCls[i], level, S.surface * S.strength * surf_gain(roadCls[i], level, speed), speed,
                            i ? right : left, n, t0, sr, i ? 0xA7u : 0x51u);
            for (UINT f = 0; f < n; f++) {
                float out[2];
                amp[0] += (g_hapAmp[0] - amp[0]) * k;
                amp[1] += (g_hapAmp[1] - amp[1]) * k;
                eng += (engT - eng) * k;
                noise = noise * 1664525u + 1013904223u;
                double nz = ((int)(noise >> 9) - (1 << 22)) / (double)(1 << 22);
                double lo = sin(ph[0]) * amp[0];
                double hi = (sin(ph[1]) * (1 - rough) + nz * rough) * amp[1];
                double car = sin(ph[2]) * eng;
                ph[0] += step[0];
                ph[1] += step[1];
                ph[2] += tau * engHz / sr;
                for (int i = 0; i < 3; i++)
                    if (ph[i] > tau)
                        ph[i] -= tau;
                out[0] = soft_limit((float)(lo + 0.35 * hi + car) + left[f]);        /* left grip: the heavy motor */
                out[1] = soft_limit((float)(hi + 0.35 * lo + car * 0.8) + right[f]); /* right grip: the light motor */
                for (int c = 0; c < wf->nChannels; c++) {
                    float v = c == 2 ? out[0] : c == 3 ? out[1] : 0.0f;
                    BYTE *p = buf + (SIZE_T)f * wf->nBlockAlign + (SIZE_T)c * (bits / 8);
                    if (isFloat)
                        *(float *)p = v;
                    else if (bits == 16)
                        *(SHORT *)p = (SHORT)(v * 32767);
                    else if (bits == 24) {
                        int s = (int)(v * 8388607);
                        p[0] = (BYTE)s;
                        p[1] = (BYTE)(s >> 8);
                        p[2] = (BYTE)(s >> 16);
                    } else if (bits == 32)
                        *(INT32 *)p = (INT32)(v * 2147483647.0);
                }
            }
            if (FAILED(IAudioRenderClient_ReleaseBuffer(rc, n, 0)))
                break;
        }
    stop:
        if (side)
            HeapFree(GetProcessHeap(), 0, side);
    }
    fh2_say("haptics: stopped");
    IAudioClient_Stop(ac);
done:
    InterlockedCompareExchange(&g_hapRunning, 0, gen);
    if (rc)
        IAudioRenderClient_Release(rc);
    if (ac)
        IAudioClient_Release(ac);
    if (wf)
        CoTaskMemFree(wf);
    if (dev)
        IMMDevice_Release(dev);
    if (en)
        IMMDeviceEnumerator_Release(en);
    if (ev)
        CloseHandle(ev);
    CoUninitialize();
    return 0;
}

/* the DualSense page's tests (page.h): which, since when */
enum { TEST_VIBRATION = 1, TEST_TRIGGERS, TEST_LIGHTS };
#define TEST_TRIGGERS_LEN 10.5
#define TEST_LIGHTS_LEN 4.0
static volatile struct {
    int kind;
    double at;
} g_test;

static int test_on(int kind, double now, double len) { return g_test.kind == kind && now >= g_test.at && now - g_test.at < len; }

/* ---------------------------------------------------------------------------------------------------------------- */
/* the controller thread: reads reports, makes the Xbox pad, sends rumble / triggers / lights                      */
typedef struct {
    /* touchpad */
    int touching, swiped;
    int x0, y0;
    double pulseUntil;
    WORD pulse;
    /* gyro */
    int gyroOn, micWas, psWas;
    float g[3];
    float neutral;
    int haveG;
    /* battery */
    int battery, charging;
    double batteryShowUntil;
    /* output: last sent */
    BYTE lastTrig[2][11];
    BYTE lastRgb[3], lastPlayer, lastMic;
    BYTE lastReport[47];
    double lastSend, lastFull, lastWrite;
    int first, emulated;
    /* the triggers' vibration from the car: on / since (hysteresis, no flicker at the threshold) */
    int lockOn, spinOn;
    double lockSince, spinSince;
} DsState;

static DsState T;
static Ds g_ds; /* for the exit report */

#include "sounds.h"

static float stick(BYTE v) { return v >= 128 ? (v - 128) / 127.0f : (v - 128) / 128.0f; }

static void parse_input(Ds *d, const BYTE *rep, DWORD len)
{
    XINPUT_GAMEPAD g;
    const BYTE *r;
    BYTE b0, b1, b2, l2, r2;
    int full = 1;
    double now = fh2_now();
    if (!d->bt && rep[0] == 0x01 && len >= 64)
        r = rep + 1;
    else if (d->bt && rep[0] == 0x31 && len >= 78)
        r = rep + 2;
    else if (d->bt && rep[0] == 0x01 && len >= 10) { /* Bluetooth before the full reports: sticks and buttons only */
        static BYTE simple[64];
        simple[0] = rep[1], simple[1] = rep[2], simple[2] = rep[3], simple[3] = rep[4];
        simple[4] = rep[8], simple[5] = rep[9];
        simple[7] = rep[5], simple[8] = rep[6], simple[9] = rep[7];
        r = simple;
        full = 0;
    } else
        return;
    ZeroMemory(&g, sizeof g);
    b0 = r[7], b1 = r[8], b2 = r[9], l2 = r[4], r2 = r[5];
    g.sThumbLX = fh2_axis(stick(r[0]));
    g.sThumbLY = fh2_axis(-stick(r[1]));
    g.sThumbRX = fh2_axis(stick(r[2]));
    g.sThumbRY = fh2_axis(-stick(r[3]));
    g.bLeftTrigger = l2;
    g.bRightTrigger = r2;
    switch (b0 & 0x0F) {
    case 0: g.wButtons |= XINPUT_GAMEPAD_DPAD_UP; break;
    case 1: g.wButtons |= XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_RIGHT; break;
    case 2: g.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT; break;
    case 3: g.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_RIGHT; break;
    case 4: g.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN; break;
    case 5: g.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT; break;
    case 6: g.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT; break;
    case 7: g.wButtons |= XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_LEFT; break;
    }
    WORD cross = S.swapCrossCircle ? XINPUT_GAMEPAD_B : XINPUT_GAMEPAD_A;
    WORD circle = S.swapCrossCircle ? XINPUT_GAMEPAD_A : XINPUT_GAMEPAD_B;
    if (b0 & 0x10) g.wButtons |= XINPUT_GAMEPAD_X;        /* square */
    if (b0 & 0x20) g.wButtons |= cross;                   /* cross */
    if (b0 & 0x40) g.wButtons |= circle;                  /* circle */
    if (b0 & 0x80) g.wButtons |= XINPUT_GAMEPAD_Y;        /* triangle */
    if (b1 & 0x01) g.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
    if (b1 & 0x02) g.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
    if (b1 & 0x10) g.wButtons |= S.create;
    if (b1 & 0x20) g.wButtons |= XINPUT_GAMEPAD_START;    /* options */
    if (b1 & 0x40) g.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
    if (b1 & 0x80) g.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;
    if (b2 & 0x02) g.wButtons |= S.touchClick;
    if (d->edge) {
        if (b2 & 0x10) g.wButtons |= S.leftFn;
        if (b2 & 0x20) g.wButtons |= S.rightFn;
        if (b2 & 0x40) g.wButtons |= S.leftPaddle;
        if (b2 & 0x80) g.wButtons |= S.rightPaddle;
    }
    /* PS: the battery on the player lights */
    int ps = (b2 & 0x01) != 0;
    if (ps && !T.psWas && S.psBattery)
        T.batteryShowUntil = now + 3.0;
    T.psWas = ps;
    /* mic: gyro steering on/off */
    int mic = (b2 & 0x04) != 0;
    if (mic && !T.micWas && S.gyro == 1) {
        T.gyroOn = !T.gyroOn;
        T.haveG = 0; /* the next reading is the neutral position */
        fh2_say("gyro steering %s", T.gyroOn ? "on" : "off");
    }
    T.micWas = mic;
    if (full) {
        /* battery */
        BYTE st = r[52];
        int cap = st & 0x0F, chg = st >> 4;
        T.charging = chg == 1;
        T.battery = chg == 2 ? 100 : (cap * 10 + 5 > 100 ? 100 : cap * 10 + 5);
        /* touchpad: finger 1 */
        const BYTE *pt = r + 32;
        int touch = !(pt[0] & 0x80);
        int x = pt[1] | (pt[2] & 0x0F) << 8, y = pt[2] >> 4 | pt[3] << 4;
        if (touch && !T.touching) {
            T.x0 = x, T.y0 = y, T.swiped = 0;
        } else if (touch && !T.swiped && S.swipes) {
            int dx = x - T.x0, dy = y - T.y0;
            if (dx * dx > 350 * 350 || dy * dy > 220 * 220) {
                T.swiped = 1;
                T.pulse = (dx < 0 ? -dx : dx) * 1080 > (dy < 0 ? -dy : dy) * 1920
                              ? (dx > 0 ? XINPUT_GAMEPAD_DPAD_RIGHT : XINPUT_GAMEPAD_DPAD_LEFT)
                              : (dy > 0 ? XINPUT_GAMEPAD_DPAD_DOWN : XINPUT_GAMEPAD_DPAD_UP);
                T.pulseUntil = now + 0.12;
            }
        }
        T.touching = touch;
        /* gyro steering: the tilt from the accelerometer (the up vector), filtered */
        if (S.gyro && (T.gyroOn || S.gyro == 2)) {
            float a[3];
            for (int i = 0; i < 3; i++)
                a[i] = (float)(SHORT)(r[21 + 2 * i] | r[22 + 2 * i] << 8);
            if (!T.haveG) {
                T.g[0] = a[0], T.g[1] = a[1], T.g[2] = a[2];
            } else
                for (int i = 0; i < 3; i++)
                    T.g[i] += (a[i] - T.g[i]) * 0.15f;
            float m = sqrtf(T.g[0] * T.g[0] + T.g[1] * T.g[1] + T.g[2] * T.g[2]);
            float roll = m > 1000 ? asinf(T.g[0] / m > 1 ? 1 : T.g[0] / m < -1 ? -1 : T.g[0] / m) * 57.29578f : 0;
            if (!T.haveG) {
                T.neutral = roll;
                T.haveG = 1;
                fh2_say("gyro: neutral tilt %.1f degrees (accelerometer %d %d %d)", roll, (int)a[0], (int)a[1], (int)a[2]);
            }
            float steer = -(roll - T.neutral) / S.gyroMaxAngle;
            if (S.gyroInvert)
                steer = -steer;
            float lx = stick(r[0]) + steer;
            g.sThumbLX = fh2_axis(lx);
        }
    }
    if (now < T.pulseUntil)
        g.wButtons |= T.pulse;
    if (used(&g))
        g_dsInputAt = GetTickCount64();
    AcquireSRWLockExclusive(&g_lock);
    g_dsPad = g;
    ReleaseSRWLockExclusive(&g_lock);
}

static BYTE mix(BYTE a, BYTE b, float t) { return (BYTE)(a + (b - a) * t + 0.5f); }

/* on at `on` and above, off below `off`, and on for `hold` seconds at least: no flicker at the threshold */
static int latch(int *state, double *since, float v, float off, float on, double hold, double now)
{
    if (!*state && v >= on)
        *state = 1, *since = now;
    else if (*state && v < off && now - *since >= hold)
        *state = 0;
    return *state;
}

/* a wall in the trigger where it is now (pos 0..255): the pedal goes hard there */
static void trig_wall(BYTE *e, int pos, int force)
{
    BYTE f[10] = {0};
    for (int i = clampi(pos * 10 / 256, 0, 9); i < 10; i++)
        f[i] = (BYTE)force;
    trig_zones(e, 0x21, f, 0);
}

/* the road's texture in a trigger (nothing on tarmac): 1 if there is one */
static int surf_trigger(BYTE *e, int cls, float level, float speed, int weaker)
{
    int a, hz;
    float amp;
    switch (cls) {
    case SURF_KERB: a = 3, hz = (int)clampf(speed / 0.95f, 8, 40); break;
    case SURF_GRAVEL: a = 2, hz = 48; break;
    case SURF_DIRT: a = 2, hz = 26; break;
    case SURF_GRASS: a = 1, hz = 18; break;
    case SURF_SAND: a = 1, hz = 30; break;
    case SURF_SNOW: a = 1, hz = 14; break;
    case SURF_COBBLE: a = 2, hz = (int)clampf(speed / 0.32f, 20, 60); break;
    case SURF_METAL: a = 1, hz = 70; break;
    case SURF_BUMPY: a = 1, hz = 20; break;
    default: return 0;
    }
    amp = a * surf_gain(cls, level, speed) * S.surface / 0.6f - weaker;
    if (amp < 0.6f)
        return 0;
    trig_vib(e, (int)(amp + 0.5f), hz);
    return 1;
}

static void brake_resistance(BYTE *e) { trig_resistance(e, S.brakeResist, S.brakeStart, S.brakeForce0, S.brakeForce1); }
static void throttle_resistance(BYTE *e)
{
    trig_resistance(e, S.throttleResist, S.throttleStart, S.throttleForce, S.throttleForce);
}

/* the DualSense page's test of the triggers: 1.5 s each -- the pedals, ABS, a locked wheel, wheel spin, gravel, a
   kerb, gear changes (press them to feel it) */
static int test_triggers(BYTE e[2][11], double now)
{
    double t = now - g_test.at;
    BYTE l2 = g_dsPad.bLeftTrigger;
    if (!test_on(TEST_TRIGGERS, now, TEST_TRIGGERS_LEN))
        return 0;
    switch ((int)(t / 1.5)) {
    case 0:
        trig_resistance(e[0], 2, 1, 1, 6);
        trig_resistance(e[1], 1, 1, 2, 2);
        break;
    case 1:
        trig_vib(e[0], ((int)(now * 28) & 1) ? 6 : 1, 34);
        trig_off(e[1]);
        break;
    case 2:
        if (((int)(now * 9) & 3) == 0)
            trig_vib(e[0], 5, 10);
        else
            trig_wall(e[0], l2, 8);
        trig_off(e[1]);
        break;
    case 3:
        trig_off(e[0]);
        trig_vib(e[1], 4, 55);
        break;
    case 4:
        trig_vib(e[0], 2, 48);
        trig_vib(e[1], 2, 48);
        break;
    case 5:
        trig_vib(e[0], 3, 12);
        trig_vib(e[1], 3, 12);
        break;
    default:
        brake_resistance(e[0]);
        if (fmod(t, 0.5) < 0.07)
            trig_vib(e[1], 5, 14);
        else
            throttle_resistance(e[1]);
        break;
    }
    return 1;
}

/* the trigger effects now: e[0] L2 (the brake), e[1] R2 (the throttle) */
#define LOCK_SLIP 3.2f
static void triggers_make(BYTE e[2][11], double now, const float game[2])
{
    int fg = fh2_foreground(), setL = 0, setR = 0;
    BYTE l2 = g_dsPad.bLeftTrigger, r2 = g_dsPad.bRightTrigger;
    if (test_triggers(e, now))
        return;
    if (!S.trigOn || (Car.hooked && !fg)) { /* the switch off; the game in the background */
        trig_off(e[0]);
        trig_off(e[1]);
        return;
    }
    if (!Car.hooked) {
        /* without the car's data: the game's impulse triggers as vibration, else the resistance */
        float lv[2] = {fg ? game[0] : 0, fg ? game[1] : 0};
        if (S.triggers == 2 && lv[0] > 0.02f)
            trig_vibration(e[0], lv[0], S.brakeHz);
        else
            brake_resistance(e[0]);
        if (S.triggers == 2 && lv[1] > 0.02f)
            trig_vibration(e[1], lv[1], S.throttleHz);
        else
            throttle_resistance(e[1]);
        return;
    }
    if (!car_driving(now)) { /* menus, the game paused, loading */
        if (S.freeInMenus) {
            trig_off(e[0]);
            trig_off(e[1]);
        } else {
            brake_resistance(e[0]);
            throttle_resistance(e[1]);
        }
        T.lockOn = T.spinOn = 0;
        return;
    }
    brake_resistance(e[0]);
    throttle_resistance(e[1]);
    if (S.triggers != 2)
        return;
    /* a crash: both jolt by its strength, fading (a scrape stays light) */
    float hit = S.crashJolt ? car_hit(now) : 0;
    if (hit > 0.12f) {
        int a = 1 + (int)(hit * 6 + 0.5f);
        trig_vib(e[0], a, 20);
        trig_vib(e[1], a, 20);
        return;
    }
    /* a landing: both jolt, by how hard */
    if (Surf.landAt && now - Surf.landAt < 0.14 && S.events > 0) {
        int a = 2 + (int)(Surf.land * 5 + 0.5f);
        trig_vib(e[0], a, 16);
        trig_vib(e[1], a, 16);
        return;
    }
    /* a gear change: a knock on the trigger pressed (up on the throttle, down on the brake) */
    float knock = S.gearKick && (l2 > 24 || r2 > 24) ? shift_knock(now) : 0;
    int kickL = knock > 0.3f && l2 > r2, kickR = knock > 0.3f && !kickL;
    /* L2 past the front tyres' grip limit: with ABS, its pump's strokes (about 14 a second, harder deeper into the
       slip); without, the tyres scrub, and a locked wheel makes the pedal go hard where it is, juddering slowly */
    float sb = Car.brake;
    if (kickL)
        trig_vib(e[0], 2 + (int)(knock * 3 + 0.5f), 14), setL = 1;
    else if (latch(&T.lockOn, &T.lockSince, sb, S.brakeFrom - 0.1f, S.brakeFrom, 0.1, now) && l2 > 10) {
        float k = slip_level(sb, S.brakeFrom, BRAKE_FULL);
        setL = 1;
        if (g_abs.on)
            trig_vib(e[0], ((int)(now * 28) & 1) ? 3 + (int)(k * 4 + 0.5f) : 1, (int)(S.brakeHz * (1 + 0.4f * k) + 0.5f));
        else if (sb >= LOCK_SLIP) {
            if (((int)(now * 9) & 3) == 0)
                trig_vib(e[0], 5, 10);
            else
                trig_wall(e[0], l2, 8);
        } else
            trig_vib(e[0], 1 + (int)(k * 3 + 0.5f), (int)(S.brakeHz * 1.3f + 0.5f));
    }
    /* R2: wheel spin vibrates, harder and faster the more they spin; the rev limiter knocks */
    float sa = Car.spin;
    if (kickR)
        trig_vib(e[1], 2 + (int)(knock * 3 + 0.5f), 14), setR = 1;
    else if (latch(&T.spinOn, &T.spinSince, sa, S.spinFrom - 0.1f, S.spinFrom, 0.1, now) && r2 > 16) {
        float k = slip_level(sa, S.spinFrom, SPIN_FULL);
        trig_vib(e[1], 1 + (int)(k * 5 + 0.5f), (int)(S.throttleHz * (1 + 0.4f * k) + 0.5f));
        setR = 1;
    } else if (S.revLimiter && Car.rpm >= 0.975f && r2 > 140 && ((int)(now * 16) & 1))
        trig_vib(e[1], 4, 30), setR = 1;
    /* the road: its texture in the trigger pressed (the brake's a little lighter) */
    if (Surf.hooked && now - Surf.at < 0.3 && S.surface > 0) {
        int c = surf_main();
        float lv = surf_speed_level(Surf.speed);
        if (!setR && r2 > 16)
            surf_trigger(e[1], c, lv, Surf.speed, 0);
        if (!setL && l2 > 16)
            surf_trigger(e[0], c, lv, Surf.speed, 1);
    }
}

static void send_output(Ds *d, int force)
{
    BYTE c[47];
    WORD motor[2];
    float trig[2];
    double now = fh2_now();
    int full = force || now - T.lastFull > 1.0;
    AcquireSRWLockShared(&g_lock);
    motor[0] = g_motor[0], motor[1] = g_motor[1];
    trig[0] = g_trigger[0], trig[1] = g_trigger[1];
    ReleaseSRWLockShared(&g_lock);
    ZeroMemory(c, sizeof c);

    /* rumble: emulation, unless the HD haptics stream carries it */
    float lo = motor[0] / 65535.0f * S.strength, hi = motor[1] / 65535.0f * S.strength;
    g_hapAmp[0] = lo > 1 ? 1 : lo;
    g_hapAmp[1] = hi > 1 ? 1 : hi;
    int emulate = S.haptics && !g_hapRunning;
    int driving = car_driving(now) && fh2_foreground();
    if (emulate) {
        float flo = 0, fhi = 0;
        if (driving && S.carFeel > 0 && Car.rpm >= 0.975f && g_dsPad.bRightTrigger > 128 && ((int)(now * 16) & 1))
            hi = fmaxf(hi, S.carFeel * 0.4f); /* the rev limiter (gear changes: the effects) */
        if (driving)
            surface_rumble(now, &flo, &fhi);
        fx_rumble(now, &flo, &fhi); /* menus' ticks, events, landings, crashes, gear changes */
        lo = fmaxf(lo, flo), hi = fmaxf(hi, fhi);
    }
    if (emulate || T.emulated) { /* the emulation, or its last values cleared once the HD haptics take over */
        c[0] |= 0x02; /* haptics select: rumble emulation */
        if (d->vibrationV2)
            c[38] |= 0x04; /* compatible vibration v2 */
        else
            c[0] |= 0x01; /* compatible vibration */
        if (emulate) {
            c[2] = (BYTE)(hi > 1 ? 255 : hi * 255); /* right: the light motor */
            c[3] = (BYTE)(lo > 1 ? 255 : lo * 255); /* left: the heavy motor */
        }
        T.emulated = emulate;
    }
    /* triggers (the strength: the controller's trigger motor power) */
    if (S.triggers || test_on(TEST_TRIGGERS, now, TEST_TRIGGERS_LEN)) {
        BYTE e[2][11];
        triggers_make(e, now, trig);
        if (full || memcmp(e, T.lastTrig, sizeof e)) {
            c[0] |= 0x04 | 0x08;
            memcpy(c + 10, e[1], 11); /* right */
            memcpy(c + 21, e[0], 11); /* left */
            memcpy(T.lastTrig, e, sizeof e);
            /* the effects' strength: 0 = full, 7 = weakest (low nibble) */
            c[36] = (BYTE)clampi((int)((1.0f - S.trigStrength) * 8), 0, 7);
            c[1] |= 0x40; /* motor power level */
        }
    }

    /* light bar */
    BYTE rgb[3] = {0, 0, 0};
    float t = 0;
    switch (S.lightMode) {
    case 1:
        memcpy(rgb, S.color, 3);
        break;
    case 2:
        t = (lo > hi ? lo : hi);
        t = t > 1 ? 1 : t;
        for (int i = 0; i < 3; i++)
            rgb[i] = mix(S.color[i], S.flash[i], t);
        break;
    case 3: {
        float b = T.battery / 100.0f;
        rgb[0] = (BYTE)(255 * (1 - b)), rgb[1] = (BYTE)(255 * b), rgb[2] = 0;
        break;
    }
    case 4: /* the revs: the colour, to red at the redline, flashing on the limiter */
        memcpy(rgb, S.color, 3);
        if (driving) {
            static const BYTE red[3] = {255, 0, 0};
            t = clampf((Car.rpm - 0.55f) / 0.4f, 0, 1);
            for (int i = 0; i < 3; i++)
                rgb[i] = mix(S.color[i], red[i], t);
            if (Car.rpm >= 0.975f && ((int)(now * 12) & 1))
                rgb[0] = rgb[1] = rgb[2] = 0;
        }
        break;
    }
    for (int i = 0; i < 3; i++)
        rgb[i] = (BYTE)(rgb[i] * S.brightness);
    if (test_on(TEST_LIGHTS, now, TEST_LIGHTS_LEN)) { /* the test: round the colours */
        float h = (float)fmod((now - g_test.at) * 0.5, 1.0) * 6;
        int i = (int)h;
        float f = h - i, v[6][3] = {{1, f, 0}, {1 - f, 1, 0}, {0, 1, f}, {0, 1 - f, 1}, {f, 0, 1}, {1, 0, 1 - f}};
        for (int c = 0; c < 3; c++)
            rgb[c] = (BYTE)(255 * v[i % 6][c]);
    }
    if (S.lowBatteryBlink && T.battery && T.battery <= 15 && !T.charging && ((int)(now * 2) & 1))
        rgb[0] = 255, rgb[1] = 0, rgb[2] = 0;
    if (T.first) {
        c[38] |= 0x02; /* light bar setup: end the blue start-up animation */
        c[41] = 0x02;
    }
    if (full || memcmp(rgb, T.lastRgb, 3)) {
        c[1] |= 0x04;
        c[44] = rgb[0], c[45] = rgb[1], c[46] = rgb[2];
        memcpy(T.lastRgb, rgb, 3);
    }

    /* player lights: player 1, or the battery while the PS button was pressed, or the skill combo */
    BYTE player = 0x04;
    static const BYTE bars[6] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F};
    int combo = S.comboLights ? combo_lights(now) : -1;
    if (test_on(TEST_LIGHTS, now, TEST_LIGHTS_LEN)) {
        static const BYTE sweep[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02};
        player = sweep[(int)((now - g_test.at) * 10) & 7];
    } else if (now < T.batteryShowUntil)
        player = bars[clampi((T.battery + 19) / 20, 0, 5)];
    else if (combo >= 0)
        player = (BYTE)combo;
    else if (S.revLights && driving) { /* a rev counter: from 70% of the redline, all five at it, flashing beyond */
        if (Car.rpm >= 0.975f)
            player = ((int)(now * 10) & 1) ? 0x1F : 0x00;
        else
            player = Car.rpm < 0.70f ? 0x00 : bars[clampi((int)((Car.rpm - 0.70f) / 0.055f) + 1, 1, 5)];
    }
    if (full || player != T.lastPlayer) {
        c[1] |= 0x10;
        c[43] = player;
        T.lastPlayer = player;
    }
    /* mic light: gyro steering */
    BYTE micLed = (BYTE)(T.gyroOn ? 1 : 0);
    if (full || micLed != T.lastMic) {
        c[1] |= 0x01;
        c[8] = micLed;
        T.lastMic = micLed;
    }
    /* nothing new: no report (a keep-alive every quarter second) */
    if (!full && !T.first && !memcmp(c, T.lastReport, sizeof c) && now - T.lastWrite < 0.25) {
        T.lastSend = now;
        return;
    }
    if (ds_write(d, c)) {
        T.first = 0;
        memcpy(T.lastReport, c, sizeof c);
        T.lastWrite = now;
        if (full)
            T.lastFull = now;
    }
    T.lastSend = now;
}

/* the controller back to itself: no rumble, no trigger effects, its own lights */
static void send_reset(Ds *d)
{
    BYTE c[47];
    ZeroMemory(c, sizeof c);
    c[0] = 0x02 | 0x01 | 0x04 | 0x08;
    trig_off(c + 10);
    trig_off(c + 21);
    c[1] = 0x08 | 0x01; /* release the LEDs to the controller; mic light off */
    ds_write(d, c);
}

/* the HD haptics stream (USB): a new one, any older one ends */
static void haptics_start(void)
{
    LONG gen = InterlockedIncrement(&g_hapGen);
    HANDLE h = CreateThread(NULL, 0, haptics_thread, (void *)(LONG_PTR)gen, 0, NULL);
    if (h)
        CloseHandle(h);
}

static DWORD WINAPI controller_thread(void *arg)
{
    (void)arg;
    Ds *d = &g_ds;
    double nextScan = 0;
    int reading = 0;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    for (;;) {
        wgi_update(d->h != NULL); /* ours among the game's gamepads while the controller is there */
        if (!d->h) {
            double now = fh2_now();
            if (now < nextScan) {
                WaitForSingleObject(g_wake, 250);
                continue;
            }
            nextScan = now + 2.0;
            if (!ds_open(d))
                continue;
            ZeroMemory(&T, sizeof T);
            T.first = 1;
            T.gyroOn = S.gyro == 2;
            InterlockedExchange(&g_connected, 1);
            fx_clear();
            if (!d->bt && (S.haptics == 2 || S.haptics == 3))
                haptics_start();
            send_output(d, 1);
            wgi_update(1);
        }
        /* one read stays pending; the output goes when the game changes something, and every 50 ms */
        if (!reading) {
            ResetEvent(d->rd.hEvent);
            if (!ReadFile(d->h, d->in, d->inLen, NULL, &d->rd) && GetLastError() != ERROR_IO_PENDING) {
                fh2_say("controller: gone (read error %lu)", GetLastError());
                goto lost;
            }
            reading = 1;
        }
        HANDLE w[2] = {d->rd.hEvent, g_wake};
        double now = fh2_now();
        /* effects playing (the motors' pulses), a test: often; driving: the triggers follow the car closely */
        double tick = fx_busy(now) || (g_test.kind && now - g_test.at < TEST_TRIGGERS_LEN) ? 0.008
                      : car_driving(now)                                                 ? 0.012
                                                                                         : 0.05;
        if (WaitForMultipleObjects(2, w, FALSE, (DWORD)(tick * 1000)) == WAIT_OBJECT_0) {
            DWORD got = 0;
            reading = 0;
            if (!GetOverlappedResult(d->h, &d->rd, &got, FALSE)) {
                fh2_say("controller: gone (error %lu)", GetLastError());
                goto lost;
            }
            parse_input(d, d->in, got);
        }
        if (InterlockedExchange(&g_outDirty, 0) || fh2_now() - T.lastSend > tick)
            send_output(d, 0);
        continue;
    lost:
        reading = 0;
        ds_close(d);
        InterlockedExchange(&g_connected, 0);
        fx_clear();
        InterlockedIncrement(&g_hapGen); /* the haptics stream ends with the controller */
        AcquireSRWLockExclusive(&g_lock);
        ZeroMemory(&g_dsPad, sizeof g_dsPad);
        ReleaseSRWLockExclusive(&g_lock);
        wgi_update(0);
        nextScan = fh2_now() + 1.0;
    }
}

#include "icons.h"
#include "page.h"

/* ---------------------------------------------------------------------------------------------------------------- */
/* the Mods page (Pause > Settings > MODS): the main switches; a change goes into DualSense.ini at once             */
static void on_switch(void *user, const char *key, int value)
{
    (void)user;
    value = value != 0;
    if (!strcmp(key, "Triggers")) {
        S.trigOn = value;
        fh2_ini_set_int(L"Triggers", L"Enabled", value);
    } else if (!strcmp(key, "HDHaptics")) {
        S.haptics = value ? 3 : 1;
        fh2_ini_set(L"Vibration", L"Mode", value ? L"Auto" : L"Rumble");
        if (!value)
            InterlockedIncrement(&g_hapGen); /* the stream ends: the rumble emulation again */
        else if (g_connected && g_ds.h && !g_ds.bt && !g_hapRunning)
            haptics_start();
    } else if (!strcmp(key, "Gyro")) {
        S.gyro = value ? 1 : 0;
        if (!value)
            T.gyroOn = 0;
        fh2_ini_set(L"Gyro", L"Steering", value ? L"Toggle" : L"Off");
    } else if (!strcmp(key, "RevLights")) {
        S.revLights = value;
        fh2_ini_set_int(L"Lightbar", L"RevLights", value);
    } else if (!strcmp(key, "ComboLights")) {
        S.comboLights = value;
        fh2_ini_set_int(L"Lightbar", L"ComboLights", value);
    } else if (!strcmp(key, "Icons")) {
        S.icons = value ? 2 : 0;
        fh2_ini_set(L"Icons", L"PlayStation", value ? L"Auto" : L"Off");
    } else if (!strcmp(key, "CheckUpdates")) {
        S.checkUpdates = value;
        fh2_ini_set_int(L"DualSense", L"CheckUpdates", value);
    }
    InterlockedExchange(&g_outDirty, 1);
    if (g_wake)
        SetEvent(g_wake);
    fh2_say("Mods page: %s %s", key, value ? "on" : "off");
}

static void on_slider(void *user, const char *key, float value)
{
    int pct = (int)(value * 100 + 0.5f);
    (void)user;
    if (!strcmp(key, "Vibration")) {
        S.strength = value;
        fh2_ini_set_int(L"Vibration", L"Strength", pct);
    } else if (!strcmp(key, "TriggerStrength")) {
        S.trigStrength = value;
        fh2_ini_set_int(L"Triggers", L"Strength", pct);
    } else if (!strcmp(key, "Car")) {
        S.carFeel = value;
        fh2_ini_set_int(L"Vibration", L"Car", pct);
    } else if (!strcmp(key, "LightBar")) {
        S.brightness = value;
        fh2_ini_set_int(L"Lightbar", L"Brightness", pct);
    } else if (!strcmp(key, "MenuHaptics")) {
        S.menus = value;
        fh2_ini_set_int(L"Vibration", L"Menus", pct);
    } else if (!strcmp(key, "Events")) {
        S.events = value;
        fh2_ini_set_int(L"Vibration", L"Events", pct);
    } else if (!strcmp(key, "Surface")) {
        S.surface = value;
        fh2_ini_set_int(L"Vibration", L"Surface", pct);
    }
    InterlockedExchange(&g_outDirty, 1);
    if (g_wake)
        SetEvent(g_wake);
    fh2_say("Mods page: %s %d%%", key, pct);
}

typedef struct {
    const char *key;
    int slider, restart;
    float min, max;
    const char *en, *enDesc, *ru, *ruDesc;
} Row;

static const Row kRows[] = {
    {"Triggers", 0, 0, 0, 0, "Adaptive triggers",
     "A brake pedal's resistance on L2; ABS at work or a locked wheel, wheel spin, gear changes, crashes, landings and "
     "the road felt in the triggers.",
     "Адаптивные курки",
     "Сопротивление педали тормоза на L2; работа ABS или блокировка колеса, пробуксовка, переключения передач, удары, "
     "приземления и покрытие дороги ощущаются в курках."},
    {"TriggerStrength", 1, 0, 0, 1, "Trigger strength", "How hard the triggers push back and vibrate.", "Сила курков",
     "Насколько сильно курки сопротивляются и вибрируют."},
    {"Vibration", 1, 0, 0, 2, "Vibration", "The game's rumble on the controller, percent.", "Вибрация",
     "Сила вибрации игры на контроллере, в процентах."},
    {"HDHaptics", 0, 0, 0, 0, "HD haptics (USB)",
     "Over a USB cable the vibration is played by the controller's own haptic actuators: finer, with the engine's hum "
     "in it. Off: the classic rumble.",
     "HD-вибрация (USB)",
     "По USB-кабелю вибрацию играют собственные вибромоторы контроллера: тоньше, с гулом двигателя. Выкл.: классическая "
     "вибрация."},
    {"Car", 1, 0, 0, 1, "Engine feel", "The engine's hum at its revs, the rev limiter and gear changes in the vibration.",
     "Двигатель в вибрации", "Гул двигателя по оборотам, отсечка и переключения передач в вибрации."},
    {"MenuHaptics", 1, 0, 0, 1, "Menu haptics",
     "Ticks and clicks in the menus, as on a PS5: every move, every choice, pages opening, the tombola's drum ticking by.",
     "Вибро-отклик меню",
     "Щелчки и клики в меню, как на PS5: каждое перемещение, каждый выбор, открытие страниц, барабан призов."},
    {"Events", 1, 0, 0, 1, "Game events",
     "Skills and combos (near misses, drifts, air), checkpoints, countdowns, jumps' landings, level-ups in the vibration.",
     "События игры",
     "Навыки и комбо (почти-столкновения, дрифт, прыжки), чекпоинты, отсчёт, приземления, новые уровни в вибрации."},
    {"Surface", 1, 0, 0, 1, "Road surface",
     "Gravel, dirt, grass, sand, cobbles and kerbs, each its own texture in the vibration (on the side it is under) and "
     "in the triggers.",
     "Покрытие дороги",
     "Гравий, грунт, трава, песок, брусчатка и поребрики — у каждого своя текстура в вибрации (с той стороны, где колесо) "
     "и в курках."},
    {"Gyro", 0, 0, 0, 0, "Gyro steering",
     "The mic button turns steering by tilting the controller like a wheel on and off; its light shows it is on.",
     "Руление наклоном",
     "Кнопка микрофона включает и выключает руление наклоном контроллера, как рулём; её подсветка показывает, что "
     "включено."},
    {"LightBar", 1, 0, 0, 1, "Light bar", "The light bar's brightness (its colour and what it shows: DualSense.ini).",
     "Световая панель", "Яркость световой панели (цвет и что она показывает — в DualSense.ini)."},
    {"RevLights", 0, 0, 0, 0, "Rev counter lights", "The player lights under the touchpad show the engine's revs while driving.",
     "Тахометр на индикаторах", "Индикаторы игрока под тачпадом показывают обороты двигателя во время езды."},
    {"ComboLights", 0, 0, 0, 0, "Combo on the player lights",
     "A light more for every skill in the chain; all five blink when it is banked, the middle one when it is lost.",
     "Комбо на индикаторах",
     "Ещё один огонёк за каждый навык в цепочке; все пять мигают, когда очки засчитаны, средний — когда цепочка сорвана."},
    {"Icons", 0, 1, 0, 0, "PlayStation button icons",
     "The game's button icons with the PlayStation pictures, when a DualSense is connected as the game starts.",
     "Иконки кнопок PlayStation", "Иконки кнопок игры с картинками PlayStation, если DualSense подключён при запуске игры."},
    {"CheckUpdates", 0, 1, 0, 0, "Check for updates", "The title screen says when a newer DualSense mod is out on GitHub.",
     "Проверять обновления", "Экран заставки сообщает, когда на GitHub выходит новая версия мода DualSense."},
};

static float row_value(const char *key)
{
    if (!strcmp(key, "Triggers"))
        return (float)S.trigOn;
    if (!strcmp(key, "TriggerStrength"))
        return S.trigStrength;
    if (!strcmp(key, "Vibration"))
        return S.strength;
    if (!strcmp(key, "HDHaptics"))
        return (float)(S.haptics >= 2);
    if (!strcmp(key, "Car"))
        return S.carFeel;
    if (!strcmp(key, "Gyro"))
        return (float)(S.gyro != 0);
    if (!strcmp(key, "LightBar"))
        return S.brightness;
    if (!strcmp(key, "RevLights"))
        return (float)(S.revLights != 0);
    if (!strcmp(key, "ComboLights"))
        return (float)(S.comboLights != 0);
    if (!strcmp(key, "MenuHaptics"))
        return S.menus;
    if (!strcmp(key, "Events"))
        return S.events;
    if (!strcmp(key, "Surface"))
        return S.surface;
    if (!strcmp(key, "Icons"))
        return (float)(S.icons != 0);
    if (!strcmp(key, "CheckUpdates"))
        return (float)S.checkUpdates;
    return 0;
}

static void settings_add(void)
{
    int added = 0;
    if (g_api->api_version < 4) {
        ds_note("Mods page: the platform is too old for it (2.3.0 or newer) -- the switches are in DualSense.ini");
        return;
    }
    for (int i = 0; i < (int)(sizeof kRows / sizeof kRows[0]); i++) {
        const Row *r = &kRows[i];
        FH2Setting s;
        if (r->slider && g_api->api_version < 5)
            continue;
        ZeroMemory(&s, sizeof s);
        s.size = (uint32_t)(g_api->api_version >= 6 ? sizeof s : g_api->api_version >= 5 ? offsetof(FH2Setting, url)
                                                                                          : offsetof(FH2Setting, kind));
        s.key = r->key;
        s.label = S.menuRu ? r->ru : r->en;
        s.description = S.menuRu ? r->ruDesc : r->enDesc;
        s.needs_restart = r->restart;
        if (r->slider) {
            s.kind = FH2_SETTING_SLIDER;
            s.min = r->min, s.max = r->max, s.step = 0.05f;
            s.slider_value = row_value(r->key);
            s.display_scale = 100;
            s.on_slide = on_slider;
        } else {
            s.value = row_value(r->key) != 0;
            s.on_change = on_switch;
        }
        added += g_api->add_setting(g_mod, &s);
    }
    fh2_say("Mods page: %d rows (%s)", added, S.menuRu ? "in Russian" : "in English");
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* the start                                                                                                        */

/* the mod's own thread: the icons (before the game reads its own), then the car's data (once the game's code may be
   hooked: after Extra Options) */
static DWORD WINAPI worker_thread(void *arg)
{
    (void)arg;
    icons_make();
    if (g_api->api_version < 3) {
        ds_note("car: the platform is too old for hooks (2.2.0 or newer) -- the triggers use the game's impulse "
                "trigger levels only");
        return 0;
    }
    BYTE *base = (BYTE *)g_api->wait_for_code(0xFFFFFFFFu);
    if (base) {
        car_install(base);
        surface_install(base);
        sounds_install(base);
    }
    return 0;
}

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi *api)
{
    FH2ModInfo info;
    g_api = api;
    ds_files_open(kDefaultIni);
    load_settings();
    ZeroMemory(&info, sizeof info);
    info.size = (uint32_t)(api->api_version >= 6 ? sizeof info : offsetof(FH2ModInfo, description));
    info.name = "DualSense";
    info.version = DS_VERSION;
    info.github_repo = "alezwork/FH2-Dualsense";
    info.platform_version = FH2MOD_PLATFORM_VERSION;
    info.check_updates = S.checkUpdates;
    info.description = S.menuRu ? "Контроллер PlayStation 5: адаптивные курки по данным машины, HD-вибрация с покрытием "
                                  "дороги, вибро-отклик меню как на PS5, световая панель, руление наклоном, иконки кнопок "
                                  "PlayStation."
                                : "The PlayStation 5 controller: adaptive triggers from the car's own data, HD haptics with "
                                  "the road's surface, PS5-like menu haptics, the light bar, gyro steering, PlayStation "
                                  "button icons.";
    g_mod = api->register_mod(&info);
    if (!g_mod) {
        fh2_say("the platform did not register the mod -- off");
        return 1;
    }
    for (DWORD i = 0; i < 256; i++) {
        DWORD c = i;
        for (int k = 0; k < 8; k++)
            c = c & 1 ? 0xEDB88320 ^ (c >> 1) : c >> 1;
        g_crcTable[i] = c;
    }
    settings_add();
    page_add();
    wgi_find_engine((BYTE *)GetModuleHandleW(NULL)); /* azimuth.exe */
    g_wake = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (S.enabled) {
        HANDLE h = CreateThread(NULL, 0, controller_thread, NULL, 0, NULL);
        if (h)
            CloseHandle(h);
    } else
        ds_note("the controller switched off ([DualSense] Enabled=0)");
    HANDLE w = CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
    if (w)
        CloseHandle(w);
    ds_note("on -- vibration %s, triggers %s, light bar mode %d, gyro steering %s; menus %d%%, events %d%%, road %d%%",
            S.haptics == 0 ? "off" : S.haptics == 1 ? "rumble" : S.haptics == 2 ? "HD haptics (USB)" : "auto",
            !S.trigOn || !S.triggers ? "off" : S.triggers == 1 ? "resistance" : "game", S.lightMode,
            S.gyro == 0 ? "off" : S.gyro == 1 ? "mic button" : "always", (int)(S.menus * 100 + 0.5f),
            (int)(S.events * 100 + 0.5f), (int)(S.surface * 100 + 0.5f));
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    else if (reason == DLL_PROCESS_DETACH && reserved && g_ds.h)
        send_reset(&g_ds); /* the process ends: our threads are gone, the handle is still open */
    return TRUE;
}
