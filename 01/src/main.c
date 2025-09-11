#include <stdio.h>

#include <GLFW/glfw3.h>

#include "common/lin_math.h"
#include "common/random.h"
#include "common/types.h"
#include "common/util.h"
#include "e2r_camera.h"
#include "e2r_core.h"
#include "e2r_draw.h"
#include "e2r_ui.h"

list_define_type(TransformList, m4);

typedef struct AppCtx
{
    E2R_Camera camera;

    bool mouse_captured;
    bool mouse_capture_toggle_old_press;

    f64 last_mouse_x, last_mouse_y;
    f64 mouse_dx_smoothed, mouse_dy_smoothed;
    bool first_mouse;

    TransformList transform_list;

    int current_light_color;
    f32 light_color_timer;
    v3 light_color;
    v3 light_pos;
    f32 light_orbit_angle;

} AppCtx;

globvar AppCtx app_ctx;

void check_input()
{
    f32 delta = e2r_get_dt();

    GLFWwindow *window = e2r_get_glfw_window_TEMP();

    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !app_ctx.mouse_capture_toggle_old_press)
    {
        app_ctx.mouse_captured = !app_ctx.mouse_captured;
        app_ctx.first_mouse = true;
        glfwSetInputMode(window, GLFW_CURSOR, app_ctx.mouse_captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        app_ctx.mouse_capture_toggle_old_press = true;
    }
    else if (glfwGetKey(window, GLFW_KEY_C) != GLFW_PRESS)
    {
        app_ctx.mouse_capture_toggle_old_press = false;
    }

    // Update camera based on mouse
    if (app_ctx.mouse_captured)
    {
        f64 mouse_x, mouse_y;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);

        if (app_ctx.first_mouse)
        {
            app_ctx.last_mouse_x = mouse_x;
            app_ctx.last_mouse_y = mouse_y;
            app_ctx.first_mouse = false;
        }

        f64 mouse_dx = mouse_x - app_ctx.last_mouse_x;
        f64 mouse_dy = mouse_y - app_ctx.last_mouse_y;
        app_ctx.last_mouse_x = mouse_x;
        app_ctx.last_mouse_y = mouse_y;

        const f64 factor = 0.3;
        app_ctx.mouse_dx_smoothed = factor * app_ctx.mouse_dx_smoothed + (1.0 - factor) * mouse_dx;
        app_ctx.mouse_dy_smoothed = factor * app_ctx.mouse_dy_smoothed + (1.0 - factor) * mouse_dy;

        f32 mouse_sens = 0.2f;
        app_ctx.camera.pitch_deg -= mouse_sens * app_ctx.mouse_dy_smoothed;
        app_ctx.camera.yaw_deg += mouse_sens * app_ctx.mouse_dx_smoothed;
        if (app_ctx.camera.pitch_deg > 89.9f) app_ctx.camera.pitch_deg = 89.9f;
        else if (app_ctx.camera.pitch_deg < -89.9f) app_ctx.camera.pitch_deg = -89.9f;
    }

    // Update camera based on keyboard
    {
        f32 speed = 3.0f;
        v3 dir = e2r_camera_get_dir(&app_ctx.camera);
        v3 right = e2r_camera_get_right(&app_ctx.camera);
        v3 up = e2r_camera_get_up(&app_ctx.camera);
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) app_ctx.camera.pos = v3_add(app_ctx.camera.pos, v3_scale(dir, speed * delta));
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) app_ctx.camera.pos = v3_add(app_ctx.camera.pos, v3_scale(dir, -speed * delta));
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) app_ctx.camera.pos = v3_add(app_ctx.camera.pos, v3_scale(right, speed * delta));
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) app_ctx.camera.pos = v3_add(app_ctx.camera.pos, v3_scale(right, -speed * delta));
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) app_ctx.camera.pos = v3_add(app_ctx.camera.pos, v3_scale(up, speed * delta));
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) app_ctx.camera.pos = v3_add(app_ctx.camera.pos, v3_scale(up, -speed * delta));
    }
}

int main()
{
    e2r_init(1000, 900, "E2R!!!");
    e2r_ui_init();

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
    app_ctx.first_mouse = true;

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

    E2R_UI_Window window = {};
    E2R_UI_Window window2 = {};

    while (e2r_is_running())
    {
        e2r_start_frame();

        check_input();

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

        e2r_draw_quad(V2(offset + 000.0f, offset + 000.0f), V2(50.0f, 50.0f), V4(1.0f, 0.0f, 0.0f, 1.0f));
        e2r_draw_circle(V2(offset + 050.0f, offset + 050.0f), V2(50.0f, 50.0f), V4(0.0f, 1.0f, 0.0f, 1.0f));
        e2r_draw_quad(V2(offset + 100.0f, offset + 100.0f), V2(50.0f, 50.0f), V4(0.0f, 0.0f, 1.0f, 1.0f));

        f32 pen_x = 600.0f;
        f32 pen_y = 600.0f;
        e2r_draw_string("Hello, world!\n", &pen_x, &pen_y, e2r_get_font_atlas_TEMP(), V4(1.0f, 1.0f, 1.0f, 1.0f));

        e2r_draw_string("YESSSS!", &pen_x, &pen_y, e2r_get_font_atlas_TEMP(), V4(1.0f, 1.0f, 0.0f, 1.0f));

        if (e2r_ui_begin_window(&window))
        {
            e2r_ui_draw_text("Line 1\n");
            e2r_ui_draw_text("Line 2\n");
            e2r_ui_draw_text("Line 3\n");
            e2r_ui_end_window();
        }
        
        window.pos.x += dt * speed;

        // if (e2r_ui_begin_window(&window2))
        // {
        //     e2r_ui_draw_text("Line a\n");
        //     e2r_ui_draw_text("Line b\n");
        //     e2r_ui_draw_text("Line c\n");
        //     e2r_ui_end_window();
        // }

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
