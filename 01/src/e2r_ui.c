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

// TODO: Actual data should live somewhere, not in global state
globvar _UICtx __ui_ctx;
globvar _UICtx *_ui_ctx = &__ui_ctx;

E2R_UI_Window *e2r_ui__create_window(v2 pos, v2 size, v4 bg_color)
{
    E2R_UI_Window *window = xmalloc(sizeof(*window));
    *window = (E2R_UI_Window){
        .pos = pos,
        .size = size,
        .bg_color = bg_color,
        .bullet_lists = (E2R_UI_BulletListList){}
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
    const v4 text_color = V4(1.0f, 1.0f, 1.0f, 1.0f);
    if (bullet_list->bullet_items.size > 0)
    {
        const char **bullet_item;
        list_iterate(&bullet_list->bullet_items, bullet_item_i, bullet_item)
        {
            e2r_draw_line(*bullet_item, pen_x, pen_y, font_atlas, text_color);
        }
    }
    else
    {
        e2r_draw_line("Bullet List", pen_x, pen_y, font_atlas, text_color);
    }
    list_clear(&bullet_list->bullet_items);
}

void e2r_ui__render_window(E2R_UI_Window *window)
{
    const FontAtlas *font_atlas = e2r_get_font_atlas_TEMP();
    e2r_draw_quad(window->pos, window->size, window->bg_color);

    E2R_UI_BulletList *bullet_list;
    f32 pen_x = window->pos.x + 4.0f;
    f32 pen_y = window->pos.y + 4.0f;
    list_iterate(&window->bullet_lists, bullet_list_i, bullet_list)
    {
        e2r_ui__render_bullet_list(&pen_x, &pen_y, bullet_list);
        pen_y += font_loader_get_ascender(font_atlas);
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

