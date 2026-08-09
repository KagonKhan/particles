#include "shader_cache.hpp"

#include "app/exceptions.hpp"
#include "utils/utils.hpp"

#include <spdlog/common.h>


namespace
{

void printShaderLog(GLuint shader)
{
    GLint log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

    std::vector<char> log(log_length > 0? log_length : 1);
    glGetShaderInfoLog(shader, log_length, nullptr, log.data());

    spdlog::error("Shader compilation failed:\n\t{}", log.data());
}

void printProgramLog(GLuint program)
{
    GLint log_length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

    std::vector<char> log(log_length > 0? log_length : 1);
    glGetProgramInfoLog(program, log_length, nullptr, log.data());

    spdlog::error("Program compilation failed:\n\t{}", log.data());
}

GLuint compileShader(Shader const& shader)
{
    GLuint gl_shader = glCreateShader(shader.type);

    std::string source   = fileToString(shader.source);
    const char* source_c = source.c_str();

    glShaderSource(gl_shader, 1, &source_c, nullptr);
    glCompileShader(gl_shader);

    checkOpenGLError();

    GLint success = GL_FALSE;
    glGetShaderiv(gl_shader, GL_COMPILE_STATUS, &success);

    if (success != GL_TRUE) {
        printShaderLog(gl_shader);
        glDeleteShader(gl_shader);
        throw ShaderError("Shader {} compilation failed", shader.source.string());
    }

    return gl_shader;
}

GLuint createProgram(std::initializer_list<GLuint> shaders)
{
    GLuint program = glCreateProgram();

    for (GLuint shader : shaders) {
        glAttachShader(program, shader);
    }

    glLinkProgram(program);
    checkOpenGLError();
    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        printProgramLog(program);
        glDeleteProgram(program);
        throw ShaderError("Program linking failed");
    }

    return program;
}

} // namespace

void ShaderCache::unload()
{
    for (const auto& [name, shader] : compiledCache_) {
        glDeleteShader(shader);
    }

    for (const auto& [name, program] : programCache_) {
        glDeleteProgram(program);
    }

    compiledCache_.clear();
    programCache_.clear();
    shaderCache_.clear();
}

GLuint ShaderCache::load(std::string const& name, Shader const& shader)
{
    GLuint compiled_shader = compileShader(shader);

    if (compiledCache_.contains(name)) {
        glDeleteShader(compiledCache_[name]);
    }

    shaderCache_[name]   = shader;
    compiledCache_[name] = compiled_shader;
    return compiled_shader;
}

GLuint ShaderCache::getShader(std::string const& name)
{
    if (!compiledCache_.contains(name)) {
        throw ShaderError("{} shader does not exist", name);
    }

    return compiledCache_[name];
}

GLuint ShaderCache::getProgram(std::string const& name)
{
    if (!programCache_.contains(name)) {
        throw ShaderError("{} program does not exist", name);
    }

    return programCache_[name];
}

GLuint ShaderCache::compileProgram(std::string const& name, std::initializer_list<GLuint> shaders)
{
    if (shaders.size() == 0) {
        throw ShaderError("{} program compilation requries at least one shader!");
    }

    GLuint program = createProgram(shaders);
    if (programCache_.contains(name)) {
        glDeleteProgram(programCache_[name]);
    }

    programCache_[name] = program;
    spdlog::info("Compiled shader program: {}", name);
    return program;
}
