#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <glad/glad.h> 

#include <GL/glew.h>
#include <unordered_map>
#include <string>


struct Shader_Program {
    uint32_t program_id;
    std::unordered_map<std::string, int> uniform_locations;
}

// Getter for uniform location
GLint get_uniform_location(const Shader_Program& shader_program, const std::string& uniform_name) {
    GLuint program_id = shader_program.program_id; // Assuming program_id is accessible
    GLint location = glGetUniformLocation(program_id, uniform_name.c_str());
    if (location == -1) {
        // Handle error if uniform doesn't exist or wasn't used in the shader
        // You might want to log an error or handle it in your application
        std::cerr << "could not find uniform" << uniform_name << ".\n";
    }
    return location;
}

// Templated set_uniform function to handle different types
template<typename T>
void set_uniform(Shader_Program& shader_program, const std::string& uniform_name, T&& value) {
    GLint location = get_uniform_location(shader_program, uniform_name);
    if (location != -1) {
        set_uniform_value(location, std::forward<T>(value));
    }
}

template<typename T>
void set_uniform_value(GLint location, T&& value);

// Specializations for different types (add more as needed)
template<>
void set_uniform_value(GLint location, float&& value) {
    glUniform1f(location, value);
}

template<>
void set_uniform_value(GLint location, int&& value) {
    glUniform1i(location, value);
}

template<>
void set_uniform_value(GLint location, const glm::vec3& value) {
    glUniform3f(location, value.x, value.y, value.z);
}

// Function to load and compile shader from file
inline uint32_t load_shader_from_file(const std::string& file_path, GLenum shader_type) {
    // Read shader source code from file
    std::ifstream shader_file(file_path);
    if (!shader_file.is_open()) {
        std::cerr << "Error: Unable to open shader file: " << file_path << std::endl;
        return 0;
    }

    std::stringstream shader_stream;
    shader_stream << shader_file.rdbuf();
    std::string shader_code = shader_stream.str();
    shader_file.close();

    // Create shader object
    uint32_t shader_id = glCreateShader(shader_type);
    const char* shader_code_ptr = shader_code.c_str();
    glShaderSource(shader_id, 1, &shader_code_ptr, NULL);

    // Compile shader
    glCompileShader(shader_id);

    // Check compilation status
    GLint compile_status;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compile_status);
    if (compile_status == GL_FALSE) {
        GLint log_length;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);
        std::string error_log(static_cast<size_t>(log_length), ' ');
        glGetShaderInfoLog(shader_id, log_length, NULL, &error_log[0]);
        std::cerr << "Error: Shader compilation failed (" << file_path << "):\n" << error_log << std::endl;

        glDeleteShader(shader_id);
        return 0;
    }

    return shader_id;
}


// expects valid shaders.
inline uint32_t create_shader_program(const uint32_t* shaders, int num_shaders) {
    // Create shader program
    uint32_t program_id = glCreateProgram();

    // Attach shaders to the program
    for (int i = 0; i < num_shaders; ++i) {
        glAttachShader(program_id, shaders[i]);
    }

    // Link the program
    glLinkProgram(program_id);

    // Check link status
    GLint link_status;
    glGetProgramiv(program_id, GL_LINK_STATUS, &link_status);
    if (link_status == GL_FALSE) {
        GLint log_length;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);
        std::string error_log(static_cast<size_t>(log_length), ' ');
        glGetProgramInfoLog(program_id, log_length, NULL, &error_log[0]);
        std::cerr << "Error: Program linking failed:\n" << error_log << std::endl;

        glDeleteProgram(program_id);
        return 0;
    }

    // Detach shaders after successful linking
    for (int i = 0; i < num_shaders; ++i) {
        glDetachShader(program_id, shaders[i]);
    }

    return program_id;
}


inline void load_shaders() 
{
        // Load vertex and fragment shaders
    GLuint vertex_shader   = load_shader_from_file("path/to/vertexShader.glsl", GL_VERTEX_SHADER);
    GLuint fragment_shader = load_shader_from_file("path/to/fragmentShader.glsl", GL_FRAGMENT_SHADER);

    if (vertexShader == 0 || fragmentShader == 0) {
        // Handle shader loading errors
        // Exit or handle errors gracefully
    }

    // Create an OpenGL program and link shaders to it
    GLuint shaders[] = { vertexShader, fragmentShader };
    GLuint shaderProgram = create_shader_program(shaders, 2);

    if (shaderProgram == 0) {
        // Handle program linking error
        // Exit or handle errors gracefully
    }

}


