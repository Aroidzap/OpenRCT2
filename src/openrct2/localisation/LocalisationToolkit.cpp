#include "LocalisationToolkit.h"
#include "../drawing/Drawing.h"

bool LocalisationToolkit::showStringID = false;

bool LocalisationToolkit::ShowStringID() {
    return showStringID;
}

void LocalisationToolkit::ToggleStringID() {
    showStringID ^= true;
    gfx_invalidate_screen();
}
