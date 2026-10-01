#include "game.hpp"
#include <iostream>
#include <stdexcept>
using namespace loose;
namespace {
int checks=0;
void check(bool condition,const char* message){checks++;if(!condition)throw std::runtime_error(message);}
void near(vec3 a,vec3 b,float tolerance,const char* message){check(glm::length(a-b)<tolerance,message);}
void run(Game& g,int steps,Input input={}){for(int i=0;i<steps;i++)g.update(fixedStep,input);}
void clearPosition(Game& g){g.physics.placePlayer({0,3,0},g.orientation.frame,g.orientation.up());g.resetHistory();}
void finishTurn(Game& g){for(int i=0;i<120&&g.state==State::Rotating;i++)run(g,1);check(g.state==State::Playing,"transition commits");}
void mathTests(){
    for(auto turn:{Turn::RollLeft,Turn::RollRight,Turn::AheadUp,Turn::AheadDown}){
        Orientation o;o.look(.371f,.8f);quat original=o.frame;
        for(int i=0;i<4;i++){o.prepare(turn,{});o.rotating=true;o.advance(1);}
        check(std::abs(glm::dot(original,o.frame))>1-1e-5f,"four quarter turns restore orientation");
        check(std::abs(glm::length(o.frame)-1)<1e-6f,"normalized quaternion");
    }
    Orientation a,b;a.look(.53f,.2f);b.look(.53f,-1.f);
    for(auto turn:{Turn::RollLeft,Turn::AheadUp}){a.prepare(turn,{});b.prepare(turn,{});near(a.axis,b.axis,1e-5f,"mouse pitch cannot tilt axis");check(std::abs(glm::dot(a.axis,a.up()))<1e-6f,"rotation axis horizontal");}
    Orientation yaw;yaw.look(.8f,0);yaw.prepare(Turn::RollLeft,{});check(glm::length(yaw.axis-vec3(0,0,-1))>.5f,"arbitrary yaw changes axis");
    Orientation q;q.pitch=0;q.prepare(Turn::RollLeft,{});quat cam=q.at(1);vec3 screen=glm::conjugate(cam)*vec3(0,1,-2);check(screen.x<0,"Q moves world up to screen left (CCW)");
    Orientation e;e.pitch=0;e.prepare(Turn::RollRight,{});screen=glm::conjugate(e.at(1))*vec3(0,1,-2);check(screen.x>0,"E moves world up to screen right (CW)");
    Orientation r;r.pitch=0;r.prepare(Turn::AheadUp,{});screen=glm::conjugate(r.at(.5f))*vec3(0,0,-2);check(screen.y>0,"R brings scene ahead upward");
    Orientation f;f.pitch=0;f.prepare(Turn::AheadDown,{});screen=glm::conjugate(f.at(.5f))*vec3(0,0,-2);check(screen.y<0,"F brings scene ahead downward");
    near(horizontal({0,1,0},{0,1,0},{0,0,-1}),{0,0,-1},1e-6f,"degenerate projection preserves heading");
    for(int i=0;i<100;i++){q.prepare(Turn(i%4),{});q.rotating=true;q.advance(1);check(std::abs(glm::length(-gravityStrength*q.up())-gravityStrength)<1e-4f,"constant gravity magnitude");vec3 h=q.heading(),s=glm::cross(h,q.up());check(std::abs(glm::dot(h,q.up()))<1e-5f&&std::abs(glm::length(s)-1)<1e-5f,"orthonormal movement basis");}
}
void physicsTests(){
    Game g;run(g,480);
    near(g.physics.cube().position,{2,.6f,0},.06f,"cube falls and settles on initial floor");
    check(g.physics.grounded(),"initial player grounded");
    check(!g.physics.cubeAwake(),"settled cube sleeps");
    float before=g.physics.center().y;run(g,1,{{},true});run(g,20);check(g.physics.center().y>before+.3f,"jump accelerates character");run(g,150);check(g.physics.grounded(),"land after jump");
    vec3 beforeDiagonal=g.physics.center();run(g,120,{{1,1},false});
    vec3 diagonal=g.physics.center()-beforeDiagonal;diagonal.y=0;
    check(std::abs(glm::length(diagonal)-g.level.player.speed)<.06f,"diagonal movement has normalized speed");
    g.reset();run(g,480);clearPosition(g);check(!g.physics.cubeAwake(),"cube sleeping before gravity change");
    check(g.turn(Turn::RollLeft),"sleeping cube turn accepted");finishTurn(g);
    check(g.physics.cubeAwake(),"orientation commit explicitly wakes sleeping cube");
    run(g,120);check(g.physics.cube().position.x<g.level.cubeSpawn.x-1,"woken cube falls toward new floor");
    for(Turn turn:{Turn::RollLeft,Turn::RollRight,Turn::AheadUp,Turn::AheadDown}){
        g.reset();clearPosition(g);vec3 anchor=g.physics.eye(g.orientation.up());Pose cubeBefore=g.physics.cube();
        check(g.turn(turn),"all axes clear at room center");run(g,20);
        near(g.renderEye(),anchor,1e-6f,"transition eye anchored");near(g.physics.cube().position,cubeBefore.position,1e-6f,"cube frozen during rotation");
        check(!g.turn(turn),"input ignored during transition");finishTurn(g);
        near(g.physics.eye(g.orientation.up()),anchor,1e-5f,"commit preserves eye/body offset");
        check(!g.physics.grounded(),"commit clears stale grounded state");
        run(g,600);check(g.physics.grounded(),"character grounds on rotated floor");
        vec3 cp=g.physics.cube().position;vec3 up=g.orientation.up();
        check(glm::dot(cp-g.level.cubeSpawn,-up)>.5f,"cube wakes and falls with new gravity");
        run(g,1,{{},true});run(g,15);check(glm::dot(g.physics.velocity(),up)>0,"jump uses new up");
    }
    g.reset();clearPosition(g);g.physics.placeCube({{2,2,0},glm::angleAxis(.3f,glm::normalize(vec3(1,0,1)))},{1,2,3},{.2f,.3f,.4f});
    vec3 v=g.physics.cubeVelocity(),av=g.physics.cubeAngularVelocity();check(g.turn(Turn::RollLeft),"velocity preservation turn accepted");finishTurn(g);
    quat turnRotation=glm::angleAxis(g.orientation.angle,g.orientation.axis);
    near(g.physics.cubeVelocity(),turnRotation*v,1e-5f,"cube preserves inertial linear velocity as room rotates");
    near(g.physics.cubeAngularVelocity(),turnRotation*av,1e-5f,"cube preserves inertial angular velocity as room rotates");
    clearPosition(g);check(g.turn(Turn::RollRight),"inverse turn accepted");finishTurn(g);
    near(g.physics.cubeVelocity(),v,1e-5f,"inverse turn restores room velocity without braking");
    near(g.physics.cubeAngularVelocity(),av,1e-5f,"inverse turn restores angular velocity");
    float maxSpin=0;for(int i=0;i<600;i++){run(g,1);maxSpin=std::max(maxSpin,glm::length(g.physics.cubeAngularVelocity()));}
    check(maxSpin>.5f,"cube has genuine angular motion during collisions");
    g.reset();g.physics.placePlayer({5.55f,3,0},quat(1,0,0,0),{0,1,0});g.resetHistory();
    check(!g.turn(Turn::RollRight),"near-wall capsule sweep rejected");check(g.hintTime>0,"clearance rejection hint");
    g.reset();run(g,240);check(g.turn(Turn::RollLeft),"support touching initial floor does not reject roll");g.reset();check(g.state==State::Playing&&!g.orientation.rotating,"reset during rotation");
    g.pause();run(g,2000);g.reset();check(g.state==State::Playing,"reset paused");near(g.physics.cubeVelocity(),{},1e-6f,"reset cube velocity");
    clearPosition(g);g.pause();vec3 c=g.physics.center();run(g,5000);g.pause();g.update(fixedStep,{});near(g.physics.center(),c,.002f,"pause discards elapsed time");
    g.reset();g.physics.placeCube({{0,6.5f,-2},quat(1,0,0,0)});run(g,1);check(g.state!=State::Completed,"cube alone cannot trigger exit");
    // Actual playable route: move to center, roll onto left wall, roll onto ceiling, enter shaft.
    g.reset();run(g,95,{{0,1},false});check(g.turn(Turn::RollLeft),"route first roll");run(g,500);check(g.physics.grounded(),"route wall landing");check(g.turn(Turn::RollLeft),"route second roll");run(g,500);check(g.physics.grounded(),"route ceiling landing");
    for(int i=0;i<1800&&g.state!=State::Completed;i++){
        vec3 delta=vec3(0,g.physics.center().y,-2)-g.physics.center();vec3 wish=glm::length(delta)>.05f?glm::normalize(delta):vec3(0);
        vec3 h=g.orientation.heading(),s=glm::cross(h,g.orientation.up());run(g,1,{{glm::dot(wish,s),glm::dot(wish,h)},false});
    }
    check(g.state==State::Completed,"player physically enters real ceiling opening and completes");
    g.reset();check(g.state==State::Playing,"reset completed");near(g.physics.eye(g.orientation.up()),g.level.spawnFeet+vec3(0,g.level.player.eyeHeight,0),1e-5f,"repeatable spawn eye");
    // Stand on and gently push the real dynamic cube.
    g.physics.placePlayer({2,2.8f,0},quat(1,0,0,0),{0,1,0});g.resetHistory();run(g,360);
    check(g.physics.grounded()&&g.physics.center().y>2,"character stands on cube");
    g.reset();g.physics.placePlayer({2,.94f,2},quat(1,0,0,0),{0,1,0});g.resetHistory();run(g,90,{{0,1},false});
    check(g.physics.cube().position.z<-.02f,"player pushes cube");check(glm::length(g.physics.cubeVelocity())<8,"cube pushing bounded");
    // Mixed axes with unsnapped yaw exercise a genuinely non-cardinal gravity and capsule.
    g.reset();g.orientation.look(.43f,.37f);
    for(Turn turn:{Turn::RollLeft,Turn::AheadUp,Turn::RollRight,Turn::AheadDown}){
        clearPosition(g);check(g.turn(turn),"mixed-axis rotation clear at center");finishTurn(g);run(g,900);
        check(g.physics.grounded(),"non-cardinal gravity supports player after mixed axes");
        vec3 oldEye=g.physics.eye(g.orientation.up());run(g,1,{{},true});run(g,18);
        check(glm::dot(g.physics.eye(g.orientation.up())-oldEye,g.orientation.up())>.2f,"jump works after mixed axes");
        run(g,240);vec3 beforeMove=g.physics.center();run(g,35,{{.5f,.7f},false});
        check(glm::length(g.physics.center()-beforeMove)>.05f,"walk works after mixed axes");
        vec3 cube=g.physics.cube().position;
        check(std::abs(cube.x)<6.3f&&cube.y>-.1f&&cube.y<7.8f&&std::abs(cube.z)<5.3f,"thick walls contain cube during repeated falls");
    }
}
}
int main(){try{mathTests();physicsTests();std::cout<<checks<<" mechanic checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<"Check "<<checks<<" failed: "<<e.what()<<'\n';return 1;}}
