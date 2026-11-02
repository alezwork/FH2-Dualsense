FH2-Input 4.1 -- the DualSense (PS5), the keyboard and the mouse for Forza Horizon 2 on the Azimuth emulator
============================================================================================================

INSTALL
  1. Close Azimuth.
  2. Run Install.cmd. It finds Azimuth (or asks for the folder with azimuth.exe) and puts d3d12.dll next to
     azimuth.exe. Nothing else changes: not the game's files, not Azimuth.
  3. Turn off Steam's PlayStation controller support for Azimuth and close DS4Windows.
  4. Start the game. Settings: in the game, Settings > Controls (below the game's rows), or
     DualSense\DualSense.ini next to azimuth.exe. The log: DualSense\DualSense.log.

  Works with Forza Horizon 2 Extra Options and without it. Another d3d12.dll that was already there (ReShade and
  the like) is kept as d3d12_next.dll and keeps working.

UNINSTALL
  Uninstall.cmd -- takes the mod's d3d12.dll away (and puts back the d3d12.dll that was there before).

WHAT IT DOES
  - The DualSense over USB or Bluetooth: HD haptics, adaptive triggers from the car and the road, light bar, gyro.
  - The keyboard as a gamepad (together with the DualSense): W/S throttle/brake, A/D steering (smooth), Space/Enter
    A, E/Backspace B, Q X, R Y, Shift LB, C RB, Esc Menu, Tab View, arrows D-pad, J/L/I/K look around. Keys: [Keys].
  - The mouse looks around (the right stick): the view goes where you move it and comes back to the front when the
    mouse is still ([Mouse]); its buttons can be bound as keys (Mouse1..Mouse5 in [Keys]).
  - The button icons follow the device you play with, as you go: PlayStation symbols for the DualSense, the keys for
    the keyboard, the game's own for an Xbox controller ([Icons] Mode=Auto, or always PlayStation/Keyboard/Xbox).
  - Nothing is written into the game's folder: what the mod gives the game is in DualSense\cache next to azimuth.exe.
