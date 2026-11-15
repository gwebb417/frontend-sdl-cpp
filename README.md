I forked this because I wanted to be able to save favorite presets. Along the way I fixed a number of issues with
toasts, settings and the UI. It also includes the preset search window from upstream PR #139.

This is an unofficial fork, not an official projectM release.

## Download

The current version is **v1.5** ([release page](https://github.com/gwebb417/frontend-sdl-cpp/releases/tag/fork-v1.5)).
Both downloads are on the [latest release](https://github.com/gwebb417/frontend-sdl-cpp/releases/latest) page. They
already include the presets and textures (see [Presets and textures](#presets-and-textures)) and run in
[portable mode](#new-features), so settings and favorites are kept in the unpacked folder.

**Windows:** get `projectMSDL-portable-windows-x64.zip`, unzip it anywhere and run `projectMSDL.exe`. It's a single
exe with no DLLs or installation needed. If Windows shows "Windows protected your PC", click **More info** and then
**Run anyway**. This appears because the exe isn't code-signed.

**Linux (x86_64):** get `projectMSDL-portable-linux-x86_64.tar.gz`, unpack it anywhere and run `./projectMSDL` in the
unpacked folder. It needs glibc 2.35 or newer (e.g. Ubuntu 22.04+, Debian 12, Fedora 36+, Arch) plus SDL2 and
FreeType, which almost every desktop Linux already has. The included `README.txt` lists the install commands in case
they're missing.

**Updating from an earlier version:** unpack the new version, then copy the `userdata` folder from your old folder
into the new one (or just replace `projectMSDL.exe` / `projectMSDL` in your old folder). Settings, favorites and
disabled presets carry over.

## What's new in v1.5

- **No more stutter on preset switches:** the next preset is loaded a few seconds early, and the graphics driver
  compiles its shaders in the background (see [Preset preloading](#new-features)).
- **Choose the transition effects**, including two new ones, **Beat Pulse** and **Datamosh** (Settings > projectM or
  Options > Transition Effects).
- **Remote control:** transitions and hard cuts, a Back button, a check mark or error on every button press, and a
  page you can rearrange and customize: button size, light theme, accent color, button names and your own buttons
  that run several steps (see [Remote control](#remote-control)).
- **libprojectM 4.1.8** with its fixes, e.g. a freeze with some preset shaders.
- Fixes: broken texture files were read again on every frame; transition effects could flash again at the end of a
  transition.

All features of this fork are listed below.

## What's different in this fork

### New features

- **Favorites:** save presets to a favorites folder, play only your favorites, remove them again and undo (see
  [Keybinds](#keybinds)).
- **Disable presets you don't like:** DEL moves the current preset into a `disabled` folder (in `userdata` in
  portable mode) and plays another one. Ctrl+Z undoes it. CTRL-DEL plays only your disabled presets, so you can
  re-enable some later. See [Disabled presets](#disabled-presets).
- **Preset search window** (ENTER), based on upstream PR #139.
- **FPS counter** (Settings > Window / Rendering): in any corner, with its own size setting independent of the UI
  scale. Optionally shows frame times: the time between frames, the GPU and CPU time per frame, and a graph of the
  last 240 frames. Repeated slow frames are also written to the log file.
- **Toasts** for Space (lock), Y (shuffle), A (aspect correction), Ctrl+C (copy path) and all favorites actions.
  Toasts no longer disappear instantly and can be shown in any corner or the center of the screen.
- **Quick Help** (Help menu) lists all keys, grouped by feature.
- **Portable mode:** put a file named `portable.txt` next to `projectMSDL.exe` and your settings, favorites, UI layout
  and log are stored in a `userdata` folder next to it instead of `%APPDATA%\projectM` (or the equivalent user config
  folder on Linux/macOS). Together with `presets` and `textures` folders next to the exe, the whole folder can then be
  copied or moved anywhere. If the folder can't be written to (e.g. inside `C:\Program Files`), a message says that
  settings and favorites can't be saved.
- **Remote control:** control projectM from your phone's web browser (see [Remote control](#remote-control)).
- **Spout output** (Windows): share the picture with OBS and other Spout programs, also while projectM is minimized
  (see [Spout output](#spout-output-obs)).
- **Mirror and zoom effects** (Settings > Effects, M and +/- keys): mirror the left half onto the right, the
  top half onto the bottom, or the top left quarter into all four quarters, and zoom into the center up to 4x. They
  apply to the window and the Spout output alike.
- **Ambient Speed** (Settings > Effects, LEFT/RIGHT keys): slows down everything a preset animates, evenly: motion,
  zoom, rotation, warp, fading trails and the reaction to the music. 1 = normal, 0.1 = slowest. Display and transition
  durations stay in real seconds.
- **Same Speed at Any FPS** (on by default): Milkdrop presets move a fixed step per frame, so they ran faster at 144
  FPS than at 60. Presets now move at their 60 FPS speed at any FPS limit. Turn it off for the original behavior.
- **Bass, Mid and Treble Sensitivity** (Settings > Audio, Options menu): how strongly presets react to each band, on
  top of Beat Sensitivity.
- **Frequency bands** (Settings > Audio): Milkdrop and the official projectM split only 0-11025 Hz into three equal
  parts, so "bass" was 0-3675 Hz. The default **Music** bands are bass 20-250 Hz, mid 250-4000 Hz and treble
  4000-20000 Hz. **Milkdrop Standard** brings back the original split, and **Custom** lets you set the ranges in Hz.
- **Transition effects** (Settings > projectM, Options > Transition Effects): choose which effects smooth
  transitions use. Each transition picks one of the checked effects at random. Besides the built-in Circle, Plasma,
  Fade, Sweep, Warp and Zoom Blur, there are two new ones: **Beat Pulse** dissolves the old preset in bubbles that
  pulse with the bass, **Datamosh** drags the old picture along a moving path like a video glitch. Both are from
  [C06alt1/projectm_ext](https://github.com/C06alt1/projectm_ext) (LGPL 2.1, like libprojectM).
- **Preset preloading** (Settings > projectM > Preload Next Preset, on by default): switching to a preset the graphics
  driver hasn't compiled before froze the picture for 0.1 to over 1 second. The next preset is now loaded a few
  seconds after each transition, and drivers that support it (e.g. NVIDIA) compile its shaders in the background, so
  the switch itself takes about 1 ms. This works for automatic switches, hard cuts, N, R and the remote's Next and
  Random. Presets picked by hand (search, P, BACKSPACE) load as before.
- **Linux records what's playing:** if no audio device is set, the "Monitor of ..." device of the current output is
  used instead of the microphone, so projectM visualizes the music the PC plays. Pick another device in the settings
  or with Ctrl+I.

### Settings window (File > Settings)

- **Type-in values instead of sliders:** type a number and press Enter or click **Apply**. Typed values that aren't
  applied yet are highlighted orange. Tab moves to the next box without applying, Up/Down steps the value and Escape
  cancels.
- **Most settings apply immediately,** including window size, position, monitor, exclusive fullscreen resolution,
  beat sensitivity and UI scale. Settings marked with **\*** are only used on startup.
- **Shows the values actually in use,** including those from the default `projectMSDL.properties`.
- **Reset** only appears when a value differs from its default, and disappears when you set it back.
- **UI Scaling Factor** now scales the whole UI, not just toasts.
- The **Help** tab explains all of the above and shows which default config file is used.

### Bug fixes

- R and shuffle never pick the preset that is already playing (with few presets, e.g. in favorites mode, it often
  seemed like nothing happened).
- The FPS limit was not exact (60 FPS gave about 62.5, 144 about 166). It now uses a high-resolution timer.
- Command-line options like `--presetPath` were ignored on Windows.
- With a Transition Duration of 0, presets stopped changing automatically after the first switch (libprojectM divides
  by the duration). 0 now means a 0.05 second transition, which still looks instant.
- "Exclusive Mode Resolution" had no effect (wrong setting name).
- "Enable Hard Cuts" toggled aspect correction instead of hard cuts.
- The "Play Next Preset" menu item went to the previous preset.
- Mouse input in the UI was offset on some multi-monitor setups.
- Borderless windows are kept fully on their monitor. This fixes a mouse offset when a borderless, monitor-sized
  window was positioned past the monitor edge.
- Reset buttons were cut off ("Res") at larger UI scales.
- Built-in defaults (mesh size, window size, V-Sync, logo preset) now match the shipped config file.
- **Beat Sensitivity** had no effect, because libprojectM 4 stores the value but never uses it. This fork patches
  libprojectM (see `vcpkg-overlay/ports/projectm/beat-sensitivity.patch`) to apply it to the bass/mid/treble values
  presets react to: 0 = presets don't react to the music, 1 = normal, 2 = twice as strong. Turn on **Beat Averaging** (Settings > Audio or the Options menu) for the standard Milkdrop reaction; Beat
  Sensitivity is then greyed out and the Up/Down keys don't change it.
- "Use Adaptive VSync" is greyed out while "Wait for Vertical Sync" is off (it has no effect then), the log says which
  vertical sync mode is actually used, and its description no longer suggests it's FreeSync/G-Sync.
- The app could freeze when switching presets (mostly on Linux, e.g. pressing R): the audio thread and the preset
  switch could each wait for a lock the other held. Audio data now uses its own notification channel.
- Broken presets (e.g. "Could not compile per-pixel code") were skipped silently, but the app still treated them as
  the current preset, so the window title showed the wrong name. Only presets that actually loaded count now, and a
  message is shown if none could be loaded.
- Linux: the app could close when switching in or out of fullscreen (X11 error "BadMatch" from `X_SetInputFocus`,
  e.g. on GNOME). Such harmless X11 errors are now logged instead.
- Windows: no presets were found if the folder name contained characters like ä, ö or ü.
- Preset paths no longer contain a double slash (e.g. in the path copied with Ctrl+C).
- OBS Game Capture (and similar capture tools) showed the image before the preset's final shader, so colors were wrong.
- Ctrl+S didn't open the settings, although the File menu lists it.
- A texture file that couldn't be loaded (e.g. a broken image a preset refers to) was read and decoded again on every
  frame, for as long as the preset played.
- Transition effects could briefly show up again at the very end of a transition, when the last frame came late (the
  transition went past 100% and the effect curves turned back).

## Keybinds

All keys are also listed in the app under **Help > Quick Help**.

### Favorites and disabled presets (new in this fork)

| Key    | Action                                                                     |
|--------|----------------------------------------------------------------------------|
| S      | Save the current preset to favorites                                       |
| F      | Switch between favorites only and all presets                              |
| DEL    | Disable the current preset (favorites-only mode: remove it from favorites; disabled-only mode: re-enable it) |
| CTRL-DEL | Switch between disabled presets only and all presets                   |
| CTRL-Z | Undo the last save, removal, disable or re-enable (up to 20 steps)        |

Favorites are stored as copies of the `.milk` files in `%APPDATA%\projectM\favorites\` by default, or in
`userdata\favorites\` next to the exe in portable mode. Set `projectM.favoritesPath` in your config file to use a
different folder.

### Disabled presets

DEL moves the current preset's file from the presets folder into `disabled` next to `favorites` (so
`userdata\disabled\` in portable mode), keeping its subfolder, e.g. `disabled\Waveform\Wire Flat\name.milk`. Set
`projectM.disabledPath` in your config file to use a different folder.

To look at your disabled presets again later, press CTRL-DEL: only the disabled presets play (N, P, R and the preset
search work as usual). DEL then re-enables the current one, which moves it back to its subfolder in `presets`, and
CTRL-Z undoes that. CTRL-DEL again (or F) switches back. You can also move a file back by hand.

When you update to a new version and copy your `userdata` folder over, the new `presets` folder contains all presets
again. Presets that also exist in `disabled` (same subfolder and name) are left out anyway, so they stay disabled.
Favorites are separate copies: disabling a preset doesn't remove it from your favorites.

### Preset search

| Key                  | Action                                            |
|----------------------|---------------------------------------------------|
| ENTER                | Open the preset search window                     |
| Up/Down/PgUp/PgDown  | Move through the list                             |
| Home/End             | Jump to the first/last preset                     |
| ENTER                | Play the selected preset (hold SHIFT for a smooth transition) |
| ESC                  | Close the search window                           |

The search filters the currently loaded presets, so in favorites mode it only searches your favorites. Every word you
type must appear in the preset name (not case sensitive).

### Other keys

| Key       | Action                                          |
|-----------|-------------------------------------------------|
| ESC       | Toggle the menu                                 |
| N / P     | Next / previous preset (SHIFT for smooth transition) |
| R         | Random preset                                   |
| BACKSPACE | Go back to the last preset played               |
| SPACE     | Lock/unlock the current preset (shows a toast)  |
| Y         | Toggle shuffle (shows a toast)                  |
| A         | Toggle aspect correction (shows a toast)        |
| UP / DOWN | Increase / decrease beat sensitivity (not while Beat Averaging is on) |
| LEFT / RIGHT | Slower / faster Ambient Speed                |
| M         | Next mirror mode (off, left to right, top to bottom, 4-way) |
| + / -     | Zoom in / out                                   |
| CTRL-S    | Open the settings                               |
| CTRL-C    | Copy the current preset's path (shows a toast)  |
| CTRL-F    | Toggle fullscreen                               |
| CTRL-M    | Move the window to the next display             |
| CTRL-I    | Switch to the next audio input device           |
| CTRL-Q    | Quit                                            |

## Remote control

Control projectM from your phone (or any web browser):

- **Presets:** previous, back (last played, like BACKSPACE), random and next, with an optional smooth transition;
  lock and shuffle.
- **Transitions:** preset display duration, transition duration, the transition effects, hard cuts on/off, the
  earliest hard cut and the hard cut threshold.
- **Effects:** mirror, zoom and Ambient Speed. **Music reaction:** Beat Averaging and Beat Sensitivity.
- **Favorites and disabled presets:** save and remove favorites, disable and re-enable presets, undo, and the
  favorites-only and disabled-only modes.

The page shows the current preset. Every button press is confirmed: the button turns green with a check mark when
projectM did it, or red if it couldn't (with the reason). The same text as the message on the screen is shown under the
preset name, and phones that support it vibrate briefly.

1. In **Settings > Remote / Spout** (on Linux: **Remote Control**), turn on **Enable Remote Control**.
2. To use it from your phone, also turn on **Allow Other Devices** and set a **Password** (press Enter to apply).
   Windows may ask whether projectM may use the network: allow it for private networks.
3. Open the address shown in the settings in your phone's browser, e.g. `http://192.168.1.20:8765`. The phone must
   be in the same network (Wi-Fi). The browser asks for the password once: any user name works.

Without **Allow Other Devices**, the page only opens on the PC itself (`http://127.0.0.1:8765`). The connection is not
encrypted, so only allow other devices in your home network. Web pages from other sites can't send commands to it.
Tip: add the page to your phone's home screen.

Settings changed with the remote apply right away and stay until projectM is closed, like changes in the menus. To
keep them, click **Save Settings** in the settings window.

### Customizing the remote page

Tap **Edit layout** on the page. Then you can:

- **Move and hide sections** with the buttons on each section.
- **Move, hide and rename buttons:** tap a button to select it (orange outline), then use the buttons below its group.
  Each group also has a **Buttons per row** setting.
- **Appearance:** button size (compact, normal, large), sliders or -/+ buttons, dark or light theme and the accent
  color.
- **My buttons:** **+ New button** creates your own button that runs several steps in order, e.g. "Calm" = Ambient
  Speed 0.4, no mirror, play favorites only. Select it to rename it and to add, reorder or remove steps. If a step
  fails, the button says which one.

Tap **Done** when finished. The layout is saved in projectM (`remote-layout.json` in the `userdata` folder), so every
phone and tablet gets the same page. **Reset to default** brings back the original layout.

## Spout output (OBS)

OBS Game Capture pauses while projectM is minimized, and it also captures the menus and toasts. Spout output (Windows
only) shares the picture directly with OBS instead:

1. Install the [OBS Spout2 plugin](https://github.com/Off-World-Live/obs-spout2-plugin/releases/latest).
2. In projectM, turn on **Options > Spout Output** (or **Settings > Remote / Spout**).
3. In OBS, add a **Spout2 Capture** source and pick **projectM**.

The picture keeps coming while projectM is minimized or covered, and contains no menus, toasts or FPS counter. Mirror
and zoom apply to it too. **Spout Resolution** sets a fixed size, e.g. 1920 x 1080, independent of the window; the
window then shows the same picture scaled to fit.

## Building the standalone Windows version

This builds a single `projectMSDL.exe` with all libraries (SDL2, Poco, FreeType, GLEW, projectM and the Visual C++
runtime) linked in statically, so no DLLs are needed. Requires Visual Studio (2026 here, adjust the generator name for
other versions) and [vcpkg](https://github.com/microsoft/vcpkg) (here in `C:\vcpkg`). vcpkg builds all dependencies
from `vcpkg.json` automatically during the first configure, which can take a while.

libprojectM 4.1.8 is built from the overlay port in `vcpkg-overlay/ports/projectm` (set in
`vcpkg-configuration.json`): the official vcpkg port plus these patches, which the Linux workflow applies too:

- `beat-sensitivity.patch`: makes the Beat Sensitivity setting work.
- `animation-speed.patch`: Ambient Speed and Same Speed at Any FPS.
- `band-sensitivity.patch`: Bass/Mid/Treble Sensitivity and the frequency bands.
- `render-fbo.patch`: lets projectM render into an offscreen image (for Spout and the effects), a backport of
  `projectm_opengl_render_frame_fbo()` from libprojectM 4.2.
- `failed-texture-retry.patch`: a texture file that failed to load isn't read again on every frame (from
  [iceyaye/projectm](https://github.com/iceyaye/projectm)).
- `transition-effects.patch`: choose the transition effects, the Beat Pulse and Datamosh effects (from
  [C06alt1/projectm_ext](https://github.com/C06alt1/projectm_ext)), and transitions that never go past 100%.
- `load-timings.patch`: how long each part of a preset load took, written to the log.
- `preset-preload.patch`: preset preloading with background shader compilation.

Spout output uses [Spout2](https://github.com/leadedge/Spout2) (BSD 2-Clause license) from vcpkg.

One-time setup of the build folder (the exe is installed into `C:\dev\projectm-standalone`):

```shell
cmake -G "Visual Studio 18 2026" -A x64 -S . -B cmake-build-static ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>" ^
  -DCMAKE_INSTALL_PREFIX=C:/dev/projectm-standalone ^
  -DENABLE_FLAT_PACKAGE=ON
```

Build and install after each change:

```shell
cmake --build cmake-build-static --config Release --parallel
cmake --install cmake-build-static --config Release
```

The install folder then contains `projectMSDL.exe` and the optional, commented default `projectMSDL.properties`. To
get the same layout as the release download, add the `presets` and `textures` folders (see below) and a
`portable.txt` file next to the exe:

```
projectMSDL.exe
projectMSDL.properties  (optional)
portable.txt            (optional, turns on portable mode)
presets\
  Dancer\
  Fractal\
  ...
textures\
```

### Building the release zip

[`packaging/windows-portable/make-release.ps1`](packaging/windows-portable/make-release.ps1) creates
`projectMSDL-portable-windows-x64.zip` exactly as it is published in the releases. Run it from the repository folder:

```shell
powershell -ExecutionPolicy Bypass -File packaging\windows-portable\make-release.ps1
```

It builds the static exe (and configures `cmake-build-static` first if it doesn't exist yet), installs it into a clean
folder, downloads the current presets and textures from the official repositories, adds `portable.txt`, `README.txt`
(both from `packaging/windows-portable`) and `LICENSE.md`, starts the exe for a few seconds as a test and zips it all
into a `release` folder next to the repository. Your own install and settings aren't touched. `-OutputDir` changes
where the zip is created and `-SkipTest` skips the test start.

### Building the Linux release

The Linux tarball is built on GitHub by the
[Portable Linux Release](.github/workflows/release-linux-portable.yaml) workflow (Actions tab > Portable Linux Release >
Run workflow). It builds on Ubuntu 22.04 with Poco and libprojectM linked in statically, adds the presets, textures and
the files from `packaging/linux-portable`, starts the program for 10 seconds on a virtual display as a test and
creates `projectMSDL-portable-linux-x86_64.tar.gz`. Enter a release tag to attach it to that release, otherwise it can
be downloaded from the workflow run.

## Presets and textures

The release download includes the presets and textures, unchanged from the official projectM repositories:

- **Presets:** [presets-cream-of-the-crop](https://github.com/projectM-visualizer/presets-cream-of-the-crop) by Jason
  Fletcher, about 9,800 hand-picked Milkdrop presets. This is projectM's default preset pack. See its `LICENSE.md` (also
  in the `presets` folder of the download).
- **Textures:** [presets-milkdrop-texture-pack](https://github.com/projectM-visualizer/presets-milkdrop-texture-pack),
  the images many presets use.

If no preset or texture path is set, the `presets` and `textures` folders next to the exe are used automatically. To
use other presets or textures, set their paths in **File > Settings**. When building it yourself, get them from the
links above: in the texture pack, the images are inside its `textures` folder.

---

*The original projectM SDL2 frontend README follows. Its build instructions still work and build the regular version,
with the libraries as separate DLLs.*


# ProjectM SDL2 Frontend

This is a reference implementation of an application that makes use of the projectM music visualization library.

It will listen to audio input and produce mesmerizing visuals. Some commands are supported.

This project is in a bit of a transition state and is in the process of being modernized. There are many rough edges at
present.

## Building from source

### Build and install libprojectM

First, [build libprojectM](https://github.com/projectM-visualizer/projectm/wiki/Building-libprojectM) or get it via your
favorite dependency management/packaging tool. For testing, you can install libprojectM somewhere inside your
home/development directory using `CMAKE_INSTALL_PREFIX`, then pass the same install path to the frontend-sdl2 build
using `CMAKE_PREFIX_PATH`. Please refer
to [CMake's documentation](https://cmake.org/cmake/help/latest/variable/CMAKE_PREFIX_PATH.html) for details.

Without `vcpkg-overlay/ports/projectm/beat-sensitivity.patch` everything works, except that the Beat Sensitivity
setting has no effect. To apply it to a libprojectM 4.1 checkout: `git -C projectm apply /path/to/beat-sensitivity.patch`.

### Dependencies

This project requires third-party libraries in addition to libprojectM's core library dependencies:

- SDL2 (version 2.0.16 or higher)
- POCO (recommended version 1.12 or higher, minimum is 1.9.x)
- Freetype 2 (optional, will provide better looking UI text)

**Important**: projectMSDL will _not compile_ against Poco versions from 1.10.0 up to 1.11.1, as these versions of Poco
include a serious issue that causes the application to crash. Either use Poco 1.9.x, or upgrade to 1.11.2 or higher.

Depending on your needs, you can either build them yourself or install them using your favorite package manager. Here
are some examples for the three major desktop platforms:

```shell
sudo apt install libsdl2-dev libpoco-dev libfreetype-dev cmake  # Debian/Ubuntu Linux
brew install sdl2 poco freetype  # macOS
vcpkg install sdl2 poco freetype # Windows, should be pulled in automatically via vcpkg.json
```

### Configure and build projectMSDL

After cloning or updating the Git repository, always remember to initialize and update the submodules as well. this is
not required when building from a release tarball or ZIP.

```shell
# Newer git versions also support "git submodule update --init" to perform both step in a single command.
git submodule init
git submodule update
```

If all dependencies are in the CMake and/or the system search directories, you can configure and build the application
with these commands, executed from the source dir:

```shell
mkdir cmake-build
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build --config Release
```

You can optionally add the `--parallel` argument with the number of CPU cores to use to the build command to speed up
the build.

If your dependencies are in different locations than the default search paths, or you're cross-compiling, you'll need to
add more options like `CMAKE_PREFIX_PATH`. Covering all CMake options is out of the scope of this document. Please read
the [Mastering CMake guide](https://cmake.org/cmake/help/book/mastering-cmake/index.html) and
the [CMake documentation](https://cmake.org/cmake/help/latest/) for more information.

The above command will use CMake's default build file generator for your current platform and build the project in
Release (optimized) configuration. If the build was successful, you should have an executable in the build directory. On
Windows, you may need to specify the correct Visual Studio generator and architecture manually.

### Install projectMSDL

While you can run projectMSDL directly from the build directory, it's recommended to install the project. This will copy
all required files, including a default configuration file, into the installation dir.

You can set the installation target path in the first CMake command which configures the build. The `install` target
will then copy everything under this directory:

```shell
mkdir cmake-build
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/path/to/install/dir
cmake --build cmake-build --config Release --target install
```

### Run directly from the build dir

You should have a directory of visual presets you wish to use. You can fetch a giant trove of curated
presets [here](https://github.com/projectM-visualizer/presets-cream-of-the-crop). You will also need textures (images)
used in many presets. The projectM team has
assembled [a pack of textures](https://github.com/projectM-visualizer/presets-milkdrop-texture-pack), which covers the
needs of most presets.

If you want to run the executable from the build dir, you'll need to pass any non-default settings via arguments. You
can also create a user configuration file in your user's home directory. Depending on the platform, this will be:

- Windows: `%APPDATA%\projectM\projectMSDL.properties`
- Linux:
    - If `XDG_CONFIG_HOME` env var is non-empty: `$XDG_CONFIG_HOME/projectM/projectMSDL.properties`
    - Otherwise: `~/.config/projectM/projectMSDL.properties`
- macOS: `~/Library/Preferences/projectM/projectMSDL.properties`

You can copy the [config file template](src/resources/projectMSDL.properties.in) there and change anything in `@@`.

Depending on the build system, you'll find the projectM executable in `cmake-build/src/`, or a subdirectory with the
name of your build type (`Release`, `Debug` and so on).

If you're not using a config file, provide the presets and texture paths you wish to use when starting projectMSDL:

```shell
cmake-build/src/projectMSDL --presetPath /path/to/presets-cream-of-the-crop --texturePath /path/to/textures
```

Press ESC to toggle the UI.

## System-Specific CMake Examples

The following examples show how to configure and build projectMSDL with CMake on the different supported platforms.

### Windows

To generate a Visual Studio 2022 project for Win64 and build for Release, with then option to also compile a Debug build
from within the generated solution:

```shell
mkdir cmake-build
cmake -G "Visual Studio 17 2022" -A x64 -S . -B cmake-build -DCMAKE_CONFIGURATION_TYPES=Debug,Release
cmake --build cmake-build --config Release
```

### Linux

To generate a UNIX Makefile project build for Release:

```shell
mkdir cmake-build
cmake -G "Unix Makefile" -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build
```

To generate a Ninja project build for Release:

```shell
mkdir cmake-build
cmake -G "Ninja" -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build
```

### macOS

To generate an Xcode project and build for Release:

```shell
mkdir cmake-build
cmake -G Xcode -S . -B cmake-build -DCMAKE_CONFIGURATION_TYPES=Debug,Release
cmake --build cmake-build --config Release
```

#### macOS: Full build from source (including libprojectM)

Pre-built macOS binaries are not currently published. Use the following to build everything from source:

**Prerequisites**

```shell
xcode-select --install
brew install cmake ninja sdl2 poco freetype git
```

**1. Build and install libprojectM**

```shell
git clone --recurse-submodules https://github.com/projectM-visualizer/projectm.git
cd projectm && mkdir cmake-build
cmake -G Ninja -S . -B cmake-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=~/dev/projectm-install \
  -DENABLE_PLAYLIST=ON \
  -DBUILD_SHARED_LIBS=ON
# Optional: add -DENABLE_MACOS_FRAMEWORK=ON to build as a .framework bundle
# (libprojectM 4.2+ only — easier to link against in Xcode projects)
cmake --build cmake-build --parallel
cmake --install cmake-build
cd ..
```

**2. Build frontend-sdl2**

```shell
git clone --recurse-submodules https://github.com/projectM-visualizer/frontend-sdl2.git
cd frontend-sdl2 && mkdir cmake-build
cmake -G Ninja -S . -B cmake-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=~/dev/projectm-install \
  -DCMAKE_INSTALL_PREFIX=~/dev/projectm-app
cmake --build cmake-build --parallel
cmake --install cmake-build
```

`projectM.app` will be in `~/dev/projectm-app/`. Edit
`projectM.app/Contents/Resources/projectMSDL.properties` to set your preset and texture paths, then launch the app.
