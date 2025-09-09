#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

layout(std140, set = 0, binding = 0) uniform UBO_3D {
    mat4 view_proj;
} ubo_3d;

layout(push_constant) uniform Push {
   mat4 model;
} push;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPos;

void main()
{
    gl_Position = ubo_3d.view_proj * push.model * vec4(inPos, 1.0);
    fragColor = inColor;
    fragUV = inUV;
    fragNormal = mat3(transpose(inverse(push.model))) * inNormal;
    fragPos = vec3(push.model * vec4(inPos, 1.0));
}
