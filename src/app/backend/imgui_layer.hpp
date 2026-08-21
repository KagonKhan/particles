#ifndef YARR_APP_IMGUI_LAYER_HPP
#define YARR_APP_IMGUI_LAYER_HPP

#include "utils/bases.hpp"

struct GLFWwindow;


class ImGuiLayer : public Immovable<ImGuiLayer>
{
public:
    explicit ImGuiLayer(GLFWwindow* window);
    ~ImGuiLayer();

    void newFrame();

    void render();
};

#endif // YARR_APP_IMGUI_LAYER_HPP
