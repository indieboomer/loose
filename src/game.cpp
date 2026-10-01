#include "game.hpp"
namespace loose {
Game::Game(){resetHistory();}
void Game::resetHistory(){accumulator=0;cubePrevious=cubeCurrent=physics.cube();eyePrevious=eyeCurrent=physics.eye(orientation.up());}
void Game::reset(){physics.reset();orientation=Orientation{};state=beforePause=State::Playing;hintTime=0;jumpPending=false;resetHistory();}
bool Game::turn(Turn turn) {
    if(state!=State::Playing)return false;
    Orientation candidate=orientation; candidate.prepare(turn,physics.eye(orientation.up()));
    if(!physics.rotationClear(candidate)){hintTime=1.7f;return false;}
    orientation=candidate;orientation.rotating=true;state=State::Rotating;
    jumpPending=false;accumulator=0;return true;
}
void Game::pause() {
    if(state==State::Completed)return;
    if(state==State::Paused)state=beforePause;
    else{beforePause=state;state=State::Paused;}
    accumulator=0;jumpPending=false;
}
void Game::update(float dt,const Input& input) {
    dt=std::clamp(dt,0.f,.1f);hintTime=std::max(0.f,hintTime-dt);
    if(state==State::Paused||state==State::Completed){accumulator=0;return;}
    if(state==State::Rotating){
        accumulator=0;
        if(orientation.advance(dt)){
            physics.commitOrientation(orientation.eyeAnchor,orientation.up(),orientation.frame);
            state=State::Playing;resetHistory();
        }
        return;
    }
    jumpPending|=input.jump;
    accumulator=std::min(accumulator+dt,.1f);
    vec3 h=orientation.heading(),s=glm::normalize(glm::cross(h,orientation.up()));
    vec3 wish=h*input.move.y+s*input.move.x;
    if(glm::length(wish)>1)wish=glm::normalize(wish);
    while(accumulator>=fixedStep){
        cubePrevious=cubeCurrent;eyePrevious=eyeCurrent;
        physics.step(fixedStep,wish,jumpPending,orientation.up());jumpPending=false;
        cubeCurrent=physics.cube();eyeCurrent=physics.eye(orientation.up());
        accumulator-=fixedStep;
        if(level.inExit(physics.center())){state=State::Completed;accumulator=0;break;}
    }
}
vec3 Game::renderEye() const {
    if(state==State::Rotating||(state==State::Paused&&beforePause==State::Rotating))return orientation.eyeAnchor;
    if(state!=State::Playing)return eyeCurrent;
    return glm::mix(eyePrevious,eyeCurrent,accumulator/fixedStep);
}
Pose Game::renderCube() const {
    if(state!=State::Playing)return cubeCurrent;
    return interpolate(cubePrevious,cubeCurrent,accumulator/fixedStep);
}
}
