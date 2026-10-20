/* page.h -- the DualSense page (MODS > Installed mods > DualSense; the FH2 Mod Platform's pages, API 8): the
   controller (its model, how it is connected, its charge, its firmware, how it vibrates), what the mod reads from the
   game, and three tests: the vibration (left, right, the menus' ticks, a click, a counter, a landing), the triggers
   (the pedals, ABS, a locked wheel, wheel spin, gravel, a kerb, gear changes) and the lights. The rows are written
   each time the page opens; A on a test starts it (the platform calls us on the game's interface thread: we only
   note what to do, the controller thread does it). */
#pragma once

#define PAGE_KEY "Controller"

/* the rows' changing texts (set_page_rows copies them) */
static char g_pageBattery[48], g_pageFirmware[24], g_pageFirmwareDesc[400];

static void page_row(FH2PageRow *r, const char *key, const char *label, const char *value, const char *desc, int style,
                     int alert)
{
    ZeroMemory(r, sizeof *r);
    r->size = sizeof *r;
    r->key = key;
    r->label = label;
    r->value = value;
    r->description = desc;
    r->style = style;
    r->value_alert = alert;
}

static void page_rows(void)
{
    FH2PageRow r[16];
    int n = 0, ru = S.menuRu, on = g_connected && g_ds.h;
    const char *yes = ru ? "Да" : "Yes", *no = ru ? "Нет" : "No";
    if (!g_api || g_api->api_version < 8)
        return;
    if (!on)
        lstrcpyA(g_pageBattery, "-");
    else if (!T.battery)
        lstrcpyA(g_pageBattery, "...");
    else if (T.charging)
        wsprintfA(g_pageBattery, ru ? "Заряжается, %d%%" : "Charging, %d%%", T.battery);
    else if (T.battery >= 100 && !g_ds.bt)
        lstrcpyA(g_pageBattery, ru ? "Заряжен" : "Full");
    else
        wsprintfA(g_pageBattery, "%d%%", T.battery);
    if (on && g_ds.update) {
        wsprintfA(g_pageFirmware, "%04X", g_ds.update);
        wsprintfA(g_pageFirmwareDesc,
                  ru ? "Версия обновления %04X (её показывает PS5). Прошивка 0x%08lX от %s, плата 0x%08lX."
                     : "Update version %04X (as a PS5 shows it). Firmware 0x%08lX built %s, hardware 0x%08lX.",
                  g_ds.update, g_ds.fw, g_ds.built[0] ? g_ds.built : "?", g_ds.hw);
    } else {
        lstrcpyA(g_pageFirmware, "-");
        lstrcpyA(g_pageFirmwareDesc, ru ? "Прошивка контроллера: видна, когда он подключён."
                                        : "The controller's firmware: shown while it is connected.");
    }

    page_row(&r[n++], NULL, ru ? "Контроллер" : "Controller", NULL, NULL, FH2_ROW_HEADING, 0);
    page_row(&r[n++], NULL, ru ? "Модель" : "Model",
             on ? (g_ds.edge ? "DualSense Edge" : "DualSense") : (ru ? "Не подключён" : "Not connected"),
             ru ? "Контроллер PlayStation 5 по USB-кабелю или Bluetooth. Мод ищет его каждые 2 секунды."
                : "The PlayStation 5 controller over a USB cable or Bluetooth. The mod looks for it every 2 seconds.",
             FH2_ROW_NORMAL, !on);
    page_row(&r[n++], NULL, ru ? "Подключение" : "Connection", on ? (g_ds.bt ? "Bluetooth" : "USB") : "-",
             ru ? "По USB работает HD-вибрация (свои волны на вибромоторах контроллера); по Bluetooth — эмуляция "
                  "вибромоторов."
                : "Over USB the vibration is HD haptics (our own waveforms on the controller's actuators); over Bluetooth "
                  "its rumble emulation.",
             FH2_ROW_NORMAL, 0);
    page_row(&r[n++], NULL, ru ? "Заряд" : "Battery", g_pageBattery,
             ru ? "Заряд батареи. Кнопка PS показывает его на индикаторах игрока (1-5 огоньков)."
                : "The battery's charge. The PS button shows it on the player lights (1-5 lights).",
             FH2_ROW_NORMAL, on && T.battery && T.battery <= 15 && !T.charging);
    page_row(&r[n++], NULL, ru ? "Прошивка" : "Firmware", g_pageFirmware, g_pageFirmwareDesc, FH2_ROW_NORMAL, 0);
    page_row(&r[n++], NULL, ru ? "Вибрация" : "Vibration",
             !S.haptics      ? (ru ? "Выключена" : "Off")
             : g_hapRunning  ? (ru ? "HD-вибрация" : "HD haptics")
             : on            ? (ru ? "Эмуляция моторов" : "Rumble emulation")
                             : "-",
             ru ? "HD-вибрация — по USB, когда в Windows есть аудиоустройство контроллера; иначе эмуляция вибромоторов."
                : "HD haptics: over USB, with the controller's audio device in Windows; else the rumble emulation.",
             FH2_ROW_NORMAL, 0);

    page_row(&r[n++], NULL, ru ? "Из игры" : "From the game", NULL, NULL, FH2_ROW_HEADING, 0);
    page_row(&r[n++], NULL, ru ? "Данные машины" : "The car's data", Car.hooked ? yes : no,
             ru ? "Обороты, проскальзывание шин, удары, переключения: для курков и вибрации."
                : "Revs, the tyres' slip, crashes, gear changes: for the triggers and the vibration.",
             FH2_ROW_NORMAL, !Car.hooked);
    page_row(&r[n++], NULL, ru ? "Покрытие под колёсами" : "Surface under the wheels", Surf.hooked ? yes : no,
             ru ? "Асфальт, гравий, грунт, трава, песок, брусчатка, поребрики — под каждым колесом."
                : "Tarmac, gravel, dirt, grass, sand, cobbles, kerbs -- under each wheel.",
             FH2_ROW_NORMAL, !Surf.hooked);
    page_row(&r[n++], NULL, ru ? "Звуки интерфейса" : "Interface sounds", g_soundsHooked ? yes : no,
             ru ? "Звуки меню и событий игры: из них вибро-отклик меню, барабан призов, навыки и комбо."
                : "The game's menu and event sounds: the menu haptics, the tombola's drum, skills and combos come from them.",
             FH2_ROW_NORMAL, !g_soundsHooked);
    page_row(&r[n++], NULL, "ABS", g_abs.on ? (ru ? "Включён" : "On") : (ru ? "Выключен" : "Off"),
             ru ? "Как его видят тормоза: колесо заблокировалось при торможении — ABS нет (курок L2 твердеет); три "
                  "сильных торможения на пределе без блокировки — ABS есть (L2 пульсирует)."
                : "As the brakes show it: a wheel locked under braking -- no ABS (L2 goes hard); three hard stops at the "
                  "limit without locking -- ABS (L2 pulses).",
             FH2_ROW_NORMAL, 0);

    page_row(&r[n++], NULL, ru ? "Проверка" : "Tests", NULL, NULL, FH2_ROW_HEADING, 0);
    page_row(&r[n++], "TestVibration", ru ? "Проверить вибрацию" : "Test the vibration", "A",
             ru ? "Слева, справа, щелчки меню слева направо, клик, счётчик, приземление."
                : "Left, right, the menu's ticks from left to right, a click, a counter, a landing.",
             FH2_ROW_NORMAL, 0);
    page_row(&r[n++], "TestTriggers", ru ? "Проверить курки" : "Test the triggers", "A",
             ru ? "Нажимайте L2 и R2: педали, ABS, заблокированное колесо, пробуксовка, гравий, поребрик, переключения "
                  "передач (по 1,5 секунды)."
                : "Press L2 and R2: the pedals, ABS, a locked wheel, wheel spin, gravel, a kerb, gear changes (1.5 seconds "
                  "each).",
             FH2_ROW_NORMAL, 0);
    page_row(&r[n++], "TestLights", ru ? "Проверить подсветку" : "Test the lights", "A",
             ru ? "Световая панель по кругу цветов, индикаторы игрока бегущим огнём (4 секунды)."
                : "The light bar round the colours, the player lights running (4 seconds).",
             FH2_ROW_NORMAL, 0);
    g_api->set_page_rows(g_mod, PAGE_KEY, r, (uint32_t)n);
}

static void page_open(void *user, const char *page_key)
{
    (void)user, (void)page_key;
    page_rows();
}

/* the vibration test: its voices, from 0.25 s on (after the menu's own click) */
static void test_vibration(void)
{
    const float t = 0.25f;
    fx_add(FX_THUMP, t, 0.8f, 50, 0, 0, 0.12f, -1, 0);
    fx_add(FX_THUMP, t + 0.5f, 0.8f, 50, 0, 0, 0.12f, 1, 0);
    for (int i = 0; i < 5; i++)
        fx_add(FX_TICK, t + 1.0f + 0.12f * i, 0.45f, 235, 0, 0, 0.0035f, -1 + 0.5f * i, 0);
    fx_add(FX_TICK, t + 1.8f, 0.62f, 170, 0, 0, 0.006f, 0, 0);
    fx_add(FX_THUMP, t + 1.8f, 0.3f, 75, 0, 0, 0.014f, 0, 0);
    fx_add(FX_RATCHET, t + 2.2f, 0.3f, 230, 22, 0.8f, 0.003f, 0, 0);
    fx_add(FX_SWEEP, t + 3.2f, 0.35f, 80, 240, 0.25f, 0, 0, 0);
    fx_add(FX_THUMP, t + 3.6f, 0.95f, 42, 0, 0, 0.11f, 0, 0);
    fx_add(FX_NOISE, t + 3.6f, 0.4f, 240, 0, 0, 0.03f, 0, 0);
}

static void page_select(void *user, const char *page_key, const char *row_key)
{
    double now = fh2_now();
    (void)user, (void)page_key;
    if (!row_key)
        return;
    if (!strcmp(row_key, "TestVibration")) {
        g_test.kind = TEST_VIBRATION, g_test.at = now;
        test_vibration();
    } else if (!strcmp(row_key, "TestTriggers")) {
        g_test.at = now + 0.25, g_test.kind = TEST_TRIGGERS;
    } else if (!strcmp(row_key, "TestLights")) {
        g_test.at = now, g_test.kind = TEST_LIGHTS;
    } else
        return;
    fh2_say("DualSense page: %s", row_key);
    InterlockedExchange(&g_outDirty, 1);
    if (g_wake)
        SetEvent(g_wake);
}

/* from fh2mod_init (a page added later would only show from the next start) */
static void page_add(void)
{
    FH2Page p;
    if (g_api->api_version < 8) {
        fh2_say("DualSense page: the platform is too old for pages (2.7.0 or newer)");
        return;
    }
    ZeroMemory(&p, sizeof p);
    p.size = sizeof p;
    p.key = PAGE_KEY;
    p.title = "DualSense";
    p.label = S.menuRu ? "Контроллер и проверка" : "The controller and tests";
    p.description = S.menuRu ? "Подключение, заряд и прошивка DualSense; проверка вибрации, курков и подсветки."
                             : "The DualSense's connection, charge and firmware; tests of the vibration, the triggers and "
                               "the lights.";
    p.on_open = page_open;
    p.on_select = page_select;
    if (g_api->add_page(g_mod, &p))
        page_rows();
    else
        fh2_say("DualSense page: the platform did not add it (see its log)");
}
