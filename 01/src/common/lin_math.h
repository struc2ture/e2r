#pragma once

#include <math.h>

#include "types.h"

#define PI32 3.14159265359f

/*
 * Column-major layout in memory
 * m[col][row]
 * m[col * 4 + row]
 * M00 M10 M20 M30
 * M01 M11 M21 M31
 * M02 M12 M22 M32
 * M03 M13 M23 M33
 * In memory:
 * M00 M01 M02 M03 M10 M11 ...
 */
typedef struct m4
{
    f32 d[16];
} m4;

static inline f32 deg_to_rad(f32 deg)
{
    f32 rad = deg / 180.0f * PI32;
    return rad;
}

static inline f32 rad_to_deg(f32 rad)
{
    f32 deg = rad / PI32 * 180.0f;
    return deg;
}

v3 v3_normalize(v3 v);

static inline v3 v3_cross(v3 a, v3 b)
{
    v3 result = {
        .x = a.y*b.z - a.z*b.y,
        .y = a.z*b.x - a.x*b.z,
        .z = a.x*b.y - a.y*b.x
    };
    return result;
}

static inline v3 v3_add(v3 a, v3 b)
{
    v3 result = {
        .x = a.x + b.x,
        .y = a.y + b.y,
        .z = a.z + b.z
    };
    return result;
}

static inline v3 v3_sub(v3 a, v3 b)
{
    v3 result = {
        .x = a.x - b.x,
        .y = a.y - b.y,
        .z = a.z - b.z,
    };
    return result;
}

static inline f32 v3_dot(v3 a, v3 b)
{
    f32 result = a.x*b.x + a.y*b.y + a.z*b.z;
    return result;
}

static inline v3 v3_scale(v3 v, float s)
{
    v3 result = {
        .x = v.x * s,
        .y = v.y * s,
        .z = v.z * s
    };
    return result;
}

m4 m4_identity();

m4 m4_translate(f32 x, f32 y, f32 z);
m4 m4_rotate(float angle_rad, v3 axis);
m4 m4_scale(v3 scale);

m4 m4_mul(m4 a, m4 b);

m4 m4_proj_ortho(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far);
m4 m4_proj_perspective(f32 fov, f32 aspect, f32 znear, f32 zfar);

m4 m4_look_at(v3 eye, v3 target, v3 up);
