#pragma once

#include <cstdint>
#include <string_view>
#include <string>

namespace host_ui::font
{
#include "../install/unifont_packed.inl"

    struct Glyph
    {
        const uint8_t* bitmap = nullptr;
        int width = 0; // 8 or 16
        int height = 16;
    };

    inline int FindIndex(uint32_t cp)
    {
        if (cp > 0xFFFF) return -1;
        uint16_t cp16 = static_cast<uint16_t>(cp);
        int low = 0;
        int high = static_cast<int>(UNIFONT_GLYPH_COUNT) - 1;
        while (low <= high)
        {
            int mid = low + (high - low) / 2;
            uint16_t val = g_unifontCodepoints[mid];
            if (val == cp16) return mid;
            if (val < cp16) low = mid + 1;
            else high = mid - 1;
        }
        return -1;
    }

    inline Glyph GetGlyph(uint32_t cp)
    {
        int idx = FindIndex(cp);
        if (idx < 0)
        {
            idx = FindIndex('?');
            if (idx < 0) return {};
        }
        uint32_t entry = g_unifontOffsets[idx];
        bool isWide = (entry & 0x80000000u) != 0;
        uint32_t offset = entry & 0x7FFFFFFFu;
        return { &g_unifontData[offset], isWide ? 16 : 8, 16 };
    }

    inline uint32_t DecodeUtf8(std::string_view text, size_t& offset)
    {
        if (offset >= text.size()) return 0;
        uint8_t c = static_cast<uint8_t>(text[offset++]);
        if (c < 0x80) return c;
        if ((c & 0xE0) == 0xC0)
        {
            if (offset >= text.size()) return 0;
            return ((c & 0x1F) << 6) | (static_cast<uint8_t>(text[offset++]) & 0x3F);
        }
        if ((c & 0xF0) == 0xE0)
        {
            if (offset + 1 >= text.size()) return 0;
            uint32_t cp = (c & 0x0F) << 12;
            cp |= (static_cast<uint8_t>(text[offset++]) & 0x3F) << 6;
            cp |= (static_cast<uint8_t>(text[offset++]) & 0x3F);
            return cp;
        }
        if ((c & 0xF8) == 0xF0)
        {
            if (offset + 2 >= text.size()) return 0;
            uint32_t cp = (c & 0x07) << 18;
            cp |= (static_cast<uint8_t>(text[offset++]) & 0x3F) << 12;
            cp |= (static_cast<uint8_t>(text[offset++]) & 0x3F) << 6;
            cp |= (static_cast<uint8_t>(text[offset++]) & 0x3F);
            return cp;
        }
        return 0;
    }
}
