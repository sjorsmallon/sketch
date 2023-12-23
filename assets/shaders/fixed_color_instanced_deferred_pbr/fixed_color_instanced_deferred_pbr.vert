layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 atexture_coords;
layout (location = 3) in vec3 position_offset; // comes from the compute shader buffer.

layout (location = 0) out vec3 fragment_position;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 texture_coords;
flat out int instance_ID;


uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 world_position = model_VS_in * vec4(position_VS_in, 1.0);
    fragment_position = world_position.xyz; 
    texture_coords = texture_coords_VS_in;
    
    mat3 normal_matrix = transpose(inverse(mat3(view * model_VS_in)));
    normal = normal_matrix * normal_VS_in;

    gl_Position = mvp_VS_in * vec4(position_VS_in, 1.0);

    instance_ID = gl_InstanceID;
}