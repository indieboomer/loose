# LOOSE — Level Zero

Native first-person C++20 prototype. Rotate the perceived world, let the player and orange cube fall to the new floor, and reach the black opening in the original ceiling. The cube is a physics demonstration, not a key.

## Build and launch on Windows x64

Install Visual Studio 2022 or 2026 with **Desktop development with C++**, the Windows SDK, CMake 3.25+ and Git. A Vulkan-capable GPU driver is required. A Vulkan SDK is optional: the build fetches pinned Vulkan headers and builds its own GLSL compiler, while the application loads the driver's Vulkan loader directly.

From the project directory:

```powershell
.\tools\build.cmd
.\tools\run.cmd
```

The build script discovers Visual Studio with `vswhere`, initializes the x64 compiler environment, selects the bundled Ninja, configures CMake, builds Release and runs the mechanics tests. The first configure downloads dependencies into ignored `build/_deps`; subsequent builds reuse them. No network connection is used by the game.

For Debug, including Jolt assertions and Vulkan validation when the layer is installed:

```powershell
.\tools\build.cmd debug
.\build\Debug\loose.exe
```

In an **x64 Native Tools Command Prompt**, the equivalent preset commands are:

```text
cmake --preset windows
cmake --build --preset release --parallel 8
ctest --preset release
build\Release\loose.exe
```

Ensure a working `ninja.exe` is on PATH; the helper script selects the Visual Studio copy. To build without network access after initial setup, pass `-DFETCHCONTENT_UPDATES_DISCONNECTED=ON` to CMake. The executable's neighboring `shaders/` directory contains the compiled SPIR-V files and must travel with it. The C++ runtime is linked statically. Dependency licenses are in `docs/licenses`.

## Controls

| Input | Action |
| --- | --- |
| WASD / mouse | Move / look in the current orientation |
| Space | Jump while grounded |
| Q / E | World rolls counterclockwise / clockwise on screen |
| Backspace | Immediately reset everything, including during a turn |
| Escape | Pause, release mouse; Escape again resumes |
| F1 | Control help and diagnostic/settings panel |

Press Escape to operate settings with the mouse. Settings include FOV, sensitivity, rotation duration, shadows, AO, AO-only/depth/normal views, collider boxes and room axes. Engineering information stays hidden in normal play. Only **Q/E rolls** are enabled in normal play; R/F pitch controls are disabled for now. Q takes priority over E for simultaneous key-downs. Held keys do not repeat turns, and inputs during a turn are discarded.

The simple solution is two rolls in the same direction, followed by walking along the former ceiling into the black opening. Leave clearance for the rotating capsule: a rejected turn displays `MOVE AWAY FROM THE SURFACE`. Intermediate walls become floors. The orange cube is never required to complete the level. Backspace recovers any blocked demonstration.

## Verification commands

```powershell
ctest --preset release
.\build\Release\loose.exe --verify --validation
.\build\Release\loose.exe --smoke 240 --size 1920 1080
py -3 tools\input_playthrough.py
```

`--verify` runs a visible deterministic playthrough, creates PPM screenshots in ignored `captures/`, exercises roll and pitch landings, saves AO/shadow comparisons, resizes/minimizes/restores the native window and restarts. It uses the game's movement and physics; it does not teleport actors to win. `input_playthrough.py` additionally sends window key messages through the real GLFW callbacks and verifies movement, held Q, ceiling entry, mouse capture/release, pause/resume and reset during a turn. It requires Windows and Python 3, with no third-party Python packages. Close other LOOSE windows before running it. If `py -3` points to an inaccessible Windows Store installation, use the full path of a normal Python installation.

Developer helpers: `--telemetry` writes `captures/input-state.json`; F12 captures the rendered frame; `--smoke N` renders N frames and exits; `--size W H` chooses the initial window size; `--no-ao` supports performance comparisons. Missing Vulkan support is reported to the console with a nonzero exit code. Debug enables the validation layer automatically when discoverable; Release accepts `--validation`. If the layer is absent, the application explicitly reports that validation is unavailable.

Read [architecture](ARCHITECTURE.md), [dependency versions and licenses](DEPENDENCIES.md) and [actual verification results](VERIFICATION.md) for implementation details and limits. The root README remains the original specification.
