#include "level.hpp"
namespace loose {
Level Level::zero() {
    Level l;
    const vec3 white{.78f,.77f,.72f}, black{.008f,.009f,.012f};
    auto box=[&](vec3 c,vec3 h,vec3 color){l.boxes.push_back({c,h,color});};
    box({0,-.2f,0},{6.4f,.2f,5.4f},white);
    box({-6.2f,3,0},{.2f,3,5.4f},{.72f,.73f,.71f});
    box({6.2f,3,0},{.2f,3,5.4f},{.81f,.79f,.74f});
    box({0,3,-5.2f},{6,.3f+2.7f,.2f},white);
    box({0,3,5.2f},{6,3,.2f},{.74f,.75f,.73f});
    // Four slabs surround an actual x=[-1,1], z=[-3,-1] ceiling aperture.
    box({-3.5f,6.2f,0},{2.5f,.2f,5},white);
    box({3.5f,6.2f,0},{2.5f,.2f,5},white);
    box({0,6.2f,-4},{1,.2f,1},white);
    box({0,6.2f,2},{1,.2f,3},white);
    // Black shaft begins at y=6 and terminates at y=7.5.
    // Lining projects 1cm inward and 5mm below the ceiling: no coplanar slab/rim faces.
    box({-1.11f,6.7475f,-2},{.12f,.7525f,1.23f},black);
    box({1.11f,6.7475f,-2},{.12f,.7525f,1.23f},black);
    box({0,6.7475f,-3.11f},{.99f,.7525f,.12f},black);
    box({0,6.7475f,-.89f},{.99f,.7525f,.12f},black);
    box({0,7.65f,-2},{1.24f,.15f,1.24f},black);
    return l;
}
bool Level::inExit(vec3 center) const {
    // Player center must pass the ceiling; eye alone cannot win through a wall.
    return center.y>6.10f && center.y<7.6f && std::abs(center.x)<.78f && std::abs(center.z+2)<.78f;
}
}
