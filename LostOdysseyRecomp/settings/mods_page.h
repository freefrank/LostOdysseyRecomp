#pragma once
// The Mods page (System → Mods): installed mods in their effective order, with
// the player's unsaved order and switches, then the language packs (read-only).
// Rows: [no mods line] mods, packs, [Open mods folder], [Save]. menu.cpp owns
// the input and the snapshot; this keeps the list model.
#include <modding/mod_api.h>
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#endif

namespace settings::mods_page
{
using Order = std::vector<std::pair<std::string, bool>>;
struct Entry
{
    modding::ModInfo info;
    bool on = true; // the mod-list.ini switch
};
inline bool IsMod(const Entry &entry) { return entry.info.kind == modding::ModKind::Mod; }
// A switch that changes something: the mod.ini is valid and does not turn the mod off.
inline bool Switchable(const Entry &entry)
{
    return IsMod(entry) && entry.info.apiVersion != 0 && entry.info.manifestEnabled;
}
inline size_t ModCount(const std::vector<Entry> &entries)
{
    return size_t(std::count_if(entries.begin(), entries.end(), IsMod));
}
// ListMods returns the mods first, then the language packs.
inline std::vector<Entry> FromList(const std::vector<modding::ModInfo> &list)
{
    std::vector<Entry> entries;
    for (const auto &info : list)
        entries.push_back({info, info.listEnabled});
    std::stable_partition(entries.begin(), entries.end(), IsMod);
    return entries;
}
// What SaveModList takes: the mods with a valid mod.ini, highest first, the first
// folder of a repeated id. A rejected mod.ini has the folder name as id, which
// may not be a valid id; its line in mod-list.ini, if any, stays in the file.
inline Order SaveOrder(const std::vector<Entry> &entries)
{
    Order order;
    for (const auto &entry : entries)
        if (IsMod(entry) && entry.info.apiVersion != 0 &&
            std::none_of(order.begin(), order.end(), [&](const auto &line) { return line.first == entry.info.id; }))
            order.emplace_back(entry.info.id, entry.on);
    return order;
}
// Applies an order saved earlier in this run (it takes effect at the next start)
// to a fresh listing: listed mods first in that order, the others after them.
inline void ApplyOrder(std::vector<Entry> &entries, const Order &order)
{
    const auto rank = [&](const Entry &entry) {
        for (size_t i = 0; i < order.size(); ++i)
            if (IsMod(entry) && order[i].first == entry.info.id) return i;
        return IsMod(entry) ? order.size() : order.size() + 1;
    };
    std::stable_sort(entries.begin(), entries.end(), [&](const Entry &a, const Entry &b) { return rank(a) < rank(b); });
    for (auto &entry : entries)
        for (const auto &[id, on] : order)
            if (IsMod(entry) && id == entry.info.id) entry.on = on;
}
// Moves the mod at index one place up (delta -1) or down (+1) among the mods.
// Returns its new index.
inline size_t Move(std::vector<Entry> &entries, size_t index, int delta)
{
    const size_t mods = ModCount(entries);
    if (index >= mods) return index;
    const size_t target = delta < 0 ? (index ? index - 1 : index) : std::min(index + 1, mods - 1);
    std::swap(entries[index], entries[target]);
    return target;
}
// Row layout; -1 when the row is absent.
struct Rows
{
    int empty = -1;  // "No mods installed": where mods go
    int first = 0;   // first entry row
    int open = -1;   // Open mods folder (Windows)
    int save = -1;   // Save mod list (with at least one mod)
    int count = 0;
    int EntryAt(int row, size_t entries) const { return row >= first && row < first + int(entries) ? row - first : -1; }
};
inline Rows Layout(const std::vector<Entry> &entries, bool canOpenFolder)
{
    Rows rows;
    const bool mods = ModCount(entries) != 0;
    if (!mods) rows.empty = rows.count++;
    rows.first = rows.count;
    rows.count += int(entries.size());
    if (canOpenFolder) rows.open = rows.count++;
    if (mods) rows.save = rows.count++;
    return rows;
}
// Display width in half-width units; CJK and other wide scripts count twice.
inline size_t Units(wchar_t c) { return c >= 0x1100 ? 2 : 1; }
// Cuts text longer than limit units and ends it with "...".
inline std::wstring Shorten(std::wstring text, size_t limit)
{
    size_t units = 0, cut = 0;
    for (; cut < text.size(); ++cut)
    {
        units += Units(text[cut]);
        if (units > limit) break;
    }
    if (cut == text.size()) return text;
    size_t keep = 0;
    units = 0;
    while (keep < text.size() && units + Units(text[keep]) + 3 <= limit) units += Units(text[keep++]);
    text.resize(keep);
    while (!text.empty() && text.back() == L' ') text.pop_back();
    return text + L"...";
}
// The game language a pack is based on, by its language.ini base code.
inline std::wstring BaseLanguageName(std::string_view code)
{
    constexpr std::pair<std::string_view, const wchar_t *> names[] = {
        {"int", L"English"}, {"jpn", L"日本語"}, {"deu", L"Deutsch"}, {"fra", L"Français"}, {"spa", L"Español"},
        {"ita", L"Italiano"}, {"kor", L"한국어"}, {"chi", L"繁體中文"}, {"sch", L"简体中文"}};
    for (const auto &[key, name] : names)
        if (key == code) return name;
    std::wstring upper;
    for (const char c : code) upper.push_back(wchar_t(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c));
    return upper;
}
#ifdef _WIN32
inline constexpr bool CanOpenFolder = true;
// File Explorer at the folder. False when the shell refused.
inline bool OpenFolder(const std::filesystem::path &folder)
{
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (SUCCEEDED(com)) CoUninitialize();
    return result > 32;
}
#else
// Other platforms show the folder path instead.
inline constexpr bool CanOpenFolder = false;
inline bool OpenFolder(const std::filesystem::path &) { return false; }
#endif
} // namespace settings::mods_page
