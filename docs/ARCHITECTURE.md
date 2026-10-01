# Architecture and important implementation notes

## Coordinates and the orientation transition

Current control decision: R/F world-pitch bindings are disabled at the user's request. Normal play exposes Q/E rolls only, with Q taking priority over E. Pitch-turn implementation and developer checks remain available internally for possible re-enabling; control hints omit R/F.

Modules are `main.cpp` (GLFW application/input), `game.cpp` (state and fixed-step timing), `orientation.cpp` (frame and transitions), `physics.cpp` (Jolt world and capsule controller), `level.cpp` (room/material definition), `renderer.cpp` plus `vulkan_api.hpp` (Vulkan), and `ui.cpp` (presentation/settings). `verification.cpp` contains the optional developer route. Character/world logic shares one small physics facade because there is only one player and one dynamic object; it does not introduce an engine framework.

Rendering, lights, exit tests and the public physics facade use logical room coordinates. Jolt uses a separate physics frame with fixed gravity `(0,-9.81,0)`. At each turn commit, all level colliders and the cube pose rotate around the anchored eye. An accumulated rigid room-to-physics transform maps positions, orientations, movement vectors and overlap queries between the two frames. The renderer retains its existing camera animation; this is visually equivalent to rotating the whole scene around the eye.

`Orientation::frame` is a normalized quaternion mapping the local horizontal frame into the room. Camera orientation is `frame * pitchQuaternion`; yaw rotates `frame` around current up. Pitch is independent, limited to ±85 degrees. `up = frame * (0,1,0)`. Horizontal forward is the camera forward projected perpendicular to up, with a previous-valid fallback; right is `cross(horizontalForward, up)`. Neither mouse pitch nor mouse movement changes gravity.

At a rotation key-down, the axis and room-space eye are latched. Q/E use horizontal forward, R/F use right. In this right-handed convention, camera-frame angles are Q +90, E -90, R -90 and F +90 degrees. Inverse camera projection makes Q's scene move counterclockwise and R's scene ahead move upward. This is checked using screen-space projected points and captured transitions.

The state machine is Playing → Rotating → Playing, with Paused storing its previous state and Completed stopping simulation. During a turn, physics and locomotion deliberately freeze for 0.45 seconds (adjustable 0.25–0.8). A smoothstep curve interpolates the quaternion angle around the latched axis; the eye stays fixed. Mouse delta is ignored and discarded each frame. There is no rotation queue.

At commit, the inverse turn rotates level colliders and the cube pose in Jolt, keeping player up aligned with physics +Y. Gravity remains fixed. Cube linear and angular velocities are preserved in inertial physics coordinates, rather than in room coordinates: rotating back therefore does not reinterpret its accumulated fall speed as opposing gravity. Collider and cube contact caches are invalidated, and the cube is activated. The small `CharacterVirtual` instance is recreated around the same eye to discard stale contact/grounding state; its velocity starts at zero. Simulation and interpolation histories restart without accumulated transition time. Reset restores all colliders and the coordinate transform. This supersedes the original gravity-vector turning implementation at the user's request; turns still freeze simulation during their animation.

## Eye-to-body conversion and capsule clearance

CharacterVirtual's stored position is the **capsule shape origin/center**, not feet. Capsule radius is 0.30 m; cylindrical half-height is 0.60 m; total height is 1.80 m. Eye offset from the stored center is `(1.65 - 0.90) * up = 0.75 * up`. Spawn center is `spawnFeet + 0.90 * up`. Jolt's 2 cm character padding shifts its collision center along up; the eye conversion uses the stored origin consistently, and clearance queries explicitly include that padding.

Before a turn, 181 Jolt narrow-phase overlaps sample the entire 90-degree capsule path at 0.5-degree intervals, including both endpoints. Each pose rotates around the anchored eye and is checked against all static boxes and the frozen dynamic cube. The inner character body is excluded with its layer. A capsule enlarged radially by 7 mm encloses intermediate arcs: the rounded shape's isotropic radius needs no rotational sweep, and the most distant point on its centerline moves less than 6 mm to the nearest sample. Query collision tolerance is 0.5 mm and accepted penetration is at most 3 mm. This deliberately conservative check allows initial support contact because the controller's padding leaves clearance, but rejects meaningful geometry penetration. Rejection leaves every actor and gravity unchanged; there is no relocation to room center.

If changing character dimensions, recalculate the inflation from `(eyeOffset + cylindricalHalfHeight + padding) * sin(halfSampleAngle)` and keep inflation above that bound. The current 7 mm value has margin for the configured centerline. This method is chosen for a tiny level with one cube; its O(samples × nearby colliders) cost occurs only on key-down.

## Jolt and fixed-step movement

Jolt 5.3.0 owns all collision and rigid-body simulation. CharacterVirtual sweeps the capsule, handles sliding, step-up and ground support. The controller has a kinematic inner capsule so the real cube can see it, including with CCD. Layer 0 is static, 1 dynamic, 2 character inner body; rigid-body pairs collide when either is dynamic. Character overlap/update queries exclude layer 2 to avoid self-collision.

The game advances at 120 Hz, with an accumulator capped at 0.1 seconds. The higher frequency keeps character/cube contact and transitions readable without requiring substepping. Player eye and cube pose interpolate between adjacent fixed steps. Pausing, minimizing, rotating and completing suspend accumulation; reset and commits clear interpolation. No elapsed paused duration is simulated on resume.

Movement is projected onto the current horizontal basis and diagonals are normalized. Vertical velocity explicitly receives `-9.81 * up * dt`. **Jolt 5.3.0 ExtendedUpdate does not automatically accelerate the character with its gravity parameter**; that parameter participates in pushing the ground body, which is why velocity integration is explicit. Movement and current up are transformed into physics coordinates before integration. Grounding, jump direction, stick-to-floor and step vectors use physics up (+Y). The 60-degree slope limit permits a supporting room face even when arbitrary gravity aims toward a three-wall corner (the steepest dominant face is 54.74 degrees). This is a generous limit appropriate to level zero.

The cube uses a 1.2 m Jolt box, 30 kg mass with computed inertia, friction 0.6, restitution 0.05, linear damping 0.05, angular damping 0.08 and LinearCast CCD. It can sleep, spin, be pushed and support the player. Character push force is bounded at 250 N; less than approximately 177 N cannot overcome this cube's initial floor friction. No manually attached visual shadow or landing teleport exists.

## Level and completion

`Level::zero()` isolates geometry, material values, light placement and player dimensions. Interior bounds are x ±6, y 0–6, z ±5. Walls and ceiling/floor slabs are 0.4 m thick. The player spawns at feet `(0,0.05,3.8)` with a 12-degree upward look to expose the ceiling exit in the initial 16:9 view. The cube starts at `(2,0.65,0)` and visibly settles.

Four ceiling slabs surround a 2×2 m opening at `(0,6,-2)`. A 1.5 m capped black shaft surrounds it. Black lining projects 1 cm into the aperture and its bottom rim projects 5 mm below the ceiling, avoiding coplanar white/black side and rim faces; usable aperture is 1.98×1.98 m. No full ceiling collider hides behind the opening. Completion explicitly tests the **player center** inside the shaft beyond y=6.10, with inset x/z bounds. The eye is not a valid upside-down trigger because it trails the shape by 0.75 m. The cube has no path to trigger completion.

## Vulkan rendering and synchronization

The renderer is written directly against Vulkan 1.1 and GLFW surfaces; it does not use ImGui's swapchain helpers. Entry points load from the platform's Vulkan driver loader. No alternate graphics backend is present.

1. Room-fixed 2048² D32 spotlight shadow pass with real room/cube transforms, depth bias and 3×3 PCF.
2. Full-resolution G-buffer: linear RGBA8 albedo, signed RGBA16F view normals, sampled D32 depth. Box vertices have outward winding and correct per-face normals, so the room's inward-facing box surfaces are visible from inside.
3. Fullscreen lighting reconstructs view positions with the inverse of the actual Vulkan zero-to-one depth projection. SSAO uses a deterministic 16-sample hemisphere with radius up to 0.55 m, plus four 8-sample neighbors weighted by normal and view-depth differences. Out-of-screen samples are skipped. AO affects only ambient/indirect light. Camera and light transforms remain consistent through turns.
4. Linear RGBA16F lit target with a fixed exposure, restrained tone-map shoulder, a rough low specular response and a room-fixed spotlight plus ambient fill.
5. Lightweight FXAA pass, bypassed in diagnostic views, then ImGui. The sRGB swapchain performs the single final linear-to-sRGB conversion. Geometry material values are already linear.

One frame is in flight. A fence protects shared uniform data, offscreen attachments and the command buffer; single flight is intentionally sufficient for this scene. Acquire uses one semaphore, and each swapchain image owns its presentation-finished semaphore so it is not recycled while presentation still uses it. Render-pass external dependencies synchronize color/depth writes with sampling. Screenshot readback explicitly transitions the swap image to transfer source and back, with a transfer-to-host buffer barrier before reading after the fence.

Resize waits for the device, recreates dependent views/images/framebuffers/pipelines and updates descriptors. UI Vulkan resources are rebuilt **before** the next UI frame, preventing old font descriptors from entering a fresh draw list. Zero-size/minimized windows skip rendering and simulation. Destruction waits for device idle and releases framebuffers before their views/images, then pipelines/passes, descriptors/samplers, commands/synchronization, device, surface and instance.

There is no temporal AA, auto-exposure, audio, external art, ECS, editor or asset pipeline. SSAO is full resolution to avoid half-resolution silhouette artifacts. Lighting and AO quality/performance can be tuned in the isolated GLSL shaders.
