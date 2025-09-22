#version 450 core

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in flat uint fragTexIndex;

layout(set = 0, binding = 1) uniform sampler2D texSampler[2];

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 t = texture(texSampler[fragTexIndex], fragUV);
    outColor = vec4(vec3(fragColor), t.a);
}
