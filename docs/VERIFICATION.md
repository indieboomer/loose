# Verification results

Verified on Windows x64 on 2026-09-30, using Visual Studio 2026 Community (MSVC 19.51), CMake/Ninja and an **NVIDIA GeForce RTX 3060**. The original repository contained only the specification. No third-party engine or renderer was used.

## Fixed-gravity level rotation update

After replacing gravity-vector turns with level rotation, Release and Debug builds pass all 320 mechanics checks. The native `--verify` route also completes all 871 frames, including ceiling exit, reset, resize and internal pitch-turn checks. New inverse-turn tests confirm inertial linear/angular velocity preservation. Existing screenshot pixel comparisons and performance figures below describe the original verification session; they were not remeasured for this update.

## Build and mechanics

Both `tools/build.cmd` (Release) and `tools/build.cmd debug` built successfully and passed CTest. The final mechanics executable reports **320 checks passed**. These cover:

- All four quarter-turn directions, projected on-screen perceptual signs, and four same-axis turns restoring the quaternion.
- Unsnapped yaw, pitch-independent horizontal axes, a degenerate projection fallback, normalized quaternions, constant gravity magnitude and orthonormal movement bases.
- Actual Jolt settling/sleeping, explicit wake on level rotation, angular motion during collisions, and preservation of inertial linear and angular velocity through a frozen transition, including inverse-turn velocity restoration.
- Capsule eye anchoring and commit offsets, both rotation axes, stale-ground-state clearing, grounded movement and jumping after cardinal and mixed-axis/non-cardinal rotations.
- Diagonal movement normalization, bounded cube pushing, standing on the cube, thick-wall containment after repeated falls and near-wall clearance rejection.
- Reset during rotation, pause and completion, repeatable spawn/velocities, paused-time discard and a full physics-driven solution through the actual ceiling aperture.
- Cube entry alone does not trigger completion. The successful route leaves cube placement to physics and never requires precisely arranging it.

Release checks took approximately 0.2–0.3 seconds; Debug checks approximately 1.2 seconds on this machine. These are correctness timings, not game performance measurements.

## Native input and rendered playthrough

`tools/input_playthrough.py` launched the game, sent key messages through its actual Windows window and GLFW callbacks, and passed. It checked W/A movement, Q held across a completed transition without repeats, two rolls and real ceiling entry, completion releasing the mouse, Backspace from completion, Escape pause/resume, F1 help, frozen paused position and Backspace during a turn. It observed 16 real key-down callbacks. The game closed normally through WM_CLOSE. The test uses the actual live frame timing and physics, rather than invoking game turns directly.

`loose.exe --verify --validation` also completed its 871-frame developer route: initial settling, Q wall landing, second Q ceiling landing, entering the shaft, completion, reset, pause/resume, resize to 960×640, minimize/restore, resize back, and R/F transitions and landings. The route moves with the same game inputs and physics; it does not teleport actors to win. Its changed camera headings during inspection do not change gravity.

The screenshots were opened and inspected. Initial and changed-floor cube views show correct materials, lighting, physical cube contact and AO; R moves the scene ahead upward and F downward. The black aperture and capped shaft are visible, and the completed frame displays the requested restart message inside the black shaft. No black/white coplanar rim faces remain after making the lining slightly proud of the ceiling and aperture sides.

The native route produces PPM files in ignored `captures/`. PNG inspection copies are also present in this workspace from the verification session. Useful artifacts:

| Capture | Evidence |
| --- | --- |
| `initial.ppm` | Spawn view, orange cube, room and black ceiling aperture |
| `cube-contact.ppm` | Full cube with real cast shadow and contact AO |
| `changed-orientation.ppm` | Cube resting on the former left wall, now a floor |
| `roll-transition.ppm` | Perceived counterclockwise Q rotation around the anchored eye |
| `r-transition.ppm`, `f-transition.ppm` | Opposite perceptual pitch directions |
| `r-landing.ppm`, `f-landing.ppm` | Cube/contact lighting after the other rotation axis |
| `initial-depth.ppm`, `initial-normals.ppm` | Actual G-buffer depth and view-space normals |
| `cube-contact-ao.ppm`, `changed-ao.ppm`, `r-ao.ppm`, `f-ao.ppm` | Depth/normal SSAO in several orientations |
| `completed.ppm` | Black shaft, completion and restart/quit overlay |
| `reset.ppm` | Repeatable scene after completion/window lifecycle tests |
| `interactive-*.ppm` | Frames captured through F12 during the GLFW input test |
| `run.txt`, `input-playthrough.log` | Native route and input-test logs |

Shadow toggles changed 6,671 pixels in the initial cube-contact view and 9,951 pixels on the changed floor. AO toggles changed 56,321 and 126,599 pixels respectively. These comparisons were made after the cube settled, with the same camera and exposure; the effects are actually used by the renderer. R/F contact AO views were also inspected. In the close R/F cube-facing views, cast shadows are hidden behind the cube; the saved shadow toggle pairs there have no visible difference. The initial and Q views are the positive visual shadow checks.

## Performance actually measured

```powershell
.\build\Release\loose.exe --smoke 240 --size 1920 1080
```

At **1920×1080**, with shadows, full-resolution SSAO and FXAA enabled, the 240-frame smoke run reported **16.6348 ms mean CPU frame including FIFO presentation** after the first 30 frames. This is approximately the 60 Hz presentation limit. It is not an isolated GPU timestamp benchmark or a promise for other hardware. The interactive 501-frame input run averaged 17.89 ms including screenshot readbacks; scripted capture/resize routes averaged about 22.9 ms and are not representative steady gameplay benchmarks.

## Explicit verification limits

**The Vulkan validation layer is not installed on this machine.** The game reports `Validation layer unavailable` and `Validation active: 0`. Its zero validation-error counter is therefore not evidence of a clean validation-layer run. Both builds support the layer when available; synchronization validation and long-duration leak profiling were not performed. No GPU-only timestamp measurements or second-GPU testing were performed. The playthrough is automated native interaction, not a separate human usability review.

To finish validation-layer verification on a machine with the [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home) installed and its layers discoverable:

```powershell
.\build\Release\loose.exe --verify --validation
```

Confirm `Validation active: 1` and no warnings/errors during startup, turns, captures, swapchain recreation and shutdown. Then run normal play with `--validation`, repeat mixed-axis turns and near-surface rejection, minimize/restore, and use F1 to toggle AO/shadows/debug views. Resource allocation is not performed per Vulkan rotation; the same offscreen targets are reused, and only resize recreates them. A long-duration memory/validation run is still the appropriate remaining check.
