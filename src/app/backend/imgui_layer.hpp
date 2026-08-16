#ifndef YARR_APP_IMGUI_LAYER_HPP
#define YARR_APP_IMGUI_LAYER_HPP

#include "utils/bases.hpp"

struct GLFWwindow;


// Must outlive nothing and be outlived by the window: the GLFW backend installs callbacks
// on it, and the OpenGL backend frees GL objects that need the context still current.
class ImGuiLayer : public Immovable<ImGuiLayer>
{
public:
    explicit ImGuiLayer(GLFWwindow* window);
    ~ImGuiLayer();

    void newFrame();

    ///@brief Ends the frame and draws it. The viewport is the caller's to set.
    void render();
};

#endif // YARR_APP_IMGUI_LAYER_HPP
