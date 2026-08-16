#ifndef YARR_APP_IMGUI_LAYER_HPP
#define YARR_APP_IMGUI_LAYER_HPP

#include "utils/bases.hpp"

class Window;


///@brief The Dear ImGui context and its two backends, as one object with one lifetime.
/// Built after the window and torn down before it: the GLFW backend installs callbacks on
/// that window, and the OpenGL backend holds GL objects that can only be freed while the
/// context is still current.
class ImGuiLayer : public Immovable<ImGuiLayer>
{
public:
    explicit ImGuiLayer(Window& window);
    ~ImGuiLayer();

    void newFrame();

    ///@brief Ends the frame and draws it. The viewport is the caller's to set.
    void render();
};

#endif // YARR_APP_IMGUI_LAYER_HPP
