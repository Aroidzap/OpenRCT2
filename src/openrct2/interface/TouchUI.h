#pragma once
#include "Window.h"

class touch_ui_offset {
public:
    sint16 left, right, top, bottom;
    bool spinner_button;
    touch_ui_offset(rct_widget *widget);
    touch_ui_offset(rct_window *window);
};