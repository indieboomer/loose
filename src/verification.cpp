#include "verification.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>
namespace loose {
namespace {
void inspectCube(Game& game){
    vec3 delta=game.physics.cube().position-game.renderEye();
    vec3 up=game.orientation.up(),heading=game.orientation.heading(),right=glm::cross(heading,up);
    float yaw=-std::atan2(glm::dot(delta,right),glm::dot(delta,heading));
    float pitch=std::atan2(glm::dot(delta,up),glm::length(delta-up*glm::dot(delta,up)));
    game.orientation.look(yaw,pitch-game.orientation.pitch);
}
}
// This deterministic developer route uses the same inputs/state/physics as play.
// It does not teleport the player or cube to complete the room.
Input VerifyRoute::tick(Game& game,RenderSettings& settings,std::string& screenshot,GLFWwindow* window){
        Input input;phaseFrames++;
        switch(phase){
        case 0:
            if(phaseFrames==60)screenshot="captures/initial.ppm";
            if(phaseFrames==61){settings.ao=false;screenshot="captures/initial-no-ao.ppm";}
            if(phaseFrames==62){settings.ao=true;settings.shadows=false;screenshot="captures/initial-no-shadows.ppm";}
            if(phaseFrames==63){settings.shadows=true;settings.debugView=1;screenshot="captures/initial-ao.ppm";}
            if(phaseFrames==64){settings.debugView=2;screenshot="captures/initial-depth.ppm";}
            if(phaseFrames==65){settings.debugView=3;screenshot="captures/initial-normals.ppm";}
            if(phaseFrames==66){settings.debugView=0;game.orientation.look(-.24f,-.40f);screenshot="captures/cube-contact.ppm";}
            if(phaseFrames==67){settings.shadows=false;screenshot="captures/cube-contact-no-shadows.ppm";}
            if(phaseFrames==68){settings.shadows=true;settings.ao=false;screenshot="captures/cube-contact-no-ao.ppm";}
            if(phaseFrames==69){settings.ao=true;settings.debugView=1;screenshot="captures/cube-contact-ao.ppm";}
            if(phaseFrames==70){settings.debugView=0;game.orientation.look(.24f,.40f);phase++;phaseFrames=0;}
            break;
        case 1:
            input.move.y=1;
            if(game.physics.center().z<2.5f){input.move={};if(!game.turn(Turn::RollLeft))throw std::runtime_error("Verify route: first roll rejected");phase++;phaseFrames=0;}
            break;
        case 2:
            if(phaseFrames==12)screenshot="captures/roll-transition.ppm";
            if(game.state==State::Playing&&game.physics.grounded()&&phaseFrames>120){
                game.orientation.look(-.12f,-.40f);screenshot="captures/changed-orientation.ppm";phase++;phaseFrames=0;
            }break;
        case 3:
            if(phaseFrames==1){settings.debugView=1;screenshot="captures/changed-ao.ppm";}
            if(phaseFrames==2){settings.debugView=0;settings.ao=false;screenshot="captures/changed-no-ao.ppm";}
            if(phaseFrames==3){settings.ao=true;settings.shadows=false;screenshot="captures/changed-no-shadows.ppm";}
            if(phaseFrames==4){settings.shadows=true;game.orientation.look(.12f,.40f);if(!game.turn(Turn::RollLeft))throw std::runtime_error("Verify route: second roll rejected");phase++;phaseFrames=0;}
            break;
        case 4:
            if(game.state==State::Playing&&game.physics.grounded()&&phaseFrames>130){screenshot="captures/ceiling.ppm";phase++;phaseFrames=0;game.orientation.look(0,-.71f);}break;
        case 5:{
            vec3 delta=vec3(0,game.physics.center().y,-2)-game.physics.center();
            vec3 h=game.orientation.heading(),s=glm::normalize(glm::cross(h,game.orientation.up()));
            if(glm::length(delta)>.10f){vec3 wish=glm::normalize(delta);input.move={glm::dot(wish,s),glm::dot(wish,h)};}
            if(game.state==State::Completed){game.orientation.pitch=-1.15f;screenshot="captures/completed.ppm";phase++;phaseFrames=0;}
            break;}
        case 6:
            if(phaseFrames==2){game.reset();game.pause();glfwSetWindowSize(window,960,640);}
            if(phaseFrames==5){game.pause();glfwIconifyWindow(window);}
            if(phaseFrames==8)glfwRestoreWindow(window);
            if(phaseFrames==15){glfwSetWindowSize(window,1280,720);game.reset();}
            if(phaseFrames==25){screenshot="captures/reset.ppm";phase++;phaseFrames=0;}
            break;
        case 7:
            input.move.y=1;
            if(game.physics.center().z<1){input.move={};if(!game.turn(Turn::AheadUp))throw std::runtime_error("Verify route: R rejected");phase++;phaseFrames=0;}
            break;
        case 8:
            if(phaseFrames==12)screenshot="captures/r-transition.ppm";
            if(game.state==State::Playing&&game.physics.grounded()&&phaseFrames>130){inspectCube(game);screenshot="captures/r-landing.ppm";phase++;phaseFrames=0;}
            break;
        case 9:
            if(phaseFrames==1){settings.debugView=1;screenshot="captures/r-ao.ppm";}
            if(phaseFrames==2){settings.debugView=0;settings.shadows=false;screenshot="captures/r-no-shadows.ppm";}
            if(phaseFrames==3){settings.shadows=true;game.reset();phase++;phaseFrames=0;}
            break;
        case 10:
            input.move.y=1;
            if(game.physics.center().z<1){input.move={};if(!game.turn(Turn::AheadDown))throw std::runtime_error("Verify route: F rejected");phase++;phaseFrames=0;}
            break;
        case 11:
            if(phaseFrames==12)screenshot="captures/f-transition.ppm";
            if(game.state==State::Playing&&game.physics.grounded()&&phaseFrames>160){inspectCube(game);screenshot="captures/f-landing.ppm";phase++;phaseFrames=0;}
            break;
        case 12:
            if(phaseFrames==1){settings.debugView=1;screenshot="captures/f-ao.ppm";}
            if(phaseFrames==2){settings.debugView=0;settings.shadows=false;screenshot="captures/f-no-shadows.ppm";}
            if(phaseFrames==3){settings.shadows=true;game.reset();done=true;}
            break;
        }
        if(phaseFrames>1200)throw std::runtime_error("Verify route timed out in phase "+std::to_string(phase));
        return input;
    }
}
