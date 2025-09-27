#pragma once

#include "common/types.h"
#include "common/util.h"

#define TEXT_INPUT_BUF_SIZE 256

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
    bool is_pressed;

} E2R_UI_Button;

typedef struct
{
    char text_buf[TEXT_INPUT_BUF_SIZE];
    int text_size;
    int current_pos;
    bool is_active;

} E2R_UI_TextInput;

typedef enum
{
    E2R_UI_WIDGET_LABEL,
    E2R_UI_WIDGET_BULLET_LIST,
    E2R_UI_WIDGET_BUTTON,
    E2R_UI_WIDGET_TEXT_INPUT

} E2R_UI_WidgetKind;

struct E2R_UI_Window;
typedef struct
{
    E2R_UI_WidgetKind kind;
    v2 pos;
    v2 size;
    struct E2R_UI_Window *window;
    bool is_hovered;
    union
    {
        E2R_UI_Label label;
        E2R_UI_BulletList bullet_list;
        E2R_UI_Button button;
        E2R_UI_TextInput text_input;
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
    bool is_visible;

} E2R_UI_Window;

void e2r_ui__init(bool enable_debug);

void e2r_ui__begin_frame();
void e2r_ui__end_frame();

E2R_UI_Window *e2r_ui__create_window(v2 pos, v2 size, const char *title);
void e2r_ui__destroy_window(E2R_UI_Window *window);

E2R_UI_Widget *e2r_ui__add_label(E2R_UI_Window *window);
E2R_UI_Widget *e2r_ui__add_bullet_list(E2R_UI_Window *window);
E2R_UI_Widget *e2r_ui__add_button(E2R_UI_Window *window);
E2R_UI_Widget *e2r_ui__add_text_input(E2R_UI_Window *window);

void e2r_ui__toggle_window_visibility(E2R_UI_Window *window);
void e2r_ui__set_label_text(E2R_UI_Widget *w, const char *text);
void e2r_ui__add_bullet_list_item(E2R_UI_Widget *w, const char *item);
void e2r_ui__set_button_text(E2R_UI_Widget *w, const char *text);

bool e2r_ui__is_button_pressed(E2R_UI_Widget *w);
