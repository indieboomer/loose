#pragma once
#include "physics.hpp"
namespace loose {
enum class State { Playing, Rotating, Paused, Completed };
struct Input {vec2 move{};bool jump=false;};
class Game {
public:
    Level level=Level::zero();
    Physics physics{level};
    Orientation orientation;
    State state=State::Playing, beforePause=State::Playing;
    float accumulator=0, hintTime=0;
    Pose cubePrevious{},cubeCurrent{};
    vec3 eyePrevious{},eyeCurrent{};
    bool jumpPending=false;
    Game();
    void reset();
    bool turn(Turn turn);
    void pause();
    void update(float dt,const Input& input);
    vec3 renderEye() const;
    Pose renderCube() const;
    void resetHistory();
};
}
