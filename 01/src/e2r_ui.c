#include "e2r_ui.h"

#include "common/types.h"
#include "e2r_draw.h"
#include "font_loader.h"

const f32 window_padding = 4.0f;

typedef struct _UI_Ctx
{
    v2 pen;

} _UI_Ctx;

globvar _UI_Ctx _ui_ctx;

void e2r_ui_begin_window(v2 position, v2 size, v4 bg_color)
{
    e2r_draw_quad(position, size, bg_color);

    _ui_ctx.pen = V2(position.x + window_padding, position.y + window_padding);
}

void e2r_ui_draw_text(const char *str, const FontAtlas *font_atlas, v4 color)
{
    e2r_draw_string(str, &_ui_ctx.pen.x, &_ui_ctx.pen.y, font_atlas, color);
}

void e2r_ui_end_window()
{
    _ui_ctx.pen = V2(window_padding, window_padding);
}
