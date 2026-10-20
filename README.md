# FH2-DualSense

**The PlayStation 5 controller for Forza Horizon 2 (Xbox One) on the Azimuth emulator 0.9.10:** the DualSense with
HD haptics and adaptive triggers driven by the car itself and the road under each wheel, PS5-like haptics in the menus
and for the game's events, the light bar and LEDs, gyro steering, and the game's button icons with the PlayStation
pictures. A mod of the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform):
drop it in, no drivers, no Steam Input, no DS4Windows.

[Русская версия](README.ru.md)

---

## Features

### The controller
- **USB or Bluetooth**, DualSense and **DualSense Edge** (back paddles and Fn buttons can be assigned).
- The game gets it as one of the PC's gamepads, next to any Xbox pad Azimuth already gives it.
- **Touchpad**: click = View, swipes = D-pad. **Create** = View, **Options** = Menu (both configurable).
- **PS button**: shows the battery level on the player LEDs for 3 seconds.
- **Gyro steering**: the mic button switches it on and off (the mic LED shows it); tilt the controller like a wheel.

### Vibration
- **HD haptics over USB**: the game's two rumble motors are played as tones on the DualSense's voice-coil actuators,
  with the mod's own effects mixed in and a soft limiter (strong moments stay round, never clip).
- **The car in your hands** (from the game's own feedback data): an engine hum that follows the revs, knocking on
  the rev limiter, a knock at each gear change, a sharp impact on crashes.
- **The road under each wheel** (the game's own surface under every wheel): gravel crunches, dirt and grass rumble
  softly, sand is grainy, cobbles knock, metal grates buzz, kerbs drum in time with the speed -- each on the side of
  the car it is under; tarmac stays quiet. A jump lifts the road away; the landing thumps by how long you flew.
- **Crashes in proportion**: the game shakes the pad at full for any touch of a fence or a bush; here its crash
  rumble grows with the real impact instead.
- **Bluetooth**: the DualSense's rumble emulation (the improved v2 on firmware 2.24 and later), with gear changes,
  the rev limiter, the road and every effect below as motor pulses.

### Menus and the game's events, felt
From the game's own interface sounds (every one of them goes through the game's audio engine, which the mod listens
to), as on a PS5:
- **Menus**: a crisp tick for every move (on the side you move to), a click with some body for every choice, a softer
  one going back, two ticks on the side of the bumper changing tabs, a swell as a page or a pop-up opens, two low
  buzzes when something can't be done.
- **The tombola's drum** (the prize at each level-up): a tick for every prize passing, about 20 a second at first,
  slowing down with the drum, a clunk and its bounce as it stops, then the win.
- **Skills and combos**: a ding for every skill (near misses, drifts, air, ...), rising ticks for a clean streak, a
  rise for the multiplier, a thud and two bright ticks when the chain is banked, a deflating sweep when it is lost; a
  skill point, XP, level-ups and prizes celebrate.
- **Races**: the countdown's ticks and its GO thump, checkpoints, speed traps, overtakes, results; counters running up
  after a race ratchet until they stop.

### Adaptive triggers
The mod reads the data the game computes every frame for its own rumble (engine revs, the tyres' slip front and rear,
braking and spinning slip, collisions, gear changes) and turns it into trigger effects:

| | |
|---|---|
| **L2, brake** | A brake pedal that gets stiffer towards the end of its travel. Past the front tyres' grip limit: **with ABS** it pulses like a pedal with the ABS at work (about 14 strokes a second, harder the deeper into the slip); **without ABS** the tyres scrub, and a locked wheel makes the pedal go hard where your finger is, juddering slowly. The mod tells which by the brakes themselves: a wheel locking under firm braking means no ABS, three hard stops at the limit without one mean ABS (shown on the DualSense page). |
| **R2, throttle** | A light constant resistance. Vibrates with wheel spin, stronger the more the wheels spin. Knocks on the rev limiter. |
| **Gear change** | A short knock on the trigger you are pressing: R2 on the way up, L2 when shifting down under braking. |
| **Crash, landing** | A jolt in both triggers that fades, by the strength of the impact: a scrape or a bush lightly, a pole or a wall at speed hard; a landing by how long the car flew. |
| **The road** | Its texture in the trigger you press: gravel buzzes, dirt and grass roll, kerbs knock in time with the speed (the brake a little lighter). |
| **Menus, pause, loading** | The triggers are free. |

If the car data is not found, the triggers follow the game's own Xbox One impulse trigger levels, which Azimuth
0.9.10 passes on with the rumble.

### Button icons
The game's button icons, prompts and the controls screen's drawing with the **PlayStation pictures**, when a DualSense
is connected as the game starts (or always, or never: `[Icons] PlayStation`). The pictures are inside the DLL; your
own `DualSense.icons` (a zip of PNGs with the same names and sizes) in the mod's folder is taken instead.

### Light bar and LEDs
- Light bar modes: **Rumble** (a colour flashing with impacts and sliding tyres), **Revs** (turns red towards the
  redline, flashes on the limiter), **Static**, **Battery**, **Off**. Low battery blinks red.
- The five player LEDs work as a **rev counter** while driving, and show the **skill combo**: a light more for every
  skill in the chain, all five blinking when it is banked, the middle one when it is lost.

### The DualSense page
**MODS > Installed mods > DualSense** (platform 2.7.0): the controller's model, connection (USB or Bluetooth),
charge, firmware (the update version a PS5 shows, the firmware and hardware versions, the build date), how it
vibrates; what the mod reads from the game (the car's data, the surface under the wheels, the interface sounds, ABS
as the brakes show it); and three tests on A: **the vibration** (left, right, the menu's ticks from left to right, a
click, a counter, a landing), **the triggers** (press L2 and R2: the pedals, ABS, a locked wheel, wheel spin, gravel, a
kerb, gear changes, 1.5 seconds each) and **the lights**.

### In the game's settings
**Pause > Settings > MODS** has the mod's main switches, in English or Russian (the game's language): *Adaptive
triggers*, *Trigger strength*, *Vibration*, *HD haptics (USB)*, *Engine feel*, *Menu haptics*, *Game events*,
*Road surface*, *Gyro steering*, *Light bar*, *Rev counter lights*, *Combo on the player lights*, *PlayStation button
icons*, *Check for updates*. A change works at once (the icons from the next
start) and is saved to `DualSense.ini`.

---

## Requirements
- Windows 10 or 11, x64.
- Forza Horizon 2 for Xbox One (1.0.0.64) running on **Azimuth 0.9.10**. Neither is included; use your own copy.
- The **FH2 Mod Platform** 2.3.0 or newer; the road surfaces need 2.6.0, the DualSense page 2.7.0 (2.7.0 in the release
  zip: `d3dcompiler_47.dll` next to `azimuth.exe`).
- [Forza Horizon 2 Extra Options](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions) for the PlayStation icons
  (the platform hands the game's files over through its `mediaoverride`). Everything else works without it.

Azimuth 0.9.9 and older (XInput) are not supported by 2.0; use [1.2.0](../../releases/tag/v1.2.0) there.

## Installation
1. Download the release zip from [Releases](../../releases).
2. Extract it into your Azimuth folder (the one with `azimuth.exe`). You end up with:
   ```
   d3dcompiler_47.dll          the FH2 Mod Platform (every platform mod ships the same file)
   LICENSE-MinHook.txt
   fh2-mods\DualSense.dll
   ```
3. For the DualSense: turn off Steam's *PlayStation Configuration Support* for the emulator and close DS4Windows.
   If you use HidHide, let `azimuth.exe` see the controller.
4. Start the game. The mod's folder `DualSense\` next to `azimuth.exe` gets `DualSense.ini` (made with the defaults)
   and `DualSense.log`; the platform's log is `fh2-mods\platform.log`.

**From 1.x (Azimuth 0.9.9)**: the mod loader (`azimuth-tls.dll` in `data\bin`) and `mods\DualSense.dll` are not needed
any more. The keyboard and the Xbox impulse triggers are not part of the mod now: Azimuth 0.9.10 gives the game the
Xbox controller's impulse triggers itself.

**With Extra Options' Autodrive**: it starts and stops on a D-pad Down press (or F2), and a downward swipe on the
DualSense's touchpad is a D-pad Down too. Steering, throttle or braking takes control back -- and so does tilting the
DualSense with gyro steering on: switch the gyro off (mic button) before letting it drive.

**Remove**: delete `fh2-mods\DualSense.dll` and the `DualSense\` folder. The platform takes its copies of the icons
away on the next start.

## Settings (`DualSense\DualSense.ini`)
Every setting is explained in the file itself. In short:

| Section | What is there |
|---|---|
| `[DualSense]` | The DualSense on/off, Japanese layout (Circle confirms), what the touchpad click, Create, the Edge paddles and Fn buttons do, touchpad swipes, the PS button, the Mods page's language, update checks. |
| `[Vibration]` | `Mode` = Auto / Audio (HD haptics) / Rumble / Off, `Strength`, the haptic tones, `Car` = how much of the engine, rev limiter and gear changes you feel, `Collisions` = the crash rumble, `Menus`, `Events`, `Surface` = the menus' ticks, the game's events, the road (percent, 0 = off). |
| `[Triggers]` | `Enabled`, `Mode` = Game / ResistanceOnly / Off, `Strength`, the brake pedal's shape (`BrakeResistance`, `BrakeStart`, `BrakeForceStart`, `BrakeForceEnd`), the throttle's resistance, vibration frequencies, where the triggers start to vibrate (`BrakeSlipStart`, `SpinSlipStart`, in percent of the grip limit), `GearChangeKick`, `RevLimiter`, `CrashJolt`, `FreeInMenus`. |
| `[Lightbar]` | `Mode`, `Color`, `FlashColor`, `Brightness`, `LowBatteryBlink`, `RevLights`, `ComboLights`. |
| `[Icons]` | `PlayStation` = Auto (a DualSense at the game's start) / On / Off. |
| `[Gyro]` | `Steering` = Toggle (mic button) / On / Off, `MaxAngle`, `Invert`. |

## Troubleshooting
- **The DualSense does nothing**: see `DualSense\DualSense.log`. "no DualSense connected" = Windows does not see it;
  "does not open" = another program has it (Steam Input, DS4Windows, HidHide). "pads: ... joins the game's gamepads"
  is the line that says the game has it.
- **"pads: not Azimuth 0.9.10's"**: another Azimuth version; this release is made for 0.9.10.
- **The mod is not loaded at all**: `fh2-mods\platform.log` lists every mod the platform loaded; check that the
  platform's `d3dcompiler_47.dll` is next to `azimuth.exe`.
- **No HD haptics over Bluetooth**: Windows only exposes the DualSense's haptics audio over USB; Bluetooth uses the
  rumble emulation.
- **Icons do not change**: they need Extra Options (`EnableMediaOverrides=1`); see the `icons:` lines in the log.
- **Steam and the mod both drive the lights/rumble**: turn off Steam's PlayStation Configuration Support for the
  emulator.

## Building from source
```
set FH2_GAME=D:\Games\Forza Horizon 2      the folder with Azimuth\ and "Forza Horizon 2"\ (default: the one above)
build.cmd            builds out\fh2-mods\DualSense.dll
build.cmd install    also copies it into %FH2_GAME%\Azimuth\fh2-mods
```
- **C compiler**: Visual Studio Build Tools (C++) or zig (`python -m pip install ziglang`); `build.cmd` uses
  whichever it finds. The DLL is built for size (`/O1` or `-Os`, unused code dropped, no symbols).
- **The platform's API**: `platform\fh2mod.h`, the header of the FH2 Mod Platform (MIT, `platform\LICENSE`).
- **The PlayStation pictures** (`tools\make_pack.py`, put inside the DLL by `tools\bin2h.py`): Python 3 with numpy
  and Pillow, and the game's files (`%FH2_GAME%\Forza Horizon 2\Mount`): the icons are drawn in the sizes of the
  game's own, and the game's `xg_x.dll` (from its `EmbeddedXvd`) is used through a small stub (`tools\xg\kernelx.c`)
  for the Xbox texture layouts. Without them the DLL is built without pictures of its own.

| Path | What is there |
|---|---|
| `DualSense\DualSense.c` | the controller: input, vibration, HD haptics, adaptive triggers, lights; the Mods page; the start (`fh2mod_init`) |
| `DualSense\wgi.h` | the DualSense as one of the PC's gamepads for Azimuth 0.9.10 |
| `DualSense\car.h` | the car's data from the game: revs, slips, crashes, the surface under each wheel, jumps, ABS |
| `DualSense\fx.h` | the haptic effects (ticks, thumps, sweeps, the tombola's drum) for HD haptics and the rumble emulation |
| `DualSense\sounds.h` | the game's interface sounds as haptics; the skill combo |
| `DualSense\page.h` | the DualSense page: the controller's state, the tests |
| `DualSense\icons.h` | the PlayStation icons; `texture.h`, `bc7.h`, `png.h`, `inflate.h`, `tables.h`: textures, decoders |
| `DualSense\base.h` | the mod's folder, log, ini, the images |
| `platform\` | the FH2 Mod Platform's API header |
| `tools\` | the icon pictures, generated tables, the ini and the pictures into C headers |

## How it works (for modders)
The addresses below are for Forza Horizon 2 1.0.0.64 and Azimuth 0.9.10; everything is found by code signatures and
checked before it is used, and when something is not as expected, the log says so and only that part is left out.

- **Input and rumble**: Azimuth 0.9.10 gives the game the PC's `Windows.Gaming.Input` gamepads: it takes the Gamepad
  statics, registers handlers of `GamepadAdded` and `GamepadRemoved` (static objects at engine+0x3360F0 and 0x336100),
  keeps the statics (engine+0x345C60), and reads each pad with `GetCurrentReading` and drives it with
  `put_Vibration` (the two motors and the two impulse triggers). The DualSense is a gamepad object of the mod's own,
  implementing `IGamepad`, handed to the engine's own `GamepadAdded` handler once its pads are up (and to
  `GamepadRemoved` when the controller goes). The handlers are found by the code that registers them, right after
  the class name `Windows.Gaming.Input.Gamepad`.
- **The car's data**: the player car's feedback update (0x2FD740) hands its inputs to the rumble and the impulse
  triggers through two thunks (0x10768B0, 0x10768A0), hooked through the platform (`hook_function`, after
  `wait_for_code`), which keeps the values and goes on into the game. Crashes are scaled logarithmically between
  "felt" and "a big one" per kind (cars 10..400, walls 25..375, objects 0.8..30, measured in the game) and fade in
  90 ms; the slips are smoothed. The game's own crash rumble curves are set to the same scale in its memory.
- **The road**: in the same feedback update the game goes over the four wheels for their surfaces' bumpiness: a
  wheel's contact (none: in the air) holds its surface's number in the game's table of surfaces (game+0x365D6F8: a
  vector of 0x1F0-byte records, the name first, from `media\physics\surfaceTypes.xml`). An instruction hook there
  (`hook_instruction`, game+0x2FDF0F) reads every wheel's number and the car's speed (xmm12, m/s); the names give
  the kinds (RumbleStrip*, Gravel*, Dirt*, Grass*, Cobblestone*, ...).
- **Interface sounds**: every interface sound (`media\audio\ui\UI4Audio.xml`'s triggers) is played by an object of
  the game's audio engine, `AudioObjectUIEvent` (its methods at 0x100ED70 and 0x100ED00, `(this, const std::string
  &trigger, ...)`), and stopped by the engine's 0x10229B0 -- whether the game's code asks for it or a XAML
  storyboard (`local:AudioEvent`, as the tombola's). The three are hooked; the trigger's name picks the feel. The
  tombola's drum (`A_Tombola.xaml`: 40040 px in 7 s, PowerEase EaseOut, the spin sound at 0.7 s) is modelled to
  tick for every prize passing.
- **The DualSense page**: the platform's pages (`add_page`, `set_page_rows`, API 8).
- **Icons**: every icon of `ControllerIcons.zip`, `Help_Buttons.zip` and `UI_Symbols.zip` that has a PlayStation
  picture is re-encoded as the game's texture (RGBA, 1D tiling) and handed over with the platform's `set_file`.
- **Settings**: the platform's Mods page rows (`add_setting`), saved to the mod's ini.

## Credits
- DualSense report layouts: the Linux kernel's `hid-playstation` driver.
- Adaptive trigger effect encoding: Nielk1's DualSense trigger effect notes (MIT).
- The FH2 Mod Platform by Trinculo54 (MIT), which this mod is built on.

## Disclaimer
This project is not affiliated with or endorsed by Microsoft, Xbox Game Studios, Playground Games, Turn 10 Studios,
the Azimuth developers or Sony Interactive Entertainment. Forza Horizon, Xbox, PlayStation and DualSense are
trademarks of their respective owners. No game files are included; you need your own copy of the game.
