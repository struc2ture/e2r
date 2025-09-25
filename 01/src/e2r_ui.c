#include "e2r_ui.h"

#include <limits.h>

#include "common/lin_math.h"
#include "common/types.h"
#include "common/util.h"
#include "e2r_core.h"
#include "e2r_draw.h"
#include "e2r_input.h"

list_define_type(E2R_UI_WindowList, E2R_UI_Window *);

typedef struct _UICtx
{
    E2R_UI_WindowList window_list;

} _UICtx;

typedef struct _UIStyling
{
    v4 window_bg_color;
    v4 window_border_color;
    v4 window_header_color;
    v4 window_separator_color;
    f32 window_padding;
    f32 window_border_width;
    v4 text_color;
    f32 button_padding;
    v4 button_color;
    v4 button_hovered_color;
    v4 button_active_color;

} _UIStyling;

// TODO: Actual data should live somewhere, not in global state
globvar _UICtx __ui_ctx;
globvar _UICtx *_ui_ctx = &__ui_ctx;
globvar _UIStyling __ui_styling;
globvar _UIStyling *_ui_styling = &__ui_styling;

// =====================================

static inline bool p_in_rect(v2 p, v2 rect_p, v2 rect_size)
{
    v2 min = rect_p;
    v2 max = v2_add(rect_p, rect_size);

    return (p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y);
}

static inline bool clicked_in_rect(v2 rect_p, v2 rect_size)
{
    return e2r_is_mouse_pressed(GLFW_MOUSE_BUTTON_LEFT) &&
        p_in_rect(e2r_get_mouse_pos(), rect_p, rect_size);
}

// =====================================

void _draw_vline(f32 x, f32 y_min, f32 y_max, f32 width, v4 color)
{
    f32 half_width = width * 0.5f;
    v2 min = V2(x - half_width, y_min - half_width);
    v2 size = V2(width, y_max - y_min + width);
    e2r_draw_quad(min, size, color);
}

void _draw_hline(f32 x_min, f32 x_max, f32 y, f32 width, v4 color)
{
    f32 half_width = width * 0.5f;
    v2 min = V2(x_min - half_width, y - half_width);
    v2 size = V2(x_max - x_min + width, width);
    e2r_draw_quad(min, size, color);
}

void _recalculate_window_layout(E2R_UI_Window *window);
void _update_implicit_interactions()
{
    E2R_UI_Window **window_it;
    list_iterate(&_ui_ctx->window_list, window_i, window_it)
    {
        E2R_UI_Window *window = *window_it;

        if (clicked_in_rect(window->pos, window->size))
        {
            window->is_dragged = true;
        }

        if (e2r_is_mouse_released(GLFW_MOUSE_BUTTON_LEFT))
        {
            window->is_dragged = false;
        }

        if (window->is_dragged)
        {
            v2 mouse_delta =e2r_get_mouse_delta();
            window->pos.x += mouse_delta.x;
            window->pos.y += mouse_delta.y;
        }

        _recalculate_window_layout(window);

        E2R_UI_Widget *widget;
        list_iterate(&window->widget_list, widget_i, widget)
        {
            if (widget->kind == E2R_UI_WIDGET_BUTTON)
            {
                widget->button.is_pressed = false;
                widget->button.is_hovered = false;

                if (p_in_rect(e2r_get_mouse_pos(), widget->pos, widget->size))
                {
                    widget->button.is_hovered = true;
                }

                if (widget->button.is_hovered && e2r_is_mouse_pressed(GLFW_MOUSE_BUTTON_LEFT))
                {
                    widget->button.is_active = true;
                }
                if (widget->button.is_active && e2r_is_mouse_released(GLFW_MOUSE_BUTTON_LEFT))
                {
                    widget->button.is_active = false;
                    if (p_in_rect(e2r_get_mouse_pos(), widget->pos, widget->size))
                    {
                        widget->button.is_pressed = true;
                    }
                }
            }
        }
    }
}

void _render_widget(E2R_UI_Widget *w)
{
    const FontAtlas *font_atlas = e2r_get_font_atlas_TEMP();
    f32 pen_x = w->pos.x;
    f32 pen_y = w->pos.y;
    switch (w->kind)
    {
        case E2R_UI_WIDGET_LABEL:
        {
            e2r_draw_line(w->label.text, &pen_x, &pen_y, font_atlas, _ui_styling->text_color);
        }
        break;

        case E2R_UI_WIDGET_BULLET_LIST:
        {
            const FontAtlas *font_atlas = e2r_get_font_atlas_TEMP();
            if (w->bullet_list.bullet_items.size > 0)
            {
                const char **bullet_item;
                list_iterate(&w->bullet_list.bullet_items, bullet_item_i, bullet_item)
                {
                    e2r_draw_line(*bullet_item, &pen_x, &pen_y, font_atlas, _ui_styling->text_color);
                }
            }
            else
            {
                e2r_draw_line("Bullet List", &pen_x, &pen_y, font_atlas, _ui_styling->text_color);
            }
        }
        break;

        case E2R_UI_WIDGET_BUTTON:
        {
            StringRect text_rect = font_loader_get_string_rect(font_atlas, w->button.text);
            f32 text_w = text_rect.max_x - text_rect.min_x;
            f32 text_h = text_rect.max_y - text_rect.min_y;

            v4 color = _ui_styling->button_color;
            if (w->button.is_hovered) color = _ui_styling->button_hovered_color;
            if (w->button.is_active) color = _ui_styling->button_active_color;

            e2r_draw_quad(w->pos, w->size, color);

            f32 text_x = w->pos.x + _ui_styling->button_padding + w->size.x * 0.5f - text_w * 0.5f;
            f32 text_y = w->pos.y - text_rect.min_y + w->size.y * 0.5f - text_h * 0.5f;

            e2r_draw_line(w->button.text, &text_x, &text_y, font_atlas, _ui_styling->text_color);
        }
        break;
    }
}


void _render_window(E2R_UI_Window *window)
{
    const FontAtlas *font_atlas = e2r_get_font_atlas_TEMP();
    e2r_draw_quad(window->pos, window->size, _ui_styling->window_bg_color);
    const v2 window_min = window->pos;
    const v2 window_max = v2_add(window->pos, window->size);
    const f32 window_w = window->size.x;
    const f32 window_h = window->size.y;

    const f32 pad = _ui_styling->window_padding;
    const f32 item_offset = 2 * pad;
    const f32 ascender = font_loader_get_ascender(font_atlas);

    StringRect title_rect = font_loader_get_string_rect(font_atlas, window->title);
    f32 title_w = title_rect.max_x - title_rect.min_x;
    f32 title_h = title_rect.max_y - title_rect.min_y;

    const f32 header_h = pad + title_rect.max_y + pad;
    e2r_draw_quad(window_min, V2(window_w, header_h), _ui_styling->window_header_color);

    const f32 border_width = _ui_styling->window_border_width;
    const v4 border_color = _ui_styling->window_border_color;
    const f32 separator_y = window_min.y + header_h;
    const v4 separator_color = _ui_styling->window_separator_color;
    _draw_hline(window_min.x, window_max.x, separator_y, border_width, separator_color);

    _draw_vline(window_min.x, window_min.y, window_max.y, border_width, border_color);
    _draw_vline(window_max.x, window_min.y, window_max.y, border_width, border_color);
    _draw_hline(window_min.x, window_max.x, window_min.y, border_width, border_color);
    _draw_hline(window_min.x, window_max.x, window_max.y, border_width, border_color);

    f32 title_x = window_min.x + pad + window_w * 0.5f - title_w * 0.5f;
    f32 title_y = window_min.y - title_rect.min_y + header_h * 0.5f - title_h * 0.5f;

    e2r_draw_line(window->title, &title_x, &title_y, font_atlas, _ui_styling->text_color);

    E2R_UI_Widget *widget;
    list_iterate(&window->widget_list, widget_i, widget)
    {
        _render_widget(widget);
    }
}

void _render_windows()
{
    E2R_UI_Window **window_it;
    list_iterate(&_ui_ctx->window_list, window_i, window_it)
    {
        _render_window(*window_it);
    }
}

void _render()
{
    _render_windows();
}

// =====================================

void _recalculate_widget_size(E2R_UI_Widget *w)
{
    const FontAtlas *font_atlas = e2r_get_font_atlas_TEMP();
    switch (w->kind)
    {
        case E2R_UI_WIDGET_LABEL:
        {
            StringRect text_rect = font_loader_get_string_rect(font_atlas, w->label.text);
            w->size = V2(text_rect.max_x, text_rect.max_y);
        }
        break;

        case E2R_UI_WIDGET_BULLET_LIST:
        {
            const char **bullet_item;
            f32 max_x = 0.0f;
            f32 max_y = 0.0f;
            f32 ascender = font_loader_get_ascender(font_atlas);
            if (w->bullet_list.bullet_items.size > 0)
            {
                list_iterate(&w->bullet_list.bullet_items, bullet_item_i, bullet_item)
                {
                    StringRect text_rect = font_loader_get_string_rect(font_atlas, *bullet_item);
                    if (text_rect.max_x > max_x) max_x = text_rect.max_x;
                    max_y += text_rect.max_y;
                }   
            }
            else
            {
                StringRect text_rect = font_loader_get_string_rect(font_atlas, "Bullet List");
                max_x = text_rect.max_x;
                max_y = text_rect.max_y;
            }
            w->size = V2(max_x, max_y);
        }
        break;

        case E2R_UI_WIDGET_BUTTON:
        {
            StringRect text_rect = font_loader_get_string_rect(font_atlas, w->button.text);
            f32 text_w = text_rect.max_x - text_rect.min_x;
            f32 text_h = text_rect.max_y - text_rect.min_y;
            w->size = V2(text_w + 2 * _ui_styling->button_padding, text_h + 2 * _ui_styling->button_padding);
        }
        break;
    }
}

void _recalculate_window_layout(E2R_UI_Window *window)
{
    f32 pen_x = window->pos.x;
    f32 pen_y = window->pos.y;

    pen_x += _ui_styling->window_padding;

    const FontAtlas *font_atlas = e2r_get_font_atlas_TEMP();
    StringRect title_rect = font_loader_get_string_rect(font_atlas, window->title);
    const f32 header_h = _ui_styling->window_padding + title_rect.max_y + _ui_styling->window_padding;
    pen_y += header_h;

    pen_y += _ui_styling->window_padding;

    E2R_UI_Widget *widget;
    list_iterate(&window->widget_list, widget_i, widget)
    {
        widget->pos = V2(pen_x, pen_y);

        _recalculate_widget_size(widget);

        pen_y += widget->size.y + _ui_styling->window_padding;
    }
}

// =====================================

void e2r_ui__init()
{
    _ui_styling->window_bg_color = V4(0.2f, 0.2f, 0.2f, 1.0f);
    _ui_styling->window_border_color = V4(0.4f, 0.4f, 0.4f, 1.0f);
    _ui_styling->window_header_color = V4(0.15f, 0.15f, 0.15f, 1.0f);
    _ui_styling->window_separator_color = V4(0.3f, 0.3f, 0.3f, 1.0f);
    _ui_styling->window_padding = 2.0f;
    _ui_styling->window_border_width = 2.0f;
    _ui_styling->text_color = V4(1.0f, 1.0f, 1.0f, 1.0f);
    _ui_styling->button_padding = 4.0f;
    _ui_styling->button_color = V4(0.6f, 0.6f, 0.6f, 1.0f);
    _ui_styling->button_hovered_color = V4(0.5f, 0.5f, 0.5f, 1.0f);
    _ui_styling->button_active_color = V4(0.4f, 0.4f, 0.4f, 1.0f);
}

void e2r_ui__begin_frame()
{
    _update_implicit_interactions();
}

void e2r_ui__end_frame()
{
    _render();
}

E2R_UI_Window *e2r_ui__create_window(v2 pos, v2 size, const char *title)
{
    E2R_UI_Window *window = xmalloc(sizeof(*window));
    *window = (E2R_UI_Window){
        .pos = pos,
        .size = size,
        .title = xstrdup(title)
    };

    list_append(&_ui_ctx->window_list, window);

    return window;
}

void e2r_ui__destroy_window(E2R_UI_Window *window)
{
    size_t delete_index = UINT_MAX;
    E2R_UI_Window **window_to_delete = NULL;
    E2R_UI_Window **window_it;
    list_iterate(&_ui_ctx->window_list, window_i, window_it)
    {
        if (*window_it == window)
        {
            delete_index = window_i;
            break;
        }
    }
    bassertf(delete_index < _ui_ctx->window_list.size, "Window to delete not found");
    list_erase(&_ui_ctx->window_list, delete_index);
}

// ==========================================

E2R_UI_Widget *e2r_ui__add_label(E2R_UI_Window *window)
{
    E2R_UI_Widget widget = {
        .kind = E2R_UI_WIDGET_LABEL,
        .window = window,
        .label.text = "Label"
    };
    list_append(&window->widget_list, widget);
    _recalculate_window_layout(window);
    return &window->widget_list.data[window->widget_list.size - 1];
}

E2R_UI_Widget *e2r_ui__add_bullet_list(E2R_UI_Window *window)
{
    E2R_UI_Widget widget = {
        .kind = E2R_UI_WIDGET_BULLET_LIST,
        .window = window
    };
    list_append(&window->widget_list, widget);
    return &window->widget_list.data[window->widget_list.size - 1];
}

E2R_UI_Widget *e2r_ui__add_button(E2R_UI_Window *window)
{
    E2R_UI_Widget widget = {
        .kind = E2R_UI_WIDGET_BUTTON,
        .window = window,
        .button.text = "Button"
    };
    list_append(&window->widget_list, widget);
    return &window->widget_list.data[window->widget_list.size - 1];
}

// ==========================================

void e2r_ui__set_label_text(E2R_UI_Widget *w, const char *text)
{
    bassert(w->kind == E2R_UI_WIDGET_LABEL);
    // TODO: This will not work if the underlying str pointer changes
    // Will work for code segment strings for now
    w->label.text = text;
    _recalculate_window_layout(w->window);
}

void e2r_ui__add_bullet_list_item(E2R_UI_Widget *w, const char *item)
{
    bassert(w->kind == E2R_UI_WIDGET_BULLET_LIST);
    // TODO: This will not work if the underlying str pointer changes
    // Will work for code segment strings for now
    list_append(&w->bullet_list.bullet_items, item);
    _recalculate_window_layout(w->window);
}

void e2r_ui__set_button_text(E2R_UI_Widget *w, const char *text)
{
    bassert(w->kind == E2R_UI_WIDGET_BUTTON);
    // TODO: This will not work if the underlying str pointer changes
    // Will work for code segment strings for now
    w->button.text = text;
    _recalculate_window_layout(w->window);
}

// ==========================================

bool e2r_ui__is_button_pressed(E2R_UI_Widget *w)
{
    bassert(w->kind == E2R_UI_WIDGET_BUTTON);
    return w->button.is_pressed;
}
