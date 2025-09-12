#include "e2r_input.h"

#include <GLFW/glfw3.h>

#include "common/lin_math.h"
#include "common/types.h"
#include "common/util.h"
#include "e2r_core.h"

typedef struct _InputCtx
{
    bool current_key_states[GLFW_KEY_LAST];
    bool previous_key_states[GLFW_KEY_LAST];
    bool current_mouse_button_states[GLFW_MOUSE_BUTTON_8];
    bool previous_mouse_button_states[GLFW_MOUSE_BUTTON_8];

    v2 current_mouse_pos;
    v2 previous_mouse_pos;
    v2 mouse_delta;
    v2 mouse_delta_smooth;
    bool prev_mouse_valid;

    bool mouse_captured;

} _InputCtx;

globvar _InputCtx __input_ctx;
globvar _InputCtx *_input_ctx = &__input_ctx;

void e2r_update_state(void *window)
{
    for (int key = 0; key < GLFW_KEY_LAST; key++)
    {
        _input_ctx->previous_key_states[key] = _input_ctx->current_key_states[key];
        _input_ctx->current_key_states[key] = (glfwGetKey(window, key) == GLFW_PRESS);
    }

    for (int mouse_button = 0; mouse_button < GLFW_MOUSE_BUTTON_8; mouse_button++)
    {
        _input_ctx->previous_mouse_button_states[mouse_button] = _input_ctx->current_mouse_button_states[mouse_button];
        _input_ctx->current_mouse_button_states[mouse_button] = (glfwGetMouseButton(window, mouse_button) == GLFW_PRESS);
    }

    f64 mouse_x, mouse_y;
    glfwGetCursorPos(window, &mouse_x, &mouse_y);
    _input_ctx->current_mouse_pos = V2(mouse_x, mouse_y);

    if (!_input_ctx->prev_mouse_valid)
    {
        _input_ctx->previous_mouse_pos = _input_ctx->current_mouse_pos;
        _input_ctx->prev_mouse_valid = true;
    }
    
    _input_ctx->mouse_delta = v2_sub(_input_ctx->current_mouse_pos, _input_ctx->previous_mouse_pos);

    _input_ctx->previous_mouse_pos = _input_ctx->current_mouse_pos;

    const f32 smooth_factor = 0.5f;
    _input_ctx->mouse_delta_smooth.x =
        smooth_factor * _input_ctx->mouse_delta_smooth.x +
        (1.0f - smooth_factor) * _input_ctx->mouse_delta.x;
    _input_ctx->mouse_delta_smooth.y =
        smooth_factor * _input_ctx->mouse_delta_smooth.y +
        (1.0f - smooth_factor) * _input_ctx->mouse_delta.y;

}

bool e2r_is_key_down(int key)
{
    bassert(key < GLFW_KEY_LAST);
    return _input_ctx->current_key_states[key];
}

bool e2r_is_key_pressed(int key)
{
    bassert(key < GLFW_KEY_LAST);
    return _input_ctx->current_key_states[key] && !_input_ctx->previous_key_states[key];
}

bool e2r_is_key_released(int key)
{
    bassert(key < GLFW_KEY_LAST);
    return !_input_ctx->current_key_states[key] && _input_ctx->previous_key_states[key];
}

v2 e2r_get_mouse_pos()
{
    return _input_ctx->current_mouse_pos;
}

v2 e2r_get_mouse_delta()
{
    return _input_ctx->mouse_delta;
}

v2 e2r_get_mouse_delta_smooth()
{
    return _input_ctx->mouse_delta_smooth;
}


bool e2r_is_mouse_down(int button)
{
    return _input_ctx->current_mouse_button_states[button];
}

bool e2r_is_mouse_pressed(int button)
{
    return _input_ctx->current_mouse_button_states[button] && !_input_ctx->previous_mouse_button_states[button];
}

bool e2r_is_mouse_released(int button)
{
    return !_input_ctx->current_mouse_button_states[button] && _input_ctx->previous_mouse_button_states[button];
}

void e2r_toggle_mouse_capture()
{
    _input_ctx->mouse_captured = !_input_ctx->mouse_captured;
    _input_ctx->prev_mouse_valid = false;

    GLFWwindow *window = e2r_get_glfw_window_TEMP();
    glfwSetInputMode(window, GLFW_CURSOR, _input_ctx->mouse_captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

bool e2r_is_mouse_captured()
{
    return _input_ctx->mouse_captured;
}
