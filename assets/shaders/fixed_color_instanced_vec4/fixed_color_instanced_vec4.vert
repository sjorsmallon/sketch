#version 430 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 position_offset;

uniform mat4 view;
uniform mat4 projection;

out flat vec3 instance_color;

void main() {
    
    vec4 world_position = vec4(aPos + vec3(position_offset.xyz), 1.0);
    gl_Position = projection * view * world_position;

    instance_color = vec3(gl_InstanceID % 256) / 255.0; // Calculate unique color based on i
}