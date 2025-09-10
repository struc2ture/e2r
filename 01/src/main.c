#include <stdio.h>

#include "e2r_core.h"
#include "e2r_draw.h"
#include "common/lin_math.h"
#include "common/random.h"
#include "common/types.h"
#include "common/util.h"

list_define_type(TransformList, m4);

int main()
{
    e2r_init(1000, 900, "E2R!!!");

    f32 offset = 100.0f;

    TransformList transform_list = {};
    const int cube_count = 32;
    for (int i = 0; i < cube_count; i++)
    {
        f32 rand_x = rand_float() * 3.0f - 1.5f;
        f32 rand_y = rand_float() * 3.0f - 1.5f;
        f32 rand_z = rand_float() * 3.0f - 1.5f;
        m4 translate = m4_translate(rand_x, rand_y, rand_z);
        f32 rand_angle = rand_float() * 360.0f;
        m4 rotate = m4_rotate(deg_to_rad(rand_angle), rand_v3(1.0f));
        m4 model = m4_mul(translate, rotate);
        list_append(&transform_list, model);
    }

    while (e2r_is_running())
    {
        e2r_start_frame();

        e2r_controls_TEMP();

        const f32 speed = 50.0f;
        f32 dt = e2r_get_dt();

        offset += dt * speed;

        e2r_draw_quad(V2(offset + 000.0f, offset + 000.0f), V2(50.0f, 50.0f), V4(1.0f, 0.0f, 0.0f, 1.0f));
        e2r_draw_circle(V2(offset + 050.0f, offset + 050.0f), V2(50.0f, 50.0f), V4(0.0f, 1.0f, 0.0f, 1.0f));
        e2r_draw_quad(V2(offset + 100.0f, offset + 100.0f), V2(50.0f, 50.0f), V4(0.0f, 0.0f, 1.0f, 1.0f));

        f32 pen_x = 600.0f;
        f32 pen_y = 600.0f;
        e2r_draw_string("Hello, world!\n", &pen_x, &pen_y, e2r_get_font_atlas_TEMP(), V4(1.0f, 1.0f, 1.0f, 1.0f));

        e2r_draw_string("YESSSS!", &pen_x, &pen_y, e2r_get_font_atlas_TEMP(), V4(1.0f, 1.0f, 0.0f, 1.0f));

        m4 *transform;
        list_iterate(&transform_list, i, transform)
        {
            e2r_draw_cube(*transform);
        }

        e2r_end_frame();
    }

    e2r_destroy();

    return 0;
}
