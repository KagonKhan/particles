#ifndef YARR_APP_WINDOW_HPP
#define YARR_APP_WINDOW_HPP

#include "utils/bases.hpp"

// This header only needs GLFW's own declarations, and glfw3.h left to itself drags in the
// system gl.h — which glew.h refuses to be included after. Every translation unit that pulls
// in GL does so through utils/opengl.hpp, which sets the same flag.
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


// glfwInit and glfwTerminate are library-wide rather than per-window, and terminate frees
// every window GLFW is still holding. Held as a member so the ordering is the compiler's
// problem: declared before the handle, it is destroyed after it, including on the paths
// where the constructor throws half-built.
struct GlfwLibrary : public Immovable<GlfwLibrary>
{
    GlfwLibrary();
    ~GlfwLibrary();
};


struct FramebufferSize
{
    int width  = 0;
    int height = 0;
};


///@brief A window and the OpenGL context it carries — in GLFW the two are one object,
/// created by one call, and the loader cannot run until that context is current.
class Window : public Immovable<Window>
{
public:
    Window(int width, int height, std::string const& title);

    [[nodiscard]] GLFWwindow* handle() const noexcept { return window_.get(); }

    [[nodiscard]] bool shouldClose() const noexcept
    {
        return glfwWindowShouldClose(window_.get()) != 0;
    }

    [[nodiscard]] bool iconified() const noexcept
    {
        return glfwGetWindowAttrib(window_.get(), GLFW_ICONIFIED) != 0;
    }

    [[nodiscard]] FramebufferSize framebufferSize() const noexcept
    {
        FramebufferSize size;
        glfwGetFramebufferSize(window_.get(), &size.width, &size.height);
        return size;
    }

    void pollEvents() const noexcept { glfwPollEvents(); }
    void swapBuffers() const noexcept { glfwSwapBuffers(window_.get()); }

private:
    GlfwLibrary  glfw_;
    WindowHandle window_;
};

#endif // YARR_APP_WINDOW_HPP
