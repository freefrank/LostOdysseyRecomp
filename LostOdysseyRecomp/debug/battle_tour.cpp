#include <stdafx.h>
#include <kernel/heap.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include "battle_tour.h"
#include "battle_menu.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>

namespace
{
// Formation table: TArray (data, count) at 0x83264978 + 128 with 160-byte
// entries; RequestBattle 0x828278A0 indexes it with the formation id. FStrings:
// +0 battle stage (empty: the loader's area stage 0x832631F0 + 1604, then
// u10_b3_scrw), +12 music, +60 AI script, +72/+84 start/end sequences.
// +148/+152 is the enemy slot array: 32-byte slots of slot id (byte), enemy
// config (the battle character's model resource) and enemy parameter id.
constexpr uint32_t FormationArray = 0x83264978 + 128;
constexpr uint32_t FormationBytes = 160;
constexpr uint32_t AreaStage = 0x832631F0 + 1604;
std::atomic<int32_t> pending{-1};
// Optional stage for the pending formation (guest game thread only).
std::string pendingStage;
uint32_t stageEntry = 0;
uint32_t savedStage[3]{};
void* stageMemory = nullptr;

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
        std::string fields;
        for (uint32_t offset = 0; offset < FormationBytes; offset += 4)
        {
            // FString fields (data, count, capacity) print as text, the rest as words.
            if (offset + 12 <= FormationBytes && PPC_LOAD_U32(entry + offset + 4) > 1 &&
                PPC_LOAD_U32(entry + offset + 4) == PPC_LOAD_U32(entry + offset + 8) &&
                !ReadFString(base, entry + offset).empty())
            {
                fields += fmt::format(" +{}={}", offset, ReadFString(base, entry + offset));
                offset += 8;
            }
            else if (const uint32_t word = PPC_LOAD_U32(entry + offset))
                fields += fmt::format(" +{}:{:08x}", offset, word);
        }
        const uint32_t slots = PPC_LOAD_U32(entry + 148);
        const uint32_t enemies = PPC_LOAD_U32(entry + 152);
        if (Address(slots) && enemies <= 64)
        {
            fields += " slots:";
            for (uint32_t offset = 0; offset < enemies * 32; offset += 4)
                fields += fmt::format(" {:08x}", PPC_LOAD_U32(slots + offset));
        }
        LOG_INFO("battle tour: formation {}{}", i, fields);
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
    else if (operation == "victory")
        LOG_INFO("battle tour: victory requested={}", debug_menu::RequestVictory());
    else if (operation == "battle" && input >> id && id >= 0 && uint32_t(id) < FormationCount(base))
    {
        std::string stage;
        input >> stage;
        if (stage.size() > 32 || stage.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos)
            stage.clear();
        pendingStage = stage;
        pending = id;
        LOG_INFO("battle tour: formation {} queued stage={}", id, stage);
    }
    else
        LOG_INFO("battle tour: rejected command serial={} {} {}", serial, operation, id);
}

bool debug_menu::battle_tour::Pending()
{
    return pending.load(std::memory_order_relaxed) >= 0;
}

// A stage override replaces the formation's stage FString until RequestBattle
// has copied it (RestoreStage after the walking update), as the world map's
// sub_8298D830 does with its own stage name.
bool debug_menu::battle_tour::Take(uint8_t* base, int32_t& formation)
{
    formation = pending.exchange(-1);
    if (formation < 0) return false;
    const std::string stage = std::exchange(pendingStage, {});
    if (stage.empty() || stageMemory) return true;
    stageMemory = g_userHeap.Alloc(16 + 2 * (stage.size() + 1));
    if (!stageMemory) return true;
    const uint32_t text = g_memory.MapVirtual(stageMemory);
    for (uint32_t i = 0; i < stage.size(); ++i)
        PPC_STORE_U16(text + i * 2, uint16_t(uint8_t(stage[i])));
    PPC_STORE_U16(text + uint32_t(stage.size()) * 2, 0);
    stageEntry = PPC_LOAD_U32(FormationArray) + uint32_t(formation) * FormationBytes;
    for (uint32_t i = 0; i < 3; ++i)
        savedStage[i] = PPC_LOAD_U32(stageEntry + i * 4);
    PPC_STORE_U32(stageEntry, text);
    PPC_STORE_U32(stageEntry + 4, uint32_t(stage.size() + 1));
    PPC_STORE_U32(stageEntry + 8, uint32_t(stage.size() + 1));
    return true;
}

void debug_menu::battle_tour::RestoreStage(uint8_t* base)
{
    if (!stageMemory) return;
    for (uint32_t i = 0; i < 3; ++i)
        PPC_STORE_U32(stageEntry + i * 4, savedStage[i]);
    g_userHeap.Free(std::exchange(stageMemory, nullptr));
}
