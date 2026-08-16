#ifndef YARR_APP_WINDOW_HPP
#define YARR_APP_WINDOW_HPP

#include "app/backend/imgui_layer.hpp"
#include "renderer/render_view.hpp"
#include "utils/bases.hpp"

// Left to itself glfw3.h pulls in the system gl.h, which glew.h refuses to follow.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <memory>
#include <string>


struct GlfwWindowDeleter
{
    void operator ()(GLFWwindow* window) const noexcept { glfwDestroyWindow(window); }
};

using WindowHandle = std::unique_ptr<GLFWwindow, GlfwWindowDeleter>;


struct GlfwLibrary : public Immovable<GlfwLibrary>
{
    GlfwLibrary();
    ~GlfwLibrary();
};


class Window : public Immovable<Window>
{
public:
    Window(int width, int height, std::string const& title);

    [[nodiscard]] GLFWwindow* handle() const noexcept      { return window_.get(); }
    [[nodiscard]] bool        shouldClose() const noexcept { return glfwWindowShouldClose(window_.get()) != 0; }
    [[nodiscard]] bool        iconified() const noexcept
    {
        return glfwGetWindowAttrib(window_.get(), GLFW_ICONIFIED) != 0;
    }

    void beginFrame();
    void pollEvents() const noexcept { glfwPollEvents(); }

    ///@brief Blocks until an event arrives. For the frames there is no reason to draw.
    void waitEvents() const noexcept { glfwWaitEvents(); }
    void endFrame();

    [[nodiscard]] FramebufferSize size() const noexcept;
    void                          setVSync(bool enabled) noexcept;


private:
    ///@brief Creates the window and makes its context current, GLEW included.
    [[nodiscard]] static WindowHandle open(int width, int height, std::string const& title);

    // Declaration order is lifetime order: GLFW before the window, the window before the
    // interface layer that installs callbacks on it and holds GL objects from its context.
    GlfwLibrary  glfw_;
    WindowHandle window_;
    ImGuiLayer   imgui_ {window_.get()};

    bool vsync_ {false};
};

#endif // YARR_APP_WINDOW_HPP
