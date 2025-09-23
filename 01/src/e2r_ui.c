#include "e2r_ui.h"

#include <limits.h>

#include "common/lin_math.h"
#include "common/types.h"
#include "common/util.h"
#include "e2r_core.h"
#include "e2r_draw.h"

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

} _UIStyling;

// TODO: Actual data should live somewhere, not in global state
globvar _UICtx __ui_ctx;
globvar _UICtx *_ui_ctx = &__ui_ctx;
globvar _UIStyling __ui_styling;
globvar _UIStyling *_ui_styling = &__ui_styling;

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
}

E2R_UI_Window *e2r_ui__create_window(v2 pos, v2 size, const char *title)
{
    E2R_UI_Window *window = xmalloc(sizeof(*window));
    *window = (E2R_UI_Window){
        .pos = pos,
        .size = size,
        .bullet_lists = (E2R_UI_BulletListList){},
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

void e2r_ui__render_bullet_list(f32 *pen_x, f32 *pen_y, E2R_UI_BulletList *bullet_list)
{
    const FontAtlas *font_atlas = e2r_get_font_atlas_TEMP();
    if (bullet_list->bullet_items.size > 0)
    {
        const char **bullet_item;
        list_iterate(&bullet_list->bullet_items, bullet_item_i, bullet_item)
        {
            e2r_draw_line(*bullet_item, pen_x, pen_y, font_atlas, _ui_styling->text_color);
        }
    }
    else
    {
        e2r_draw_line("Bullet List", pen_x, pen_y, font_atlas, _ui_styling->text_color);
    }
    list_clear(&bullet_list->bullet_items);
}

void e2r_ui__render_window(E2R_UI_Window *window)
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
    e2r_draw_quad(window->pos, V2(window_w, header_h), _ui_styling->window_header_color);

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

    E2R_UI_BulletList *bullet_list;
    f32 pen_x = window_min.x + pad;
    f32 pen_y = separator_y + pad;
    list_iterate(&window->bullet_lists, bullet_list_i, bullet_list)
    {
        e2r_ui__render_bullet_list(&pen_x, &pen_y, bullet_list);
        pen_y += item_offset;
    }
}

void e2r_ui__render_windows()
{
    E2R_UI_Window **window_it;
    list_iterate(&_ui_ctx->window_list, window_i, window_it)
    {
        e2r_ui__render_window(*window_it);
    }
}

E2R_UI_BulletList *e2r_ui__add_bullet_list(E2R_UI_Window *window)
{
    E2R_UI_BulletList bullet_list = {};
    list_append(&window->bullet_lists, bullet_list);
    return &window->bullet_lists.data[window->bullet_lists.size - 1];
}

void e2r_ui__submit_bullet_list_item(E2R_UI_BulletList *bullet_list, const char *item)
{
    // TODO: This will not work if the underlying str pointer changes
    // Will work for code segment strings for now
    list_append(&bullet_list->bullet_items, item);
}

