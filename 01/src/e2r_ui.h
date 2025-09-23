#pragma once

#include "common/types.h"
#include "common/util.h"

list_define_type(E2R_UI_BulletItemList, const char *);

typedef struct E2R_UI_BulletList
{
    E2R_UI_BulletItemList bullet_items;

} E2R_UI_BulletList;

list_define_type(E2R_UI_BulletListList, E2R_UI_BulletList);

typedef struct E2R_UI_Button
{
    v2 size;
    v2 pos;

    const char *text;
    bool is_active;

} E2R_UI_Button;

list_define_type(E2R_UI_ButtonList, E2R_UI_Button);

typedef struct E2R_UI_Window
{
    v2 size;
    v2 pos;
    const char *title;

    E2R_UI_BulletListList bullet_lists;
    E2R_UI_ButtonList buttons;

    bool is_dragged;

} E2R_UI_Window;

void e2r_ui__init();

void e2r_ui__begin_frame();
void e2r_ui__end_frame();

E2R_UI_Window *e2r_ui__create_window(v2 pos, v2 size, const char *title);
void e2r_ui__destroy_window(E2R_UI_Window *window);

E2R_UI_BulletList *e2r_ui__add_bullet_list(E2R_UI_Window *window);
void e2r_ui__submit_bullet_list_item(E2R_UI_BulletList *bullet_list, const char *item);

E2R_UI_Button *e2r_ui__add_button(E2R_UI_Window *window, v2 pos, v2 size);
void e2r_ui__set_button_text(E2R_UI_Button *button, const char *text);
bool e2r_ui__is_button_pressed(E2R_UI_Button *button);
