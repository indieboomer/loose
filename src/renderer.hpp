#pragma once
#include "game.hpp"
#include <memory>
#include <string>
struct GLFWwindow;
namespace loose {
struct RenderSettings {
    bool shadows=true,ao=true,axes=false,colliders=false;
    int debugView=0;
    float horizontalFov=90.f,sensitivity=.0022f,exposure=1.15f;
};
class Renderer {
public:
    Renderer(GLFWwindow* window,const Level& level,bool validation);
    ~Renderer();
    void beginUI();
    bool draw(const Game& game,const RenderSettings& settings,const std::string& capture="");
    const std::string& hardware()const;
    uint32_t validationErrors()const;
    bool validationActive()const;
private:
    struct Impl;
    std::unique_ptr<Impl> p;
};
}
