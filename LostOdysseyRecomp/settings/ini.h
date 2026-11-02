#pragma once
#include <string_view>

namespace settings::ini
{
// Accept the spacing used by external INI editors, including CRLF files on POSIX.
inline std::string_view Trim(std::string_view value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return value.substr(value.size());
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
}
