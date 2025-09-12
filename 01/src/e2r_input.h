#pragma once

#include "common/types.h"

void e2r_update_state(void *window);
bool e2r_is_key_down(int key);
bool e2r_is_key_pressed(int key);
bool e2r_is_key_released(int key);
v2 e2r_get_mouse_pos();
v2 e2r_get_mouse_delta();
v2 e2r_get_mouse_delta_smooth();
bool e2r_is_mouse_down(int button);
bool e2r_is_mouse_pressed(int button);
bool e2r_is_mouse_released(int button);
void e2r_toggle_mouse_capture();
bool e2r_is_mouse_captured();
