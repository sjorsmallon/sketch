#pragma once

#include <glad/glad.h> 

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <initializer_list>

#include <unordered_map>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>


struct Shader_Program {
    uint32_t program_id;
};

// get uniform location (no storage)
int32_t get_uniform_location(uint32_t program_id, const std::string& uniform_name) {

    //@FIXME: we should cache uniform locations. for now, keep it stateless.
    int32_t location = glGetUniformLocation(program_id, uniform_name.c_str());

    if (location == -1) {
        // Handle error if uniform doesn't exist or wasn't used in the shader
        // You might want to log an error or handle it in your application
        std::cerr << "could not find uniform" << uniform_name << ".\n";
    }
    return location;
}


//@NOTE: we are just copying all over the place here.
template<typename T>
void set_uniform_value(int32_t location, T value);

// Specializations for different types (add more as needed)
template<>
void set_uniform_value(int32_t location, float value)
{

    glUniform1f(location, value);
}

template<>
void set_uniform_value(int32_t location, int value)
{
    glUniform1i(location, value);
}

// After some deliberation: since this is all gl related, we just feed it a glm::* type. 
// we can always convert it before sending it over.
// if it turns out this is actually causing issues, we'll deal with it then
template<>
void set_uniform_value(int32_t location, glm::vec3 value) {
    glUniform3f(location, value.x, value.y, value.z);
}


template<>
void set_uniform_value(int32_t location, glm::mat4 value)
{
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}


// Templated set_uniform function to handle different types
template<typename T>
void set_uniform(Shader_Program& shader_program, const std::string& uniform_name, T&& value) {
    int32_t location = get_uniform_location(shader_program.program_id, uniform_name);


    if (location != -1) {
        set_uniform_value(location, std::forward<T>(value));
    }
}



void list_all_attributes(GLuint program_id)
{
    GLint attribute_count;
    glGetProgramiv(program_id, GL_ACTIVE_ATTRIBUTES, &attribute_count);

    GLint max_attribute_name_length;
    glGetProgramiv(program_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_attribute_name_length);

    std::vector<GLchar> attribute_name(max_attribute_name_length);

    for (int i = 0; i < attribute_count; ++i) {
        GLint size;
        GLenum type;
        glGetActiveAttrib(program_id, i, max_attribute_name_length, nullptr, &size, &type, attribute_name.data());

        GLint location = glGetAttribLocation(program_id, attribute_name.data());

        std::cout << "Attribute #" << i << ": ";
        std::cout << "Name: " << attribute_name.data() << ", ";
        std::cout << "Type: " << type << ", ";
        std::cout << "Location: " << location << std::endl;
    }
}

void list_all_uniforms(GLuint program_id) {

    // establish a mapping between glenum and the string representation.
    auto uniform_type_to_string = [](GLenum type) -> std::string {
        using Pair = std::pair<GLenum, std::string>;

        auto mapping = std::initializer_list<Pair>{
            {GL_FLOAT, "float"},
            {GL_FLOAT_VEC2, "vec2"},
            {GL_FLOAT_VEC3, "vec3"},
            {GL_FLOAT_VEC4, "vec4"},
            {GL_FLOAT_MAT3x4, "mat3x4"},
            {GL_FLOAT_MAT4, "mat4"},
            {GL_INT, "int"},
            {GL_INT_VEC2, "vec2"},
            {GL_INT_VEC3, "vec3"},
            {GL_SAMPLER_2D, "sampler2D"},
            {GL_UNSIGNED_INT, "uint"},
            {GL_UNSIGNED_INT_ATOMIC_COUNTER, "uatomic_uint"}
        };

        for (auto& [gl_enum, type_string]: mapping)
        {
            if (type == gl_enum) 
            {
                return type_string;
            }
        }

        // unknown uniform type!
        return "";
    };


    GLint uniform_count;
    glGetProgramiv(program_id, GL_ACTIVE_UNIFORMS, &uniform_count);

    GLint max_uniform_name_length;
    glGetProgramiv(program_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_uniform_name_length);

    for (int i = 0; i < uniform_count; ++i) {
        std::string uniform_name(max_uniform_name_length, '\0');
        GLint size;
        GLenum type;
        glGetActiveUniform(program_id, i, max_uniform_name_length, nullptr, &size, &type, &uniform_name[0]);

        GLint location = glGetUniformLocation(program_id, uniform_name.c_str());

        std::cout << "uniform #" << i << ": ";
        std::cout << "name: " << uniform_name << ", ";
        std::cout << "type: " << uniform_type_to_string(type) << ", ";
        std::cout << "location: " << location << std::endl;

    }
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
    int32_t compile_status;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compile_status);
    if (compile_status == GL_FALSE) {
        int32_t log_length;
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
    int32_t link_status;
    glGetProgramiv(program_id, GL_LINK_STATUS, &link_status);
    if (link_status == GL_FALSE) {
        int32_t log_length;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);
        std::string error_log(static_cast<size_t>(log_length), ' ');
        glGetProgramInfoLog(program_id, log_length, NULL, &error_log[0]);
        std::cerr << "Error: Program linking failed:\n" << error_log << std::endl;

        glDeleteProgram(program_id);
        return 0;
    }

    // Detach shaders after successful linking
    //@NOTE: shaders are not deleted after detaching. it is possible to just delete them at this point.
    for (int i = 0; i < num_shaders; ++i) {
        glDetachShader(program_id, shaders[i]);
    }

    return program_id;
}

// public API

using Shader_Pair = std::pair<std::string, GLenum>;
inline uint32_t create_shader_program_from_files(std::initializer_list<Shader_Pair> list)
{

    auto shader_list = std::vector<uint32_t>();
    for (auto& [path, shader_type]: list)
    {
        shader_list.push_back(load_shader_from_file(path, shader_type));
    }

    uint32_t shader_program = create_shader_program(shader_list.data(), shader_list.size());

    if (shader_program == 0)
    {
        // Handle program linking error
        // Exit or handle errors gracefully
        std::cerr << "shader program failed.";
        return 0;
    }
    else {
        std::cout << "create program succeeded.\n";
        list_all_attributes(shader_program);
        list_all_uniforms(shader_program);
    }

    return shader_program;
}
