#pragma once
#include "math.hpp"
#include <vector>
namespace loose {
struct Box { vec3 center, half, color; };
struct PlayerConfig { float radius=.30f, height=1.8f, eyeHeight=1.65f, speed=3.8f, jump=4.5f; };
struct Level {
    std::vector<Box> boxes;
    PlayerConfig player;
    vec3 spawnFeet{0,.05f,3.8f};
    vec3 cubeSpawn{2,.65f,0};
    float cubeSide=1.2f;
    vec3 cubeColor{1.f,.19f,.018f};
    vec3 light{0,5.7f,0};
    static Level zero();
    bool inExit(vec3 center) const;
};
}
