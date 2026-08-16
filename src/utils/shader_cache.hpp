#ifndef YARR_SHADERS_HPP
#define YARR_SHADERS_HPP

#include "bases.hpp"
#include "opengl.hpp"

#include <filesystem>
#include <initializer_list>
#include <unordered_map>


struct Shader
{
    std::filesystem::path source;
    GLenum type; // TODO: add a proper enum class later for shader types (compute, fragment, vertex, ...)
};

class ShaderCache : public PureStatic<ShaderCache>
{
public:

    ///@brief clears the shader cache
    static void unload();

    ///@brief load all programs
    static void loadDefaults();
    ///@brief compiles a shader and returns the handle. Overwrites if exists
    static GLuint load(std::string const& name, Shader const& shader);
    ///@brief compiles a program and returns the handle. Requires at least one shader.
    static GLuint compileProgram(std::string const& name, std::initializer_list<GLuint> shaders);

    ///@brief returns compiled shader handle. Throws ShaderError on missing entry
    static GLuint getShader(std::string const& name);
    ///@brief returns compiled program handle. Throws ShaderError on missing entry
    static GLuint getProgram(std::string const& name);

private:
    inline static std::unordered_map<std::string, Shader> shaderCache_; // TODO: remove later and introduce some shader-identity
    inline static std::unordered_map<std::string, GLuint> compiledCache_;
    inline static std::unordered_map<std::string, GLuint> programCache_;
};


#endif // YARR_SHADERS_HPP
