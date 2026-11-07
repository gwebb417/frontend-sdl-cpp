I forked this because I wanted to be able to save favorite presets. Along the way I fixed a number of issues with
toasts, settings and the UI. It also includes the preset search window from upstream PR #139.

## What's different in this fork

### New features

- **Favorites:** save presets to a favorites folder, play only your favorites, remove them again and undo (see
  [Keybinds](#keybinds)).
- **Preset search window:** from upstream PR #139, plus fixes. Old key presses no longer end up in the search
  field, the window no longer closes right after opening, and it no longer crashes when no presets are loaded.
- **Toasts** for Space (lock), Y (shuffle), A (aspect correction), Ctrl+C (copy path) and all favorites actions.
  Toasts are shown in the top-right corner and no longer disappear instantly.
- **Quick Help** (Help menu) lists all keys, grouped by feature.
- **Portable mode:** put a file named `portable.txt` next to `projectMSDL.exe` and your settings, favorites, UI layout
  and log are stored in a `userdata` folder next to it instead of `%APPDATA%\projectM` (or the equivalent user config
  folder on Linux/macOS). Together with `presets` and `textures` folders next to the exe, the whole folder can then be
  copied or moved anywhere.

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
- M opens the preset search; Ctrl+M only moves the window to the next monitor.
- "Exclusive Mode Resolution" had no effect (wrong setting name).
- "Enable Hard Cuts" toggled aspect correction instead of hard cuts.
- The "Play Next Preset" menu item went to the previous preset.
- Mouse input in the UI was offset on some multi-monitor setups.
- Borderless windows are kept fully on their monitor. This fixes a mouse offset when a borderless, monitor-sized
  window was positioned past the monitor edge.
- Reset buttons were cut off ("Res") at larger UI scales.
- Built-in defaults (mesh size, window size, V-Sync, logo preset) now match the shipped config file.

## Keybinds

All keys are also listed in the app under **Help > Quick Help**.

### Favorites (new in this fork)

| Key    | Action                                                         |
|--------|----------------------------------------------------------------|
| S      | Save the current preset to favorites                           |
| DEL    | Remove the current preset from favorites                       |
| F      | Switch between favorites only and all presets                  |
| CTRL-Z | Undo the last save/delete (up to 20 steps)                     |

Favorites are stored as copies of the `.milk` files in `%APPDATA%\projectM\favorites\` by default. Set
`projectM.favoritesPath` in your config file to use a different folder.

### Preset search

| Key                  | Action                                            |
|----------------------|---------------------------------------------------|
| M                    | Open the preset search window                     |
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
| UP / DOWN | Increase / decrease beat sensitivity            |
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

The install folder then contains `projectMSDL.exe` and the optional, commented default `projectMSDL.properties`. For
a portable setup, add next to the exe:

- `presets\`: e.g. [Cream of the Crop](https://github.com/projectM-visualizer/presets-cream-of-the-crop)
- `textures\`: e.g. the [Milkdrop texture pack](https://github.com/projectM-visualizer/presets-milkdrop-texture-pack)
- `portable.txt`: turns on portable mode, settings and favorites are then kept in `userdata\`

If no preset/texture path is set, the `presets` and `textures` folders next to the exe are used, with or without
`projectMSDL.properties`.


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
