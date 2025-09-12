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
bool e2r_ui_begin_window(E2R_UI_Window *window);
void e2r_ui_draw_text(const char *str);
void e2r_ui_end_window();
