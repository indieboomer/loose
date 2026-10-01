#include "orientation.hpp"
namespace loose {
vec3 Orientation::heading(){return previousHeading=horizontal(forward(),up(),previousHeading);}
void Orientation::look(float yawDelta,float pitchDelta) {
    if(rotating)return;
    frame=glm::normalize(glm::angleAxis(yawDelta,up())*frame);
    pitch=std::clamp(pitch+pitchDelta,glm::radians(-85.f),glm::radians(85.f));
}
void Orientation::prepare(Turn turn,vec3 eye) {
    vec3 h=heading(), s=glm::normalize(glm::cross(h,up()));
    axis=(turn==Turn::RollLeft||turn==Turn::RollRight)?h:s;
    angle=(turn==Turn::RollLeft||turn==Turn::AheadDown)?glm::half_pi<float>():-glm::half_pi<float>();
    start=frame; target=glm::normalize(glm::angleAxis(angle,axis)*start);
    eyeAnchor=eye; elapsed=0;
}
quat Orientation::at(float t) const{return glm::normalize(glm::angleAxis(angle*t,axis)*start);}
bool Orientation::advance(float dt) {
    elapsed+=dt;
    float t=std::clamp(elapsed/std::clamp(duration,.25f,.8f),0.f,1.f);
    frame=at(t*t*(3-2*t));
    if(t>=1){frame=target; rotating=false; heading(); return true;}
    return false;
}
}
