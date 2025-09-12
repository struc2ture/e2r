#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "common/types.h"
#include "font_loader.h"


void e2r_init(int width, int height, const char *name);
void e2r_destroy();

bool e2r_is_running();
GLFWwindow *e2r_get_glfw_window_TEMP();
void e2r_start_frame();
void e2r_end_frame();
f32 e2r_get_dt();
void e2r_set_view_data(m4 view, v3 view_pos);
void e2r_set_light_data(
    f32 ambient_strength,
    v3 color,
    f32 specular_strength,
    v3 pos,
    f32 shininess);
v2 e2r_get_mouse_pos();
bool e2r_get_mouse_down();
bool e2r_get_mouse_clicked();
bool e2r_get_mouse_released();
v2 e2r_get_mouse_delta();
const FontAtlas *e2r_get_font_atlas_TEMP();
