#pragma once

#include "common/types.h"
#include "font_loader.h"

void e2r_ui_begin_window(v2 position, v2 size, v4 bg_color);
void e2r_ui_draw_text(const char *str, const FontAtlas *font_atlas, v4 color);
void e2r_ui_end_window();
