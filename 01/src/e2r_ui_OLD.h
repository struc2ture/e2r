#if 0
#pragma once

#include "common/types.h"
#include "font_loader.h"

typedef struct E2R_UI_Window
{
    bool initialized;
    bool open;
    bool close_button_pressed;
    bool is_dragged;

    v2 pos;
    v2 size;
    v4 bg_color;

} E2R_UI_Window;

void e2r_ui_init();
void e2r_ui_process_input();
bool e2r_ui_begin_window(E2R_UI_Window *window);
bool e2r_ui_button_ex(v2 pos, v2 size, const char ch, bool *button_active);
void e2r_ui_draw_button(v2 pos, v2 size, char ch, bool hovered, bool active);
v2 e2r_ui_center_glyph_in_box(v2 pos, v2 size, char ch);
void e2r_ui_draw_text(const char *str);
void e2r_ui_end_window();
#endif