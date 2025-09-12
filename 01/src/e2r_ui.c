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
    v4 active_button_bg;
    v2 default_window_size;
    f32 window_offset;
    v4 default_text_color;

    v2 next_widget_pos;
    v2 next_window_pos;

    bool mouse_down;
    bool mouse_clicked;
    bool mouse_released;
    bool mouse_consumed;
    v2 mouse_pos;
    v2 mouse_delta;

} _UI_Ctx;

globvar _UI_Ctx _ui_ctx;

// --------------------------------------

static inline bool _mouse_in_rect(v2 pos, v2 size)
{
    v2 mouse_pos = _ui_ctx.mouse_pos;
    return (mouse_pos.x >= pos.x && mouse_pos.x <= (pos.x + size.x) &&
        mouse_pos.y >= pos.y && mouse_pos.y <= (pos.y + size.y));
}

static inline bool _mouse_clicked()
{
    return !_ui_ctx.mouse_consumed && _ui_ctx.mouse_clicked;
}

static inline bool _mouse_down()
{
    return !_ui_ctx.mouse_consumed && _ui_ctx.mouse_down;
}

static inline bool _mouse_released()
{
    return _ui_ctx.mouse_released;
}

static inline void _consume_mouse()
{
    _ui_ctx.mouse_consumed = true;
}

// --------------------------------------

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
    _ui_ctx.active_button_bg = V4(0.3f, 0.3f, 0.3f, 1.0f);

    _ui_ctx.next_window_pos = V2(_ui_ctx.padding, _ui_ctx.padding);
}

void e2r_ui_process_input()
{
    _ui_ctx.mouse_down = e2r_get_mouse_down();
    _ui_ctx.mouse_clicked = e2r_get_mouse_clicked();
    _ui_ctx.mouse_released = e2r_get_mouse_released();
    _ui_ctx.mouse_consumed = false;
    _ui_ctx.mouse_pos = e2r_get_mouse_pos();
    _ui_ctx.mouse_delta = e2r_get_mouse_delta();
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

        v2 button_size = V2(24.0f, 24.0f);
        v2 button_pos = V2(right_side - button_size.x, window->pos.y + _ui_ctx.padding);

        if (e2r_ui_button_ex(button_pos, button_size, 'X', &window->close_button_pressed))
        {
            window->open = false;
        }

        if (_mouse_in_rect(window->pos, window->size) && _mouse_clicked())
        {
            window->is_dragged = true;
            _consume_mouse();
        }
        else if (_mouse_released())
        {
            window->is_dragged = false;
        }

        if (window->is_dragged)
        {
            window->pos = v2_add(window->pos, _ui_ctx.mouse_delta);
        }

        _ui_ctx.next_widget_pos = V2(window->pos.x + _ui_ctx.padding, window->pos.y + _ui_ctx.padding);
    }

    return window->open;
}

bool e2r_ui_button_ex(v2 pos, v2 size, const char ch, bool *button_active)
{
    bool hovered = _mouse_in_rect(pos, size);
    bool clicked = hovered && _mouse_clicked();
    bool mouse_down = _mouse_down();
    bool released = hovered && _mouse_released();

    if (clicked) _consume_mouse();

    bool button_pressed = false;

    if (clicked) *button_active = true;
    if (*button_active && released) button_pressed = true;

    e2r_ui_draw_button(pos, size, ch, hovered, *button_active);

    if (!mouse_down) *button_active = false;

    return button_pressed;
}

void e2r_ui_draw_button(v2 pos, v2 size, char ch, bool hovered, bool active)
{
    v4 button_color;
    if (hovered)
    {
        if (active) button_color = _ui_ctx.active_button_bg;
        else button_color = _ui_ctx.hovered_button_bg;
    }
    else button_color = _ui_ctx.close_button_bg;

    e2r_draw_quad(pos, size, button_color);

    v2 glyph_pos = e2r_ui_center_glyph_in_box(pos, size, ch);
    const FontAtlas *font_atlas = e2r_get_font_atlas_TEMP();
    e2r_draw_char(ch, &glyph_pos.x, &glyph_pos.y, font_atlas, V4(1.0f, 1.0f, 1.0f, 1.0f));
}

v2 e2r_ui_center_glyph_in_box(v2 pos, v2 size, char ch)
{
    const FontAtlas *font_atlas = e2r_get_font_atlas_TEMP();
    GlyphQuad q = font_loader_get_glyph_quad(font_atlas, ch, 0.0f, 0.0f);
    f32 button_text_x = pos.x + size.x * 0.5f - q.screen_max_x * 0.5f;
    f32 button_text_y = pos.y + size.y * 0.5f - q.screen_min_y * 0.5f - font_loader_get_ascender(font_atlas);
    return V2(button_text_x, button_text_y);
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
