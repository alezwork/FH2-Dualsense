# FH2-DualSense

The PlayStation 5 controller (DualSense, DualSense Edge) for **Forza Horizon 2 on the Azimuth emulator 0.9.10**:
HD haptics and adaptive triggers driven by the car and the road, PS5-like menu haptics, the light bar and LEDs, gyro
steering and PlayStation button icons. One DLL: no other mods, drivers, Steam Input or DS4Windows.

## Features
- **Controller**: USB or Bluetooth; Edge paddles and Fn buttons; touchpad click and swipes; gyro steering (mic button).
- **Adaptive triggers**: L2 is a brake pedal that pulses like ABS at the grip limit, or goes hard when a wheel locks
  without ABS (told from how the brakes behave); R2 vibrates with wheel spin and knocks on the rev limiter; gear
  changes knock, crashes and landings jolt, and the road's texture is felt in the trigger you press.
- **HD haptics** (USB): the engine's hum, the road under each side of the car (gravel, dirt, grass, sand, cobbles,
  kerbs), crashes and landings. Over Bluetooth: the controller's rumble emulation.
- **Menu and event haptics**: ticks and clicks in the menus, the prize drum ticking by, skills, combos, checkpoints,
  countdowns, level-ups.
- **Lights**: light bar (impacts, revs or battery), player LEDs as a rev counter and skill combo, PS button shows the
  battery.
- **PlayStation button icons** in the prompts and the controls screen.
- **Settings in the game's own menu**: Settings > Controls, below the game's rows, in every language the game has,
  with the controller's state (connection, battery, firmware) and tests of the vibration, triggers and lights.

## Requirements
- Windows 10 or 11, x64.
- Forza Horizon 2 for Xbox One (1.0.0.64) on **Azimuth 0.9.10**. Neither is included. (Azimuth 0.9.9: use
  [v1.2.0](../../releases/tag/v1.2.0).)

## Installation
1. Download `d3d12.dll` from [Releases](../../releases).
2. Put it next to `azimuth.exe`.
3. Turn off Steam's PlayStation controller support for Azimuth and close DS4Windows (with HidHide, allow
   `azimuth.exe`).
4. Start the game. The main settings are in Settings > Controls; all of them are in `DualSense\DualSense.ini` next to
   `azimuth.exe`, and the log is `DualSense\DualSense.log`.

Upgrading from 2.0 or 2.1: delete `fh2-mods\DualSense.dll`. The mod works alongside Extra Options and the FH2 Mod
Platform. To uninstall, delete `d3d12.dll` and the `DualSense\` folder.

## How it works
- `azimuth.exe` loads `d3d12.dll` from its own folder; every Direct3D 12 call is passed on to the system's.
- The DualSense is handed to the game as a Windows.Gaming.Input gamepad through Azimuth's own handlers.
- Car data, wheel surfaces and interface sounds come from hooks into the game (MinHook), found by code signatures and
  checked before use; anything unexpected is logged and left out.
- The menu, its texts and the icons are modified copies of `UI.zip`, the string tables and the icon archives (with a
  matching `zipmanifest.xml`) in `DualSense\cache\`, given to Azimuth in place of the originals. The game's files are
  not changed.

## Building
```
set FH2_GAME=D:\Games\Forza Horizon 2
build.cmd            builds out\d3d12.dll
build.cmd install    also copies it next to azimuth.exe
```
Needs zig (`python -m pip install ziglang`). The PlayStation pictures need Python with numpy and Pillow and the
game's files; without them the DLL is built without pictures of its own.

## Credits
- DualSense report layouts: the Linux kernel's `hid-playstation` driver.
- Adaptive trigger effects: Nielk1's DualSense trigger effect notes (MIT).
- Hooks: [MinHook](https://github.com/TsudaKageyu/minhook) by Tsuda Kageyu (BSD 2-Clause, `third_party/minhook`).
- Notes on the game: the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform) by Trinculo54 (MIT).

## Disclaimer
Not affiliated with or endorsed by Microsoft, Playground Games, Turn 10 Studios, the Azimuth developers or Sony
Interactive Entertainment. Forza Horizon, Xbox, PlayStation and DualSense are trademarks of their owners. No game files
are included.
