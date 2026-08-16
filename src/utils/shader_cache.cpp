#include "shader_cache.hpp"

#include "exceptions.hpp"
#include "utils/utils.hpp"

#include <spdlog/common.h>

#include <filesystem>
#include <format>
#include <sstream>
#include <string>
#include <string_view>


namespace
{

std::filesystem::path      RESOURCE_ROOT     = PARTICLES_STRINGIFY(RESOURCE_DIR);
constexpr std::string_view INCLUDE_DIRECTIVE = "#include";
constexpr int              MAX_INCLUDE_DEPTH = 8;

[[nodiscard]] std::string_view trimLeft(std::string_view text)
{
    std::size_t const first = text.find_first_not_of(" \t");
    return (first == std::string_view::npos)? std::string_view {} : text.substr(first);
}

// The quoted name of an `#include "..."` line, or nothing if the line is not one. No angled
// form: there is no system include path for a shader to reach into.
[[nodiscard]] std::string_view includedName(std::string_view line)
{
    std::string_view rest = trimLeft(line);

    if (!rest.starts_with(INCLUDE_DIRECTIVE)) {
        return {};
    }

    rest = trimLeft(rest.substr(INCLUDE_DIRECTIVE.size()));

    if (!rest.starts_with('"')) {
        return {};
    }

    rest = rest.substr(1);
    std::size_t const end = rest.find('"');

    return (end == std::string_view::npos)? std::string_view {} : rest.substr(0, end);
}

// GLSL has no #include of its own short of GL_ARB_shading_language_include, which is not
// widely served. One shared file — the distance functions the simulation evaluates too, so
// that bodies collide where they are drawn — is worth resolving it here instead.
//
// The `#line` after a splice puts the host file's numbering back, so an error below an
// include still names the line you would count to. Note that GLSL through 4.40 specifies
// `#line n` as meaning the *next* line is n+1 where 4.50 and C mean it is n, so on a strict
// 4.40 driver the reported line may be one out. It is diagnostics either way.
[[nodiscard]] std::string resolveIncludes(std::filesystem::path const& path, int depth = 0)
{
    if (depth > MAX_INCLUDE_DEPTH) {
        throw ShaderError(
                  "{}: #include nested more than {} deep, which is a cycle in all but name",
                  path.string(),
                  MAX_INCLUDE_DEPTH);
    }

    std::istringstream source {fileToString(path)};
    std::string        resolved;
    std::string        line;

    for (int number = 1; std::getline(source, line); ++number) {
        std::string_view const name = includedName(line);

        if (name.empty()) {
            resolved += line;
            resolved += '\n';
            continue;
        }

        // Resolved against the including file's own directory, so a shader names its
        // neighbours the way its text reads rather than the way it happened to be launched.
        resolved += "#line 1\n";
        resolved += resolveIncludes(path.parent_path() / name, depth + 1);
        resolved += std::format("#line {}\n", number + 1);
    }

    return resolved;
}

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

    std::string source   = resolveIncludes(shader.source);
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

DefaultShaders::DefaultShaders()
{
    ShaderCache::compileProgram(
        "PointProgram",
        {
            ShaderCache::load(
                "point_vertex",
                {
                    .source = RESOURCE_ROOT / "shaders/point.vert",
                    .type   = GL_VERTEX_SHADER
                }),
            ShaderCache::load(
                "point_fragment",
                {
                    .source = RESOURCE_ROOT / "shaders/point.frag",
                    .type   = GL_FRAGMENT_SHADER
                }),
        }
    );

    ShaderCache::compileProgram(
        "ShapeProgram",
        {
            ShaderCache::load(
                "shape_vertex",
                {
                    .source = RESOURCE_ROOT / "shaders/shape.vert",
                    .type   = GL_VERTEX_SHADER
                }),
            ShaderCache::load(
                "shape_fragment",
                {
                    .source = RESOURCE_ROOT / "shaders/shape.frag",
                    .type   = GL_FRAGMENT_SHADER
                }),
        }
    );

    ShaderCache::compileProgram(
        "SplatProgram",
        {
            ShaderCache::load(
                "splat_compute",
                {
                    .source = RESOURCE_ROOT / "shaders/splat.comp",
                    .type   = GL_COMPUTE_SHADER
                })
        }
    );

    ShaderCache::compileProgram(
        "ResolveProgram",
        {
            ShaderCache::load(
                "fullscreen_vertex",
                {
                    .source = RESOURCE_ROOT / "shaders/fullscreen.vert",
                    .type   = GL_VERTEX_SHADER
                }),
            ShaderCache::load(
                "density_fragment",
                {
                    .source = RESOURCE_ROOT / "shaders/density.frag",
                    .type   = GL_FRAGMENT_SHADER
                }),
        }
    );
}
