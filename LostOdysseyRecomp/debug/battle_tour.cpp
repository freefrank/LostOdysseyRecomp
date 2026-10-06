#include <stdafx.h>
#include <os/logger.h>
#include "battle_tour.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string>

namespace
{
// Formation table: TArray (data, count) at 0x83264978 + 128 with 160-byte
// entries; RequestBattle 0x828278A0 indexes it with the formation id. The
// first field is the battle stage FString; when it is empty the stage is the
// loader's area stage (0x832631F0 + 1604), then u10_b3_scrw.
constexpr uint32_t FormationArray = 0x83264978 + 128;
constexpr uint32_t FormationBytes = 160;
constexpr uint32_t AreaStage = 0x832631F0 + 1604;
std::atomic<int32_t> pending{-1};

bool Address(uint32_t p) { return p >= 0x100000 && p < 0xC0000000 && (p & 1) == 0; }

std::string ReadFString(uint8_t* base, uint32_t fstring)
{
    const uint32_t data = PPC_LOAD_U32(fstring);
    const uint32_t count = PPC_LOAD_U32(fstring + 4);
    std::string text;
    if (!Address(data) || count < 2 || count > 64) return text;
    for (uint32_t i = 0; i + 1 < count; ++i)
    {
        const uint16_t c = PPC_LOAD_U16(data + i * 2);
        text += c >= 0x20 && c < 0x7F ? char(c) : '?';
    }
    return text;
}

uint32_t FormationCount(uint8_t* base)
{
    const uint32_t count = PPC_LOAD_U32(FormationArray + 4);
    return Address(PPC_LOAD_U32(FormationArray)) && count < 4096 ? count : 0;
}

void List(uint8_t* base)
{
    const uint32_t data = PPC_LOAD_U32(FormationArray);
    const uint32_t count = FormationCount(base);
    LOG_INFO("battle tour: formations={} area_stage={}", count, ReadFString(base, AreaStage));
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint32_t entry = data + i * FormationBytes;
        std::string words;
        for (uint32_t offset = 12; offset < FormationBytes; offset += 4)
            words += fmt::format(" {:08x}", PPC_LOAD_U32(entry + offset));
        LOG_INFO("battle tour: formation {} stage={}{}", i, ReadFString(base, entry), words);
    }
}
}

void debug_menu::battle_tour::Poll(uint8_t* base)
{
    static const char* path = std::getenv("LO_DEBUG_BATTLE_FILE");
    if (!path) return;
    static std::chrono::steady_clock::time_point next{};
    static uint64_t last = 0;
    const auto now = std::chrono::steady_clock::now();
    if (now < next) return;
    next = now + std::chrono::milliseconds(250);
    std::ifstream input(path);
    uint64_t serial = 0;
    std::string operation;
    if (!(input >> serial >> operation) || serial <= last) return;
    last = serial;
    int32_t id = -1;
    if (operation == "list")
        List(base);
    else if (operation == "battle" && input >> id && id >= 0 && uint32_t(id) < FormationCount(base))
    {
        pending = id;
        LOG_INFO("battle tour: formation {} queued", id);
    }
    else
        LOG_INFO("battle tour: rejected command serial={} {} {}", serial, operation, id);
}

bool debug_menu::battle_tour::Pending()
{
    return pending.load(std::memory_order_relaxed) >= 0;
}

bool debug_menu::battle_tour::Take(int32_t& formation)
{
    formation = pending.exchange(-1);
    return formation >= 0;
}
