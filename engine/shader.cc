#include "engine/shader.hh"

#include <OpenGL/gl3.h>

#include <fstream>
#include <iostream>

#include "engine/gl.hh"

namespace engine {
namespace {
int compile_shader(int type, const std::string& source) {
    // Create an empty shader handle
    int shader = glCreateShader(type);

    // Send the vertex shader source code to GL
    const char* data = source.c_str();
    glShaderSource(shader, 1, &data, 0);

    // Compile the vertex shader
    glCompileShader(shader);

    int compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        int length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

        // The maxLength includes the NULL character
        std::string log;
        log.resize(length);
        glGetShaderInfoLog(shader, length, &length, &log[0]);

        // We don't need the shader anymore.
        glDeleteShader(shader);

        throw std::runtime_error("Failed to compile " + std::to_string(type) + " shader: " + log);
    }
    return shader;
}
}  // namespace

//
// #############################################################################
//

Shader Shader::from_files(const std::filesystem::path& vertex, const std::filesystem::path& fragment,
                          const std::optional<std::filesystem::path>& geometry) {
    auto load_file = [](const std::filesystem::path& path) {
        std::ifstream file(path);
        return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    };

    return Shader(load_file(vertex), load_file(fragment),
                  geometry ? std::make_optional(load_file(*geometry)) : std::nullopt);
}

//
// #############################################################################
//

Shader::Shader(std::string vertex, std::string fragment, std::optional<std::string> geometry)
    : vertex_(std::move(vertex)), fragment_(std::move(fragment)), geometry_(std::move(geometry)) {}

//
// #############################################################################
//

void Shader::init() {
    int vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_);
    int fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_);
    if (vertex_shader < 0 || fragment_shader < 0) {
        throw std::runtime_error("Failed to build at least one of the shaders.");
    }

    // Vertex and fragment shaders are successfully compiled.
    // Now time to link them together into a program_.
    // Get a program_ object.
    program_ = glCreateProgram();

    // Attach our shaders to our program_
    gl_check(glAttachShader, program_, vertex_shader);
    gl_check(glAttachShader, program_, fragment_shader);

    int geometry_shader = 0;
    if (geometry_) {
        geometry_shader = compile_shader(GL_GEOMETRY_SHADER, *geometry_);
        if (geometry_shader < 0) {
            throw std::runtime_error("Failed to build geometry shader.");
        }
        gl_check(glAttachShader, program_, geometry_shader);
    }

    // Link our program_
    gl_check(glLinkProgram, program_);

    // Note the different functions here: glGetProgram* instead of glGetShader*.
    int linked = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        int length = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &length);

        std::string log;
        log.resize(length);
        glGetProgramInfoLog(program_, length, &length, &log[0]);

        // No need for program_ or shaders anymore
        glDeleteProgram(program_);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        if (geometry_) glDeleteShader(geometry_shader);

        throw std::runtime_error("Failed to link shaders: " + log);
    }

    // Always detach shaders after a successful link.
    gl_check(glDetachShader, program_, vertex_shader);
    gl_check(glDetachShader, program_, fragment_shader);
    if (geometry_) gl_check(glDetachShader, program_, geometry_shader);
}

//
// #############################################################################
//

void Shader::activate() const {
    if (program_ < 0) {
        throw std::runtime_error("Can't call Shader::activate() before Shader::init()");
    }
    gl_check(glUseProgram, program_);
}

//
// #############################################################################
//

int Shader::get_program_id() const { return program_; }
}  // namespace engine
