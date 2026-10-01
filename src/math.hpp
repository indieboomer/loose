#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
namespace loose {
using glm::vec2; using glm::vec3; using glm::vec4; using glm::quat; using glm::mat4;
inline constexpr float gravityStrength = 9.81f;
inline constexpr float fixedStep = 1.0f / 120.0f;
inline vec3 horizontal(vec3 forward, vec3 up, vec3 fallback) {
    vec3 h = forward - up * glm::dot(forward, up);
    return glm::dot(h,h) > 1e-8f ? glm::normalize(h) : fallback;
}
struct Pose { vec3 position{}; quat rotation{1,0,0,0}; };
inline Pose interpolate(const Pose& a, const Pose& b, float t) {
    return {glm::mix(a.position,b.position,t), glm::slerp(a.rotation,b.rotation,t)};
}
}
