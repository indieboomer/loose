#include "ui.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <array>
namespace loose {
namespace {
const char* stateName(State s){switch(s){case State::Playing:return "PLAYING";case State::Rotating:return "ROTATING";case State::Paused:return "PAUSED";case State::Completed:return "COMPLETE";}return "";}
void centeredText(ImDrawList* draw,ImVec2 center,const char* text,ImU32 color,float size){
    ImFont* font=ImGui::GetFont();ImVec2 bounds=font->CalcTextSizeA(size,10000,0,text);
    draw->AddText(font,size,{center.x-bounds.x*.5f,center.y},color,text);
}
void debugGeometry(const Game& game,const RenderSettings& settings){
    if(!settings.axes&&!settings.colliders)return;
    auto* d=ImGui::GetForegroundDrawList();ImVec2 size=ImGui::GetIO().DisplaySize;
    float aspect=size.x/size.y;float vfov=2*std::atan(std::tan(glm::radians(settings.horizontalFov)*.5f)/aspect);
    mat4 projection=glm::perspective(vfov,aspect,.06f,50.f);
    mat4 view=glm::mat4_cast(glm::conjugate(game.orientation.camera()))*glm::translate(mat4(1),-game.renderEye());
    auto line=[&](vec3 a,vec3 b,ImU32 color){
        vec4 aa=projection*view*vec4(a,1),bb=projection*view*vec4(b,1);
        if(aa.w<=.06f||bb.w<=.06f)return;
        vec2 pa=vec2(aa)/aa.w,pb=vec2(bb)/bb.w;
        d->AddLine({(pa.x*.5f+.5f)*size.x,(-pa.y*.5f+.5f)*size.y},{(pb.x*.5f+.5f)*size.x,(-pb.y*.5f+.5f)*size.y},color,1);
    };
    if(settings.axes){line({0,2,0},{1,2,0},IM_COL32(240,90,90,255));line({0,2,0},{0,3,0},IM_COL32(90,240,90,255));line({0,2,0},{0,2,1},IM_COL32(90,130,255,255));}
    if(settings.colliders){
        auto box=[&](vec3 center,vec3 half,quat rotation){
            std::array<vec3,8> corners;for(int i=0;i<8;i++)corners[i]=center+rotation*(half*vec3(i&1?1.f:-1.f,i&2?1.f:-1.f,i&4?1.f:-1.f));
            for(int i=0;i<8;i++)for(int bit:{1,2,4})if(!(i&bit))line(corners[i],corners[i|bit],IM_COL32(70,200,235,150));
        };
        for(auto& b:game.level.boxes)box(b.center,b.half,quat(1,0,0,0));
        auto cube=game.renderCube();box(cube.position,vec3(game.level.cubeSide*.5f),cube.rotation);
    }
}
}
void drawUI(Game& game,RenderSettings& settings,bool help,float dt,const std::string& gpu,GLFWwindow* window){
    auto& io=ImGui::GetIO();ImVec2 size=io.DisplaySize;
    auto* draw=ImGui::GetForegroundDrawList();ImU32 white=IM_COL32(237,238,230,230),orange=IM_COL32(255,130,38,255);
    bool overlay=game.state==State::Paused||game.state==State::Completed;
    if(!overlay){
        float cx=size.x*.5f,cy=size.y*.5f;
        draw->AddCircleFilled({cx,cy},3,IM_COL32(15,17,20,160));draw->AddCircleFilled({cx,cy},1.2f,white);
        draw->AddText({24,21},white,"LOOSE / LEVEL ZERO");
        draw->AddText({24,size.y-30},white,"Q / E  ROLL     F1  HELP     ESC  PAUSE");
        if(game.hintTime>0)centeredText(draw,{size.x*.5f,size.y*.68f},"MOVE AWAY FROM THE SURFACE",orange,18);
    }
    if(help||game.state==State::Paused){
        ImGui::SetNextWindowPos({20,55},ImGuiCond_FirstUseEver);ImGui::SetNextWindowBgAlpha(.9f);
        ImGui::Begin("Controls & settings",nullptr,ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted("WASD move   Mouse look   SPACE jump");
        ImGui::TextUnformatted("Q world counterclockwise   E clockwise");
        ImGui::TextUnformatted("BACKSPACE reset   ESC pause / resume   F1 help");
        ImGui::Separator();
        ImGui::Checkbox("Shadows",&settings.shadows);ImGui::SameLine();ImGui::Checkbox("Ambient occlusion",&settings.ao);
        const char* modes[]={"Lit","AO only","Depth","View normals"};ImGui::Combo("View",&settings.debugView,modes,4);
        ImGui::SliderFloat("Horizontal FOV",&settings.horizontalFov,65,110,"%.0f degrees");
        ImGui::SliderFloat("Sensitivity",&settings.sensitivity,.0005f,.006f,"%.4f");
        ImGui::SliderFloat("Turn duration",&game.orientation.duration,.25f,.8f,"%.2f s");
        ImGui::Checkbox("Room axes",&settings.axes);ImGui::SameLine();ImGui::Checkbox("Collider boxes",&settings.colliders);
        ImGui::Separator();
        vec3 gravity=-gravityStrength*game.orientation.up();
        ImGui::Text("%.1f FPS | %.2f ms",io.Framerate,dt*1000);
        ImGui::Text("Gravity (%.2f, %.2f, %.2f)",gravity.x,gravity.y,gravity.z);
        ImGui::Text("%s | Grounded: %s | Cube %.2f m/s",stateName(game.state),game.physics.grounded()?"yes":"no",glm::length(game.physics.cubeVelocity()));
        ImGui::TextWrapped("%s",gpu.c_str());
        ImGui::End();
    }
    if(overlay){
        draw->AddRectFilled({0,0},size,IM_COL32(0,0,0,105));
        ImGui::SetNextWindowPos({size.x*.5f,size.y*.45f},ImGuiCond_Always,{.5f,.5f});
        ImGui::Begin("LOOSE",nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove);
        ImGui::TextUnformatted(game.state==State::Completed?"LEVEL ZERO COMPLETE":"PAUSED");
        if(game.state==State::Completed)ImGui::TextUnformatted("BACKSPACE TO RESTART");
        if(game.state==State::Paused&&ImGui::Button("Resume"))game.pause();
        if(ImGui::Button("Restart"))game.reset();ImGui::SameLine();
        if(ImGui::Button("Quit"))glfwSetWindowShouldClose(window,GLFW_TRUE);
        ImGui::End();
    }
    debugGeometry(game,settings);
}
}
