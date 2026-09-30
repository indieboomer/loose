LOOSE — LEVEL ZERO
Implementation prompt for Codex

Build a complete, runnable native first-person game prototype from an empty project. Implement it, build it, and verify it; do not stop at a design document or skeleton. The working title is LOOSE. This is a demonstration of one mechanic: rotating the perceived world around the player, then watching the player and loose objects fall toward the new floor.

1. SCOPE AND TECHNOLOGY

Primary target: Windows 10/11 x64 desktop. Use C++20, CMake, Vulkan, GLFW for window/input, GLM for math, and Jolt Physics for collision detection, rigid bodies and character collision. Small infrastructure libraries such as Vulkan Memory Allocator and a Vulkan loader/bootstrap helper are allowed. Dear ImGui is allowed for a small debug/settings overlay. Use stable dependencies pinned to explicit tags or commits, with documented licenses. Check actual APIs against the pinned dependency versions.

The renderer and game application must be written from scratch using Vulkan. No Unity, Unreal, Godot, existing game engine, OpenGL fallback, web implementation or pre-rendered scene. Third-party physics and low-level utility libraries are explicitly permitted. Do not implement a physics engine from scratch. Do not build a general-purpose editor, ECS framework or asset pipeline.

Create all geometry procedurally. No downloaded models, external art assets, accounts, network services or runtime AI. The end result must launch directly into a playable room.

2. DESIGN INTENT

The setting is a minimalist preservation machine. Fixed architecture is matte off-white. Unsecured physical objects are saturated orange. The exit is black. This distinction must be understandable without reading anything.

One empty rectangular room, one orange dynamic cube, one black exit in the original ceiling. No enemies, traps, hinges, inventory, weapons, story sequences, health, fall damage or additional rooms. Do not add features from a hypothetical full game.

The cube demonstrates physics; it is NOT a key and is NOT required to unlock the exit. The player completes the room by changing orientation and reaching the ceiling exit. The level must remain completable without precisely positioning the cube.

3. CONTROLS

WASD: move relative to the player's horizontal viewing direction and current up vector.
Mouse: yaw and pitch, with pitch limited to approximately +/-85 degrees.
Space: jump when grounded.
Q/E: opposite 90-degree world rolls around the horizontal forward axis.
R/F: opposite 90-degree world pitches around the horizontal right axis.
Backspace: reset the entire level immediately.
Escape: pause and release mouse capture; allow resume and quit.
F1: toggle control help/debug information.

R is an orientation control, never reset. Consume a rotation on key-down, not every frame. Holding a key must not repeatedly rotate. Ignore additional rotation inputs during a transition; no hidden queue. If simultaneous rotation keys arrive, use a documented deterministic priority.

Q makes the world appear to roll counterclockwise on screen; E clockwise. Define and label R as bringing the scene ahead upward and F as bringing it downward. Verify these perceptually, not just by quaternion sign.

Use a small center reticle and unobtrusive control hints. No head bob, motion blur, camera shake, automatic forward motion or cinematic camera.

4. PRECISE ROTATION SEMANTICS

Use room coordinates for all simulation data. Initially +Y is up, -Z is forward, and gravity is (0,-9.81,0). Static level colliders never rotate or teleport in simulation space.

Implement perceived world rotation through the camera/player orientation and a changed room-space gravity vector. This is a deliberately controlled prototype, not the simulation of an accelerating physical building. Do NOT additionally transform all rigid bodies, or apply both camera rotation and a render-root rotation.

Maintain a quaternion orientation frame and a current player-up vector U. At the start of a rotation, derive horizontal forward H by projecting the camera forward vector onto the plane perpendicular to U, then normalize. Compute horizontal right S from H and U using the project's handedness. Preserve the previous valid H if the projection becomes numerically degenerate.

Latch the requested axis and camera eye position at key-down. Q/E use H; R/F use S. The axis is fixed throughout that transition. Mouse pitch must not tilt these axes out of the current horizontal plane. Looking around must not alter gravity. Horizontal headings can be arbitrary; do not silently snap the player's yaw to cardinal room directions.

State machine: Playing -> Rotating -> Playing, plus Paused and Completed.

For Rotating:
- Duration initially 0.45 seconds, exposed as a setting between roughly 0.25 and 0.8 seconds.
- Freeze rigid-body simulation and character locomotion for this short transition. This freeze is an intentional design choice.
- Anchor the camera eye at its current room-space position.
- Smoothly rotate the camera orientation by the inverse of the desired apparent world rotation, using a quaternion and an ease-in/ease-out curve.
- Rotate the player-up vector by that same camera-frame rotation. Preserve the player's relative look pitch.
- Ignore mouse-look input during the transition and discard accumulated mouse delta afterward.
- Do not rotate any static or dynamic object transform in room coordinates. Render the frozen cube and room through the rotating camera.

At the end:
- Commit the exact 90-degree target orientation, normalize the quaternion, and set gravity to -9.81 * newUp.
- Update both character up and character collision-shape orientation; changing gravity alone is insufficient.
- Reposition the character shape consistently around the anchored eye, resolving the eye-to-body offset explicitly.
- Clear the player's velocity for a readable, forgiving restart of falling; preserve the cube's room-space linear and angular velocities from before the pause.
- Wake all dynamic bodies, clear stale grounded state and physics interpolation history, and resume fixed-step simulation.
- The cube and player now fall toward the new floor. Never attach the cube to the camera or teleport it to a landing point.

Changing gravity is global: the player and cube must agree on down. The visual result must look like the room rotated around the viewer and the cube then fell downward in the viewer's new orientation.

5. COLLISION SAFETY DURING ROTATION

The anchored eye and a rotating capsule require careful handling near walls. Do not ignore this geometry problem.

Before starting a rotation, validate the proposed character-shape path around the anchored eye against frozen level geometry and the cube. Use appropriate shape queries/conservative sampling, including the endpoint, with a small contact tolerance. If the rotation would place the player through geometry, reject it with a brief 'MOVE AWAY FROM THE SURFACE' hint. Do not teleport the player to the center of the room. Support-touching at the initial pose should not incorrectly reject every rotation.

This limitation is acceptable for level zero. There must be ample space at spawn and in the room center for both rotation axes and successive rotations. Document the chosen clearance method. If a safe conservative implementation requires slightly more clearance, prefer it to clipping or explosive penetration correction.

6. CHARACTER AND JOLT INTEGRATION

Use Jolt CharacterVirtual or an equivalently robust Jolt-backed capsule controller. Start with radius approximately 0.30 m, total height 1.8 m, eye height 1.65 m, movement speed 3.8 m/s and jump speed 4.5 m/s. Keep dimensions configurable. Define whether the stored position is feet, center or shape origin and consistently convert it to camera eye position for every up direction.

Project movement onto the current horizontal plane. Normalize diagonal movement. Apply gravity explicitly to character velocity as required by the chosen Jolt character API; passing gravity to an update helper may not itself accelerate the character. Check the pinned Jolt documentation. Ground detection, slope limits, jumping and camera offsets must all use currentUp rather than hardcoded +Y.

The player collides with the cube, can gently push it, and can stand on it. Bound pushing impulses so touching the cube does not launch it. No grabbing mechanic. Configure appropriate collision layers and character/rigid-body interactions.

Use a fixed simulation step of 1/120 second or 1/60 second with justified settings, a capped accumulator and render interpolation. Suspend accumulation while paused/rotating; never simulate the entire paused duration on resume. Reset interpolation on level reset and orientation commits.

The cube must be a real dynamic Jolt box with angular motion, friction, restitution and sleeping. Suggested starting values: side 1.2 m, mass 30 kg, friction 0.6, restitution 0.05, modest damping. Enable suitable continuous collision handling if needed to prevent tunneling. Thick static walls must contain it after repeated falls.

7. LEVEL ZERO GEOMETRY AND WIN CONDITION

Use meters. Suggested interior bounds: x=[-6,6], y=[0,6], z=[-5,5]. Static walls are at least 0.3 m thick. Interior wall faces must render correctly from inside; use consistent winding and normals. Avoid coplanar duplicate surfaces.

Spawn the player near feet position (0,0.05,3.0), facing toward -Z with a slight upward look so the room and ceiling exit can be discovered immediately. Spawn the cube near center (2,0.65,0), slightly above the original floor so its first settling motion demonstrates that it is physical.

Build an actual approximately 2 x 2 m opening in the original ceiling centered around (0,6,-2). Construct the ceiling from surrounding slabs, not a full collider hidden behind a black decal. Above the opening, add a short black-lined shaft about 1.5 m deep, with side walls and a capped far end. The opening and its rim must read as unmistakably black against the white architecture.

Place a player-only completion volume inside the shaft, beyond the original ceiling plane. Reaching it displays 'LEVEL ZERO COMPLETE' and 'BACKSPACE TO RESTART', releasing mouse capture or presenting a minimal restart/quit overlay. The cube must never trigger completion. If CharacterVirtual is not tracked by Jolt sensor events in the chosen integration, explicitly query the player shape/position against the trigger instead of assuming events will work.

The intended simple solution is to change orientation until gravity points toward the original ceiling, land on it, then move into the black opening and fall into the short exit shaft. Intermediate walls may become floors. The cube falls under the same gravity and makes the change obvious. Ensure the shaft dimensions accommodate the player and that the cube does not make the exit permanently inaccessible. Backspace must always recover a blocked demonstration.

Add only sparse seams or subtly different off-white values to distinguish room faces and support orientation. No decorative clutter, text-covered walls or additional puzzle elements. All geometry and material values should be adjustable in a small level configuration or clearly isolated definition.

8. VULKAN RENDERING AND VISUAL QUALITY

The room must look intentionally designed, not like an unlit debug scene. Implement working real-time lighting, dynamic shadows and genuine screen-space ambient occlusion.

Use a conventional, compact raster pipeline. A depth/normal prepass followed by SSAO and forward lighting is acceptable; a small deferred renderer is also acceptable. Choose one and finish it rather than building multiple backends.

Required:
- Perspective camera; approximately 85-90 degree horizontal FOV with a clear aspect conversion, configurable sensitivity and FOV.
- Procedural meshes with correct normals. A tiny bevel on the rendered cube is welcome, with a simple box collider.
- Linear-space lighting and correct sRGB conversion exactly once.
- Matte rough off-white architecture; saturated orange, moderately rough cube; near-black exit lining. Keep black readable at its rim without turning it grey overall.
- A broad room-fixed key light inside the room, with a real shadow map and modest PCF filtering. A shadowed spotlight around (0,5.7,0), aimed into the room with a sufficiently wide cone, is a reasonable first implementation. Add restrained fill/ambient lighting so every orientation remains readable.
- Do not rely on external sunlight whose shadows are blocked by the closed room. Lights are attached to the room, not the camera.
- Dynamic cube shadows that move with the actual rigid body. No fake dark disc attached beneath the cube.
- SSAO from actual depth and view-space normals, including room corners and cube contact. Start with a modest sample count and radius around 0.4-0.7 m; use edge-aware denoising/upsampling if rendered at half resolution.
- Reconstruct positions using the actual Vulkan projection/depth convention. Keep AO, normals, shadow matrices and camera transforms consistent while rotating. AO must not swim, produce dark screen borders or leave large halos.
- Apply AO to indirect/ambient lighting, not as a blanket multiplier on all direct light.
- Fixed exposure with restrained tone mapping, stable during rotation. Avoid auto-exposure pumping, exaggerated bloom, chromatic aberration, fog and film grain.
- A simple antialiasing solution if feasible; no need for a temporal reconstruction system.

Provide toggles for shadows and AO plus AO-only/depth/normal debug views. The AO effect must be visible when toggled, but not so strong that white surfaces become dirty. A flat ambient term is acceptable for this prototype; full global illumination and ray tracing are not required.

Handle swapchain recreation, resize, minimize, synchronization, image layout transitions and resource destruction correctly. Compile GLSL to SPIR-V through the build. Enable Vulkan validation in debug when available. Report missing Vulkan support clearly; do not silently switch APIs.

Aim for smooth 1080p presentation on a mainstream Vulkan-capable desktop GPU. Report actual measured hardware/performance if available, rather than promising an unmeasured frame rate.

9. RESET, DEBUGGING AND PRESENTATION

Backspace restores player position/orientation, cube transform and velocities, gravity, transition state, completion state, contacts as appropriate and interpolation history. It must work while falling, rotating or completed.

Debug overlay: FPS/frame time, current gravity vector, grounded state, orientation transition state, cube speed, shadow/AO toggles. Hide engineering details in normal play. Include an optional collider/axis visualization for development.

No soundtrack is required. Audio must not block delivery. If simple local procedural impact/rotation sounds are easy to add, keep them optional and understated; do not fetch copyrighted music.

10. IMPLEMENTATION AND VERIFICATION

Work in small buildable stages, but complete the requested prototype:
1) Native window, Vulkan initialization, procedural room/cube rendering.
2) Jolt room, dynamic cube, character movement and collision.
3) Two-axis orientation transitions, gravity update, clearance checks and reset.
4) Real ceiling opening, black shaft and player-only completion.
5) Lighting, real dynamic shadows, SSAO and visual polish.
6) Build/run verification and concise handoff documentation.

Meaningful checks:
- Cube falls and settles on the initial floor.
- Both pairs of rotation controls have the requested perceptual directions.
- Four same-axis quarter turns restore orientation within tolerance when tested in a clear location.
- Changing mouse pitch does not change the horizontal rotation axes; turning yaw deliberately changes their heading.
- Gravity magnitude stays constant, quaternions remain normalized and the movement basis remains orthogonal.
- The cube wakes after gravity changes and falls toward the new floor, retaining genuine rigid-body rotation.
- Player walks, jumps and grounds correctly after mixed-axis rotations.
- Invalid rotations near surfaces fail gracefully; no wall clipping or explosive impulses.
- Reset works from all states and restores a repeatable starting scene.
- The ceiling opening has no invisible collider. The player can actually enter the shaft and complete the level. Cube entry alone does not complete it.
- Resize/minimize/resume work. Repeated rotations do not leak resources or produce validation errors.
- Shadows/AO are verified visually in several orientations, not merely present as unused shader files.

Automate small math/state tests where useful. Perform an actual interactive playthrough if a GUI/Vulkan device is available. Capture screenshots of initial state, a changed orientation showing the cube on another surface, and the black exit/completion. If execution or visual verification is unavailable, state that explicitly and provide exact commands for the missing checks; do not claim they passed.

11. DELIVERABLES

Deliver source code, CMake configuration/presets, shaders, pinned dependency setup, a clear Windows build/run guide, controls and a brief architecture note explaining room coordinates, currentUp, gravity changes and the intentional transition pause. Include convenient build/run scripts if useful and dependency/license notes. Do not commit fetched dependency build trees or generated binaries unless specifically requested.

Organize code into modest modules: application/platform, Vulkan renderer, physics world, player controller, orientation controller, level zero and UI/debug. Avoid a giant single source file, but also avoid engine-framework overengineering.

At handoff, summarize what works, what was actually tested and any remaining limitations. Do not substitute a plan, mockup or video for the playable implementation.

REFERENCE DOCUMENTATION
Jolt source and integration examples: https://github.com/jrouwe/JoltPhysics
Jolt architecture: https://github.com/jrouwe/JoltPhysics/blob/master/Docs/Architecture.md
Jolt CharacterVirtual reference: https://jrouwe.github.io/JoltPhysics/class_character_virtual.html
Use documentation matching the pinned version when implementing.
