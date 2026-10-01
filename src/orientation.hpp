#pragma once
#include "math.hpp"
namespace loose {
enum class Turn { RollLeft, RollRight, AheadUp, AheadDown };
struct Orientation {
    quat frame{1,0,0,0}; // Local +Y is player up; -Z is horizontal heading.
    float pitch=.21f;
    float duration=.45f;
    bool rotating=false;
    float elapsed=0;
    vec3 eyeAnchor{}, axis{}, previousHeading{0,0,-1};
    quat start{1,0,0,0}, target{1,0,0,0};
    float angle=0;
    vec3 up() const {return frame*vec3(0,1,0);}
    quat camera() const {return glm::normalize(frame*glm::angleAxis(pitch,vec3(1,0,0)));}
    vec3 forward() const {return camera()*vec3(0,0,-1);}
    vec3 heading();
    void look(float yawDelta,float pitchDelta);
    void prepare(Turn turn,vec3 eye);
    quat at(float t) const;
    bool advance(float dt);
};
}
