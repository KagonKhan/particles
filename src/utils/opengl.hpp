#ifndef YARR_UTILS_OPENGL_HPP
#define YARR_UTILS_OPENGL_HPP
 #define GLM_ENABLE_EXPERIMENTAL
#define GLFW_INCLUDE_NONE
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include <cstdlib>


#define PARTICLES_STRINGIFY_IMPL(x) #x
#define PARTICLES_STRINGIFY(x)      PARTICLES_STRINGIFY_IMPL(x)


[[nodiscard]] bool inline checkOpenGLError()
{
    bool   found_error = false;
    GLenum gl_err      = glGetError();
    while (gl_err != GL_NO_ERROR) {
        spdlog::info("glError: {}", gl_err);
        found_error = true;
        gl_err      = glGetError();
    }

    return found_error;
}

#endif // YARR_UTILS_OPENGL_HPP
