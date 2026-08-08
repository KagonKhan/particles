#include "renderer.hpp"


#include <cstddef>
#include <filesystem>
#include <fstream>

constexpr std::size_t VAOcount {1};
GLuint                renderingProgram;
GLuint                vao[VAOcount];


float x   = 0.0f; // location of triangle on x axis
float inc = 0.01f; // offset for moving the triangle

std::filesystem::path resource_string = PARTICLES_STRINGIFY(RESOURCE_DIR);
std::string load(std::filesystem::path path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}

GLuint createShaderProgram()
{
    GLint vertCompiled;
    GLint fragCompiled;
    GLint linked;

    std::string vshaderSource = load(resource_string / "shaders/vertexShader.glsl");
    std::string fshaderSource = load(resource_string / "shaders/fragmentShader.glsl");
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

Renderer::Renderer()
{
    renderingProgram = createShaderProgram();
    glGenVertexArrays(VAOcount, vao);
    glBindVertexArray(vao[0]);
}

void Renderer::render()
{
    glUseProgram(renderingProgram);


    x += inc; // move the triangle along x axis
    if (x > 1.0f) {
        inc = -0.01f;       // switch to moving the triangle to the left
    }

    if (x < -1.0f) {
        inc = 0.01f;        // switch to moving the triangle to the right
    }

    GLuint offsetLoc = glGetUniformLocation(renderingProgram, "offset"); // get ptr to "offset"
    glProgramUniform1f(renderingProgram, offsetLoc, x); // send value in "x" to "offset"

    glDrawArrays(GL_TRIANGLES, 0, 3);
}
