/* sounds.h -- the game's own interface sounds, felt: a tick for every move in the menus, a click for every choice, a
   swell as a page opens, the tombola's drum ticking by, the skills, combos, checkpoints, countdowns, level-ups...
   Every interface sound of the game (media\audio\ui\UI4Audio.xml: its "triggers", /Master/UI/...) is played by an
   object of the game's audio engine (AudioObjectUIEvent): its methods 0 and 1 (FH2 1.0.0.64: 0x100ED70 and 0x100ED00,
   (this, const std::string &trigger, ...)) start one, the engine's StopUIEvent (0x10229B0, (engine, const std::string
   &trigger)) stops it -- whether the game's code or a XAML storyboard (local:AudioEvent, as the tombola's) asks for
   it. All three are hooked (MinHook; found by their bytes); ours reads the trigger's name, picks the feel for it (kUi,
   the first match) and goes on into the game's.
   The skills also drive the combo on the player lights: one more light for every skill in the chain, all five
   blinking when it is banked, the middle one when it is lost. */
#pragma once

enum {
    P_NONE, P_MOVE, P_SCROLL, P_SLIDER, P_SELECT, P_BACK, P_TAB, P_OPEN, P_CLOSE, P_PAUSE, P_FAIL, P_MESSAGE,
    P_SKILL, P_STREAK, P_BIGSKILL, P_MULTIPLIER, P_BANK, P_LOSE, P_SKILLPOINT, P_XP, P_CHECKPOINT, P_MISSED,
    P_COUNT_HIGH, P_COUNT_LOW, P_COUNT_GO, P_NOTIFY, P_NOTIFY_BIG, P_SPEEDTRAP, P_CELEBRATE, P_CELEBRATE_BIG,
    P_RATCHET, P_THUD, P_RING, P_TOMBOLA, P_STAMP, P_SHUTTER
};
enum { G_NONE, G_MENU, G_EVENT };

/* what in the trigger's name -> menus or events, the feel */
static const struct {
    const char *has;
    BYTE group, preset;
} kUi[] = {
    /* the tombola (a prize at each level-up) */
    {"Tombola_Spin", G_EVENT, P_TOMBOLA},
    {"Tombola_Win_Gold", G_EVENT, P_CELEBRATE_BIG},
    {"Tombola_Winner", G_EVENT, P_CELEBRATE_BIG},
    {"Tombola_Win_Silver", G_EVENT, P_CELEBRATE},
    {"Tombola_Win", G_EVENT, P_THUD},
    {"Tombola_Lose", G_EVENT, P_LOSE},
    {"Tombola_Open", G_MENU, P_OPEN},
    {"Tombola_Close", G_MENU, P_CLOSE},
    {"Tombola_SeeRep", G_MENU, P_SELECT},
    /* skills: near misses, drifts, air, ... and their chain */
    {"SkillNameCleanStreak", G_EVENT, P_STREAK},
    {"SkillNameHighValue", G_EVENT, P_BIGSKILL},
    {"SkillNameAppear", G_EVENT, P_SKILL},
    {"DrivingSkills_Multiplier", G_EVENT, P_MULTIPLIER},
    {"DrivingSkills_Combo", G_EVENT, P_MULTIPLIER},
    {"PointsBank", G_EVENT, P_BANK},
    {"PointsLose", G_EVENT, P_LOSE},
    {"SkillPointEarned", G_EVENT, P_SKILLPOINT},
    {"XPEarned", G_EVENT, P_XP},
    {"PointsCount", G_NONE, P_NONE}, /* the chain's points counting up while driving: the road is felt instead */
    /* races, speed traps and zones, rivals */
    {"CheckpointPassed", G_EVENT, P_CHECKPOINT},
    {"Checkpoint_Missed", G_EVENT, P_MISSED},
    {"Countdown_High", G_EVENT, P_COUNT_HIGH},
    {"Countdown_Low", G_EVENT, P_COUNT_LOW},
    {"TimerCountdown", G_EVENT, P_COUNT_LOW},
    {"Countdown_End", G_EVENT, P_COUNT_GO},
    {"SpeedTrap", G_EVENT, P_SPEEDTRAP},
    {"SpeedStatsOvertakeShine", G_NONE, P_NONE},
    {"SpeedStatsOvertake", G_EVENT, P_NOTIFY_BIG},
    {"SpeedStatsMajorOn", G_EVENT, P_NOTIFY_BIG},
    {"SpeedStatsMinorOn", G_EVENT, P_NOTIFY},
    {"ScoreFlash", G_EVENT, P_NOTIFY},
    {"Race_SignUp", G_EVENT, P_SELECT},
    {"RaceComplete", G_EVENT, P_THUD},
    /* after a race: the results, the counters, a level-up */
    {"PositionResultOn", G_EVENT, P_THUD},
    {"TotalScreenFlash", G_EVENT, P_THUD},
    {"LevelUpBarFull", G_EVENT, P_CELEBRATE},
    {"XPBarLevelUp", G_EVENT, P_CELEBRATE_BIG},
    {"LevelUpNumberDrop", G_EVENT, P_THUD},
    {"Count_lp", G_EVENT, P_RATCHET},
    {"BarFill_lp", G_EVENT, P_RATCHET},
    {"TotalOn", G_EVENT, P_NOTIFY_BIG},
    {"ItemOn", G_EVENT, P_NOTIFY},
    {"TitleOn", G_EVENT, P_NOTIFY},
    {"InitialFlash", G_EVENT, P_THUD},
    {"RewardFlash", G_EVENT, P_THUD},
    {"EventUnlock", G_EVENT, P_CELEBRATE},
    {"LocationUnlock", G_EVENT, P_CELEBRATE},
    {"RewardBoardDiscovered", G_EVENT, P_CELEBRATE},
    {"Perks_Activate", G_EVENT, P_CELEBRATE},
    {"GainedPlayer", G_EVENT, P_BIGSKILL},
    {"GivenPlayer", G_EVENT, P_BIGSKILL},
    {"KingLostPlayer", G_EVENT, P_LOSE},
    {"Phone_Ring", G_EVENT, P_RING},
    {"Notification_GenericOn", G_EVENT, P_NOTIFY},
    {"Notification_BoardOn", G_EVENT, P_NOTIFY},
    {"Notification_BoardUpdate", G_EVENT, P_NOTIFY},
    /* photos, the paint shop */
    {"Promo_TakePhoto", G_MENU, P_SHUTTER},
    {"Promo_Zoom", G_MENU, P_SLIDER},
    {"PaintItemStamp", G_MENU, P_STAMP},
    {"PaintItemSlide", G_MENU, P_SCROLL},
    /* the menus */
    {"PauseMenu_Open", G_MENU, P_PAUSE},
    {"ChangeTab", G_MENU, P_TAB},
    {"SubMenuOpen", G_MENU, P_OPEN},
    {"PopUpOpen", G_MENU, P_OPEN},
    {"LeaderboardOpen", G_MENU, P_OPEN},
    {"InfoScreenOpen", G_MENU, P_OPEN},
    {"_Open_", G_MENU, P_OPEN},
    {"SubMenuClose", G_MENU, P_CLOSE},
    {"PopUpClose", G_MENU, P_CLOSE},
    {"PopUpMenu_Close", G_MENU, P_CLOSE},
    {"PopUpInfo_Close", G_MENU, P_CLOSE},
    {"LeaderboardClose", G_MENU, P_CLOSE},
    {"InfoScreenClose", G_MENU, P_CLOSE},
    {"_Close_", G_MENU, P_BACK},
    {"_Back_", G_MENU, P_BACK},
    {"_Fail_", G_MENU, P_FAIL},
    {"MessageRead", G_MENU, P_MESSAGE},
    {"Vote_Result", G_MENU, P_MESSAGE},
    {"Slider", G_MENU, P_SLIDER},
    {"CarScroll", G_MENU, P_SCROLL},
    {"_Scroll_", G_MENU, P_SCROLL},
    {"_Move_", G_MENU, P_MOVE},
    {"_Select_", G_MENU, P_SELECT},
    {"Nav_Accept", G_MENU, P_SELECT},
    {"Nav_Setup", G_MENU, P_SELECT},
    {"SetRoute", G_MENU, P_SELECT},
    {"ANNA_Wake", G_MENU, P_OPEN},
    {"ANNA_Sleep", G_MENU, P_CLOSE},
};

/* the combo on the player lights */
static volatile struct {
    int level;           /* skills in the chain, 0..5 */
    double at;           /* the last skill */
    double endAt;        /* banked or lost then */
    int ended;           /* 1 banked, 2 lost */
} g_combo;

static void combo_skill(double now)
{
    g_combo.level = g_combo.level < 5 ? g_combo.level + 1 : 5;
    g_combo.at = now;
}

static void combo_end(double now, int how)
{
    if (!g_combo.level && how == 2)
        return;
    g_combo.level = 0;
    g_combo.endAt = now;
    g_combo.ended = how;
}

/* the player lights for the combo now (-1: not showing) */
static int combo_lights(double now)
{
    static const BYTE bars[6] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F};
    double e = now - g_combo.endAt;
    if (g_combo.ended == 1 && e < 0.9) /* banked: all five blink three times */
        return ((int)(e * 6.67) & 1) ? 0x00 : 0x1F;
    if (g_combo.ended == 2 && e < 0.6) /* lost: the middle one twice */
        return ((int)(e * 6.67) & 1) ? 0x00 : 0x04;
    if (g_combo.level && now - g_combo.at > 9) /* the chain is long over (its end not heard) */
        g_combo.level = 0;
    return g_combo.level ? bars[g_combo.level] : -1;
}

/* the feel of a sound; k: its group's strength; pan: where (the D-pad's direction for moves) */
static void ui_feel(int p, float k, float pan, UINT32 id)
{
    switch (p) {
    case P_MOVE: /* the lightest, crisp */
        fx_add(FX_TICK, 0, 0.6f * k, 235, 0, 0, 0.0035f, pan, 0);
        break;
    case P_SCROLL:
        fx_add(FX_TICK, 0, 0.48f * k, 255, 0, 0, 0.003f, pan, 0);
        break;
    case P_SLIDER:
        fx_add(FX_TICK, 0, 0.36f * k, 285, 0, 0, 0.0025f, pan, 0);
        break;
    case P_SELECT: /* a click with some body */
        fx_add(FX_TICK, 0, 0.8f * k, 170, 0, 0, 0.006f, 0, 0);
        fx_add(FX_THUMP, 0, 0.4f * k, 75, 0, 0, 0.014f, 0, 0);
        break;
    case P_BACK: /* softer, lower */
        fx_add(FX_TICK, 0, 0.6f * k, 120, 0, 0, 0.007f, 0, 0);
        break;
    case P_TAB: /* two ticks on the side of the bumper */
        fx_add(FX_TICK, 0, 0.6f * k, 210, 0, 0, 0.004f, pan, 0);
        fx_add(FX_TICK, 0.045f, 0.45f * k, 250, 0, 0, 0.0035f, pan, 0);
        break;
    case P_OPEN: /* a swell up */
        fx_add(FX_SWEEP, 0, 0.24f * k, 70, 170, 0.09f, 0, 0, 0);
        break;
    case P_CLOSE:
        fx_add(FX_SWEEP, 0, 0.2f * k, 160, 70, 0.08f, 0, 0, 0);
        break;
    case P_PAUSE:
        fx_add(FX_SWEEP, 0, 0.3f * k, 60, 150, 0.12f, 0, 0, 0);
        fx_add(FX_TICK, 0.11f, 0.4f * k, 200, 0, 0, 0.004f, 0, 0);
        break;
    case P_FAIL: /* no: two low buzzes */
        fx_add(FX_BUZZ, 0, 0.45f * k, 55, 0, 0.06f, 0, 0, 0);
        fx_add(FX_BUZZ, 0.1f, 0.45f * k, 55, 0, 0.06f, 0, 0, 0);
        break;
    case P_MESSAGE:
    case P_XP:
        fx_add(FX_TICK, 0, 0.42f * k, 205, 0, 0, 0.004f, 0, 0);
        break;
    case P_SKILL: /* ding-ding */
        fx_add(FX_TICK, 0, 0.5f * k, 190, 0, 0, 0.006f, 0, 0);
        fx_add(FX_TICK, 0.06f, 0.38f * k, 240, 0, 0, 0.005f, 0, 0);
        break;
    case P_STREAK: /* three, rising */
        for (int i = 0; i < 3; i++)
            fx_add(FX_TICK, 0.05f * i, 0.45f * k, 170.0f + 40 * i, 0, 0, 0.005f, 0, 0);
        break;
    case P_BIGSKILL:
        fx_add(FX_THUMP, 0, 0.55f * k, 70, 0, 0, 0.04f, 0, 0);
        fx_add(FX_TICK, 0.04f, 0.5f * k, 220, 0, 0, 0.005f, 0, 0);
        break;
    case P_MULTIPLIER: /* a rise and a tick on top */
        fx_add(FX_SWEEP, 0, 0.35f * k, 110, 240, 0.12f, 0, 0, 0);
        fx_add(FX_TICK, 0.12f, 0.45f * k, 260, 0, 0, 0.004f, 0, 0);
        break;
    case P_BANK: /* the chain banked: a thud, then two bright ticks */
        fx_add(FX_THUMP, 0, 0.7f * k, 50, 0, 0, 0.07f, 0, 0);
        fx_add(FX_TICK, 0.09f, 0.5f * k, 220, 0, 0, 0.005f, 0, 0);
        fx_add(FX_TICK, 0.15f, 0.4f * k, 260, 0, 0, 0.004f, 0, 0);
        break;
    case P_LOSE: /* deflating */
        fx_add(FX_SWEEP, 0, 0.5f * k, 150, 45, 0.22f, 0, 0, 0);
        fx_add(FX_THUMP, 0.2f, 0.4f * k, 40, 0, 0, 0.06f, 0, 0);
        break;
    case P_SKILLPOINT:
        for (int i = 0; i < 4; i++)
            fx_add(FX_TICK, 0.06f * i, 0.45f * k, 160.0f + 40 * i, 0, 0, 0.005f, 0, 0);
        fx_add(FX_THUMP, 0.26f, 0.6f * k, 60, 0, 0, 0.08f, 0, 0);
        break;
    case P_CHECKPOINT:
        fx_add(FX_TICK, 0, 0.45f * k, 200, 0, 0, 0.005f, 0, 0);
        fx_add(FX_TICK, 0.04f, 0.35f * k, 240, 0, 0, 0.004f, 0, 0);
        break;
    case P_MISSED:
        fx_add(FX_BUZZ, 0, 0.45f * k, 50, 0, 0.12f, 0, 0, 0);
        break;
    case P_COUNT_HIGH:
        fx_add(FX_TICK, 0, 0.55f * k, 180, 0, 0, 0.008f, 0, 0);
        fx_add(FX_THUMP, 0, 0.25f * k, 70, 0, 0, 0.02f, 0, 0);
        break;
    case P_COUNT_LOW:
        fx_add(FX_TICK, 0, 0.4f * k, 220, 0, 0, 0.005f, 0, 0);
        break;
    case P_COUNT_GO:
        fx_add(FX_THUMP, 0, 0.8f * k, 55, 0, 0, 0.1f, 0, 0);
        fx_add(FX_TICK, 0, 0.6f * k, 160, 0, 0, 0.008f, 0, 0);
        break;
    case P_NOTIFY:
        fx_add(FX_TICK, 0, 0.36f * k, 220, 0, 0, 0.004f, 0, 0);
        break;
    case P_NOTIFY_BIG:
        fx_add(FX_TICK, 0, 0.45f * k, 190, 0, 0, 0.005f, 0, 0);
        fx_add(FX_THUMP, 0, 0.25f * k, 70, 0, 0, 0.025f, 0, 0);
        break;
    case P_SPEEDTRAP:
        fx_add(FX_THUMP, 0, 0.6f * k, 65, 0, 0, 0.06f, 0, 0);
        fx_add(FX_TICK, 0, 0.55f * k, 230, 0, 0, 0.005f, 0, 0);
        fx_add(FX_TICK, 0.04f, 0.4f * k, 260, 0, 0, 0.004f, 0, 0);
        break;
    case P_CELEBRATE_BIG:
        fx_add(FX_THUMP, 0.55f, 0.8f * k, 50, 0, 0, 0.12f, 0, 0);
        fx_add(FX_TICK, 0.62f, 0.5f * k, 280, 0, 0, 0.004f, 0, 0);
        /* fall through: the same rise first */
    case P_CELEBRATE:
        fx_add(FX_SWEEP, 0, 0.35f * k, 80, 240, 0.25f, 0, 0, 0);
        fx_add(FX_THUMP, 0.25f, 0.6f * k, 55, 0, 0, 0.08f, 0, 0);
        for (int i = 0; i < 3; i++)
            fx_add(FX_TICK, 0.33f + 0.06f * i, 0.35f * k, 250, 0, 0, 0.004f, 0, 0);
        break;
    case P_RATCHET: /* a counter running up: ticks till the game stops its sound */
        fx_add(FX_RATCHET, 0, 0.26f * k, 230, 22, 6.0f, 0.003f, 0, id);
        break;
    case P_THUD:
        fx_add(FX_THUMP, 0, 0.6f * k, 55, 0, 0, 0.07f, 0, 0);
        fx_add(FX_TICK, 0, 0.35f * k, 160, 0, 0, 0.006f, 0, 0);
        break;
    case P_RING: /* a phone buzzing twice */
        fx_add(FX_BUZZ, 0, 0.45f * k, 170, 22, 0.35f, 0, 0, 0);
        fx_add(FX_BUZZ, 0.55f, 0.45f * k, 170, 22, 0.35f, 0, 0, 0);
        break;
    case P_TOMBOLA:
        fx_add(FX_TOMBOLA, 0, 0.5f * k, 230, 0, TOMBOLA_SPIN + 0.45f - TOMBOLA_SOUND_AT, 0.0035f, 0, id);
        break;
    case P_STAMP:
        fx_add(FX_THUMP, 0, 0.65f * k, 60, 0, 0, 0.05f, 0, 0);
        fx_add(FX_TICK, 0, 0.5f * k, 140, 0, 0, 0.006f, 0, 0);
        break;
    case P_SHUTTER:
        fx_add(FX_TICK, 0, 0.5f * k, 260, 0, 0, 0.003f, 0, 0);
        fx_add(FX_TICK, 0.03f, 0.4f * k, 180, 0, 0, 0.004f, 0, 0);
        break;
    }
}

/* the std::string (MSVC) of a trigger's name: its characters, or NULL */
static const char *std_str(const BYTE *s)
{
    SIZE_T n, cap;
    const char *p;
    if (!s)
        return NULL;
    n = *(const SIZE_T *)(s + 0x10), cap = *(const SIZE_T *)(s + 0x18);
    if (n < 4 || n > 400 || cap < n || cap > 0x100000)
        return NULL;
    p = cap >= 16 ? *(const char *const *)s : (const char *)s;
    return p && p[n] == 0 ? p : NULL;
}

static UINT32 name_hash(const char *s)
{
    UINT32 h = 5381;
    while (*s)
        h = h * 33 + (BYTE)*s++;
    return h ? h : 1;
}

/* the log: every sound once (to see what the game plays and what it feels like) */
static void ui_log_once(UINT32 h, const char *name, int group, int preset)
{
    static UINT32 seen[512];
    static int count;
    static SRWLOCK lock = SRWLOCK_INIT;
    AcquireSRWLockExclusive(&lock);
    for (int i = 0; i < count; i++)
        if (seen[i] == h) {
            ReleaseSRWLockExclusive(&lock);
            return;
        }
    if (count < 512)
        seen[count++] = h;
    ReleaseSRWLockExclusive(&lock);
    fh2_say("ui sound: %s -> %s %d", name, group == G_MENU ? "menu" : group == G_EVENT ? "event" : "-", preset);
}

/* a sound starts (stop: it stops) */
static void ui_sound(const char *name, int stop)
{
    static volatile UINT32 lastHash;
    static volatile double lastAt;
    UINT32 h;
    double now = fh2_now();
    int group = G_NONE, preset = P_NONE;
    float pan = 0, k;
    if (!name)
        return;
    h = name_hash(name);
    if (stop) {
        fx_stop(h);
        return;
    }
    for (int i = 0; i < (int)(sizeof kUi / sizeof kUi[0]); i++)
        if (strstr(name, kUi[i].has)) {
            group = kUi[i].group, preset = kUi[i].preset;
            break;
        }
    ui_log_once(h, name, group, preset);
    /* the combo, whatever the feel's strength */
    if (preset == P_SKILL || preset == P_STREAK || preset == P_BIGSKILL) {
        if (strstr(name, "DrivingSkills"))
            combo_skill(now);
    } else if (preset == P_BANK)
        combo_end(now, 1);
    else if (preset == P_LOSE && strstr(name, "DrivingSkills"))
        combo_end(now, 2);
    if (preset == P_NONE || !g_connected || !fh2_foreground())
        return;
    /* one sound twice at once (the code and a storyboard): felt once */
    if (h == lastHash && now - lastAt < 0.03)
        return;
    lastHash = h, lastAt = now;
    k = group == G_MENU ? S.menus : S.events;
    if (k <= 0)
        return;
    /* moves and tabs on the side they go to */
    if (preset == P_MOVE || preset == P_SCROLL || preset == P_SLIDER) {
        WORD b = g_dsPad.wButtons;
        if ((b & XINPUT_GAMEPAD_DPAD_LEFT) || g_dsPad.sThumbLX < -16000)
            pan = -0.55f;
        else if ((b & XINPUT_GAMEPAD_DPAD_RIGHT) || g_dsPad.sThumbLX > 16000)
            pan = 0.55f;
    } else if (preset == P_TAB) {
        WORD b = g_dsPad.wButtons;
        pan = (b & XINPUT_GAMEPAD_LEFT_SHOULDER) ? -0.7f : (b & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 0.7f : 0;
    }
    ui_feel(preset, k, pan, h);
}

typedef unsigned char(__fastcall *UiStartFn)(void *self, const BYTE *name, void *a, void *b, void *c);
typedef unsigned char(__fastcall *UiPlayFn)(void *self, const BYTE *name);
typedef void(__fastcall *UiStopFn)(void *engine, const BYTE *name);
static UiStartFn g_uiStart;
static UiPlayFn g_uiPlay;
static UiStopFn g_uiStop;
static int g_soundsHooked;

static unsigned char __fastcall hk_ui_start(void *self, const BYTE *name, void *a, void *b, void *c)
{
    ui_sound(std_str(name), 0);
    return g_uiStart(self, name, a, b, c);
}

static unsigned char __fastcall hk_ui_play(void *self, const BYTE *name)
{
    ui_sound(std_str(name), 0);
    return g_uiPlay(self, name);
}

static void __fastcall hk_ui_stop(void *engine, const BYTE *name)
{
    ui_sound(std_str(name), 1);
    g_uiStop(engine, name);
}

/* at the game's code: the three hooked */
static int sounds_install(BYTE *base)
{
    static const char kPlay[] = "40 53 48 83 EC 50 48 C7 44 24 20 FE FF FF FF 48 8B D9 48 8B 0D ?? ?? ?? ?? 48 8B 01 4C 8B C2 "
                                "48 8D 54 24 28 FF 90 20 03 00 00 90 48 8B 0D ?? ?? ?? ?? 48 8B 01 48 8D 54 24 28 FF 90 28 03 "
                                "00 00";
    static const char kStart[] = "4C 8B DC 57 48 83 EC 70 49 C7 43 B8 FE FF FF FF 49 89 5B 08 49 89 73 10 49 8B F9 49 8B F0 48 "
                                 "8B D9 48 8B 0D ?? ?? ?? ?? 48 8B 01 4C 8B C2 49 8D 53 D8 FF 90 20 03 00 00";
    static const char kStop[] = "41 56 48 83 EC 50 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 60 48 89 74 24 68 48 89 7C 24 70 4C "
                                "8B F1 48 8B 01 4C 8B C2 48 8D 54 24 28 FF 90 20 03 00 00 90 49 8B 06 48 8D 54 24 28 49 8B CE "
                                "FF 90 28 03 00 00";
    Image g;
    int n[3];
    BYTE *play, *start, *stop;
    if (!image_of(base, &g))
        return 0;
    play = find_unique(g.text, g.textEnd, kPlay, &n[0]);
    start = find_unique(g.text, g.textEnd, kStart, &n[1]);
    stop = find_unique(g.text, g.textEnd, kStop, &n[2]);
    if (!play || !start || !stop) {
        ds_note("sounds: the game's interface sound code is not as expected (%d %d %d found) -- no menu haptics", n[0], n[1],
                n[2]);
        return 0;
    }
    if (!mh_hook("ui sounds: start", start, (void *)hk_ui_start, (void **)&g_uiStart) ||
        !mh_hook("ui sounds: play", play, (void *)hk_ui_play, (void **)&g_uiPlay) ||
        !mh_hook("ui sounds: stop", stop, (void *)hk_ui_stop, (void **)&g_uiStop)) {
        MH_DisableHook(start), MH_DisableHook(play), MH_DisableHook(stop);
        ds_note("sounds: the game's interface sounds could not be hooked -- no menu haptics");
        return 0;
    }
    g_soundsHooked = 1;
    fh2_say("sounds: the game's interface sounds hooked (game+0x%lX, +0x%lX, +0x%lX)", (DWORD)(start - g.base),
            (DWORD)(play - g.base), (DWORD)(stop - g.base));
    return 1;
}
