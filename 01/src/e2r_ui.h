#pragma once

#include "common/types.h"
#include "common/util.h"


typedef struct
{
    const char *text;
} E2R_UI_Label;

list_define_type(E2R_UI_BulletItemList, const char *);

typedef struct
{
    E2R_UI_BulletItemList bullet_items;

} E2R_UI_BulletList;

typedef struct
{
    const char *text;
    bool is_active;

} E2R_UI_Button;

typedef enum {
    E2R_UI_WIDGET_LABEL,
    E2R_UI_WIDGET_BULLET_LIST,
    E2R_UI_WIDGET_BUTTON,

} E2R_UI_WidgetKind;

typedef struct
{
    E2R_UI_WidgetKind kind;
    v2 pos;
    v2 size;
    union
    {
        E2R_UI_Label label;
        E2R_UI_BulletList bullet_list;
        E2R_UI_Button button;
    };

} E2R_UI_Widget;

list_define_type(E2R_UI_WidgetList, E2R_UI_Widget);

typedef struct E2R_UI_Window
{
    v2 pos;
    v2 size;
    const char *title;

    E2R_UI_WidgetList widget_list;

    bool is_dragged;

} E2R_UI_Window;

void e2r_ui__init();

void e2r_ui__begin_frame();
void e2r_ui__end_frame();

E2R_UI_Window *e2r_ui__create_window(v2 pos, v2 size, const char *title);
void e2r_ui__destroy_window(E2R_UI_Window *window);

E2R_UI_Widget *e2r_ui__add_label(E2R_UI_Window *window);
E2R_UI_Widget *e2r_ui__add_bullet_list(E2R_UI_Window *window);
E2R_UI_Widget *e2r_ui__add_button(E2R_UI_Window *window);

void e2r_ui__add_bullet_list_item(E2R_UI_Widget *w, const char *item);
void e2r_ui__set_button_text(E2R_UI_Widget *w, const char *text);
