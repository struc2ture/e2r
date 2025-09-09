#version 450 core

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(std140, set = 0, binding = 2) uniform UBO_Lighting
{
    vec3 view_pos;
    float ambient_strength;
    vec3 light_color;
    float specular_strength;
    vec3 light_pos;
    float shininess;

} ubo_lighting;

void main()
{
    // ambient
    vec3 ambient = ubo_lighting.ambient_strength * ubo_lighting.light_color;

    // diffuse 
    vec3 norm = normalize(fragNormal);
    vec3 light_dir = normalize(ubo_lighting.light_pos - fragPos);
    float diff = max(dot(norm, light_dir), 0.0);
    vec3 diffuse = diff * ubo_lighting.light_color;

    // specular
    vec3 view_dir = normalize(ubo_lighting.view_pos - fragPos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), ubo_lighting.shininess);
    vec3 specular = ubo_lighting.specular_strength * spec * ubo_lighting.light_color;

    vec4 c = fragColor;
    vec4 l = vec4(ambient + diffuse + specular, 1.0);
    vec4 t = texture(texSampler, fragUV);
    outColor = c * l * t;
}
