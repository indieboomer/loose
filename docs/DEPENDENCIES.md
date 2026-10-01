# Pinned dependencies and license notes

All dependencies are fetched only at configure/build time into ignored `build/_deps`. `CMakeLists.txt` pins explicit release tags. These are the resolved commits used for verification:

| Dependency | Tag | Resolved commit | License |
| --- | --- | --- | --- |
| [GLFW](https://github.com/glfw/glfw) | 3.4 | `7b6aead9fb88b3623e3b3725ebb42670cbe4c579` | zlib |
| [GLM](https://github.com/g-truc/glm) | 1.0.1 | `0af55ccecd98d4e5a8d1fad7de25ba429d60e863` | MIT option of the dual license |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | v5.3.0 | `0373ec0dd762e4bc2f6acdb08371ee84fa23c6db` | MIT |
| [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) | v1.3.296 | `29f979ee5aa58b7b005f805ea8df7a855c39ff37` | Apache-2.0 OR MIT; see per-file notices |
| [glslang](https://github.com/KhronosGroup/glslang) | 15.0.0 | `46ef757e048e760b46601e6e77ae0cb72c97bd2f` | BSD-3-Clause core; MIT/Apache-2.0 portions, see full license |
| [Dear ImGui](https://github.com/ocornut/imgui) | v1.91.5 | `f401021d5a5d56fe2304056c391e78f81c8d4b8f` | MIT |

Upstream license files are preserved in `docs/licenses`. Keep their notices with any redistribution. glslang is a build tool; it is not loaded by the running game. GLFW, GLM, Jolt and ImGui are compiled into the native executable. The Vulkan loader is supplied by the installed GPU driver and is not redistributed here. ImGui is used only for UI, not window/rendering infrastructure.

APIs were checked against the fetched Jolt 5.3.0 headers and implementation (`CharacterVirtual.h/.cpp`, `CharacterBase.h`, `NarrowPhaseQuery.h`, `BodyInterface.h`) and ImGui 1.91.5 Vulkan/GLFW backend headers. Jolt's [versioned CharacterVirtual reference](https://jrouwe.github.io/JoltPhysicsDocs/5.3.0/class_character_virtual.html) is useful alongside source. GLFW's [Vulkan documentation](https://www.glfw.org/docs/3.4/vulkan_guide.html) describes native surface and mouse handling. CMake compiles the game's GLSL to Vulkan 1.1 SPIR-V through the pinned glslang executable.

Jolt uses its cross-platform deterministic math option and statically linked MSVC runtime. This avoids reliance on the newest MSVC runtime DLLs. The verified build uses Jolt's default AVX2-capable x64 configuration and requires a Vulkan 1.1 device; no SDK or account is required to play. Older CPUs can configure all Jolt `USE_SSE4_1`, `USE_SSE4_2`, `USE_AVX`, `USE_AVX2`, `USE_LZCNT`, `USE_TZCNT`, `USE_F16C` and `USE_FMADD` options OFF for its SSE2 baseline; that alternate configuration was not tested.
