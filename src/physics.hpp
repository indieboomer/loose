#pragma once
#include "level.hpp"
#include "orientation.hpp"
#include <memory>
namespace loose {
class Physics {
public:
    explicit Physics(const Level& level);
    ~Physics();
    Physics(const Physics&)=delete;
    void reset();
    void step(float dt,vec3 wish,bool jump,vec3 up);
    void commitOrientation(vec3 eye,vec3 up,quat frame);
    bool rotationClear(const Orientation& proposed) const;
    vec3 center() const;
    vec3 eye(vec3 up) const;
    vec3 velocity() const;
    Pose cube() const;
    vec3 cubeVelocity() const;
    vec3 cubeAngularVelocity() const;
    bool cubeAwake() const;
    bool grounded() const;
    void placePlayer(vec3 center,quat rotation,vec3 up); // Deterministic test harness.
    void placeCube(Pose pose,vec3 velocity={},vec3 angularVelocity={});
private:
    struct Impl;
    std::unique_ptr<Impl> p;
};
}
