#pragma once

#include "common/types.h"
#include "common/util.h"

list_define_type(E2R_UI_BulletItemList, const char *);

typedef struct E2R_UI_BulletList
{
    E2R_UI_BulletItemList bullet_items;
} E2R_UI_BulletList;

list_define_type(E2R_UI_BulletListList, E2R_UI_BulletList);

typedef struct E2R_UI_Window
{
    v2 size;
    v2 pos;
    v4 bg_color;

    E2R_UI_BulletListList bullet_lists;

} E2R_UI_Window;

E2R_UI_Window *e2r_ui__create_window(v2 pos, v2 size, v4 bg_color);
void e2r_ui__destroy_window(E2R_UI_Window *window);

void e2r_ui__render_bullet_list(f32 *pen_x, f32 *pen_y, E2R_UI_BulletList *bullet_list);
void e2r_ui__render_window(E2R_UI_Window *window);
void e2r_ui__render_windows();

E2R_UI_BulletList *e2r_ui__add_bullet_list(E2R_UI_Window *window);
void e2r_ui__submit_bullet_list_item(E2R_UI_BulletList *bullet_list, const char *item);
