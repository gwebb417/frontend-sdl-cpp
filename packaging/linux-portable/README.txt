projectM SDL - portable Linux version (unofficial fork with favorites)
=====================================================================

This is an unofficial build from https://github.com/gwebb417/frontend-sdl-cpp,
a fork of the projectM SDL frontend. It is not an official projectM release.

Getting started
---------------

1. Unpack this folder anywhere:
     tar -xzf projectMSDL-portable-linux-x86_64.tar.gz
2. Start it:
     cd projectMSDL-portable-linux-x86_64
     ./projectMSDL
   You can also double-click projectMSDL in your file manager.
3. Play some music. projectM visualizes whatever your PC is playing.
   To pick the audio device, open the menu with ESC or press CTRL-I.

Requirements: a 64-bit (x86_64) Linux with glibc 2.35 or newer (e.g. Ubuntu
22.04 or newer, Debian 12, Fedora 36+, Arch), OpenGL 3.3, and the SDL2 (2.0.16
or newer) and FreeType libraries, which almost every desktop Linux already has.
If they are missing:
  Debian/Ubuntu:  sudo apt install libsdl2-2.0-0 libfreetype6
  Fedora:         sudo dnf install SDL2 freetype
  Arch:           sudo pacman -S sdl2 freetype2

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
CTRL-F       Fullscreen
CTRL-Q       Quit

What's in this folder
---------------------

projectMSDL             The program.
portable.txt            Turns on portable mode: settings and favorites are kept
                        in the "userdata" folder here instead of
                        ~/.config/projectM. Delete it to use ~/.config instead.
projectMSDL.properties  Default settings with comments (optional).
presets/                Cream of the Crop preset pack, about 9,800 presets.
textures/               Milkdrop texture pack, images used by many presets.
LICENSE.md              License of the program (GNU GPL 3.0).

Credits
-------

- projectM: https://github.com/projectM-visualizer
- Presets: "Cream of the Crop" by Jason Fletcher,
  https://github.com/projectM-visualizer/presets-cream-of-the-crop
  See presets/LICENSE.md.
- Textures: Milkdrop texture pack,
  https://github.com/projectM-visualizer/presets-milkdrop-texture-pack
  See textures/README.md.

Source code, full documentation and how to build it yourself:
https://github.com/gwebb417/frontend-sdl-cpp
