I forked this because I wanted to be able to save favorite presets. Along the way I fixed a number of issues with
toasts, settings and the UI. It also includes the preset search window from upstream PR #139.

This is an unofficial fork, not an official projectM release.

## Download

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

## What's different in this fork

### New features

- **Favorites:** save presets to a favorites folder, play only your favorites, remove them again and undo (see
  [Keybinds](#keybinds)).
- **Disable presets you don't like:** DEL moves the current preset into a `disabled` folder (in `userdata` in
  portable mode) and plays another one. Ctrl+Z undoes it. CTRL-DEL plays only your disabled presets, so you can
  re-enable some later. See [Disabled presets](#disabled-presets).
- **Preset search window** (ENTER): from upstream PR #139, plus fixes. Old key presses no longer end up in the search
  field, the window no longer closes right after opening, and it no longer crashes when no presets are loaded.
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

- Shuffle no longer turns itself off after pressing N, P or R.
- R and shuffle never pick the preset that is already playing (with few presets, e.g. in favorites mode, it often
  seemed like nothing happened).
- The FPS limit was not exact (60 FPS gave about 62.5, 144 about 166). It now uses a high-resolution timer.
- Command-line options like `--presetPath` were ignored on Windows.
- With a Transition Duration of 0, presets stopped changing automatically after the first switch (libprojectM divides
  by the duration). 0 now means a 0.05 second transition, which still looks instant.
- The preset search window was cut off in windows smaller than about 1130x540 pixels. It now fits the window and
  opens centered.
- Ctrl+M only moves the window to the next monitor (the preset search is on ENTER now).
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
  presets react to: 0 = presets don't react to the music, 1 = normal, 2 = twice as strong. (v1.1 scaled the audio
  instead, which faded out after a few seconds because libprojectM compares the values with their recent average.)
  Turn on **Beat Averaging** (Settings > Audio or the Options menu) for the standard Milkdrop reaction; Beat
  Sensitivity is then greyed out and the Up/Down keys don't change it.
- "Use Adaptive VSync" is greyed out while "Wait for Vertical Sync" is off (it has no effect then), the log says which
  vertical sync mode is actually used, and its description no longer suggests it's FreeSync/G-Sync.
- The app could freeze when switching presets (mostly on Linux, e.g. pressing R): the audio thread and the preset
  switch could each wait for a lock the other held. Audio data now uses its own notification channel.
- Broken presets (e.g. "Could not compile per-pixel code") were skipped silently, but the app still treated them as
  the current preset: the title showed the wrong name, S saved the broken preset instead of the one on screen, and
  favorites mode seemed stuck. Only presets that actually loaded count now, and a message is shown if none could be
  loaded.
- Linux: the app could close when switching in or out of fullscreen (X11 error "BadMatch" from `X_SetInputFocus`,
  e.g. on GNOME). Such harmless X11 errors are now logged instead.
- Windows: no presets were found if the folder name contained characters like ä, ö or ü.
- Preset paths no longer contain a double slash (e.g. in the path copied with Ctrl+C).

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
| CTRL-C    | Copy the current preset's path (shows a toast)  |
| CTRL-F    | Toggle fullscreen                               |
| CTRL-M    | Move the window to the next display             |
| CTRL-I    | Switch to the next audio input device           |
| CTRL-Q    | Quit                                            |

## Building the standalone Windows version

This builds a single `projectMSDL.exe` with all libraries (SDL2, Poco, FreeType, GLEW, projectM and the Visual C++
runtime) linked in statically, so no DLLs are needed. Requires Visual Studio (2026 here, adjust the generator name for
other versions) and [vcpkg](https://github.com/microsoft/vcpkg) (here in `C:\vcpkg`). vcpkg builds all dependencies
from `vcpkg.json` automatically during the first configure, which can take a while.

libprojectM is built from the overlay port in `vcpkg-overlay/ports/projectm` (set in `vcpkg-configuration.json`): the
official vcpkg port plus `beat-sensitivity.patch`, which makes the Beat Sensitivity setting work. The Linux workflow
applies the same patch.

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
