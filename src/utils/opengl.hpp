// opengl.hpp
#ifndef YARR_UTILS_OPENGL_HPP
#define YARR_UTILS_OPENGL_HPP

#include "app/exceptions.hpp"
#include <exception>

#define GLFW_INCLUDE_NONE
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>


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

static std::string fileToString(std::filesystem::path path)
{
    if (std::filesystem::exists(path) == false ) {
        throw FileError("{} file does not exist", path.string());
    }

    try {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), {}};
    }
    catch (std::exception const& e) {
        throw FileError("{} file could not be parsed: {}", path.string(), e.what());
    }
}

struct ShaderPaths
{
    std::filesystem::path vertex;
    std::filesystem::path fragment;
};

static GLuint createShaderProgram(ShaderPaths const& paths)
{
    GLint vertCompiled;
    GLint fragCompiled;
    GLint linked;

    std::string vshaderSource = fileToString(paths.vertex);
    std::string fshaderSource = fileToString(paths.fragment);
    const char* vertShaderSrc = vshaderSource.c_str();
    const char* fragShaderSrc = fshaderSource.c_str();

    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vShader, 1, &vertShaderSrc, NULL);
    glShaderSource(fShader, 1, &fragShaderSrc, NULL);
    glCompileShader(vShader);
    checkOpenGLError();
    glGetShaderiv(vShader, GL_COMPILE_STATUS, &vertCompiled);
    if (vertCompiled != 1) {
        spdlog::error("vertex compilation failed");
        printShaderLog(vShader);
    }

    glCompileShader(fShader);
    checkOpenGLError();
    glGetShaderiv(fShader, GL_COMPILE_STATUS, &fragCompiled);
    if (fragCompiled != 1) {
        spdlog::error("fragment compilation failed");
        printShaderLog(vShader);
    }

    GLuint vfProgram = glCreateProgram();
    glAttachShader(vfProgram, vShader);
    glAttachShader(vfProgram, fShader);

    glLinkProgram(vfProgram);

    checkOpenGLError();
    glGetProgramiv(vfProgram, GL_LINK_STATUS, &linked);
    if (linked != 1) {
        spdlog::error("linking failed");
        printProgramLog(vfProgram);
    }

    return vfProgram;
}

static GLuint compileShader(GLenum type, const std::string& source, const std::filesystem::path& debugPath)
{
    GLuint      shader = glCreateShader(type);
    const char* src    = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

        std::vector<char> log(logLength > 0? logLength : 1);
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());

        glDeleteShader(shader);
        throw std::runtime_error(
                  "Shader compilation failed (" + debugPath.string() + "):\n" + log.data());
    }

    return shader;
}

static GLuint createComputeShaderProgram(const std::filesystem::path& computePath)
{
    std::string source  = fileToString(computePath);
    GLuint      compute = compileShader(GL_COMPUTE_SHADER, source, computePath);

    GLuint program = glCreateProgram();
    glAttachShader(program, compute);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

        std::vector<char> log(logLength > 0? logLength : 1);
        glGetProgramInfoLog(program, logLength, nullptr, log.data());

        glDeleteShader(compute);
        glDeleteProgram(program);
        throw std::runtime_error(
                  "Compute shader program linking failed (" + computePath.string() + "):\n" + log.data());
    }

    // Shader object no longer needed once linked into the program.
    glDetachShader(program, compute);
    glDeleteShader(compute);

    spdlog::info("Compiled compute shader program: {}", computePath.string());

    return program;
}

#endif // YARR_UTILS_OPENGL_HPP
