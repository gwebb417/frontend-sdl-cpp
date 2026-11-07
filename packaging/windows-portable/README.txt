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
M            Search presets
N / P / R    Next / previous / random preset
SPACE        Lock the current preset
S            Save the current preset to favorites
F            Switch between favorites only and all presets
DEL          Remove the current preset from favorites
CTRL-Z       Undo the last favorites save/remove
CTRL-F       Fullscreen
CTRL-Q       Quit

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

Credits
-------

- projectM: https://github.com/projectM-visualizer
- Presets: "Cream of the Crop" by Jason Fletcher,
  https://github.com/projectM-visualizer/presets-cream-of-the-crop
  See presets\LICENSE.md.
- Textures: Milkdrop texture pack,
  https://github.com/projectM-visualizer/presets-milkdrop-texture-pack
  See textures\README.md.

Source code, full documentation and how to build it yourself:
https://github.com/gwebb417/frontend-sdl-cpp
