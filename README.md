# FH2-DualSense

**Controllers for Forza Horizon 2 (Xbox One) on the Azimuth 0.9.7 emulator, in one DLL:** the PlayStation 5
DualSense with HD haptics and adaptive triggers driven by the car itself, the **Xbox controller's impulse triggers**,
the keyboard, and button icons that follow the device you are using. Drop it in: no drivers, no Steam Input,
no DS4Windows.

[Русская версия](README.ru.md)

---

## Features

### DualSense
- **USB or Bluetooth**, DualSense and **DualSense Edge** (back paddles and Fn buttons can be assigned).
- The game sees it as player 1's Xbox gamepad, together with any Xbox pad and the keyboard.
- **Touchpad**: click = View, swipes = D-pad. **Create** = View, **Options** = Menu (both configurable).
- **PS button**: shows the battery level on the player LEDs for 3 seconds.
- **Gyro steering**: the mic button switches it on and off (the mic LED shows it); tilt the controller like a wheel.

### Vibration
- **HD haptics over USB**: the game's two rumble motors are played as tones on the DualSense's voice-coil actuators.
- **The car in your hands** (from the game's own feedback data): an engine hum that follows the revs, knocking on
  the rev limiter, a knock at each gear change, the road's roughness.
- **Crashes in proportion**: the game shakes the pad at full for any touch of a fence or a bush; here its crash
  rumble grows with the real impact instead.
- **Bluetooth**: the DualSense's rumble emulation (the improved v2 on firmware 2.24 and later), with gear changes
  and the rev limiter.

### Adaptive triggers (DualSense)
The mod reads the data the game computes every frame for its own rumble (engine revs, the tyres' slip front and rear,
braking and spinning slip, collisions, gear changes) and turns it into trigger effects:

| | |
|---|---|
| **L2, brake** | A brake pedal that gets stiffer towards the end of its travel. When the front tyres reach their grip limit it starts to chatter like a pedal with ABS at work; the deeper the wheels lock, the harder and faster it judders. |
| **R2, throttle** | A light constant resistance. Vibrates with wheel spin, stronger the more the wheels spin. Knocks on the rev limiter. |
| **Gear change** | A short knock on the trigger you are pressing: R2 on the way up, L2 when shifting down under braking. |
| **Crash** | A jolt in both triggers that fades, by the strength of the impact: a scrape or a bush lightly, a pole or a wall at speed hard. |
| **Menus, pause, loading** | The triggers are free. |

### Impulse triggers (Xbox controller)
The rumble motors in the triggers of Xbox One and Xbox Series controllers, which the game used on the Xbox One and
which plain XInput on a PC cannot reach. The mod drives them through Windows.Gaming.Input from the same car data:

| | |
|---|---|
| **LT** | Pulses like ABS when the front tyres reach their grip limit under braking; a steady buzz, growing, as the wheels lock. |
| **RT** | Buzzes with wheel spin, stronger the more the wheels spin; knocks on the rev limiter. |
| **Gear change, crash** | A kick on the trigger you are pressing; both on a crash. |

The pad's two big motors get the game's rumble as before (plus the gear changes and the rev limiter). Works over USB
and the Xbox Wireless Adapter; over Bluetooth only where Windows passes the trigger motors on.

There is a **Trigger Feedback On/Off switch in the game's own controller settings** (Settings > Controls); it
switches the DualSense's adaptive triggers and the Xbox impulse triggers together and is saved to `DualSense.ini`.
If the game build differs and the car data is not found, both fall back to the game's Xbox One impulse trigger levels.

### Keyboard
The keyboard is a gamepad too, with smooth steering and pedals; every key can be changed in `DualSense.ini`.

### Button icons
- **Three looks**: PlayStation (the DualSense), the game's own Xbox icons (an Xbox pad), and **keyboard keys**
  (drawn from your key bindings).
- The icons **switch live** to the device you used last, in menus, prompts and hints.
- The controls screen keeps the game's own controller drawing; only the button marks are replaced.
- The PlayStation pictures are inside the DLL. To use your own, put a `DualSense.icons` (a zip of PNGs with the same
  names and sizes; [the original](https://github.com/alezwork/FH2-Dualsense/releases/tag/v1.0.0) is a good start)
  next to the DLL: it is taken instead.

### Light bar and LEDs
- Light bar modes: **Rumble** (a colour flashing with impacts and sliding tyres), **Revs** (turns red towards the
  redline, flashes on the limiter), **Static**, **Battery**, **Off**. Low battery blinks red.
- The five player LEDs work as a **rev counter** while driving.

---

## Requirements
- Windows 10 or 11, x64.
- Forza Horizon 2 for Xbox One (1.0.0.64) running on **Azimuth 0.9.7**. Neither is included; use your own copy.
- The mod loader from the release (`azimuth-tls.dll`), or any loader that loads `mods\*.dll` once the game is mapped.

Made and tested for Forza Horizon 2. Forza Horizon 2 Presents Fast & Furious uses the same engine but is not tested.

## Installation
1. Download `DualSense.dll` and `azimuth-tls.dll` from [Releases](../../releases).
2. **The mod loader** (once): in `<Azimuth>\data\bin\` rename `azimuth-tls.dll` to `azimuth-tls.dll.original`
   and put the downloaded `azimuth-tls.dll` there.
3. Create `<Azimuth>\mods\` and put `DualSense.dll` in it.
4. For the DualSense: turn off Steam's *PlayStation Configuration Support* for the emulator and close DS4Windows.
   If you use HidHide, let `azimuth.exe` see the controller.
5. Start the game. `DualSense.ini` is created in `mods\` on the first start; the log is `mods\DualSense.log`.

**From 1.0**: delete `Keyboard.dll` and `DualSense.icons` from `mods\`. Your `Keyboard.ini` settings are taken
into `DualSense.ini` on the first start.

**Remove**: delete `DualSense.dll` (and `DualSense.ini`, `DualSense.log`, `DualSense.cache\`) from `mods\` and rename
`azimuth-tls.dll.original` back.

## Settings (`mods\DualSense.ini`)
Every setting is explained in the file itself. In short:

| Section | What is there |
|---|---|
| `[DualSense]` | The DualSense on/off, Japanese layout (Circle confirms), what the touchpad click, Create, the Edge paddles and Fn buttons do, touchpad swipes, the PS button. |
| `[Vibration]` | `Mode` = Auto / Audio (HD haptics) / Rumble / Off, `Strength`, the haptic tones, `Car` = how much of the engine, rev limiter and gear changes you feel, `Collisions` = the crash rumble. |
| `[Triggers]` | `Enabled` (same as the in-game switch), `Mode` = Game / ResistanceOnly / Off, `Strength`, the brake pedal's shape (`BrakeResistance`, `BrakeStart`, `BrakeForceStart`, `BrakeForceEnd`), the throttle's resistance, vibration frequencies, where the triggers start to vibrate (`BrakeSlipStart`, `SpinSlipStart`, in percent of the grip limit, for the Xbox pad too), `GearChangeKick`, `RevLimiter`, `CrashJolt`, `FreeInMenus`. |
| `[ImpulseTriggers]` | The Xbox controller's impulse triggers: `Enabled`, `Strength`. |
| `[Lightbar]` | `Mode`, `Color`, `FlashColor`, `Brightness`, `LowBatteryBlink`, `RevLights`. |
| `[Icons]` | `PlayStation` = Auto (follow the device) / On (always PlayStation) / Off (the game's own). |
| `[Gyro]` | `Steering` = Toggle (mic button) / On / Off, `MaxAngle`, `Invert`. |
| `[Keyboard]` | The keyboard on/off, only while the game is in front, steering and pedal speeds. |
| `[Keys]` | The keys for every control (up to 4 each). |

Default keys:

| Control | Keys | Control | Keys |
|---|---|---|---|
| Throttle / Brake | W / S | Steer | A / D |
| A | Space, Enter | B | E, Backspace |
| X | Q | Y | R |
| LB / RB | Left Shift / C | L3 / R3 | F / V |
| Menu / View | Esc / Tab | D-pad | arrow keys |
| Look around | I J K L | | |

## Troubleshooting
- **The DualSense does nothing**: see `mods\DualSense.log`. "no DualSense connected" = Windows does not see it;
  "does not open" = another program has it (Steam Input, DS4Windows, HidHide).
- **No impulse triggers on the Xbox pad**: the log says how many pads Windows.Gaming.Input sees (`xbox:`). Use USB
  or the Xbox Wireless Adapter; over Bluetooth the trigger motors often do not get through.
- **No HD haptics over Bluetooth**: Windows only exposes the DualSense's haptics audio over USB; Bluetooth uses the
  rumble emulation.
- **The triggers have no effect**: switch *Trigger Feedback* on in the game's controller settings, or set
  `[Triggers] Enabled=1`.
- **Icons do not change**: check `[Icons] PlayStation=Auto` and the `icons:` lines in the log.
- **Steam and the mod both drive the lights/rumble**: turn off Steam's PlayStation Configuration Support for the
  emulator.

## Building from source
```
build.cmd            builds out\mods\DualSense.dll
build.cmd install    also copies it into ..\Azimuth\mods and ..\Azimuth FF\mods
```
- **C compiler**: Visual Studio Build Tools (C++) or zig (`python -m pip install ziglang`); `build.cmd` uses
  whichever it finds. The DLL is built for size (`/O1` or `-Os`, unused code dropped, no symbols).
- **The PlayStation pictures** (`tools\make_pack.py`, put inside the DLL by `tools\bin2h.py`): Python 3 with numpy
  and Pillow, and the game's files (`..\Forza Horizon 2\Mount`): the icons are drawn in the sizes of the game's own,
  and the game's `xg_x.dll` (from its `EmbeddedXvd`) is used through a small stub (`tools\xg\kernelx.c`) for the Xbox
  texture layouts. Without them the DLL is built without pictures of its own.
- **The mod loader** (`loader\modloader.c`): MSVC, without the C runtime (`/NODEFAULTLIB`, entry `DllMain`).

| Path | What is there |
|---|---|
| `DualSense\DualSense.c` | the DualSense: input, vibration, adaptive triggers, lights; the XInput hooks; the start |
| `DualSense\car.h` | the car's data from the game |
| `DualSense\xbox.h` | the Xbox controller's impulse triggers (Windows.Gaming.Input) |
| `DualSense\keyboard.h` | the keyboard |
| `DualSense\ui.h` | the switch in the game's settings |
| `DualSense\icons.h` | the icon looks and their live switch; `texture.h`, `bc7.h`, `png.h`, `inflate.h`, `keyicons.h`, `tables.h`: textures, decoders, the keycaps |
| `common\fh2mod.h` | shared code: log, ini, finding the engine's XInput table and the game's code |
| `loader\` | the mod loader |
| `tools\` | the icon pictures, generated tables, the ini and the pictures into C headers |

## How it works (for modders)
Everything is found by code signatures, not fixed addresses; when something is not found, the log says so and only
that part is left out. The addresses below are for Forza Horizon 2 1.0.0.64 and Azimuth 0.9.7.

- **Input/rumble**: the engine calls XInput through its own table of function pointers; `GetState` and `SetState`
  are replaced. `Gamepad::SetVibration` is hooked for the impulse trigger levels, which the engine drops.
- **The car's data**: the player car's feedback update (0x2FD740) hands its inputs to the rumble and the impulse
  triggers through two thunks (0x10768B0, 0x10768A0); their first bytes become jumps to the mod, which keeps the
  values and calls the game's functions itself. Crashes are scaled logarithmically between "felt" and "a big one"
  per kind (cars 10..400, walls 25..375, objects 0.8..30, measured in the game) and fade in 90 ms; the slips are
  smoothed. The game's own crash rumble curves (three points each) are set to the same scale in its memory.
- **Impulse triggers**: `Windows.Gaming.Input.Gamepad` through its WinRT interfaces, declared in C by their vtables;
  while it sees Xbox pads, their whole rumble goes that way (XInput and it would fight over the big motors).
- **The settings switch**: a new property (`TriggerFeedback`) is added to the controller options view model when
  the game registers its properties, modelled on the camera look-back switch; a row is added to the screen's XAML.
- **Files**: the engine's `CreateFileW` and related imports are hooked. The icon archives, the controls screen
  drawing, the settings XAML and `zipmanifest.xml` are served from `mods\DualSense.cache\`, built at the game's
  start.
- **Live icon switch**: Azimuth seals texture memory nobody writes to (read-only) and uploads it again when written.
  The mod finds the icons in memory and writes the wanted look over them; the engine's exception handler unseals the
  pages and sends the textures to the GPU again.
- **Exports**: `Fh2IconLook()`, `Fh2SwitchIcons(int)`, `Fh2KeyboardLastInput()`.

## Credits
- DualSense report layouts: the Linux kernel's `hid-playstation` driver.
- Adaptive trigger effect encoding: Nielk1's DualSense trigger effect notes (MIT).

## Disclaimer
This project is not affiliated with or endorsed by Microsoft, Xbox Game Studios, Playground Games, Turn 10 Studios or
Sony Interactive Entertainment. Forza Horizon, Xbox, PlayStation and DualSense are trademarks of their respective
owners. No game files are included; you need your own copy of the game.
