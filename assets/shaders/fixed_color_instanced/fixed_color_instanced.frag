#version 430 core
out vec4 FragColor;

in flat vec3 instance_color;

void main() {
  FragColor = vec4(instance_color, 1); // Orange color
  
}