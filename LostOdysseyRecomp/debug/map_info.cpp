#include <stdafx.h>
#include <os/logger.h>
#include <map>
#include "map_info.h"

extern "C" PPC_FUNC(__imp__sub_82A0D648);
namespace {
std::mutex mutex;
std::map<std::wstring, uint32_t> definitions;
debug_menu::MapInfo snapshot;
std::chrono::steady_clock::time_point lastUpdate;

bool Address(uint32_t p) { return p >= 0x100000 && p < 0x7BFF0000 && !(p & 3); }
std::wstring Text(uint8_t* base, uint32_t p, uint32_t limit = 256) {
    if (p < 0x100000 || p >= 0x7BFF0000 || (p & 1)) return {};
    std::wstring result;
    for (uint32_t i = 0; i < limit; ++i) {
        const auto c = PPC_LOAD_U16(p + i * 2);
        if (!c) return result;
        result += wchar_t(c);
    }
    return {};
}
std::wstring Key(std::wstring value) {
    for (auto& c : value) if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
    return value;
}
std::wstring ObjectName(uint8_t* base, uint32_t object) {
    if (!Address(object)) return {};
    const auto names = PPC_LOAD_U32(0x833690D0), count = PPC_LOAD_U32(0x833690D4);
    const auto index = PPC_LOAD_U32(object + 0x2C);
    if (!Address(names) || count > 1000000 || index >= count) return {};
    const auto entry = PPC_LOAD_U32(names + index * 4);
    return Address(entry) ? Key(Text(base, entry + 16)) : std::wstring{};
}
}

PPC_FUNC(sub_82A0D648) {
    // Native map-definition insertion: key r4, row r5, FString package at +4.
    const auto row = ctx.r5.u32;
    if (Address(row)) {
        const auto package = Key(Text(base, PPC_LOAD_U32(row + 4)));
        if (!package.empty()) {
            std::lock_guard lock(mutex);
            definitions[package] = ctx.r4.u32;
        }
    }
    __imp__sub_82A0D648(ctx, base);
}

void debug_menu::UpdateMapInfo(uint8_t* base) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(mutex);
    if (now - lastUpdate < std::chrono::milliseconds(250)) return;
    lastUpdate = now;
    MapInfo current;
    const auto world = PPC_LOAD_U32(0x83318744);
    if (Address(world)) {
        const auto levels = PPC_LOAD_U32(world + 0x44), count = PPC_LOAD_U32(world + 0x48);
        if (Address(levels) && count <= 256) {
            for (uint32_t i = 0; i < count; ++i) {
                auto object = PPC_LOAD_U32(levels + i * 4);
                // ULevel -> UWorld -> package. Names are matched to native definitions.
                for (unsigned depth = 0; depth < 4 && Address(object); ++depth) {
                    const auto package = ObjectName(base, object);
                    const auto found = definitions.find(package);
                    if (found != definitions.end()) {
                        if (current.available && current.id != found->second) {
                            current = {}; // A streaming transition is ambiguous.
                            goto done;
                        }
                        current.available = true;
                        current.id = found->second;
                        current.package = package;
                        break;
                    }
                    object = PPC_LOAD_U32(object + 0x28);
                }
            }
        }
    }
    if (current.available) {
        // Live localized FString array indexed by map-definition ID, not font text ID.
        const auto data = PPC_LOAD_U32(0x832C9728), count = PPC_LOAD_U32(0x832C972C);
        if (Address(data) && count <= 4096 && current.id < count) {
            const auto item = data + current.id * 12;
            const auto length = PPC_LOAD_U32(item + 4);
            if (length > 0 && length <= 512)
                current.name = Text(base, PPC_LOAD_U32(item), length);
        }
    }
done:
    if (getenv("LO_TRACE_MAP_INFO") && (current.available != snapshot.available ||
        current.id != snapshot.id || current.name != snapshot.name)) {
        const auto name = std::filesystem::path(current.name).u8string();
        LOG_INFO("current map available={} id={} name={}", current.available, current.id,
            reinterpret_cast<const char*>(name.c_str()));
    }
    snapshot = std::move(current);
}

debug_menu::MapInfo debug_menu::GetMapInfo() {
    std::lock_guard lock(mutex);
    if (std::chrono::steady_clock::now() - lastUpdate > std::chrono::seconds(2)) return {};
    return snapshot;
}
