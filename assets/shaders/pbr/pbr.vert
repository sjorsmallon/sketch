#version 330 core

// Input vertex attributes
layout(location = 0) in vec3 aPos;     // Vertex position
layout(location = 1) in vec3 aNormal;  // Vertex normal
layout(location = 2) in vec2 aTexCoord;// Texture coordinates

// Output data for fragment shader
out vec3 frag_pos;    // Fragment position in world space
out vec3 normal;      // Normal in world space
out vec2 tex_coords;  // Texture coordinates

// Uniforms
uniform mat4 model;   // Model matrix
uniform mat4 view;    // View matrix
uniform mat4 projection; // Projection matrix

void main() {
    // Transform vertex position and normal to world space
    vec4 world_pos = model * vec4(aPos, 1.0);
    frag_pos = vec3(world_pos);
    normal = mat3(
                transpose(
                      inverse(model))) * aNormal;

    // Pass texture coordinates
    tex_coords = aTexCoord;

    // Output transformed vertex position (clip space)
    gl_Position = projection * view * world_pos;
}