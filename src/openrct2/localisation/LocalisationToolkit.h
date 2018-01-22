#pragma once
#include "../common.h"

class LocalisationToolkit {
private:
    static bool showStringID;
public:
    static void ToggleStringID();
    static bool ShowStringID();
    static const utf8 * GetString(rct_string_id stringId);
};