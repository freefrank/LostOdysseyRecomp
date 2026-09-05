#include <stdafx.h>
#include <kernel/heap.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <utility>
#include <cmath>
#include "teleport.h"
#include "map_poi.h"

extern "C" PPC_FUNC(__imp__sub_82290B60);
extern "C" PPC_FUNC(__imp__sub_822FA548);

namespace
{
using debug_menu::Position;
enum class Operation { None, Absolute, Offset, Save, Restore, Poi };
struct Identity
{
    uint32_t world{}, level{}, pawn{};
    bool operator==(const Identity&) const = default;
};
std::mutex stateMutex;
debug_menu::TeleportSnapshot snapshot;
Identity identity;
Operation pending = Operation::None;
Position requested{};
uint64_t lastTick = 0;
uint64_t nextPoiRefresh = 0, nextPoiId = 1, requestedPoi = 0;
std::vector<debug_menu::LiveMapPoi> livePois;

uint64_t Now()
{
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool Finite(Position p)
{
    // Debug input limit, not an inferred engine/world bound.
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
        std::abs(p.x) <= debug_menu::MaxTeleportCoordinate &&
        std::abs(p.y) <= debug_menu::MaxTeleportCoordinate &&
        std::abs(p.z) <= debug_menu::MaxTeleportCoordinate;
}

bool ObjectAddress(uint32_t p)
{
    return p >= 0x100000 && p < 0x7C000000 && (p & 3) == 0;
}

Position ReadPosition(uint8_t* base, uint32_t pawn)
{
    // AActor::execSetLocation -> 822FA548 verifies these FVector fields.
    return {std::bit_cast<float>(PPC_LOAD_U32(pawn + 0xF8)),
        std::bit_cast<float>(PPC_LOAD_U32(pawn + 0xFC)),
        std::bit_cast<float>(PPC_LOAD_U32(pawn + 0x100))};
}

bool PlayerControllable(uint8_t* base, uint32_t controller)
{
    // 822A95C8: StateFrame -> StateNode -> UObject FName.
    const auto frame = PPC_LOAD_U32(controller + 0x18);
    if (!ObjectAddress(frame)) return false;
    const auto state = PPC_LOAD_U32(frame + 0x1C);
    if (!ObjectAddress(state) || PPC_LOAD_U32(state + 4) == 0xFFFFFFFF ||
        PPC_LOAD_U32(state + 0x30) != 0) return false;
    const auto index = PPC_LOAD_U32(state + 0x2C);
    // 822A9668 resolves a FName through this array, entry text at +16.
    const auto names = PPC_LOAD_U32(0x833690D0);
    const auto count = PPC_LOAD_U32(0x833690D4);
    if (!ObjectAddress(names) || index >= count || count > 1000000) return false;
    const auto entry = PPC_LOAD_U32(names + index * 4);
    if (!ObjectAddress(entry)) return false;
    // 82A1C0C8 SetPlayerControl(true) selects this state; false selects
    // PlayerNoControl. Compare text, never a session-specific FName index.
    // Live free-roaming also transitions to PlayerIdling while standing still.
    for (const auto expected : {u"PlayerWalking", u"PlayerIdling"})
    {
        bool equal = true;
        for (uint32_t i = 0;; ++i)
        {
            if (PPC_LOAD_U16(entry + 16 + i * 2) != expected[i]) { equal = false; break; }
            if (!expected[i]) break;
        }
        if (equal) return true;
    }
    return false;
}

Identity FindPlayer(uint8_t* base)
{
    auto unavailable = [](const char* reason) -> Identity {
        static const char* previous = nullptr;
        if (getenv("LO_TELEPORT_COMMAND_FILE") && previous != reason)
        {
            LOG_INFO("teleport unavailable: {}", reason);
            previous = reason;
        }
        return {};
    };
    // Observed free-roaming contexts: Hypocenter=0, Wasteland=10. This is
    // not a universal map/battle enum; keep unverified contexts disabled.
    const uint32_t scene = PPC_LOAD_U32(0x832CB6B4);
    if (scene != 0 && scene != 10) return unavailable("HUD state");
    const auto world = PPC_LOAD_U32(0x83318744);
    const auto engine = PPC_LOAD_U32(0x83315FB4);
    if (!ObjectAddress(world) || !ObjectAddress(engine)) return unavailable("world/engine");
    const auto level = PPC_LOAD_U32(world + 0x50);
    // Native LocalPlayerControllers (8250E158): GamePlayers -> Player.Actor.
    const auto players = PPC_LOAD_U32(engine + 0x2B8);
    const auto count = PPC_LOAD_U32(engine + 0x2BC);
    if (!ObjectAddress(level) || !ObjectAddress(players) || count == 0 || count > 4) return unavailable("level/players");
    const auto player = PPC_LOAD_U32(players);
    if (!ObjectAddress(player)) return unavailable("local player");
    const auto controller = PPC_LOAD_U32(player + 0x40);
    if (!ObjectAddress(controller) || !PlayerControllable(base, controller)) return unavailable("controller control state");
    // 82A18080 suspends pawn physics and sets these fields for the game menu;
    // 82A151D8 / 8235FCA0 consume them as control gates.
    if ((PPC_LOAD_U32(controller + 0x5C8) & 0x20) || PPC_LOAD_U8(controller + 0x639))
        return unavailable("game menu/control suspended");
    const auto pawn = PPC_LOAD_U32(controller + 0x204);
    if (!ObjectAddress(pawn) || PPC_LOAD_U32(pawn + 0x210) != controller) return unavailable("possessed pawn");
    // Native GetViewTarget (822A6020) checks these pending-deletion flags.
    if ((PPC_LOAD_U32(pawn + 0x70) & 0x02000000) ||
        (PPC_LOAD_U32(pawn + 0x74) & 0x00080000)) return unavailable("pawn deletion flags");
    return {world, level, pawn};
}

bool Request(Operation op, Position position = {}, uint64_t poiId = 0)
{
    std::lock_guard lock(stateMutex);
    if (!snapshot.available || Now() - lastTick > 1000 || pending != Operation::None ||
        !Finite(position) || (op == Operation::Restore && !snapshot.bookmarkAvailable)) return false;
    if (op == Operation::Poi && std::none_of(snapshot.pois.begin(), snapshot.pois.end(),
        [poiId](const auto& p) { return p.id == poiId; })) return false;
    pending = op;
    requested = position;
    requestedPoi = poiId;
    snapshot.status = L"等待游戏线程执行";
    return true;
}

void RefreshPois(uint8_t* base, Identity current)
{
    if (!current.pawn || Now() < nextPoiRefresh) return;
    auto fresh = debug_menu::ScanMapPois(base, current.world);
    std::lock_guard lock(stateMutex);
    std::vector<debug_menu::MapPoi> points;
    std::unordered_map<std::wstring, unsigned> categoryCounts;
    for (const auto& poi : fresh)
    {
        uint64_t id = 0;
        for (size_t i = 0; i < livePois.size(); ++i)
            if (livePois[i].actor == poi.actor && livePois[i].level == poi.level &&
                livePois[i].objectIndex == poi.objectIndex && livePois[i].key == poi.key)
            { id = snapshot.pois[i].id; break; }
        if (!id) id = nextPoiId++;
        points.push_back({id, poi.category + L" " + std::to_wstring(++categoryCounts[poi.category]), poi.position});
    }
    bool changed = points.size() != snapshot.pois.size();
    for (size_t i = 0; !changed && i < points.size(); ++i)
        changed = points[i].id != snapshot.pois[i].id || points[i].label != snapshot.pois[i].label;
    if (changed) ++snapshot.poiRevision;
    snapshot.pois = std::move(points);
    livePois = std::move(fresh);
    nextPoiRefresh = Now() + 1000;
}

void PollCommandFile()
{
    static const char* path = getenv("LO_TELEPORT_COMMAND_FILE");
    static unsigned polls = 0;
    static uint64_t previousSerial = 0;
    if (!path || ++polls % 12 != 0) return;
    std::ifstream stream(path);
    uint64_t serial = 0;
    std::string operation;
    if (!(stream >> serial >> operation) || serial <= previousSerial) return;
    Position p{};
    uint64_t poiId = 0;
    if (operation == "poi" && !(stream >> poiId)) return;
    if ((operation == "offset" || operation == "absolute") && !(stream >> p.x >> p.y >> p.z)) return;
    previousSerial = serial;
    bool accepted = false;
    if (operation == "save") accepted = debug_menu::RequestSavePosition();
    else if (operation == "restore") accepted = debug_menu::RequestRestorePosition();
    else if (operation == "offset") accepted = debug_menu::RequestTeleportOffset(p);
    else if (operation == "absolute") accepted = debug_menu::RequestTeleport(p);
    else if (operation == "poi") accepted = debug_menu::RequestPoiTeleport(poiId);
    else if (operation == "poi-list")
    {
        std::lock_guard lock(stateMutex);
        LOG_INFO("teleport POI list serial={} revision={} count={}", serial, snapshot.poiRevision, snapshot.pois.size());
        for (size_t i = 0; i < snapshot.pois.size(); ++i)
        {
            const auto& poi = snapshot.pois[i];
            const auto key = std::filesystem::path(livePois[i].key).u8string();
            LOG_INFO("teleport POI id={} key={} position={},{},{}", poi.id,
                reinterpret_cast<const char*>(key.c_str()), poi.position.x, poi.position.y, poi.position.z);
        }
        return;
    }
    else if (operation == "status")
    {
        const auto s = debug_menu::GetTeleportSnapshot();
        LOG_INFO("teleport status serial={} available={} position={},{},{} bookmark={} saved={},{},{}",
            serial, s.available, s.current.x, s.current.y, s.current.z,
            s.bookmarkAvailable, s.bookmark.x, s.bookmark.y, s.bookmark.z);
        return;
    }
    LOG_INFO("teleport command serial={} operation={} accepted={}", serial, operation, accepted);
}

void Tick(PPCContext& ctx, uint8_t* base)
{
    const Identity current = FindPlayer(base);
    const Position position = current.pawn ? ReadPosition(base, current.pawn) : Position{};
    {
        std::lock_guard lock(stateMutex);
        if (current != identity || (lastTick && Now() - lastTick > 1000))
        {
            identity = current;
            pending = Operation::None;
            snapshot.bookmarkAvailable = false;
            snapshot.pois.clear();
            livePois.clear();
            ++snapshot.poiRevision;
            nextPoiRefresh = 0;
            snapshot.status = L"场景已变化，已清除记录坐标";
        }
        lastTick = Now();
        snapshot.available = current.pawn != 0 && Finite(position);
        snapshot.current = position;
        if (!snapshot.available)
        {
            pending = Operation::None;
            snapshot.bookmarkAvailable = false;
            snapshot.status = L"仅可在可控制的地图中传送";
        }
        else if (snapshot.status.empty()) snapshot.status = L"地图传送就绪";
    }
    RefreshPois(base, current);
    PollCommandFile();
    Position destination{};
    debug_menu::LiveMapPoi poiTarget{};
    bool usePoi = false;
    {
        std::lock_guard lock(stateMutex);
        const auto op = std::exchange(pending, Operation::None);
        if (!snapshot.available || op == Operation::None) return;
        if (op == Operation::Save)
        {
            snapshot.bookmark = position;
            snapshot.bookmarkAvailable = true;
            snapshot.status = L"已记录当前地图坐标";
            LOG_INFO("teleport bookmark saved pawn={:#x} position={},{},{}",
                current.pawn, position.x, position.y, position.z);
            return;
        }
        destination = op == Operation::Restore ? snapshot.bookmark : requested;
        if (op == Operation::Poi)
        {
            const auto item = std::find_if(snapshot.pois.begin(), snapshot.pois.end(),
                [](const auto& p) { return p.id == requestedPoi; });
            if (item == snapshot.pois.end()) { snapshot.status = L"POI 已失效，请重新选择"; return; }
            poiTarget = livePois[size_t(item - snapshot.pois.begin())];
            usePoi = true;
        }
        if (op == Operation::Offset)
            destination = {position.x + requested.x, position.y + requested.y, position.z + requested.z};
        if (!Finite(destination))
        {
            snapshot.status = L"坐标必须为有限数值且绝对值不超过 1000000";
            return;
        }
    }
    if (usePoi)
    {
        // Validate membership and object identity again at execution, including
        // streaming changes that occurred since the UI's last snapshot.
        const auto fresh = debug_menu::ScanMapPois(base, current.world);
        const auto match = std::find_if(fresh.begin(), fresh.end(), [&](const auto& p) {
            return p.actor == poiTarget.actor && p.level == poiTarget.level &&
                p.objectIndex == poiTarget.objectIndex && p.key == poiTarget.key;
        });
        if (match == fresh.end() || !Finite(match->position))
        {
            std::lock_guard lock(stateMutex);
            snapshot.status = L"POI 已卸载或坐标无效，请重新选择";
            return;
        }
        destination = match->position;
    }
    // Persistent guest-addressable scratch, independent of the active PPC
    // stack. The game copies this FVector during the synchronous native call.
    static void* vectorMemory = g_userHeap.Alloc(16);
    if (!vectorMemory)
    {
        std::lock_guard lock(stateMutex);
        snapshot.status = L"无法分配传送参数";
        return;
    }
    const uint32_t vector = g_memory.MapVirtual(vectorMemory);
    const PPCContext saved = ctx;
    // Same call as AActor::execSetLocation (82470DA8), retaining collision,
    // component, attachment and touch updates rather than writing Location.
    auto move = [&](Position target) {
        PPC_STORE_U32(vector, std::bit_cast<uint32_t>(target.x));
        PPC_STORE_U32(vector + 4, std::bit_cast<uint32_t>(target.y));
        PPC_STORE_U32(vector + 8, std::bit_cast<uint32_t>(target.z));
        ctx.r3.u64 = current.world;
        ctx.r4.u64 = current.pawn;
        ctx.r5.u64 = vector;
        ctx.r6.u64 = 0;
        ctx.r7.u64 = 0;
        ctx.r8.u64 = 0;
        __imp__sub_822FA548(ctx, base);
        const bool result = ctx.r3.u32 != 0;
        ctx = saved;
        return result;
    };
    bool moved = move(destination);
    if (!moved && usePoi)
    {
        // Interaction markers may be inside the prop. Bounded nearby probes
        // retain native collision checks; explicit XYZ requests stay exact.
        const Position center = destination;
        constexpr Position directions[] = {{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},
            {-0.7071f,-0.7071f,0},{-0.7071f,0.7071f,0},{0.7071f,-0.7071f,0},{0.7071f,0.7071f,0}};
        for (float radius : {128.0f, 256.0f})
        {
            for (const auto direction : directions)
            {
                if (FindPlayer(base) != current) break;
                const Position target{center.x + radius * direction.x, center.y + radius * direction.y, center.z};
                if (!Finite(target)) continue;
                if (move(target))
                {
                    destination = target;
                    moved = true;
                    LOG_INFO("teleport POI found nearby landing radius={}", radius);
                    break;
                }
            }
            if (moved || FindPlayer(base) != current) break;
        }
    }
    // Touch callbacks may initiate a scene transition during the native move.
    if (FindPlayer(base) != current)
    {
        std::lock_guard lock(stateMutex);
        identity = {};
        pending = Operation::None;
        snapshot.available = false;
        snapshot.bookmarkAvailable = false;
        snapshot.pois.clear();
        livePois.clear();
        ++snapshot.poiRevision;
        nextPoiRefresh = 0;
        snapshot.status = L"传送触发场景变化，已清除记录坐标";
        LOG_INFO("teleport triggered scene change, native result={}", moved);
        return;
    }
    const Position actual = ReadPosition(base, current.pawn);
    {
        std::lock_guard lock(stateMutex);
        snapshot.current = actual;
        snapshot.status = moved ? L"传送完成" : L"目标被碰撞或游戏规则拒绝";
    }
    LOG_INFO("teleport native result={} pawn={:#x} target={},{},{} actual={},{},{}",
        moved, current.pawn, destination.x, destination.y, destination.z, actual.x, actual.y, actual.z);
}
}

debug_menu::TeleportSnapshot debug_menu::GetTeleportSnapshot()
{
    std::lock_guard lock(stateMutex);
    auto result = snapshot;
    if (Now() - lastTick > 1000)
    {
        result.available = false;
        result.bookmarkAvailable = false;
        result.pois.clear();
        result.status = L"等待可控制地图的更新";
    }
    return result;
}

bool debug_menu::RequestTeleport(Position p) { return Request(Operation::Absolute, p); }
bool debug_menu::RequestTeleportOffset(Position p) { return Request(Operation::Offset, p); }
bool debug_menu::RequestSavePosition() { return Request(Operation::Save); }
bool debug_menu::RequestRestorePosition() { return Request(Operation::Restore); }
bool debug_menu::RequestPoiTeleport(uint64_t id) { return Request(Operation::Poi, {}, id); }

// Live GEngine vtable +0x108 resolves here. The prior bhHUD update only
// runs in battle and is unsuitable for free-roaming requests.
PPC_FUNC(sub_82290B60)
{
    static std::atomic<bool> observed{false};
    if (getenv("LO_TELEPORT_COMMAND_FILE") && !observed.exchange(true))
        LOG_INFO("teleport scene hook this={:#x} caller={:#x}", ctx.r3.u32, uint32_t(ctx.lr));
    const bool engine = ctx.r3.u32 == PPC_LOAD_U32(0x83315FB4);
    __imp__sub_82290B60(ctx, base);
    if (engine) Tick(ctx, base);
}
