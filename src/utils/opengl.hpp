#ifndef YARR_UTILS_OPENGL_HPP
#define YARR_UTILS_OPENGL_HPP

// Left to itself glfw3.h pulls in the system gl.h, which glew.h refuses to follow.
#define GLFW_INCLUDE_NONE

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>


#define PARTICLES_STRINGIFY_IMPL(x) #x
#define PARTICLES_STRINGIFY(x)      PARTICLES_STRINGIFY_IMPL(x)


[[nodiscard]] inline bool checkOpenGLError()
{
    bool   found_error = false;
    GLenum gl_err      = glGetError();
    while (gl_err != GL_NO_ERROR) {
        spdlog::error("glError: {}", gl_err);
        found_error = true;
        gl_err      = glGetError();
    }

    return found_error;
}

#endif // YARR_UTILS_OPENGL_HPP
