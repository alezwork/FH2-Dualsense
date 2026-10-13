/* Keyboard.dll -- Forza Horizon 2 (and Fast & Furious) on a keyboard, a mod for Azimuth (Azimuth\mods\).

   The game reads one Xbox gamepad; the engine gives it XInput user 0. This mod puts its own XInputGetState in the
   engine's XInput table (common\fh2mod.h): keys become a gamepad, merged with any real pad (and with the DualSense
   mod, when it is there). Steering and pedals move smoothly, like a stick and triggers, at the speeds in the ini.
   Keys count only while the game window is in front. Settings and key names: mods\Keyboard.ini; log mods\Keyboard.log. */
#include "../common/fh2mod.h"
#include "ini_default.h"

enum {
    C_THROTTLE, C_BRAKE, C_LEFT, C_RIGHT,                   /* RT, LT, left stick X */
    C_STICK_UP, C_STICK_DOWN,                              /* left stick Y */
    C_LOOK_LEFT, C_LOOK_RIGHT, C_LOOK_UP, C_LOOK_DOWN,     /* right stick */
    C_A, C_B, C_X, C_Y, C_LB, C_RB, C_LS, C_RS, C_MENU, C_VIEW,
    C_DUP, C_DDOWN, C_DLEFT, C_DRIGHT,
    C_COUNT
};

static const struct {
    const WCHAR *key; /* in [Keys] */
    const WCHAR *def;
    WORD button; /* 0 = analog */
} kControls[C_COUNT] = {
    {L"Throttle", L"W", 0},
    {L"Brake", L"S", 0},
    {L"SteerLeft", L"A", 0},
    {L"SteerRight", L"D", 0},
    {L"StickUp", L"", 0},
    {L"StickDown", L"", 0},
    {L"LookLeft", L"J", 0},
    {L"LookRight", L"L", 0},
    {L"LookUp", L"I", 0},
    {L"LookDown", L"K", 0},
    {L"A", L"Space, Enter", XINPUT_GAMEPAD_A},
    {L"B", L"E, Backspace", XINPUT_GAMEPAD_B},
    {L"X", L"Q", XINPUT_GAMEPAD_X},
    {L"Y", L"R", XINPUT_GAMEPAD_Y},
    {L"LB", L"LShift", XINPUT_GAMEPAD_LEFT_SHOULDER},
    {L"RB", L"C", XINPUT_GAMEPAD_RIGHT_SHOULDER},
    {L"LeftStickClick", L"F", XINPUT_GAMEPAD_LEFT_THUMB},
    {L"RightStickClick", L"V", XINPUT_GAMEPAD_RIGHT_THUMB},
    {L"Menu", L"Escape", XINPUT_GAMEPAD_START},
    {L"View", L"Tab", XINPUT_GAMEPAD_BACK},
    {L"DPadUp", L"Up", XINPUT_GAMEPAD_DPAD_UP},
    {L"DPadDown", L"Down", XINPUT_GAMEPAD_DPAD_DOWN},
    {L"DPadLeft", L"Left", XINPUT_GAMEPAD_DPAD_LEFT},
    {L"DPadRight", L"Right", XINPUT_GAMEPAD_DPAD_RIGHT},
};

#define MAX_KEYS 4
static BYTE g_keys[C_COUNT][MAX_KEYS];
static int g_enabled, g_focusOnly;
static float g_steerSpeed, g_steerReturn, g_pedalSpeed, g_lookSpeed;

static Fh2Pads g_pads;
static SRWLOCK g_lock = SRWLOCK_INIT;
static float g_steer, g_throttle, g_brake, g_lookX, g_lookY;
static double g_last;
static XINPUT_GAMEPAD g_out; /* the keyboard's pad, last poll */
static DWORD g_packet;
static volatile ULONGLONG g_lastInput; /* the last time a key moved the pad (for DualSense.dll's icons) */

/* for the DualSense mod: when the keyboard was last used (GetTickCount64), 0 = never */
__declspec(dllexport) ULONGLONG Fh2KeyboardLastInput(void) { return g_lastInput; }

/* ---- key names ---- */
static const struct {
    const WCHAR *name;
    BYTE vk;
} kNames[] = {
    {L"Space", VK_SPACE}, {L"Enter", VK_RETURN}, {L"Return", VK_RETURN}, {L"Backspace", VK_BACK},
    {L"Tab", VK_TAB}, {L"Escape", VK_ESCAPE}, {L"Esc", VK_ESCAPE}, {L"Shift", VK_SHIFT},
    {L"LShift", VK_LSHIFT}, {L"RShift", VK_RSHIFT}, {L"Ctrl", VK_CONTROL}, {L"LCtrl", VK_LCONTROL},
    {L"RCtrl", VK_RCONTROL}, {L"Alt", VK_MENU}, {L"LAlt", VK_LMENU}, {L"RAlt", VK_RMENU},
    {L"Up", VK_UP}, {L"Down", VK_DOWN}, {L"Left", VK_LEFT}, {L"Right", VK_RIGHT},
    {L"Home", VK_HOME}, {L"End", VK_END}, {L"PageUp", VK_PRIOR}, {L"PageDown", VK_NEXT},
    {L"Insert", VK_INSERT}, {L"Delete", VK_DELETE}, {L"CapsLock", VK_CAPITAL},
    {L"NumpadPlus", VK_ADD}, {L"NumpadMinus", VK_SUBTRACT}, {L"NumpadStar", VK_MULTIPLY},
    {L"NumpadSlash", VK_DIVIDE}, {L"NumpadDot", VK_DECIMAL}, {L"NumpadEnter", VK_RETURN},
    {L"Comma", VK_OEM_COMMA}, {L"Period", VK_OEM_PERIOD}, {L"Minus", VK_OEM_MINUS}, {L"Equals", VK_OEM_PLUS},
    {L"Semicolon", VK_OEM_1}, {L"Slash", VK_OEM_2}, {L"Backquote", VK_OEM_3}, {L"LBracket", VK_OEM_4},
    {L"Backslash", VK_OEM_5}, {L"RBracket", VK_OEM_6}, {L"Quote", VK_OEM_7},
    {L"Mouse1", VK_LBUTTON}, {L"Mouse2", VK_RBUTTON}, {L"Mouse3", VK_MBUTTON}, {L"Mouse4", VK_XBUTTON1},
    {L"Mouse5", VK_XBUTTON2},
};

static BYTE vk_of(const WCHAR *name)
{
    int n = lstrlenW(name);
    if (n == 1) {
        WCHAR c = name[0];
        if (c >= 'a' && c <= 'z')
            c -= 32;
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            return (BYTE)c;
    }
    if ((name[0] == 'F' || name[0] == 'f') && n >= 2 && n <= 3) {
        int f = name[1] - '0';
        if (n == 3)
            f = f * 10 + (name[2] - '0');
        if (f >= 1 && f <= 24)
            return (BYTE)(VK_F1 + f - 1);
    }
    if (n == 7 && CompareStringOrdinal(name, 6, L"Numpad", 6, TRUE) == CSTR_EQUAL && name[6] >= '0' && name[6] <= '9')
        return (BYTE)(VK_NUMPAD0 + name[6] - '0');
    for (int i = 0; i < (int)(sizeof kNames / sizeof kNames[0]); i++)
        if (CompareStringOrdinal(name, -1, kNames[i].name, -1, TRUE) == CSTR_EQUAL)
            return kNames[i].vk;
    return 0;
}

static void load_settings(void)
{
    g_enabled = fh2_ini_int(L"Keyboard", L"Enabled", 1);
    g_focusOnly = fh2_ini_int(L"Keyboard", L"OnlyWhenGameInFront", 1);
    g_steerSpeed = fh2_ini_float(L"Keyboard", L"SteerSpeed", 3.0f);
    g_steerReturn = fh2_ini_float(L"Keyboard", L"SteerReturn", 5.0f);
    g_pedalSpeed = fh2_ini_float(L"Keyboard", L"PedalSpeed", 8.0f);
    g_lookSpeed = fh2_ini_float(L"Keyboard", L"LookSpeed", 0.0f);
    for (int c = 0; c < C_COUNT; c++) {
        WCHAR list[128], name[32];
        int k = 0, n = 0;
        fh2_ini_str(L"Keys", kControls[c].key, kControls[c].def, list, 128);
        for (WCHAR *p = list;; p++) {
            if (*p == ',' || *p == 0) {
                while (n && name[n - 1] == ' ')
                    n--;
                name[n] = 0;
                if (n) {
                    BYTE vk = vk_of(name);
                    if (vk && k < MAX_KEYS)
                        g_keys[c][k++] = vk;
                    else if (!vk)
                        fh2_say_w("unknown key name in [Keys]: ", name);
                }
                n = 0;
                if (!*p)
                    break;
            } else if (n || *p != ' ') {
                if (n < 31)
                    name[n++] = *p;
            }
        }
    }
}

static int down(int c)
{
    for (int k = 0; k < MAX_KEYS && g_keys[c][k]; k++)
        if (GetAsyncKeyState(g_keys[c][k]) & 0x8000)
            return 1;
    return 0;
}

/* v towards target at rate per second (rate 0: at once) */
static float approach(float v, float target, float rate, float dt)
{
    float d = target - v, step = rate * dt;
    if (rate <= 0 || (d <= step && d >= -step))
        return target;
    return v + (d > 0 ? step : -step);
}

/* the keyboard's pad now (called on every poll of the engine) */
static void read_keys(XINPUT_GAMEPAD *g)
{
    double now = fh2_now();
    float dt = g_last ? (float)(now - g_last) : 0;
    int in = !g_focusOnly || fh2_foreground();
    g_last = now;
    if (dt > 0.1f)
        dt = 0.1f;
    ZeroMemory(g, sizeof *g);
    float steer = in ? (float)(down(C_RIGHT) - down(C_LEFT)) : 0;
    /* turning in at SteerSpeed, back to the centre (or across it) at SteerReturn */
    g_steer = approach(g_steer, steer, steer == 0 || steer * g_steer < 0 ? g_steerReturn : g_steerSpeed, dt);
    g_throttle = approach(g_throttle, in && down(C_THROTTLE) ? 1.0f : 0.0f, g_pedalSpeed, dt);
    g_brake = approach(g_brake, in && down(C_BRAKE) ? 1.0f : 0.0f, g_pedalSpeed, dt);
    g_lookX = approach(g_lookX, in ? (float)(down(C_LOOK_RIGHT) - down(C_LOOK_LEFT)) : 0, g_lookSpeed, dt);
    g_lookY = approach(g_lookY, in ? (float)(down(C_LOOK_UP) - down(C_LOOK_DOWN)) : 0, g_lookSpeed, dt);
    g->sThumbLX = fh2_axis(g_steer);
    g->sThumbLY = fh2_axis(in ? (float)(down(C_STICK_UP) - down(C_STICK_DOWN)) : 0);
    g->sThumbRX = fh2_axis(g_lookX);
    g->sThumbRY = fh2_axis(g_lookY);
    g->bRightTrigger = (BYTE)(g_throttle * 255.0f + 0.5f);
    g->bLeftTrigger = (BYTE)(g_brake * 255.0f + 0.5f);
    if (in)
        for (int c = 0; c < C_COUNT; c++)
            if (kControls[c].button && down(c))
                g->wButtons |= kControls[c].button;
}

static DWORD WINAPI kb_get_state(DWORD user, XINPUT_STATE *st)
{
    XINPUT_GAMEPAD pads, kb;
    DWORD packet;
    if (user != 0)
        return ERROR_DEVICE_NOT_CONNECTED; /* every pad is user 0 */
    AcquireSRWLockExclusive(&g_lock);
    fh2_pads_get(&g_pads, &pads, &packet);
    read_keys(&kb);
    if (memcmp(&kb, &g_out, sizeof kb)) {
        g_out = kb;
        g_packet++;
    }
    if (kb.wButtons || kb.bLeftTrigger || kb.bRightTrigger || kb.sThumbLX || kb.sThumbLY || kb.sThumbRX || kb.sThumbRY)
        g_lastInput = GetTickCount64();
    ReleaseSRWLockExclusive(&g_lock);
    fh2_merge(&pads, &kb);
    st->dwPacketNumber = packet + g_packet;
    st->Gamepad = pads;
    return ERROR_SUCCESS; /* the keyboard is always there */
}

static DWORD WINAPI kb_set_state(DWORD user, XINPUT_VIBRATION *vib)
{
    if (user != 0)
        return ERROR_DEVICE_NOT_CONNECTED;
    fh2_pads_set(&g_pads, vib);
    return ERROR_SUCCESS;
}

static void install(void)
{
    Fh2Engine e;
    fh2_mod_open(L"Keyboard");
    fh2_ini_default(kDefaultIni);
    load_settings();
    if (!g_enabled) {
        fh2_say("Keyboard: switched off in Keyboard.ini ([Keyboard] Enabled=0)");
        return;
    }
    if (!fh2_engine(&e))
        return;
    void **table = fh2_xinput_table(&e);
    if (!table)
        return;
    g_pads.get = (Fh2GetStateFn)table[1];
    g_pads.set = (Fh2SetStateFn)table[2];
    InterlockedExchangePointer(&table[2], (void *)kb_set_state);
    InterlockedExchangePointer(&table[1], (void *)kb_get_state);
    fh2_say("Keyboard: on -- keys are XInput user 0 (with every real pad), steering %.1f/%.1f per second, pedals %.1f",
            g_steerSpeed, g_steerReturn, g_pedalSpeed);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        install();
    }
    return TRUE;
}
