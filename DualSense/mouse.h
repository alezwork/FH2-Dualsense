/* mouse.h -- the mouse looks around: its movement becomes the right stick of the keyboard's pad (keyboard.h), merged
   with the DualSense into the one gamepad the game gets. The movement comes as raw input (to a window of the mod's
   own, on its own thread), so it goes on at the screen's edge; Azimuth and Extra Options take none. Like the game's
   look on the stick, the view goes where the mouse puts it and back to the front when the mouse is still. While the
   game is in front the cursor is kept in its window. The buttons are keys in [Keys] (Mouse1..Mouse5). Settings:
   [Mouse] in DualSense.ini. */
#pragma once

enum { MS_HOLD, MS_SPEED };

static int g_msOn, g_msStyle, g_msInvertY, g_msLock;
static float g_msSens, g_msReturn, g_msDelay;
static volatile LONG g_msDX, g_msDY;           /* counts since the last reading */
static float g_msX, g_msY;                      /* the look stick, -1..1 (under g_kbLock) */
static double g_msMovedAt, g_msTakenAt;
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
    if (!g_kbOn)
        g_msOn = 0;
    if (g_msOn)
        fh2_say("mouse: on -- looking around (%s), sensitivity %.2f, back to the front at %.1f per second after %.2f s%s",
                g_msStyle == MS_SPEED ? "speed" : "hold", g_msSens, g_msReturn, g_msDelay,
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
            ri.header.dwType == RIM_TYPEMOUSE && !(ri.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) &&
            (ri.data.mouse.lLastX || ri.data.mouse.lLastY)) {
            InterlockedAdd(&g_msDX, ri.data.mouse.lLastX);
            InterlockedAdd(&g_msDY, ri.data.mouse.lLastY);
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

/* the look stick now (kb_poll, under g_kbLock): in = the keyboard counts now, dt = since its last reading */
static void ms_poll(int in, double now, float dt, float *x, float *y)
{
    float dx, dy;
    if (!g_msOn || !in) {
        InterlockedExchange(&g_msDX, 0), InterlockedExchange(&g_msDY, 0);
        g_msX = g_msY = 0;
        *x = *y = 0;
        return;
    }
    if (g_msStyle == MS_SPEED) { /* a speed needs some time between readings: the engine may read twice a frame */
        dt = (float)(now - g_msTakenAt);
        if (dt < 0.004f) {
            *x = g_msX, *y = g_msY;
            return;
        }
        if (dt > 0.1f)
            dt = 0.1f;
        g_msTakenAt = now;
    }
    dx = (float)InterlockedExchange(&g_msDX, 0), dy = (float)InterlockedExchange(&g_msDY, 0);
    if (g_msInvertY)
        dy = -dy;
    if (dx || dy)
        g_msMovedAt = now;
    if (g_msStyle == MS_HOLD) { /* 1.0: 500 counts from the centre to the stick's edge */
        g_msX = ms_clamp(g_msX + dx * g_msSens / 500.0f);
        g_msY = ms_clamp(g_msY - dy * g_msSens / 500.0f);
        if (now - g_msMovedAt >= g_msDelay) {
            g_msX = kb_approach(g_msX, 0, g_msReturn, dt);
            g_msY = kb_approach(g_msY, 0, g_msReturn, dt);
        }
    } else { /* 1.0: 2000 counts a second for the stick's edge, eased */
        float k = dt * 20.0f > 1 ? 1 : dt * 20.0f;
        g_msX += (ms_clamp(dx / dt * g_msSens / 2000.0f) - g_msX) * k;
        g_msY += (ms_clamp(-dy / dt * g_msSens / 2000.0f) - g_msY) * k;
        if (g_msX < 0.01f && g_msX > -0.01f && !dx)
            g_msX = 0;
        if (g_msY < 0.01f && g_msY > -0.01f && !dy)
            g_msY = 0;
    }
    *x = g_msX, *y = g_msY;
}
