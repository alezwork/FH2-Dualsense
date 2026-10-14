/* keyboard.h -- the keyboard as the game's Xbox gamepad (it was Keyboard.dll). Keys become a pad, merged with the
   real pads and the DualSense into the one pad the game reads; steering and pedals move smoothly, like a stick and
   triggers, at the speeds in the ini. Keys count only while the game window is in front. Settings: [Keyboard] and
   [Keys] in DualSense.ini (taken over from an old mods\Keyboard.ini once). With an old Keyboard.dll still in mods\
   this one stays off and that one does the keyboard. */
#pragma once

enum {
    KB_THROTTLE, KB_BRAKE, KB_LEFT, KB_RIGHT,                 /* RT, LT, left stick X */
    KB_STICK_UP, KB_STICK_DOWN,                              /* left stick Y */
    KB_LOOK_LEFT, KB_LOOK_RIGHT, KB_LOOK_UP, KB_LOOK_DOWN,   /* right stick */
    KB_A, KB_B, KB_X, KB_Y, KB_LB, KB_RB, KB_LS, KB_RS, KB_MENU, KB_VIEW,
    KB_DUP, KB_DDOWN, KB_DLEFT, KB_DRIGHT,
    KB_COUNT
};

static const struct {
    const WCHAR *key; /* in [Keys] */
    const WCHAR *def;
    WORD button; /* 0 = analog */
} kKbControls[KB_COUNT] = {
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

#define KB_MAX_KEYS 4
static BYTE g_kbKeys[KB_COUNT][KB_MAX_KEYS];
static int g_kbOn, g_kbFocusOnly;
static float g_kbSteerSpeed, g_kbSteerReturn, g_kbPedalSpeed, g_kbLookSpeed;
static SRWLOCK g_kbLock = SRWLOCK_INIT;
static float g_kbSteer, g_kbThrottle, g_kbBrake, g_kbLookX, g_kbLookY;
static double g_kbLast;
static XINPUT_GAMEPAD g_kbOut; /* the keyboard's pad, last poll */
static DWORD g_kbPacket;
static volatile ULONGLONG g_kbInputAt; /* the last time a key moved the pad (the icons follow it) */

/* for other mods (and an old DualSense.dll): when the keyboard was last used (GetTickCount64), 0 = never */
__declspec(dllexport) ULONGLONG Fh2KeyboardLastInput(void) { return g_kbInputAt; }

static const struct {
    const WCHAR *name;
    BYTE vk;
} kKbNames[] = {
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

static BYTE kb_vk_of(const WCHAR *name)
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
    for (int i = 0; i < (int)(sizeof kKbNames / sizeof kKbNames[0]); i++)
        if (CompareStringOrdinal(name, -1, kKbNames[i].name, -1, TRUE) == CSTR_EQUAL)
            return kKbNames[i].vk;
    return 0;
}

/* an old mods\Keyboard.ini: its [Keyboard] and [Keys] into DualSense.ini, once (when DualSense.ini has no keyboard) */
static void kb_take_old_ini(void)
{
    static const WCHAR *const kSections[] = {L"Keyboard", L"Keys"};
    WCHAR old[MAX_PATH], probe[8];
    lstrcpyW(old, g_modDir);
    lstrcatW(old, L"Keyboard.ini");
    if (GetFileAttributesW(old) == INVALID_FILE_ATTRIBUTES ||
        GetPrivateProfileStringW(L"Keyboard", L"Enabled", L"", probe, 8, g_modIni))
        return;
    for (int s = 0; s < 2; s++) {
        static WCHAR buf[8192];
        DWORD n = GetPrivateProfileSectionW(kSections[s], buf, 8192, old);
        for (WCHAR *p = buf; n && *p; p += lstrlenW(p) + 1) {
            WCHAR *eq = p;
            while (*eq && *eq != '=')
                eq++;
            if (*eq != '=')
                continue;
            *eq = 0;
            WritePrivateProfileStringW(kSections[s], p, eq + 1, g_modIni);
            *eq = '=';
        }
    }
    fh2_say("keyboard: the settings of the old Keyboard.ini taken into DualSense.ini");
}

static void kb_load_settings(void)
{
    kb_take_old_ini();
    g_kbOn = fh2_ini_int(L"Keyboard", L"Enabled", 1);
    g_kbFocusOnly = fh2_ini_int(L"Keyboard", L"OnlyWhenGameInFront", 1);
    g_kbSteerSpeed = fh2_ini_float(L"Keyboard", L"SteerSpeed", 3.0f);
    g_kbSteerReturn = fh2_ini_float(L"Keyboard", L"SteerReturn", 5.0f);
    g_kbPedalSpeed = fh2_ini_float(L"Keyboard", L"PedalSpeed", 8.0f);
    g_kbLookSpeed = fh2_ini_float(L"Keyboard", L"LookSpeed", 0.0f);
    for (int c = 0; c < KB_COUNT; c++) {
        WCHAR list[128], name[32];
        int k = 0, n = 0;
        fh2_ini_str(L"Keys", kKbControls[c].key, kKbControls[c].def, list, 128);
        for (WCHAR *p = list;; p++) {
            if (*p == ',' || *p == 0) {
                while (n && name[n - 1] == ' ')
                    n--;
                name[n] = 0;
                if (n) {
                    BYTE vk = kb_vk_of(name);
                    if (vk && k < KB_MAX_KEYS)
                        g_kbKeys[c][k++] = vk;
                    else if (!vk)
                        fh2_say_w("keyboard: unknown key name in [Keys]: ", name);
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

static int kb_down(int c)
{
    for (int k = 0; k < KB_MAX_KEYS && g_kbKeys[c][k]; k++)
        if (GetAsyncKeyState(g_kbKeys[c][k]) & 0x8000)
            return 1;
    return 0;
}

/* v towards target at rate per second (rate 0: at once) */
static float kb_approach(float v, float target, float rate, float dt)
{
    float d = target - v, step = rate * dt;
    if (rate <= 0 || (d <= step && d >= -step))
        return target;
    return v + (d > 0 ? step : -step);
}

/* the keyboard's pad now (on every poll of the engine); returns its packet number */
static DWORD kb_poll(XINPUT_GAMEPAD *out)
{
    XINPUT_GAMEPAD g;
    AcquireSRWLockExclusive(&g_kbLock);
    double now = fh2_now();
    float dt = g_kbLast ? (float)(now - g_kbLast) : 0;
    int in = !g_kbFocusOnly || fh2_foreground();
    g_kbLast = now;
    if (dt > 0.1f)
        dt = 0.1f;
    ZeroMemory(&g, sizeof g);
    float steer = in ? (float)(kb_down(KB_RIGHT) - kb_down(KB_LEFT)) : 0;
    /* turning in at SteerSpeed, back to the centre (or across it) at SteerReturn */
    g_kbSteer = kb_approach(g_kbSteer, steer, steer == 0 || steer * g_kbSteer < 0 ? g_kbSteerReturn : g_kbSteerSpeed, dt);
    g_kbThrottle = kb_approach(g_kbThrottle, in && kb_down(KB_THROTTLE) ? 1.0f : 0.0f, g_kbPedalSpeed, dt);
    g_kbBrake = kb_approach(g_kbBrake, in && kb_down(KB_BRAKE) ? 1.0f : 0.0f, g_kbPedalSpeed, dt);
    g_kbLookX = kb_approach(g_kbLookX, in ? (float)(kb_down(KB_LOOK_RIGHT) - kb_down(KB_LOOK_LEFT)) : 0, g_kbLookSpeed, dt);
    g_kbLookY = kb_approach(g_kbLookY, in ? (float)(kb_down(KB_LOOK_UP) - kb_down(KB_LOOK_DOWN)) : 0, g_kbLookSpeed, dt);
    g.sThumbLX = fh2_axis(g_kbSteer);
    g.sThumbLY = fh2_axis(in ? (float)(kb_down(KB_STICK_UP) - kb_down(KB_STICK_DOWN)) : 0);
    g.sThumbRX = fh2_axis(g_kbLookX);
    g.sThumbRY = fh2_axis(g_kbLookY);
    g.bRightTrigger = (BYTE)(g_kbThrottle * 255.0f + 0.5f);
    g.bLeftTrigger = (BYTE)(g_kbBrake * 255.0f + 0.5f);
    if (in)
        for (int c = 0; c < KB_COUNT; c++)
            if (kKbControls[c].button && kb_down(c))
                g.wButtons |= kKbControls[c].button;
    if (memcmp(&g, &g_kbOut, sizeof g)) {
        g_kbOut = g;
        g_kbPacket++;
    }
    if (g.wButtons || g.bLeftTrigger || g.bRightTrigger || g.sThumbLX || g.sThumbLY || g.sThumbRX || g.sThumbRY)
        g_kbInputAt = GetTickCount64();
    DWORD packet = g_kbPacket;
    ReleaseSRWLockExclusive(&g_kbLock);
    *out = g;
    return packet;
}

/* at load: on unless switched off, or an old Keyboard.dll does it */
static int kb_install(void)
{
    WCHAR old[MAX_PATH];
    kb_load_settings();
    lstrcpyW(old, g_modDir);
    lstrcatW(old, L"Keyboard.dll");
    if (GetFileAttributesW(old) != INVALID_FILE_ATTRIBUTES) {
        fh2_say("keyboard: an old Keyboard.dll is in mods\\ -- it does the keyboard; the keyboard is in DualSense.dll now, "
                "Keyboard.dll can go");
        return 0;
    }
    if (!g_kbOn) {
        fh2_say("keyboard: off ([Keyboard] Enabled=0)");
        return 0;
    }
    fh2_say("keyboard: on -- steering %.1f/%.1f per second, pedals %.1f", g_kbSteerSpeed, g_kbSteerReturn, g_kbPedalSpeed);
    return 1;
}
