#pragma once

#include "common/lin_math.h"
#include "common/types.h"

typedef struct E2R_Camera
{
    v3 pos;
    f32 pitch_deg;
    f32 yaw_deg;

} E2R_Camera;

E2R_Camera e2r_camera_set_from_pos_target(v3 pos, v3 target);
v3 e2r_camera_get_dir(const E2R_Camera *c);
v3 e2r_camera_get_right(const E2R_Camera *c);
v3 e2r_camera_get_up(const E2R_Camera *c);
m4 e2r_camera_get_view(const E2R_Camera *c);
