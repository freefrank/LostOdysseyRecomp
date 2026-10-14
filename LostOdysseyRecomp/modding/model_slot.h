#pragma once
// Which SkeletalMesh material slot a glTF material maps to (model_import.cpp).

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace modding::model_slot
{
// Blender adds .001 to a duplicated material's name; the game's names are compared without case.
inline std::string MaterialName(std::string name)
{
    const auto dot = name.rfind('.');
    if (dot != std::string::npos && dot + 4 == name.size() &&
        std::all_of(name.begin() + std::ptrdiff_t(dot) + 1, name.end(), [](char c) { return c >= '0' && c <= '9'; }))
        name.resize(dot);
    for (auto &c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return name;
}

// The slot of the original's materials that the named glTF material (the
// file's material number `material`) maps to, or nothing. Several slots can
// hold the same material: on party models without tears, slot 0 (the tears
// slot on the others) is a copy of slot 1 that no section uses, and a body
// moved there vanished in cutscenes. A slot the original's sections draw with
// wins, then the slot numbered like the file's material (the exporter writes
// one per slot, in order), then the first.
inline std::optional<uint16_t> ChooseSlot(const std::vector<std::string> &slots, const std::set<uint16_t> &used,
                                          const std::string &glTfName, int material)
{
    const auto name = MaterialName(glTfName);
    std::vector<uint16_t> matches, drawn;
    for (size_t i = 0; i < slots.size(); ++i)
        if (MaterialName(slots[i]) == name)
        {
            matches.push_back(uint16_t(i));
            if (used.contains(uint16_t(i))) drawn.push_back(uint16_t(i));
        }
    const auto &pool = drawn.empty() ? matches : drawn;
    if (pool.empty()) return std::nullopt;
    return std::find(pool.begin(), pool.end(), material) != pool.end() ? uint16_t(material) : pool.front();
}
}
