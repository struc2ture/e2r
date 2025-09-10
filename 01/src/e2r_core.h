#pragma once

#include "common/types.h"

void e2r_init(int width, int height, const char *name);
void e2r_destroy();

bool e2r_is_running();
void e2r_start_frame();
void e2r_end_frame();
f32 e2r_get_dt();
void e2r_draw();
