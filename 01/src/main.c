#include <stdio.h>

#include "e2r_core.h"
#include "e2r_draw.h"
#include "common/types.h"
#include "common/util.h"

int main()
{
    e2r_init(1000, 900, "E2R!!!");

    f32 offset = 100.0f;

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

        e2r_end_frame();
    }

    e2r_destroy();

    return 0;
}
