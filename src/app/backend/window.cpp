#include "window.hpp"

#include "exceptions.hpp"
#include "renderer/render_view.hpp"
#include "utils/opengl.hpp"

#include <spdlog/spdlog.h>

#include <string_view>

namespace
{

void glfw_error_callback(int error, const char* description)
{
    spdlog::error("Glfw Error {}: {}\n", error, description);
}

} // namespace


GlfwLibrary::GlfwLibrary()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (glfwInit() == 0) {
        throw InitializationError("glfwInit failed!");
    }
}

GlfwLibrary::~GlfwLibrary()
{
    glfwTerminate();
}

Window::Window(int width, int height, std::string const& title)
    : window_{open(width, height, title)}
{}

WindowHandle Window::open(int width, int height, std::string const& title)
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);   // 3.2+
    //  only glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // 3.0+ only

    WindowHandle window {glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr)};
    if (!window) {
        throw InitializationError("Could not create a window");
    }

    glfwMakeContextCurrent(window.get());
    if (glewInit() != GLEW_OK) {
        throw InitializationError("glewInit failed!");
    }

    // Which driver answered, said out loud. A software rasterizer is a working renderer and
    // announces itself no other way — it cost a day of benchmarking the wrong thing once.
    auto const* const renderer_name = reinterpret_cast<char const*>(glGetString(GL_RENDERER));
    spdlog::info(
        "GL renderer: {} | {}",
        renderer_name,
        reinterpret_cast<char const*>(glGetString(GL_VERSION)));

    if (std::string_view {renderer_name}.contains("llvmpipe")) {
        spdlog::warn("Rendering in software. Expect ~10x the CPU, on threads that compete with the simulation.");
    }

    glfwSwapInterval(0);
    // glfwSwapInterval(1); // Enable vsync

    return window;
}

void Window::beginFrame()
{
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    imgui_.newFrame();
}

void Window::endFrame()
{
    auto const [display_w, display_h] = size();
    glViewport(0, 0, display_w, display_h);

    imgui_.render();
    glfwSwapBuffers(window_.get());
}

FramebufferSize Window::size() const noexcept
{
    FramebufferSize size;
    glfwGetFramebufferSize(window_.get(), &size.width, &size.height);
    return size;
}

void Window::setVSync(bool enabled) noexcept
{
    if (enabled == vsync_) {
        return;
    }

    vsync_ = enabled;
    glfwSwapInterval(enabled? 1 : 0);
}
