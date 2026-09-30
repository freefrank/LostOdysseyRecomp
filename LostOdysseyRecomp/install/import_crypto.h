#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace install::crypto
{
// Hex encoding for the content ID stored in STFS metadata; no content hashing.
inline std::string HexString(const uint8_t* data, size_t len)
{
    static constexpr char hexChars[] = "0123456789abcdef";
    std::string result;
    result.reserve(len * 2);
    for (size_t i = 0; i < len; ++i)
    {
        const uint8_t value = data[i];
        result.push_back(hexChars[value >> 4]);
        result.push_back(hexChars[value & 0xf]);
    }
    return result;
}
}
