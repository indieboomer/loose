#include "renderer.hpp"
#include "ui.hpp"
#include "verification.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <iostream>
#include <chrono>
#include <array>
#include <fstream>
#include <filesystem>
#include <thread>
#include <stdexcept>
namespace loose {
namespace {
struct Platform {
    std::array<bool,GLFW_KEY_LAST+1> pressed{};
    double lastX=0,lastY=0;
    bool mouseValid=false,captured=false;
    float dx=0,dy=0;
    int keyDowns=0;
    static void key(GLFWwindow* window,int key,int,int action,int){
        auto* p=static_cast<Platform*>(glfwGetWindowUserPointer(window));
        if(key>=0&&key<=GLFW_KEY_LAST&&action==GLFW_PRESS){p->pressed[size_t(key)]=true;p->keyDowns++;}
    }
    static void mouse(GLFWwindow* window,double x,double y){
        auto* p=static_cast<Platform*>(glfwGetWindowUserPointer(window));
        if(p->captured&&p->mouseValid){p->dx+=float(x-p->lastX);p->dy+=float(y-p->lastY);}
        p->lastX=x;p->lastY=y;p->mouseValid=true;
    }
    void capture(GLFWwindow* window,bool active){
        if(captured==active)return;
        captured=active;mouseValid=false;dx=dy=0;
        glfwSetInputMode(window,GLFW_CURSOR,active?GLFW_CURSOR_DISABLED:GLFW_CURSOR_NORMAL);
        if(glfwRawMouseMotionSupported())glfwSetInputMode(window,GLFW_RAW_MOUSE_MOTION,active?GLFW_TRUE:GLFW_FALSE);
    }
};
const char* stateName(State s){switch(s){case State::Playing:return "PLAYING";case State::Rotating:return "ROTATING";case State::Paused:return "PAUSED";case State::Completed:return "COMPLETE";}return "";}
}
}
int main(int argc,char** argv){
    using namespace loose;
    bool verify=false,validation=false,noAO=false,telemetry=false;int smokeFrames=0,widthSetting=1280,heightSetting=720;
#ifndef NDEBUG
    validation=true;
#endif
    for(int i=1;i<argc;i++){
        std::string arg=argv[i];if(arg=="--verify")verify=true;else if(arg=="--validation")validation=true;
        else if(arg=="--smoke"&&i+1<argc)smokeFrames=std::stoi(argv[++i]);
        else if(arg=="--size"&&i+2<argc){widthSetting=std::stoi(argv[++i]);heightSetting=std::stoi(argv[++i]);}
        else if(arg=="--no-ao")noAO=true;
        else if(arg=="--telemetry")telemetry=true;
        else if(arg=="--help"){std::cout<<"loose [--validation] [--verify | --smoke frames] [--size width height] [--no-ao]\n";return 0;}
        else{std::cerr<<"Unknown option: "<<arg<<'\n';return 1;}
    }
    glfwSetErrorCallback([](int code,const char* text){std::cerr<<"GLFW "<<code<<": "<<text<<'\n';});
    if(!glfwInit())return 1;
    glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);
    GLFWwindow* window=glfwCreateWindow(std::clamp(widthSetting,640,3840),std::clamp(heightSetting,480,2160),"LOOSE / LEVEL ZERO",nullptr,nullptr);
    if(!window){glfwTerminate();return 1;}
    int result=0;
    try{
        Platform platform;glfwSetWindowUserPointer(window,&platform);glfwSetKeyCallback(window,Platform::key);glfwSetCursorPosCallback(window,Platform::mouse);
        Game game;Renderer renderer(window,game.level,validation);RenderSettings settings;VerifyRoute route;
        settings.ao=!noAO;
        bool help=false;int frame=0;double previous=glfwGetTime();double frameSum=0;int measured=0;
        platform.capture(window,!verify&&!smokeFrames);
        while(!glfwWindowShouldClose(window)){
            glfwPollEvents();double now=glfwGetTime();float dt=float(now-previous);previous=now;
            bool focused=glfwGetWindowAttrib(window,GLFW_FOCUSED)!=0;
            if(!focused&&!verify&&!smokeFrames&&game.state!=State::Paused&&game.state!=State::Completed)game.pause();
            if(platform.pressed[GLFW_KEY_BACKSPACE])game.reset();
            if(platform.pressed[GLFW_KEY_ESCAPE])game.pause();
            if(platform.pressed[GLFW_KEY_F1])help=!help;
            if(game.state==State::Playing){
                game.orientation.look(-platform.dx*settings.sensitivity,-platform.dy*settings.sensitivity);
                // Stable priority for simultaneous key-downs. Repeats never enter pressed[].
                if(platform.pressed[GLFW_KEY_Q])game.turn(Turn::RollLeft);
                else if(platform.pressed[GLFW_KEY_E])game.turn(Turn::RollRight);
            }
            platform.dx=platform.dy=0;
            Input input;
            auto down=[&](int key){return glfwGetKey(window,key)==GLFW_PRESS;};
            input.move={float(down(GLFW_KEY_D))-float(down(GLFW_KEY_A)),float(down(GLFW_KEY_W))-float(down(GLFW_KEY_S))};input.jump=platform.pressed[GLFW_KEY_SPACE];
            std::string capture;
            if(platform.pressed[GLFW_KEY_F12])capture="captures/interactive-"+std::to_string(frame)+".ppm";
            if(verify){dt=1.f/60;input=route.tick(game,settings,capture,window);}
            if(smokeFrames)dt=1.f/60;
            int width,height;glfwGetFramebufferSize(window,&width,&height);
            if(width&&height)game.update(dt,input);
            platform.pressed.fill(false);
            renderer.beginUI();drawUI(game,settings,help,dt,renderer.hardware(),window);ImGui::Render();
            bool drawn=renderer.draw(game,settings,capture);
            platform.capture(window,focused&&game.state!=State::Paused&&game.state!=State::Completed&&!verify&&!smokeFrames);
            if(telemetry&&frame%6==0){
                std::filesystem::create_directories("captures");vec3 center=game.physics.center(),up=game.orientation.up();
                std::ofstream trace("captures/input-state.json");
                trace<<"{\"state\":\""<<stateName(game.state)<<"\",\"grounded\":"<<(game.physics.grounded()?"true":"false")<<",\"captured\":"<<(platform.captured?"true":"false")<<",\"keys\":"<<platform.keyDowns<<",\"center\":["<<center.x<<','<<center.y<<','<<center.z<<"],\"up\":["<<up.x<<','<<up.y<<','<<up.z<<"]}";
            }
            if(drawn&&frame>30){frameSum+=glfwGetTime()-now;measured++;}
            frame++;
            if(!width||!height)glfwWaitEventsTimeout(.01);
            if((verify&&route.done)||(smokeFrames&&frame>=smokeFrames))break;
        }
        std::cout<<"Frames: "<<frame<<" | average CPU frame including presentation: "<<(measured?frameSum*1000/measured:0)<<" ms | Validation active: "<<renderer.validationActive()<<" | Validation errors: "<<renderer.validationErrors()<<'\n';
        if(verify){std::filesystem::create_directories("captures");std::ofstream report("captures/run.txt");report<<"GPU: "<<renderer.hardware()<<"\nFrames: "<<frame<<"\nRoute completed: "<<route.done<<"\nMean CPU frame including FIFO: "<<(measured?frameSum*1000/measured:0)<<" ms\nValidation active: "<<renderer.validationActive()<<"\nValidation errors: "<<renderer.validationErrors()<<'\n';if(!route.done)result=1;}
        if(renderer.validationErrors())result=1;
    }catch(const std::exception& e){std::cerr<<"LOOSE: "<<e.what()<<'\n';result=1;}
    glfwDestroyWindow(window);glfwTerminate();return result;
}
