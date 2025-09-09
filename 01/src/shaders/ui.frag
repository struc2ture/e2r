#version 450 core

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 t = texture(texSampler, fragUV);
    outColor = vec4(vec3(fragColor), t.a);
}
