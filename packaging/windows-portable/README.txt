projectM SDL - portable Windows version (unofficial fork with favorites)
=======================================================================

This is an unofficial build from https://github.com/gwebb417/frontend-sdl-cpp,
a fork of the projectM SDL frontend. It is not an official projectM release.

Getting started
---------------

1. Unzip this folder anywhere (desktop, USB stick, ...).
2. Double-click projectMSDL.exe. No installation and no extra DLLs are needed.
3. Play some music. projectM visualizes whatever your PC is playing.

If Windows shows "Windows protected your PC", click "More info" and then
"Run anyway". This appears because the exe isn't code-signed.

Keys
----

ESC          Show/hide the menu (File > Settings, Help > Quick Help lists all keys)
ENTER        Search presets
N / P / R    Next / previous / random preset
SPACE        Lock the current preset
S            Save the current preset to favorites
F            Switch between favorites only and all presets
DEL          Disable the current preset (favorites mode: remove from favorites)
CTRL-DEL     Switch to disabled presets only (there, DEL re-enables)
CTRL-Z       Undo the last save, removal, disable or re-enable
UP / DOWN    Beat sensitivity
LEFT / RIGHT Ambient Speed (slower / faster animation)
M            Mirror (left to right, top to bottom, 4-way, off)
+ / -        Zoom in / out
CTRL-S       Settings
CTRL-F       Fullscreen
CTRL-Q       Quit

Remote control from your phone: Settings > Remote / Spout. Turn on "Enable
Remote Control" and "Allow Other Devices", set a password, then open the
address shown there in your phone's browser (same Wi-Fi).

OBS: install the OBS Spout2 plugin
(https://github.com/Off-World-Live/obs-spout2-plugin/releases/latest), turn
on Options > Spout Output in projectM and add a "Spout2 Capture" source in OBS.
It keeps working while projectM is minimized.

What's in this folder
---------------------

projectMSDL.exe         The program.
portable.txt            Turns on portable mode: settings and favorites are kept
                        in the "userdata" folder here instead of
                        %APPDATA%\projectM. Delete it to use %APPDATA% instead.
projectMSDL.properties  Default settings with comments (optional).
presets\                Cream of the Crop preset pack, about 9,800 presets.
textures\               Milkdrop texture pack, images used by many presets.
LICENSE.md              License of the program (GNU GPL 3.0).
LICENSE-Spout2.txt      License of Spout2, used for the Spout output (BSD).

Credits
-------

- projectM: https://github.com/projectM-visualizer
- Spout2 (Spout output) by Lynn Jarvis: https://github.com/leadedge/Spout2
- Presets: "Cream of the Crop" by Jason Fletcher,
  https://github.com/projectM-visualizer/presets-cream-of-the-crop
  See presets\LICENSE.md.
- Textures: Milkdrop texture pack,
  https://github.com/projectM-visualizer/presets-milkdrop-texture-pack
  See textures\README.md.

Source code, full documentation and how to build it yourself:
https://github.com/gwebb417/frontend-sdl-cpp
