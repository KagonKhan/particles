#ifndef YARR_APP_WINDOW_HPP
#define YARR_APP_WINDOW_HPP

#include "renderer/render_view.hpp"
#include "utils/bases.hpp"
#include "utils/opengl.hpp"

// Left to itself glfw3.h pulls in the system gl.h, which glew.h refuses to follow.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <memory>
#include <string>


// GLFWwindow is opaque — GLFW allocates it and frees it, and never completes the type. The
// default deleter is a `delete` on an incomplete type, which does not compile and would be
// the wrong call even if it did.
struct GlfwWindowDeleter
{
    void operator ()(GLFWwindow* window) const noexcept { glfwDestroyWindow(window); }
};

using WindowHandle = std::unique_ptr<GLFWwindow, GlfwWindowDeleter>;


// glfwTerminate frees every window GLFW still holds, so it has to run after the handle
// below is destroyed. Declared before it, it does.
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

    void clear()
    {
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    [[nodiscard]] FramebufferSize size() const noexcept
    {
        FramebufferSize size;
        glfwGetFramebufferSize(window_.get(), &size.width, &size.height);
        return size;
    }

    void pollEvents() const noexcept  { glfwPollEvents(); }
    void swapBuffers() const noexcept { glfwSwapBuffers(window_.get()); }

    // Idempotent, so a caller holding a setting can hand it over every frame without
    // paying for a driver call it does not need.
    void setVSync(bool enabled) noexcept
    {
        if (enabled == vsync_) {
            return;
        }

        vsync_ = enabled;
        glfwSwapInterval(enabled? 1 : 0);
    }

private:
    GlfwLibrary  glfw_;
    WindowHandle window_;

    bool vsync_ {false};
};

#endif // YARR_APP_WINDOW_HPP
