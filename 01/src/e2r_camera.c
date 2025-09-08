#include "e2r_camera.h"

#include <stdlib.h>

#include "common/lin_math.h"
#include "common/random.h"
#include "common/types.h"

E2R_Camera e2r_camera_set_from_pos_target(v3 pos, v3 target)
{
    v3 dir = v3_normalize(v3_sub(target, pos));
    f32 pitch_rad = asinf(dir.y);
    f32 yaw_rad = atan2f(dir.z, dir.x);
    E2R_Camera c = {};
    c.pos = pos;
    c.pitch_deg = rad_to_deg(pitch_rad);
    c.yaw_deg = rad_to_deg(yaw_rad);
    if (c.pitch_deg > 89.9f) c.pitch_deg = 89.9f;
    else if (c.pitch_deg < -89.9f) c.pitch_deg = -89.9f;
    return c;
}

v3 e2r_camera_get_dir(const E2R_Camera *c)
{
    v3 dir;
    f32 pitch = deg_to_rad(c->pitch_deg);
    f32 yaw = deg_to_rad(c->yaw_deg);
    dir.x = cosf(pitch) * cosf(yaw);
    dir.y = sinf(pitch);
    dir.z = cosf(pitch) * sinf(yaw);
    dir = v3_normalize(dir);
    return dir;
}

v3 e2r_camera_get_right(const E2R_Camera *c)
{
    v3 dir = e2r_camera_get_dir(c);
    v3 world_up = V3(0.0f, 1.0f, 0.0f);
    v3 right = v3_normalize(v3_cross(world_up, dir));
    return right;
}

v3 e2r_camera_get_up(const E2R_Camera *c)
{
    v3 dir = e2r_camera_get_dir(c);
    v3 world_up = V3(0.0f, 1.0f, 0.0f);
    v3 right = v3_normalize(v3_cross(world_up, dir));
    v3 up = v3_cross(dir, right);
    return up;
}

m4 e2r_camera_get_view(const E2R_Camera *c)
{
    v3 dir = e2r_camera_get_dir(c);
    v3 world_up = V3(0.0f, 1.0f, 0.0f);
    v3 right = v3_normalize(v3_cross(world_up, dir));
    v3 up = v3_cross(dir, right);
    m4 view = m4_look_at(c->pos, v3_add(c->pos, dir), up);
    return view;
}
