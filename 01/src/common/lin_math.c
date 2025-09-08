#include "lin_math.h"

v3 v3_normalize(v3 v)
{
    f32 mag = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    if (mag == 0.0f) return (v3){{{}}};
    f32 i_mag = 1.0f / mag;
    v3 result = {
        .x = v.x * i_mag,
        .y = v.y * i_mag,
        .z = v.z * i_mag
    };
    return result;
}

// --------------------------------------------

m4 m4_identity()
{
    m4 m;
    m.d[0] = 1; m.d[4] = 0; m.d[ 8] = 0; m.d[12] = 0;
    m.d[1] = 0; m.d[5] = 1; m.d[ 9] = 0; m.d[13] = 0;
    m.d[2] = 0; m.d[6] = 0; m.d[10] = 1; m.d[14] = 0;
    m.d[3] = 0; m.d[7] = 0; m.d[11] = 0; m.d[15] = 1;
    return m;
}

// --------------------------------------------

m4 m4_translate(f32 x, f32 y, f32 z)
{
    m4 m = m4_identity();
    m.d[12] = x;
    m.d[13] = y;
    m.d[14] = z;
    return m;
}

m4 m4_rotate(float angle_rad, v3 axis)
{
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    float ic = 1.0f - c;

    axis = v3_normalize(axis);

    m4 r;
    r.d[0]  = c + axis.x*axis.x*ic;
    r.d[1]  = axis.y*axis.x*ic + axis.z*s;
    r.d[2]  = axis.z*axis.x*ic - axis.y*s;
    r.d[3]  = 0.0f;

    r.d[4]  = axis.x*axis.y*ic - axis.z*s;
    r.d[5]  = c + axis.y*axis.y*ic;
    r.d[6]  = axis.z*axis.y*ic + axis.x*s;
    r.d[7]  = 0.0f;

    r.d[8]  = axis.x*axis.z*ic + axis.y*s;
    r.d[9]  = axis.y*axis.z*ic - axis.x*s;
    r.d[10] = c + axis.z*axis.z*ic;
    r.d[11] = 0.0f;

    r.d[12] = 0.0f;
    r.d[13] = 0.0f;
    r.d[14] = 0.0f;
    r.d[15] = 1.0f;
    return r;
}

m4 m4_scale(v3 scale)
{
    m4 m = m4_identity();
    m.d[0] = scale.x;
    m.d[5] = scale.y;
    m.d[10] = scale.z;
    return m;
}

// --------------------------------------------

m4 m4_mul(m4 a, m4 b)
{
    m4 m;
    for (int col = 0; col < 4; col++)
    {
        for (int row = 0; row < 4; row++)
        {
            m.d[col * 4 + row] = 0.0f;
            for (int k = 0; k < 4; k++)
            {
                m.d[col * 4 + row] += a.d[k * 4 + row] * b.d[col * 4 + k];
            }
        }
    }
    return m;
}

// --------------------------------------------

m4 m4_proj_ortho(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far)
{
    m4 m;
    f32 rl = right - left;
    f32 tb = top - bottom;
    f32 fn = far - near;

    m.d[0]  = 2.0f / rl;
    m.d[1]  = 0;
    m.d[2]  = 0;
    m.d[3]  = 0;

    m.d[4]  = 0;
    m.d[5]  = 2.0f / tb;
    m.d[6]  = 0;
    m.d[7]  = 0;

    m.d[8]  = 0;
    m.d[9]  = 0;
    m.d[10] = -2.0f / fn;
    m.d[11] = 0;

    m.d[12] = -(right + left) / rl;
    m.d[13] = -(top + bottom) / tb;
    m.d[14] = -(far + near) / fn;
    m.d[15] = 1.0f;

    return m;
}

m4 m4_proj_perspective(f32 fov, f32 aspect, f32 znear, f32 zfar)
{
    f32 tan_half = tanf(fov / 2.0f);

    m4 m = {};
    m.d[0] = 1.0f / (aspect * tan_half);
    m.d[5] = -1.0f / tan_half; // flip Y axis, as Vulkan flips the final framebuffer
    m.d[10] = -(zfar + znear) / (zfar - znear);
    m.d[11] = -1.0f;
    m.d[14] = -(2.0f * zfar * znear) / (zfar - znear);

    return m;
}

// --------------------------------------------

m4 m4_look_at(v3 eye, v3 target, v3 up)
{
    v3 f = v3_normalize(v3_sub(target, eye));
    v3 r = v3_normalize(v3_cross(f, up));
    v3 u = v3_cross(r, f);

    m4 m;
    m.d[0] = r.x;
    m.d[1] = u.x;
    m.d[2] = -f.x;
    m.d[3] = 0;

    m.d[4] = r.y;
    m.d[5] = u.y;
    m.d[6] = -f.y;
    m.d[7] = 0;

    m.d[8] = r.z;
    m.d[9] = u.z;
    m.d[10] = -f.z;
    m.d[11] = 0;

    m.d[12] = -v3_dot(r, eye);
    m.d[13] = -v3_dot(u, eye);
    m.d[14] = v3_dot(f, eye);
    m.d[15] = 1;

    return m;
}
