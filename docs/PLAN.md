# LOOSE implementation plan

1. Pin GLFW 3.4, GLM 1.0.1, Jolt 5.3.0, Vulkan headers 1.3.296, glslang 15.0.0 and ImGui 1.91.5; build native C++20 with CMake. Load Vulkan entry points from the platform loader so an SDK is not required.
2. Isolate room definitions, orientation mathematics, Jolt integration and game state. Implement capsule movement and the real ceiling shaft. Test these without a display.
3. Freeze simulation during eased eye-anchored quarter turns. Check the swept capsule with conservative Jolt shape overlaps. Commit the inverse level rotation with fixed physics gravity, preserving inertial cube velocities.
4. Render a procedural G-buffer, room-fixed shadow map, fullscreen lighting and depth/normal SSAO, then a small ImGui overlay. Handle resize and minimized windows.
5. Build, run mechanic tests, run Vulkan smoke tests and capture the room and changed orientations where desktop access permits. Record measured results and limitations.

No engine, downloaded art or runtime network requests. Simulation transforms stay in room coordinates throughout.

Implementation stages are complete: both native build configurations pass the core tests; the release game runs a real GLFW-input playthrough and a scripted Vulkan capture route. Lighting, AO, shadows and the physical exit have been inspected. The installed machine lacks the Vulkan validation layer, so validation-layer verification remains an explicitly documented environment gap. See `VERIFICATION.md` for actual evidence and `ARCHITECTURE.md` for implementation decisions.
