#pragma once
#include "renderer.hpp"
namespace loose {
// Developer-only deterministic route; normal play uses platform input.
struct VerifyRoute {
    int phase=0,phaseFrames=0;
    bool done=false;
    Input tick(Game& game,RenderSettings& settings,std::string& screenshot,GLFWwindow* window);
};
}
