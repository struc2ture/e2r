#include "e2r_ui.h"

#include "common/types.h"
#include "common/lin_math.h"
#include "common/util.h"
#include "e2r_core.h"
#include "e2r_draw.h"
#include "font_loader.h"

typedef struct _UI_Ctx
{
    f32 padding;
    v4 default_window_bg;
    v4 close_button_bg;
    v4 hovered_button_bg;
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
    _ui_ctx.close_button_bg = V4(0.5f, 0.5f, 0.5f, 1.0f);
    _ui_ctx.hovered_button_bg = V4(0.7f, 0.7f, 0.7f, 1.0f);

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
        window->close_button_pressed = false;

        _ui_ctx.next_window_pos = V2(window->pos.x + _ui_ctx.window_offset, window->pos.y);
    }

    if (window->open)
    {
        e2r_draw_quad(window->pos, window->size, window->bg_color);
        
        f32 right_side = window->pos.x + window->size.x - _ui_ctx.padding;

        v2 button_size = V2(30.0f, 30.0f);
        v2 button_pos = V2(right_side - button_size.x, window->pos.y + _ui_ctx.padding);

        const FontAtlas *font_atlas = e2r_get_font_atlas_TEMP();
        GlyphQuad q = font_loader_get_glyph_quad(font_atlas, 'X', 0.0f, 0.0f);
        f32 button_text_x = button_pos.x + button_size.x * 0.5f - q.screen_max_x * 0.5f;
        f32 button_text_y = button_pos.y + button_size.y * 0.5f - q.screen_min_y * 0.5f - font_loader_get_ascender(font_atlas);

        v4 button_color = _ui_ctx.close_button_bg;
        v2 mouse_pos = e2r_get_mouse_pos();
        if (mouse_pos.x >= button_pos.x && mouse_pos.x <= (button_pos.x + button_size.x) &&
            mouse_pos.y >= button_pos.y && mouse_pos.y <= (button_pos.y + button_size.y))
        {
            button_color = _ui_ctx.hovered_button_bg;

            if (e2r_get_mouse_clicked())
            {
                window->close_button_pressed = true;
            }

            if (window->close_button_pressed && e2r_get_mouse_released())
            {
                window->open = false;
            }
        }
        else
        {
            window->close_button_pressed = false;
            if (mouse_pos.x >= window->pos.x && mouse_pos.x <= (window->pos.x + window->size.x) &&
                mouse_pos.y >= window->pos.y && mouse_pos.y <= (window->pos.y + window->size.y))
            {
                if (e2r_get_mouse_clicked())
                {
                    window->is_dragged = true;
                }
            }
            if (e2r_get_mouse_released())
            {
                window->is_dragged = false;
            }
        }

        if (window->is_dragged)
        {
            window->pos = v2_add(window->pos, e2r_get_mouse_delta());
        }

        e2r_draw_quad(button_pos, button_size, button_color);
        e2r_draw_char('X', &button_text_x, &button_text_y, font_atlas, V4(1.0f, 1.0f, 1.0f, 1.0f));

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
