#include "TouchUI.h"
#include "Widget.h"
#include "../localisation/StringIds.h"

static int workaround = 0;

touch_ui_offset::touch_ui_offset(rct_window * window) : left(0), right(0), top(0), bottom(0),
    spinner_button(false)
{
    top = -10 + 10 * workaround; // dirty block bug!!!
}

touch_ui_offset::touch_ui_offset(rct_widget * widget) : left(0), right(0), top(0), bottom(0), 
    spinner_button(false)
{
    switch (widget->type) {
    case WWT_FRAME:
        top = -10 + 10 * workaround; // dirty block bug!!!
        break;
    case WWT_CAPTION:
        top = -10 + 10 * workaround; // dirty block bug!!!
        bottom = 10 * workaround;
        break;
    case WWT_BUTTON:
        if(widget->text == STR_NUMERIC_DOWN && widget->bottom - widget->top == 4){
            left = -21;
            right = -16;
            top = -5;
            spinner_button = true;
        }
        else if (widget->text == STR_NUMERIC_UP && widget->bottom - widget->top == 4){
            left = -5;
            bottom = 5;
            spinner_button = true;
        }
        break;
    case WWT_CLOSEBOX:
        top = -10 + 10 * workaround; // dirty block bug!!!
        bottom = 10 * workaround;
        left = -10;
        break;
    case WWT_TRNBTN:
        break;
    }
}

