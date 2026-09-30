#pragma once
#include "cheat_data.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

struct PPCContext;

namespace debug_menu::cheats {
inline constexpr uint32_t GoldLimit = 9999999;
// CT offsets, NOT executable offsets. Never write host x64 code or scan it.
inline constexpr uint32_t Gold = 0x44, Party = 0x60, Hp = 0x16C,
    MaxHp = 0x170, Mp = 0x188, MaxMp = 0x18C, Exp = 0x84,
    Weapon = 0xA9E, Ring = 0xAA2, Accessory = 0xAA6, AccessoryCount = 0xACB;
struct Memory {
    std::span<uint8_t> bytes;
    uint8_t U8(uint32_t p) const { return bytes[p]; }
    uint16_t U16(uint32_t p) const { return uint16_t((uint16_t(bytes[p]) << 8) | bytes[p+1]); }
    uint32_t U32(uint32_t p) const { return (uint32_t(U16(p)) << 16) | U16(p+2); }
    float F32(uint32_t p) const { return std::bit_cast<float>(U32(p)); }
    bool Character(unsigned n) const {
        return n < 9 && F32(MaxHp + n * data::CharacterStride) > 0;
    }
};
struct Context {
    uint32_t root = 0, world = 0, level = 0;
    bool operator==(const Context&) const = default;
};
inline bool InRange(float f, float maximum) { return std::isfinite(f) && f >= 0 && f <= maximum; }
inline bool Validate(Memory m) {
    if (m.bytes.size() < data::DataSize || m.U32(Gold) > 999999999u) return false;
    unsigned members = 0, seen = 0;
    for (unsigned i = 0; i < 5; ++i) {
        const auto id = m.U32(Party + i*4);
        if (id == UINT32_MAX) continue;
        if (id >= 9 || (seen & (1u << id)) || !m.Character(id)) return false;
        seen |= 1u << id; ++members;
    }
    if (!members) return false;
    for (unsigned i = 0; i < 9; ++i) {
        const auto b = i * data::CharacterStride;
        const auto hp = m.F32(Hp+b), maxHp = m.F32(MaxHp+b);
        const auto mp = m.F32(Mp+b), maxMp = m.F32(MaxMp+b);
        if (!InRange(maxHp, 1000000) || !InRange(maxMp, 1000000) ||
            !InRange(hp, maxHp) || !InRange(mp, maxMp) || !InRange(m.F32(Exp+b), 100)) return false;
    }
    for (const auto& e : data::Items) if (!InRange(m.F32(e.value), 9999)) return false;
    for (const auto& e : data::Materials) if (!InRange(m.F32(e.value), 9999)) return false;
    return true;
}
enum class Action { AddGold, SetGold, Heal, HealParty, SetExp, Item, Material,
    AllItems, AllMaterials, LearnSkill, LearnSupportedSkills, SetWeapon, SetRing,
    SetAccessory, SetFormation, SetRow, SetFieldCharacter };
struct Request {
    Action action = Action::AddGold;
    unsigned character = 0, index = 0, value = 0;
    uint64_t generation = 0;
};
struct Write { uint32_t offset = 0, value = 0; unsigned bytes = 4; };
inline bool CatalogValue(std::span<const data::Entry> list, uint32_t v) {
    return std::any_of(list.begin(), list.end(), [v](const auto& e) { return e.value == v; });
}
// Build the whole transaction first. An invalid member aborts without any write.
inline std::optional<std::vector<Write>> Plan(Memory m, const Request& r) {
    if (!Validate(m)) return {};
    std::vector<Write> writes;
    const auto b = r.character < 9 ? r.character * data::CharacterStride : 0;
    auto number = [&](uint32_t p, float v) { writes.push_back({p, std::bit_cast<uint32_t>(v), 4}); };
    auto heal = [&](unsigned n) {
        const auto o = n * data::CharacterStride;
        writes.push_back({Hp+o, m.U32(MaxHp+o), 4});
        writes.push_back({Mp+o, m.U32(MaxMp+o), 4});
    };
    const bool characterAction = r.action == Action::Heal || r.action == Action::SetExp ||
        r.action == Action::LearnSkill || r.action == Action::LearnSupportedSkills ||
        r.action == Action::SetWeapon || r.action == Action::SetRing ||
        r.action == Action::SetAccessory || r.action == Action::SetRow;
    if (characterAction && !m.Character(r.character)) return {};
    switch (r.action) {
    case Action::AddGold:
        if (r.value > GoldLimit) return {};
        // Adding never lowers a balance already above this UI's conservative cap.
        writes.push_back({Gold, std::max(m.U32(Gold), uint32_t(std::min<uint64_t>(GoldLimit,
            uint64_t(m.U32(Gold)) + r.value))), 4}); break;
    case Action::SetGold:
        if (r.value > GoldLimit) return {};
        writes.push_back({Gold, r.value, 4}); break;
    case Action::Heal: heal(r.character); break;
    case Action::HealParty:
        for (unsigned i=0; i<5; ++i) { auto n=m.U32(Party+i*4); if (n<9) heal(n); } break;
    case Action::SetExp:
        if (r.value > 99) return {};
        number(Exp+b, float(r.value)); break;
    case Action::Item: case Action::Material: {
        const auto list = r.action == Action::Item ? std::span<const data::Entry>(data::Items)
            : std::span<const data::Entry>(data::Materials);
        if (r.index >= list.size() || r.value > 99) return {};
        number(list[r.index].value, float(r.value)); break;
    }
    case Action::AllItems: case Action::AllMaterials: {
        if (r.value > 99) return {};
        const auto list = r.action == Action::AllItems ? std::span<const data::Entry>(data::Items)
            : std::span<const data::Entry>(data::Materials);
        for (const auto& e : list) number(e.value, float(r.value));
        break;
    }
    case Action::LearnSkill:
        if (r.index >= std::size(data::LearnedSkills)) return {};
        writes.push_back({b+data::LearnedSkills[r.index].value,
            uint32_t(m.U8(b+data::LearnedSkills[r.index].value) | 0x80), 1}); break;
    case Action::LearnSupportedSkills:
        for (const auto& e : data::LearnedSkills)
            writes.push_back({b+e.value, uint32_t(m.U8(b+e.value) | 0x80), 1});
        break;
    case Action::SetWeapon:
        if (!CatalogValue(data::Weapons, r.value)) return {};
        writes.push_back({Weapon+b, r.value, 2}); break;
    case Action::SetRing:
        if (!CatalogValue(data::Rings, r.value)) return {};
        writes.push_back({Ring+b, r.value, 2}); break;
    case Action::SetAccessory:
        if (r.index >= 8 || m.U8(AccessoryCount+b) > 8 || r.index >= m.U8(AccessoryCount+b) ||
            !CatalogValue(data::Accessories, r.value)) return {};
        writes.push_back({Accessory+b+r.index*4, r.value, 2}); break;
    case Action::SetFormation: {
        if (r.index >= 5 || !m.Character(r.value)) return {};
        const auto old = m.U32(Party+r.index*4);
        for (unsigned i=0; i<5; ++i)
            if (i != r.index && m.U32(Party+i*4) == r.value) writes.push_back({Party+i*4, old, 4});
        writes.push_back({Party+r.index*4, r.value, 4}); break;
    }
    case Action::SetRow:
        if (r.value != 0 && r.value != 0x80) return {};
        writes.push_back({0x7C+b, r.value, 1}); break;
    case Action::SetFieldCharacter:
        if (!m.Character(r.value)) return {};
        writes.push_back({0x53, r.value, 1}); break;
    default: return {};
    }
    for (const auto& w : writes)
        if ((w.bytes != 1 && w.bytes != 2 && w.bytes != 4) ||
            w.offset > m.bytes.size() || w.bytes > m.bytes.size()-w.offset) return {};
    return writes;
}
inline void Commit(Memory m, std::span<const Write> writes) {
    for (const auto& w : writes)
        for (unsigned j=0; j<w.bytes; ++j) m.bytes[w.offset+j] = uint8_t(w.value >> (8*(w.bytes-1-j)));
}
struct CharacterState {
    bool available = false;
    float hp = 0, maxHp = 0, mp = 0, maxMp = 0, exp = 0;
    uint16_t weapon = 0, ring = 0;
    uint8_t accessoryCount = 0, row = 0;
    std::array<uint16_t,8> accessories{};
};
enum class Result { Ready, Unavailable, Disabled, Pending, Applied, Cancelled, Invalid };
struct Snapshot {
    bool enabled = false, available = false, editorRequested = false, editorApplied = false;
    uint64_t generation = 0, serial = 0;
    uint32_t gold = 0;
    Result result = Result::Unavailable;
    std::array<CharacterState,9> characters{};
    std::array<uint32_t,5> party{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
};
// UI owns only requests and by-value snapshots; Tick is the only guest writer.
class Session {
    mutable std::mutex mutex;
    Snapshot snapshot;
    Context identity;
    std::optional<Request> pending;
public:
    Snapshot Get() const { std::lock_guard lock(mutex); return snapshot; }
    void Enable(bool enabled) {
        std::lock_guard lock(mutex); snapshot.enabled = enabled;
        if (!enabled) { pending.reset(); snapshot.editorRequested = false; snapshot.result = Result::Disabled; }
        else snapshot.result = snapshot.available ? Result::Ready : Result::Unavailable;
    }
    bool SetEditor(bool enabled) {
        std::lock_guard lock(mutex);
        if (enabled && (!snapshot.enabled || !snapshot.available)) return false;
        snapshot.editorRequested = enabled; return true;
    }
    void EditorApplied(bool applied) { std::lock_guard lock(mutex); snapshot.editorApplied = applied; }
    bool Queue(Request request) {
        std::lock_guard lock(mutex);
        if (!snapshot.enabled || !snapshot.available || pending || request.generation != snapshot.generation) return false;
        pending = request; snapshot.result = Result::Pending; ++snapshot.serial; return true;
    }
    bool HasPending() const { std::lock_guard lock(mutex); return pending.has_value(); }
    // Called from the guest engine tick, with a validated live allocation.
    void Tick(Context context, Memory memory, bool field) {
        const bool available = field && context.root && context.world && context.level && Validate(memory);
        std::lock_guard lock(mutex);
        if (context != identity || available != snapshot.available) {
            identity = context; ++snapshot.generation;
            if (pending) { pending.reset(); snapshot.result = Result::Cancelled; }
            else snapshot.result = available ? Result::Ready : Result::Unavailable;
            // Do not carry a retail editor override across loads or new worlds.
            snapshot.editorRequested = false;
        }
        snapshot.available = available;
        if (!available) { pending.reset(); return; }
        if (pending) {
            const auto r = *pending; pending.reset();
            if (!snapshot.enabled || r.generation != snapshot.generation) snapshot.result = Result::Cancelled;
            else if (auto writes = Plan(memory, r)) { Commit(memory, *writes); snapshot.result = Result::Applied; }
            else snapshot.result = Result::Invalid;
        }
        snapshot.gold = memory.U32(Gold);
        for (unsigned i=0; i<5; ++i) snapshot.party[i] = memory.U32(Party+i*4);
        for (unsigned i=0; i<9; ++i) {
            auto& c = snapshot.characters[i]; const auto b = i*data::CharacterStride;
            c.available=memory.Character(i); c.hp=memory.F32(Hp+b); c.maxHp=memory.F32(MaxHp+b);
            c.mp=memory.F32(Mp+b); c.maxMp=memory.F32(MaxMp+b); c.exp=memory.F32(Exp+b);
            c.weapon=memory.U16(Weapon+b); c.ring=memory.U16(Ring+b);
            c.accessoryCount=memory.U8(AccessoryCount+b); c.row=memory.U8(0x7C+b);
            for (unsigned j=0; j<8; ++j) c.accessories[j]=memory.U16(Accessory+b+j*4);
        }
    }
};
inline Session session;
void Tick(PPCContext& ctx, uint8_t* base);
void PollHostControls();
} // namespace debug_menu::cheats
