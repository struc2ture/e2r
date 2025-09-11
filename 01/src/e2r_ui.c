#include "e2r_ui.h"

#include "common/print_helpers.h"
#include "common/types.h"
#include "common/util.h"
#include "e2r_core.h"
#include "e2r_draw.h"
#include "font_loader.h"

typedef struct _UI_Ctx
{
    f32 padding;
    v4 default_window_bg;
    v2 default_window_size;
    f32 window_offset;
    v4 default_text_color;

    v2 next_widget_pos;
    v2 next_window_pos;

} _UI_Ctx;

globvar _UI_Ctx _ui_ctx;

void e2r_ui_init()
{
    _ui_ctx.padding = 4.0f;
    _ui_ctx.window_offset = 10.0f;
    _ui_ctx.default_window_bg = V4(0.2f, 0.2f, 0.2f, 1.0f);
    _ui_ctx.default_window_size = V2(400.0f, 200.0f);
    _ui_ctx.next_widget_pos = V2_ZERO;
    _ui_ctx.default_text_color = V4(1.0f, 1.0f, 1.0f, 1.0f);

    _ui_ctx.next_window_pos = V2(_ui_ctx.padding, _ui_ctx.padding);
}

bool e2r_ui_begin_window(E2R_UI_Window *window)
{
    if (!window->initialized)
    {
        window->pos = _ui_ctx.next_window_pos;
        window->size = _ui_ctx.default_window_size;
        window->bg_color = _ui_ctx.default_window_bg;
        window->open = true;
        window->initialized = true;

        _ui_ctx.next_window_pos = V2(window->pos.x + _ui_ctx.window_offset, window->pos.y);
    }

    if (window->open)
    {
        e2r_draw_quad(window->pos, window->size, window->bg_color);
        _ui_ctx.next_widget_pos = V2(window->pos.x + _ui_ctx.padding, window->pos.y + _ui_ctx.padding);
    }

    return window->open;
}

void e2r_ui_draw_text(const char *str)
{
    const FontAtlas *font_atlas = e2r_get_font_atlas_TEMP();
    e2r_draw_string(str, &_ui_ctx.next_widget_pos.x, &_ui_ctx.next_widget_pos.y, font_atlas, _ui_ctx.default_text_color);
}

void e2r_ui_end_window()
{
    _ui_ctx.next_widget_pos = V2(_ui_ctx.padding, _ui_ctx.padding);
}
