#include <stdio.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "common/lin_math.h"
#include "common/random.h"
#include "common/types.h"
#include "common/util.h"
#include "e2r_camera.h"
#include "e2r_core.h"
#include "e2r_draw.h"
#include "e2r_input.h"
#include "e2r_ui.h"

list_define_type(TransformList, m4);

typedef struct AppCtx
{
    E2R_Camera camera;

    TransformList transform_list;

    int current_light_color;
    f32 light_color_timer;
    v3 light_color;
    v3 light_pos;
    f32 light_orbit_angle;

} AppCtx;

globvar AppCtx app_ctx;

void process_3d_scene_inputs()
{
    f32 delta = e2r_get_dt();

    if (e2r_is_key_pressed(GLFW_KEY_C))
    {
        e2r_toggle_mouse_capture();
    }

    // Update camera based on mouse
    if (e2r_is_mouse_captured())
    {
        f32 mouse_sens = 0.2f;
        v2 mouse_delta = e2r_get_mouse_delta_smooth();
        app_ctx.camera.pitch_deg -= mouse_sens * mouse_delta.y;
        app_ctx.camera.yaw_deg += mouse_sens * mouse_delta.x;
        if (app_ctx.camera.pitch_deg > 89.9f) app_ctx.camera.pitch_deg = 89.9f;
        else if (app_ctx.camera.pitch_deg < -89.9f) app_ctx.camera.pitch_deg = -89.9f;
    }

    // Update camera based on keyboard
    {
        f32 speed = 3.0f;
        v3 dir = e2r_camera_get_dir(&app_ctx.camera);
        v3 right = e2r_camera_get_right(&app_ctx.camera);
        v3 up = e2r_camera_get_up(&app_ctx.camera);
        if (e2r_is_key_down(GLFW_KEY_W)) app_ctx.camera.pos = v3_add(app_ctx.camera.pos, v3_scale(dir, speed * delta));
        if (e2r_is_key_down(GLFW_KEY_S)) app_ctx.camera.pos = v3_add(app_ctx.camera.pos, v3_scale(dir, -speed * delta));
        if (e2r_is_key_down(GLFW_KEY_A)) app_ctx.camera.pos = v3_add(app_ctx.camera.pos, v3_scale(right, speed * delta));
        if (e2r_is_key_down(GLFW_KEY_D)) app_ctx.camera.pos = v3_add(app_ctx.camera.pos, v3_scale(right, -speed * delta));
        if (e2r_is_key_down(GLFW_KEY_SPACE)) app_ctx.camera.pos = v3_add(app_ctx.camera.pos, v3_scale(up, speed * delta));
        if (e2r_is_key_down(GLFW_KEY_LEFT_SHIFT)) app_ctx.camera.pos = v3_add(app_ctx.camera.pos, v3_scale(up, -speed * delta));
    }
}

int main()
{
    e2r_init(1000, 900, "E2R!!!");

    f32 offset = 100.0f;

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
        list_append(&app_ctx.transform_list, model);
    }

    app_ctx.camera = e2r_camera_set_from_pos_target(V3(0.0f, 0.0f, 5.0f), V3(0.0f, 0.0f, 0.0f));

    v3 light_colors[] =
    {
        V3(1.0f, 0.0f, 0.0f),
        V3(0.0f, 1.0f, 0.0f),
        V3(0.0f, 0.0f, 1.0f),
        V3(1.0f, 1.0f, 0.0f),
        V3(0.0f, 1.0f, 1.0f),
        V3(1.0f, 0.0f, 1.0f),
        V3(0.5f, 0.5f, 0.5f),
        V3(1.0f, 1.0f, 1.0f)
    };

    app_ctx.light_pos = V3(10.0f, 0.0f, 0.0f);

    e2r_ui__init();

    // E2R_UI_Window *window2 = e2r_ui__create_window(V2(300.0f, 300.0f), V2(200.0f, 200.0f));
    E2R_UI_Window *window1 = e2r_ui__create_window(V2(100.0f, 100.0f), V2(300.0f, 300.0f), "Hello world!");
    E2R_UI_BulletList *bullet_list1 = e2r_ui__add_bullet_list(window1);
    E2R_UI_BulletList *bullet_list2 = e2r_ui__add_bullet_list(window1);
    // E2R_UI_BulletList *bullet_list3 = e2r_ui__add_bullet_list(window2);
    E2R_UI_Button *button = e2r_ui__add_button(window1, V2(310.0f, 350.0f), V2(50.0f, 50.0f));
    bool show_bullet_items = true;

    while (e2r_is_running())
    {
        e2r_start_frame();
        e2r_ui__begin_frame();

        process_3d_scene_inputs();

        const f32 delta = e2r_get_dt();

        m4 camera_view = e2r_camera_get_view(&app_ctx.camera);
        e2r_set_view_data(camera_view, app_ctx.camera.pos);

        app_ctx.light_color_timer += delta;
        if (app_ctx.light_color_timer >= 1.0f)
        {
            app_ctx.light_color_timer -= 1.0f;
            app_ctx.current_light_color = (app_ctx.current_light_color + 1) % array_count(light_colors);
        }

        app_ctx.light_orbit_angle += delta * 0.2f;
        const f32 radius = 10.0f;
        app_ctx.light_pos = V3(
            cosf(app_ctx.light_orbit_angle) * radius,
            sinf(app_ctx.light_orbit_angle) * radius,
            0.0f
        );

        e2r_set_light_data(
            0.05f,
            light_colors[app_ctx.current_light_color],
            0.5f,
            app_ctx.light_pos,
            1024.0f
        );

        const f32 speed = 50.0f;
        f32 dt = e2r_get_dt();

        offset += dt * speed;

        f32 pen_x = 600.0f;
        f32 pen_y = 600.0f;
        e2r_draw_string("Hello, world!\n", &pen_x, &pen_y, e2r_get_font_atlas_TEMP(), V4(1.0f, 1.0f, 1.0f, 1.0f));

        e2r_draw_string("YESSSS!", &pen_x, &pen_y, e2r_get_font_atlas_TEMP(), V4(1.0f, 1.0f, 0.0f, 1.0f));

        e2r_draw_quad(V2(offset + 000.0f, offset + 000.0f), V2(50.0f, 50.0f), V4(1.0f, 0.0f, 0.0f, 1.0f));
        e2r_draw_circle(V2(offset + 050.0f, offset + 050.0f), V2(50.0f, 50.0f), V4(0.0f, 1.0f, 0.0f, 1.0f));
        e2r_draw_quad(V2(offset + 100.0f, offset + 100.0f), V2(50.0f, 50.0f), V4(0.0f, 0.0f, 1.0f, 1.0f));

        if (e2r_is_key_pressed(GLFW_KEY_B))
        {
            show_bullet_items = !show_bullet_items;
        }

        if (show_bullet_items)
        {
            e2r_ui__submit_bullet_list_item(bullet_list1, "Hello 1");
            e2r_ui__submit_bullet_list_item(bullet_list1, "Hello 2");
        }

        if (e2r_ui__is_button_pressed(button))
        {
            trace("button pressed");
        }
        
        e2r_ui__end_frame();

        m4 *transform;
        list_iterate(&app_ctx.transform_list, i, transform)
        {
            e2r_draw_cube(*transform);
        }

        e2r_end_frame();
    }

    e2r_destroy();

    return 0;
}
