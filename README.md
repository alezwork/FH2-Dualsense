# FH2-DualSense

The PlayStation 5 controller (DualSense, DualSense Edge) for **Forza Horizon 2 on the Azimuth emulator 0.9.10**:
HD haptics and adaptive triggers driven by the car and the road, PS5-like menu haptics, the light bar and LEDs, gyro
steering and PlayStation button icons. One DLL that works on top of Forza Horizon 2 Extra Options (required): no
drivers, Steam Input or DS4Windows.

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
- **Forza Horizon 2 Extra Options 0.2.98** by Arthur (RokkuDayo): its `winmm.dll` and `FH2_ExtraOptions.ini` next to
  `azimuth.exe`, with `[Global] Enabled=1` and `[Files] EnableMediaOverrides=1` (its defaults). Without it the mod
  does nothing.
- Azimuth's own files stay as they are: `datainzimuth-tls.dll` is not replaced or renamed.

## Installation
1. Install Extra Options and check that it works (its `FH2_ExtraOptions.log` says `OK`).
2. Download `d3d12.dll` from [Releases](../../releases) and put it next to `azimuth.exe` (beside Extra Options'
   `winmm.dll`).
3. Turn off Steam's PlayStation controller support for Azimuth and close DS4Windows (with HidHide, allow
   `azimuth.exe`).
4. Start Azimuth, then the game. The main settings are in Settings > Controls; all of them are in
   `DualSense\DualSense.ini` next to `azimuth.exe`, and the log is `DualSense\DualSense.log`.

Upgrading from 2.x: delete `fh2-mods\DualSense.dll` (2.0, 2.1) and the `DualSense\cache\` folder (2.2).

## Uninstalling
1. Set `[DualSense] Enabled=0` in `DualSense\DualSense.ini` and start Azimuth once: the mod takes its files out of the
   game's `Mount\mediaoverride\` folder.
2. Delete `d3d12.dll` and the `DualSense\` folder.

(Or delete the files listed in `DualSense\mediaoverride.txt` from `Mount\mediaoverride\` by hand.)

## How it works
- `azimuth.exe` loads `d3d12.dll` from its own folder, as it loads Extra Options' `winmm.dll`; every Direct3D 12 call
  is passed on to the system's. Nothing of Azimuth is changed or hooked.
- Without Extra Options (no `winmm.dll` with its `Fh2TweaksStatus` export) the mod stays off.
- The DualSense is handed to the game as a Windows.Gaming.Input gamepad through Azimuth's own handlers.
- Car data, wheel surfaces and interface sounds come from hooks into the game (MinHook), found by code signatures and
  checked before use; anything unexpected is logged and left out. They go in only after Extra Options reports that
  its own patches are in (`Fh2TweaksStatus()` says `OK`), so its checks of the game's code see it as it is on disk.
  If Extra Options refuses or is switched off, the mod leaves the game alone (the controller still works as a
  gamepad).
- The menu page, its texts and the icons are written as Azimuth starts into the game's `Mount\mediaoverride\`
  folder, which Extra Options gives the game in place of the originals: `UI\Scenes\Anthem\A_ControllerOptions.xaml`,
  `stripped\stringtables\<language>\MyProfile.str` and `ui\textures\data_bound\<archive>\*.swatchbin`. The game's
  own files are not changed. What the mod wrote is listed in `DualSense\mediaoverride.txt`; a file there that is not
  the mod's (another mod's) is left alone.

## Building
```
set FH2_GAME=D:\Games\Forza Horizon 2
build.cmd            builds out\d3d12.dll
build.cmd install    also copies it next to azimuth.exe
```
Needs zig (`python -m pip install ziglang`); the DLL itself needs no Extra Options files to build. The PlayStation pictures need Python with numpy and Pillow and the
game's files; without them the DLL is built without pictures of its own.

## Credits
- DualSense report layouts: the Linux kernel's `hid-playstation` driver.
- Adaptive trigger effects: Nielk1's DualSense trigger effect notes (MIT).
- Hooks: [MinHook](https://github.com/TsudaKageyu/minhook) by Tsuda Kageyu (BSD 2-Clause, `third_party/minhook`).
- Notes on the game: the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform) by Trinculo54 (MIT).
- Forza Horizon 2 Extra Options by Arthur (RokkuDayo): the media overrides and the patches the mod builds on.

## Disclaimer
Not affiliated with or endorsed by Microsoft, Playground Games, Turn 10 Studios, the Azimuth developers, the Extra
Options author or Sony
Interactive Entertainment. Forza Horizon, Xbox, PlayStation and DualSense are trademarks of their owners. No game files
are included.
