#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <mutex>
#include <string_view>

namespace settings::quit_text
{
inline constexpr uint32_t LabelId = 9104;
inline constexpr uint32_t HelpId = 9114;
inline constexpr uint32_t PromptId = 9120;
inline constexpr size_t DescriptorSize = 60;
inline constexpr uint32_t TextIds[] = {LabelId, HelpId, PromptId};

constexpr size_t TextIndex(uint32_t id)
{
    for (size_t i = 0; i < std::size(TextIds); ++i)
        if (TextIds[i] == id) return i;
    return std::size(TextIds);
}

// The resource language IDs are shared with settings::GameLanguage and the
// original INT/JPN/DEU/FRA/SPA/ITA/KOR/CHI/SCH resource table.
constexpr std::u16string_view Text(uint32_t id, uint32_t language)
{
    switch (id)
    {
    case LabelId:
        switch (language)
        {
        case 1: return u"Quit to Desktop";
        case 2: return u"デスクトップに戻る";
        case 3: return u"Zum Desktop zurückkehren";
        case 4: return u"Retour au bureau";
        case 5: return u"Salir al escritorio";
        case 6: return u"Esci al desktop";
        case 7: return u"바탕 화면으로 종료";
        case 8: case 9: return u"退出到桌面";
        }
        break;
    case HelpId:
        switch (language)
        {
        case 1: return u"Quit the game and return to the desktop.";
        case 2: return u"ゲームを終了してデスクトップに戻ります。";
        case 3: return u"Spiel beenden und zum Desktop zurückkehren.";
        case 4: return u"Quitter le jeu et retourner au bureau.";
        case 5: return u"Salir del juego y volver al escritorio.";
        case 6: return u"Esci dal gioco e torna al desktop.";
        case 7: return u"게임을 종료하고 바탕 화면으로 돌아갑니다.";
        case 8: return u"結束遊戲並返回桌面。";
        case 9: return u"退出游戏并返回桌面。";
        }
        break;
    case PromptId:
        switch (language)
        {
        case 1: return u"Quit the game and return to the desktop? Unsaved progress will be lost.";
        case 2: return u"ゲームを終了してデスクトップに戻りますか？保存していない進行状況は失われます。";
        case 3: return u"Spiel beenden und zum Desktop zurückkehren? Nicht gespeicherter Fortschritt geht verloren.";
        case 4: return u"Quitter le jeu et retourner au bureau ? La progression non sauvegardée sera perdue.";
        case 5: return u"¿Salir del juego y volver al escritorio? Se perderá el progreso no guardado.";
        case 6: return u"Uscire dal gioco e tornare al desktop? I progressi non salvati andranno persi.";
        case 7: return u"게임을 종료하고 바탕 화면으로 돌아갈까요? 저장하지 않은 진행 상황은 사라집니다.";
        case 8: return u"結束遊戲並返回桌面嗎？未儲存的進度將會遺失。";
        case 9: return u"退出游戏并返回桌面吗？未保存的进度将会丢失。";
        }
        break;
    default: return {};
    }
    return {};
}

struct Cache
{
    std::mutex mutex;
    std::array<std::array<uint32_t, 10>, std::size(TextIds)> entries{};
};

inline uint32_t Read32(const uint8_t* bytes)
{
    return uint32_t(bytes[0]) << 24 | uint32_t(bytes[1]) << 16 |
        uint32_t(bytes[2]) << 8 | uint32_t(bytes[3]);
}

inline void Write32(uint8_t* bytes, uint32_t value)
{
    bytes[0] = uint8_t(value >> 24);
    bytes[1] = uint8_t(value >> 16);
    bytes[2] = uint8_t(value >> 8);
    bytes[3] = uint8_t(value);
}

// Original returns a pointer to a shared 60-byte resource row. Its +8/+12
// fields form a guest UTF-16BE string pointer and a copied code-unit count.
// 828B6968 passes row+8 to 822B3F50, which allocates count*2 bytes through
// 8229F678 and copies precisely count*2 bytes. Include the terminal zero in
// that count so the owned string cannot expose unrelated trailing characters.
// Keep a private guest row and text per language; consumers may retain them.
// 'original' must be invoked with the untouched input context (r4 is caller-
// saved and may be changed by the guest implementation).
template<class Context, class Original, class Allocate>
void Lookup(Context& ctx, uint8_t* base, uint32_t language, Cache& cache,
            Original&& original, Allocate&& allocate)
{
    const uint32_t id = ctx.r4.u32;
    original(ctx, base);
    const auto index = TextIndex(id);
    const auto text = index < std::size(TextIds) ? Text(id, language) : std::u16string_view{};
    if (text.empty() || !ctx.r3.u32 || Read32(base + ctx.r3.u32) != id)
        return;

    std::lock_guard lock(cache.mutex);
    uint32_t& entry = cache.entries[index][language];
    if (!entry)
    {
        const size_t bytes = (text.size() + 1) * 2;
        const uint32_t address = allocate(DescriptorSize + bytes);
        if (!address) return;
        auto* descriptor = base + address;
        std::memcpy(descriptor, base + ctx.r3.u32, DescriptorSize);
        const uint32_t characters = address + uint32_t(DescriptorSize);
        for (size_t i = 0; i < text.size(); ++i)
        {
            descriptor[DescriptorSize + i * 2] = uint8_t(text[i] >> 8);
            descriptor[DescriptorSize + i * 2 + 1] = uint8_t(text[i]);
        }
        descriptor[DescriptorSize + bytes - 2] = 0;
        descriptor[DescriptorSize + bytes - 1] = 0;
        Write32(descriptor + 8, characters);
        Write32(descriptor + 12, uint32_t(text.size() + 1));
        entry = address;
    }
    ctx.r3.u64 = entry;
}
} // namespace settings::quit_text
