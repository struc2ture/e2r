#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;
layout(location = 3) in uint inTexIndex;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out flat uint fragTexIndex;

layout(std140, set = 0, binding = 0) uniform UBO_2D {
    mat4 view_proj;
} ubo_2d;

void main()
{
    gl_Position = ubo_2d.view_proj * vec4(inPos, 1.0);
    fragColor = inColor;
    fragUV = inUV;
    fragTexIndex = inTexIndex;
}
