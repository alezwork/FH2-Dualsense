/* mouse.h -- the mouse on the keyboard's pad (keyboard.h), merged with the DualSense into the one gamepad the game
   gets. Driving, it looks around (the right stick): like the game's look on the stick, the view goes where the mouse
   puts it and back to the front when the mouse is still. In the menus (the car's feedback not coming: menus, paused,
   loading) it moves the left stick by its speed -- the map's cursor, the lists --, its wheel presses the D-pad (or
   LB/RB) and its left and right buttons are A and B. The movement comes as raw input (to a window of the mod's own, on its own thread), so it goes on at the
   screen's edge; Azimuth and Extra Options take none. While the game is in front the cursor is kept in its window.
   The buttons are keys in [Keys] (Mouse1..Mouse5). Settings: [Mouse] in DualSense.ini. */
#pragma once

enum { MS_HOLD, MS_SPEED };
enum { MS_MENU_CURSOR, MS_MENU_LOOK, MS_MENU_OFF };

static int ms_menus(double now); /* DualSense.c, from the car's feedback */

static int g_msOn, g_msStyle, g_msInvertY, g_msLock, g_msMenu, g_msClicks;
static float g_msSens, g_msReturn, g_msDelay, g_msMenuSens;
static WORD g_msWheelUp, g_msWheelDown;          /* the buttons the wheel presses in the menus (0: none) */
static volatile LONG g_msDX, g_msDY, g_msWheel; /* counts since the last reading; the wheel in 1/120 notches */
static float g_msX, g_msY;                       /* the stick the mouse moves, -1..1 (under g_kbLock) */
static int g_msInMenu, g_msNotches, g_msPart;    /* where the stick is; wheel notches waiting (+ up, - down) */
static double g_msMovedAt, g_msTakenAt, g_msPressUntil;
static WORD g_msPress;
static volatile LONG g_msClipped;

static void ms_load_settings(void)
{
    WCHAR v[32];
    g_msOn = fh2_ini_int(L"Mouse", L"Enabled", 1);
    fh2_ini_str(L"Mouse", L"Look", L"Hold", v, 32);
    g_msStyle = CompareStringOrdinal(v, -1, L"Speed", -1, TRUE) == CSTR_EQUAL ? MS_SPEED : MS_HOLD;
    g_msSens = fh2_ini_float(L"Mouse", L"Sensitivity", 1.0f);
    g_msReturn = fh2_ini_float(L"Mouse", L"Return", 3.0f);
    g_msDelay = fh2_ini_float(L"Mouse", L"ReturnAfter", 0.3f);
    g_msInvertY = fh2_ini_int(L"Mouse", L"InvertY", 0);
    g_msLock = fh2_ini_int(L"Mouse", L"KeepCursorInWindow", 1);
    fh2_ini_str(L"Mouse", L"Menus", L"Cursor", v, 32);
    g_msMenu = CompareStringOrdinal(v, -1, L"Look", -1, TRUE) == CSTR_EQUAL  ? MS_MENU_LOOK
               : CompareStringOrdinal(v, -1, L"Off", -1, TRUE) == CSTR_EQUAL ? MS_MENU_OFF
                                                                             : MS_MENU_CURSOR;
    g_msMenuSens = fh2_ini_float(L"Mouse", L"MenuSensitivity", 1.0f);
    fh2_ini_str(L"Mouse", L"Wheel", L"DPad", v, 32);
    if (CompareStringOrdinal(v, -1, L"Shoulders", -1, TRUE) == CSTR_EQUAL)
        g_msWheelUp = XINPUT_GAMEPAD_LEFT_SHOULDER, g_msWheelDown = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    else if (CompareStringOrdinal(v, -1, L"Off", -1, TRUE) == CSTR_EQUAL)
        g_msWheelUp = g_msWheelDown = 0;
    else
        g_msWheelUp = XINPUT_GAMEPAD_DPAD_UP, g_msWheelDown = XINPUT_GAMEPAD_DPAD_DOWN;
    g_msClicks = fh2_ini_int(L"Mouse", L"Clicks", 1);
    if (!g_kbOn)
        g_msOn = 0;
    if (g_msOn)
        fh2_say("mouse: on -- looking around (%s), sensitivity %.2f, back to the front at %.1f per second after %.2f s; "
                "in the menus %s, the wheel %s, clicks %s%s",
                g_msStyle == MS_SPEED ? "speed" : "hold", g_msSens, g_msReturn, g_msDelay,
                g_msMenu == MS_MENU_CURSOR ? "the left stick" : g_msMenu == MS_MENU_LOOK ? "looking around" : "nothing",
                !g_msWheelUp ? "off" : g_msWheelUp == XINPUT_GAMEPAD_DPAD_UP ? "on the D-pad" : "on LB/RB",
                g_msClicks ? "A/B" : "off",
                g_msLock ? "; the cursor stays in the game's window" : "");
    else
        fh2_say("mouse: off ([Mouse] Enabled=0 or the keyboard off)");
}

/* the cursor kept in the game's window while it is in front, the keyboard's pad is in the game and the mouse has
   looked around */
static void ms_clip(void)
{
    HWND w = GetForegroundWindow();
    DWORD pid = 0;
    RECT r;
    POINT p = {0, 0};
    if (w)
        GetWindowThreadProcessId(w, &pid);
    if (g_msLock && g_kbJoined && g_msMovedAt > 0 && w && pid == GetCurrentProcessId() && !IsIconic(w) &&
        GetClientRect(w, &r) && r.right > r.left && r.bottom > r.top && ClientToScreen(w, &p)) {
        OffsetRect(&r, p.x, p.y);
        ClipCursor(&r);
        g_msClipped = 1;
    } else if (g_msClipped) {
        ClipCursor(NULL);
        g_msClipped = 0;
    }
}

static LRESULT CALLBACK ms_wndproc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_INPUT) {
        RAWINPUT ri;
        UINT n = sizeof ri;
        if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, &ri, &n, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
            ri.header.dwType == RIM_TYPEMOUSE) {
            if (!(ri.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) && (ri.data.mouse.lLastX || ri.data.mouse.lLastY)) {
                InterlockedAdd(&g_msDX, ri.data.mouse.lLastX);
                InterlockedAdd(&g_msDY, ri.data.mouse.lLastY);
            }
            if (ri.data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
                InterlockedAdd(&g_msWheel, (SHORT)ri.data.mouse.usButtonData);
        }
    } else if (m == WM_TIMER)
        ms_clip();
    return DefWindowProcW(w, m, wp, lp);
}

static DWORD WINAPI ms_thread(void *arg)
{
    WNDCLASSW wc;
    RAWINPUTDEVICE rid;
    HWND w;
    MSG msg;
    (void)arg;
    ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = ms_wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"FH2DualSenseMouse";
    RegisterClassW(&wc);
    w = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, wc.hInstance, NULL);
    rid.usUsagePage = 0x01, rid.usUsage = 0x02; /* generic desktop, mouse */
    rid.dwFlags = RIDEV_INPUTSINK, rid.hwndTarget = w;
    if (!w || !RegisterRawInputDevices(&rid, 1, sizeof rid)) {
        fh2_say("mouse: no raw input (%lu) -- the mouse does not look around", GetLastError());
        return 0;
    }
    SetTimer(w, 1, 250, NULL);
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
        DispatchMessageW(&msg);
    return 0;
}

static void ms_start(void)
{
    HANDLE h;
    if (!g_msOn)
        return;
    h = CreateThread(NULL, 0, ms_thread, NULL, 0, NULL);
    if (h)
        CloseHandle(h);
}

static float ms_clamp(float v) { return v > 1 ? 1 : v < -1 ? -1 : v; }

/* the wheel in the menus: a press of WheelUp/WheelDown for each notch, long enough for the game to see it, with a gap
   between two (up to 5 notches wait their turn) */
static WORD ms_wheel(int menu, double now)
{
    int w = (int)InterlockedExchange(&g_msWheel, 0);
    if (!menu || !g_msWheelUp) {
        g_msPart = g_msNotches = 0, g_msPress = 0;
        return 0;
    }
    g_msPart += w;
    g_msNotches += g_msPart / 120, g_msPart %= 120;
    if (g_msNotches > 5)
        g_msNotches = 5;
    if (g_msNotches < -5)
        g_msNotches = -5;
    if (now < g_msPressUntil)
        return g_msPress;
    if (now < g_msPressUntil + 0.05 || !g_msNotches) /* the gap after a press, or nothing waiting */
        return 0;
    g_msPress = g_msNotches > 0 ? g_msWheelUp : g_msWheelDown;
    g_msNotches += g_msNotches > 0 ? -1 : 1;
    g_msPressUntil = now + 0.07;
    return g_msPress;
}

/* the mouse's sticks now (kb_poll, under g_kbLock): in = the keyboard counts now, dt = since its last reading;
   st = the left stick's x, y and the right's (on top of the keys'); buttons |= the wheel's and the clicks */
static void ms_poll(int in, double now, float dt, float st[4], WORD *buttons)
{
    float dx, dy;
    int menu, speed;
    st[0] = st[1] = st[2] = st[3] = 0;
    if (!g_msOn || !in) {
        InterlockedExchange(&g_msDX, 0), InterlockedExchange(&g_msDY, 0), InterlockedExchange(&g_msWheel, 0);
        g_msX = g_msY = 0;
        return;
    }
    menu = g_msMenu != MS_MENU_LOOK && ms_menus(now);
    *buttons |= ms_wheel(menu, now);
    if (menu && g_msClicks) /* the buttons as the user has them (swapped or not) */
        *buttons |= (GetAsyncKeyState(VK_LBUTTON) & 0x8000 ? XINPUT_GAMEPAD_A : 0) |
                    (GetAsyncKeyState(VK_RBUTTON) & 0x8000 ? XINPUT_GAMEPAD_B : 0);
    if (menu != g_msInMenu) /* into the menus or out: the stick from the centre */
        g_msX = g_msY = 0, g_msInMenu = menu;
    if (menu && g_msMenu == MS_MENU_OFF) {
        InterlockedExchange(&g_msDX, 0), InterlockedExchange(&g_msDY, 0);
        return;
    }
    speed = menu || g_msStyle == MS_SPEED;
    if (speed) { /* a speed needs some time between readings: the engine may read twice a frame */
        dt = (float)(now - g_msTakenAt);
        if (dt < 0.004f) {
            st[menu ? 0 : 2] = g_msX, st[menu ? 1 : 3] = g_msY;
            return;
        }
        if (dt > 0.1f)
            dt = 0.1f;
        g_msTakenAt = now;
    }
    dx = (float)InterlockedExchange(&g_msDX, 0), dy = (float)InterlockedExchange(&g_msDY, 0);
    if (g_msInvertY && !menu)
        dy = -dy;
    if (dx || dy)
        g_msMovedAt = now;
    if (!speed) { /* 1.0: 500 counts from the centre to the stick's edge */
        g_msX = ms_clamp(g_msX + dx * g_msSens / 500.0f);
        g_msY = ms_clamp(g_msY - dy * g_msSens / 500.0f);
        if (now - g_msMovedAt >= g_msDelay) {
            g_msX = kb_approach(g_msX, 0, g_msReturn, dt);
            g_msY = kb_approach(g_msY, 0, g_msReturn, dt);
        }
    } else { /* 1.0: 2000 counts a second for the stick's edge, eased */
        float k = dt * 20.0f > 1 ? 1 : dt * 20.0f, sens = (menu ? g_msMenuSens : g_msSens) / 2000.0f;
        g_msX += (ms_clamp(dx / dt * sens) - g_msX) * k;
        g_msY += (ms_clamp(-dy / dt * sens) - g_msY) * k;
        if (g_msX < 0.01f && g_msX > -0.01f && !dx)
            g_msX = 0;
        if (g_msY < 0.01f && g_msY > -0.01f && !dy)
            g_msY = 0;
    }
    st[menu ? 0 : 2] = g_msX, st[menu ? 1 : 3] = g_msY;
}
