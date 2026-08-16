#include "window.hpp"

#include "app/exceptions.hpp"
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
    if (!glfwInit()) {
        throw InitializationError("glfwInit failed!");
    }
}

GlfwLibrary::~GlfwLibrary()
{
    glfwTerminate();
}


Window::Window(int width, int height, std::string const& title)
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);   // 3.2+
    //  only glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // 3.0+ only

    window_.reset(glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr));
    if (!window_) {
        throw InitializationError("Could not create a window");
    }

    glfwMakeContextCurrent(window_.get());
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
}
