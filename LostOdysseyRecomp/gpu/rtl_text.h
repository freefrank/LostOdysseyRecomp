#pragma once

namespace rtl_text
{
// Right-to-left placement of Arabic text (rtl_text.cpp): on while the selected language pack has
// direction=rtl in its language.ini. Set before the game draws text.
void SetEnabled(bool enabled);
}
