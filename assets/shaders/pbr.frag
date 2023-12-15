#version 330 core

// Input data from vertex shader
in vec3 frag_pos;     // Fragment position in world space
in vec3 normal;       // Normal in world space
in vec2 tex_coords;   // Texture coordinates

// Output data
out vec4 fragment_color;  

// Material properties
uniform sampler2D albedo_map;    // Albedo (color) texture
uniform sampler2D normal_map;    // Normal map
uniform sampler2D metallic_map;  // Metallic map
uniform sampler2D roughness_map; // Roughness map
uniform sampler2D ao_map;        // Ambient occlusion map

struct Light {
    vec3 position;
    vec3 color;
};

uniform Light light;

void main() {
    // Retrieve material properties from textures
    vec3 albedo = texture(albedo_map, tex_coords).rgb;
    float metallic = texture(metallic_map, tex_coords).r;
    float roughness = texture(roughness_map, tex_coords).r;
    float ao = texture(ao_map, tex_coords).r;

    // Apply normal mapping
    vec3 normal_map = normalize(texture(normal_map, tex_coords).rgb * 2.0 - 1.0);

    // Calculate lighting
    vec3 view_dir = normalize(-frag_pos);
    vec3 light_dir = normalize(light.position - frag_pos);
    vec3 halfway_dir = normalize(view_dir + light_dir);

    // Calculate Fresnel term
    float f0 = 0.04; // Reflectance at normal incidence for dielectric surfaces (approximate for non-metals)
    float f = f0 + (1.0 - f0) * pow(1.0 - dot(view_dir, halfway_dir), 5.0);

    // Calculate specular term using Cook-Torrance BRDF
    float ndf = exp(-pow(max(dot(normal_map, halfway_dir), 0.0), 2.0) / (roughness * roughness)) / (3.14159 * roughness * roughness * dot(normal_map, halfway_dir) * dot(normal_map, halfway_dir));
    float g = min(1.0, min(2.0 * dot(normal_map, halfway_dir) * dot(normal_map, view_dir) / dot(view_dir, halfway_dir), 2.0 * dot(normal_map, halfway_dir) * dot(normal_map, light_dir) / dot(view_dir, halfway_dir)));
    vec3 ks = f * metallic + (1.0 - metallic);
    vec3 kd = 1.0 - ks;
    kd *= 1.0 - metallic;

    vec3 specular = (ndf * g * f) / (4.0 * dot(normal_map, view_dir) * dot(normal_map, light_dir));
    vec3 diffuse = (kd * albedo) / 3.14159;

    vec3 color = (diffuse + specular) * light.color * ao;

    fragment_color = vec4(color, 1.0);
}