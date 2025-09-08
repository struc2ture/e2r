#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

layout(std140, set = 0, binding = 0) uniform UBO_3D {
    mat4 model;
    mat4 view_proj;
} ubo_3d;

//layout(push_constant) uniform Push {
//    mat4 model;
//} push;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPos;

void main()
{
    gl_Position = ubo_3d.view_proj * ubo_3d.model * vec4(inPos, 1.0);
    // mat4 proj;
    // proj[0] = vec4(1.559, 0.000, 0.000, 0.000);
    // proj[1] = vec4(0.000, -1.732, 0.000, 0.000);
    // proj[2] = vec4(0.000, 0.000, -1.002, -1.000);
    // proj[3] = vec4(0.000, 0.000, -0.200, 0.000);
    // gl_Position = proj * vec4(vec2(inPos), 0.0, 1.0);
    fragColor = inColor;
    fragUV = inUV;
    fragNormal = mat3(transpose(inverse(ubo_3d.model))) * inNormal;
    fragPos = vec3(ubo_3d.model * vec4(inPos, 1.0));
}
