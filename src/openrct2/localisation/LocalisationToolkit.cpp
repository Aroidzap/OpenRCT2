#include "LocalisationToolkit.h"
#include "../drawing/Drawing.h"
#include <unordered_map>
#include <string>

std::unordered_map<size_t, std::string> strings;

bool LocalisationToolkit::showStringID = false;

bool LocalisationToolkit::ShowStringID() {
    return showStringID;
}

void LocalisationToolkit::ToggleStringID() {
    showStringID ^= true;
    gfx_invalidate_screen();
}

const utf8 * LocalisationToolkit::GetString(rct_string_id stringId)
{
    size_t id = (size_t)stringId;
    strings.insert({ id, "STR" + std::to_string(id) });
    const utf8 *text = strings[id].c_str();
    return text;
}
