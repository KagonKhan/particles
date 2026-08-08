// opengl.hpp
#ifndef YARR_UTILS_OPENGL_HPP
#define YARR_UTILS_OPENGL_HPP

#include <cstdlib>
#define GLFW_INCLUDE_NONE
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>


#define PARTICLES_STRINGIFY_IMPL(x) #x
#define PARTICLES_STRINGIFY(x)      PARTICLES_STRINGIFY_IMPL(x)


static void printShaderLog(GLuint shader)
{
    int   len      = 0;
    int   chWrittn = 0;
    char* log;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len > 0) {
        log = (char*)malloc(len);
        glGetShaderInfoLog(shader, len, &chWrittn, log);
        spdlog::info("Shader Info Log: {}", log);
        free(log);
    }
}

static void printProgramLog(int prog)
{
    int   len      = 0;
    int   chWrittn = 0;
    char* log;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    if (len > 0) {
        log = (char*)malloc(len);
        glGetProgramInfoLog(prog, len, &chWrittn, log);
        spdlog::info("Program Info Log: {}", log);
        free(log);
    }
}

static bool checkOpenGLError()
{
    bool foundError = false;
    int  glErr      = glGetError();
    while (glErr != GL_NO_ERROR) {
        spdlog::info("glError: {}", glErr);
        foundError = true;
        glErr      = glGetError();
    }

    return foundError;
}

#endif // YARR_UTILS_OPENGL_HPP
