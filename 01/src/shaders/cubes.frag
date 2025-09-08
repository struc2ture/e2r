#version 450 core

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

layout(std140, set = 0, binding = 0) uniform UBO_3D {
    mat4 model;
    mat4 proj_view;
} ubo_3d;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main()
{
    // ambient
    vec3 ambient = ubo.ambient_strength * ubo.light_color;

    // diffuse 
    vec3 norm = normalize(fragNormal);
    vec3 light_dir = normalize(ubo.light_pos - fragPos);
    float diff = max(dot(norm, light_dir), 0.0);
    vec3 diffuse = diff * ubo.light_color;

    // specular
    vec3 view_dir = normalize(ubo.view_pos - fragPos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), ubo.shininess);
    vec3 specular = ubo.specular_strength * spec * ubo.light_color;

    vec4 c = fragColor;
    vec4 l = vec4(ambient + diffuse + specular, 1.0);
    vec4 t = texture(texSampler, fragUV);
    outColor = l * t * c;
}
