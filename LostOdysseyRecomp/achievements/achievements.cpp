#include "achievements.h"
#include <modding/mod_api.h>
#include <os/logger.h>
#include <os/user_paths.h>
#include <settings/config.h>
#include <xdbf_wrapper.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

namespace achievements {
namespace {
std::mutex gMutex;
std::vector<uint8_t> gSpa;
std::vector<Info> gList;            // in the game's language, built on first use
bool gListBuilt = false;
std::map<uint16_t, int64_t> gEarned; // id -> unix time
bool gEarnedLoaded = false;
std::deque<uint16_t> gAnnouncements;
std::map<uint16_t, Icon> gIcons;
// Checked every presented frame before taking the lock.
std::atomic<uint32_t> gAnnouncementCount{0};
const bool gTestAnnouncements = [] { const char* ids = std::getenv("LO_TEST_ACHIEVEMENTS"); return ids && *ids; }();

std::filesystem::path EarnedPath() { return os::user_paths::ProfileDir() / "achievements.ini"; }

void LoadEarnedLocked() {
    if (gEarnedLoaded) return;
    gEarnedLoaded = true;
    std::ifstream in(EarnedPath());
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos || line[0] == '#') continue;
        const int id = std::atoi(line.substr(0, eq).c_str());
        if (id > 0 && id < 0x10000) gEarned[uint16_t(id)] = std::atoll(line.substr(eq + 1).c_str());
    }
}

// Written in full on every unlock (a handful of lines), through a temporary file.
void SaveEarnedLocked() {
    const auto path = EarnedPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    auto temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        out << "# Achievements earned: id=unix time\n";
        for (const auto& [id, time] : gEarned) out << id << '=' << time << '\n';
        if (!out) {
            LOG_WARNING("achievements: cannot write {}", temp.string());
            return;
        }
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) LOG_WARNING("achievements: cannot replace {}: {}", path.string(), ec.message());
}

// Traditional to Simplified Chinese for the characters the SPA's Chinese texts use
// (the SPA has no Simplified Chinese table; the game's language 9 is Simplified).
constexpr std::pair<char32_t, char32_t> kSimplified[] = {
    {U'並', U'并'}, {U'來', U'来'}, {U'個', U'个'}, {U'傷', U'伤'}, {U'內', U'内'}, {U'凱', U'凯'}, {U'勵', U'励'},
    {U'區', U'区'}, {U'喚', U'唤'}, {U'單', U'单'}, {U'國', U'国'}, {U'圖', U'图'}, {U'場', U'场'}, {U'夢', U'梦'},
    {U'學', U'学'}, {U'寶', U'宝'}, {U'對', U'对'}, {U'師', U'师'}, {U'後', U'后'}, {U'憶', U'忆'}, {U'戰', U'战'},
    {U'戲', U'戏'}, {U'擊', U'击'}, {U'敗', U'败'}, {U'敵', U'敌'}, {U'時', U'时'}, {U'會', U'会'}, {U'東', U'东'},
    {U'條', U'条'}, {U'楊', U'杨'}, {U'極', U'极'}, {U'標', U'标'}, {U'滿', U'满'}, {U'爾', U'尔'}, {U'獎', U'奖'},
    {U'獲', U'获'}, {U'獸', U'兽'}, {U'現', U'现'}, {U'環', U'环'}, {U'積', U'积'}, {U'競', U'竞'}, {U'簡', U'简'},
    {U'級', U'级'}, {U'組', U'组'}, {U'結', U'结'}, {U'絲', U'丝'}, {U'經', U'经'}, {U'總', U'总'}, {U'續', U'续'},
    {U'習', U'习'}, {U'聖', U'圣'}, {U'藍', U'蓝'}, {U'蟲', U'虫'}, {U'裡', U'里'}, {U'記', U'记'}, {U'訪', U'访'},
    {U'請', U'请'}, {U'讓', U'让'}, {U'賣', U'卖'}, {U'賽', U'赛'}, {U'跡', U'迹'}, {U'軍', U'军'}, {U'輕', U'轻'},
    {U'這', U'这'}, {U'連', U'连'}, {U'遊', U'游'}, {U'過', U'过'}, {U'遺', U'遗'}, {U'隱', U'隐'}, {U'難', U'难'},
    {U'頓', U'顿'}, {U'領', U'领'}, {U'頭', U'头'}, {U'館', U'馆'}, {U'馬', U'马'}, {U'騎', U'骑'}, {U'鬥', U'斗'},
    {U'黃', U'黄'}, {U'點', U'点'}, {U'龍', U'龙'},
};

std::u32string Decode(const std::string& text) {
    std::u32string out;
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = text[i];
        const size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        char32_t cp = n == 1 ? c : n == 2 ? (c & 0x1F) : n == 3 ? (c & 0x0F) : (c & 0x07);
        for (size_t k = 1; k < n && i + k < text.size(); ++k) cp = cp << 6 | (text[i + k] & 0x3F);
        out += cp;
        i += n;
    }
    return out;
}

std::string Encode(const std::u32string& text) {
    std::string out;
    for (const char32_t cp : text) {
        if (cp < 0x80) out += char(cp);
        else if (cp < 0x800) { out += char(0xC0 | cp >> 6); out += char(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += char(0xE0 | cp >> 12); out += char(0x80 | (cp >> 6 & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
        else { out += char(0xF0 | cp >> 18); out += char(0x80 | (cp >> 12 & 0x3F)); out += char(0x80 | (cp >> 6 & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
    }
    return out;
}

std::string Simplify(const std::string& text) {
    auto chars = Decode(text);
    for (auto& c : chars)
        for (const auto& [from, to] : kSimplified)
            if (c == from) { c = to; break; }
    auto out = Encode(chars);
    // A disc is 光盘 in Simplified Chinese.
    for (size_t at; (at = out.find("光碟")) != std::string::npos;) out.replace(at, std::string("光碟").size(), "光盘");
    return out;
}

// Game language IDs (settings: INT JPN DEU FRA SPA ITA KOR CHI SCH) to SPA tables.
EXDBFLanguage SpaLanguage(uint32_t game) {
    if (game >= 1 && game <= 8) return EXDBFLanguage(game);
    if (game == 9) return XDBF_LANGUAGE_CHINESE_TRAD;
    return XDBF_LANGUAGE_ENGLISH;
}

// The selected language pack's achievements.ini (modding::AchievementTexts), read once: a language
// the game's title data does not have names its achievements and the "unlocked" heading there.
const std::map<std::string, std::string>& PackTexts() {
    static const auto texts = [] {
        std::map<std::string, std::string> found;
        const auto id = settings::GameLanguagePack();
        if (!id.empty())
            for (const auto& pack : modding::LanguagePacks())
                if (pack.id == id) {
                    found = modding::AchievementTexts(pack);
                    break;
                }
        if (!found.empty()) LOG_INFO("achievements: {} texts from language pack {}", found.size(), id);
        return found;
    }();
    return texts;
}

void BuildListLocked() {
    if (gListBuilt || gSpa.empty()) return;
    gListBuilt = true;
    const uint32_t game = settings::GameLanguage();
    XDBFWrapper spa(gSpa.data(), gSpa.size());
    const auto local = spa.GetAchievements(SpaLanguage(game));
    const auto english = spa.GetAchievements(XDBF_LANGUAGE_ENGLISH);
    // XACH flag 0x8 (show unachieved) is clear on secret achievements.
    std::map<uint16_t, uint32_t> flags;
    if (const auto block = spa.GetResource(XDBF_SPA_NAMESPACE_METADATA, XACH_SIGNATURE); block && block.BufferSize >= sizeof(XACHHeader)) {
        const auto* header = reinterpret_cast<const XACHHeader*>(block.pBuffer);
        const auto* entry = reinterpret_cast<const XACHEntry*>(block.pBuffer + sizeof(XACHHeader));
        const size_t count = std::min<size_t>(header->AchievementCount, (block.BufferSize - sizeof(XACHHeader)) / sizeof(XACHEntry));
        for (size_t i = 0; i < count; ++i) flags[entry[i].AchievementID] = entry[i].Flags;
    }
    for (size_t i = 0; i < english.size(); ++i) {
        const auto& en = english[i];
        const auto* lo = i < local.size() && local[i].ID == en.ID ? &local[i] : nullptr;
        Info info;
        info.id = en.ID;
        info.score = en.Score;
        auto pick = [&](const std::string& localized, const std::string& fallback) {
            const auto& text = localized.empty() ? fallback : localized;
            return game == 9 && !localized.empty() ? Simplify(text) : text;
        };
        info.name = pick(lo ? lo->Name : std::string{}, en.Name);
        info.unlockedText = pick(lo ? lo->UnlockedDesc : std::string{}, en.UnlockedDesc);
        info.lockedText = pick(lo ? lo->LockedDesc : std::string{}, en.LockedDesc);
        for (const auto& [field, text] : {std::pair{".name", &info.name}, std::pair{".unlocked", &info.unlockedText},
                                          std::pair{".locked", &info.lockedText}})
            if (const auto found = PackTexts().find(std::to_string(info.id) + field); found != PackTexts().end())
                *text = found->second;
        if (en.pImageBuffer && en.ImageBufferSize) info.iconPng.assign(en.pImageBuffer, en.pImageBuffer + en.ImageBufferSize);
        info.secret = flags.contains(en.ID) && !(flags[en.ID] & 8);
        gList.push_back(std::move(info));
    }
    LOG_INFO("achievements: {} in the title data, language {}", gList.size(), game);
}

// LO_TEST_ACHIEVEMENTS=16,23: announce these once, LO_TEST_ACHIEVEMENT_DELAY
// seconds (default 10) after the first presented frame, without saving them.
void QueueTestAnnouncementsLocked() {
    static const char* ids = std::getenv("LO_TEST_ACHIEVEMENTS");
    if (!ids || !*ids) return;
    static const auto first = std::chrono::steady_clock::now();
    static bool done = false;
    if (done) return;
    const char* delay = std::getenv("LO_TEST_ACHIEVEMENT_DELAY");
    if (std::chrono::steady_clock::now() - first < std::chrono::duration<double>(delay ? std::atof(delay) : 10.0)) return;
    done = true;
    std::stringstream list(ids);
    for (std::string id; std::getline(list, id, ',');)
        if (const int value = std::atoi(id.c_str()); value > 0) gAnnouncements.push_back(uint16_t(value));
    gAnnouncementCount = uint32_t(gAnnouncements.size());
}
}

void SetTitleData(std::vector<uint8_t> spa) {
    std::lock_guard lock(gMutex);
    gSpa = std::move(spa);
    gList.clear();
    gListBuilt = false;
}

std::vector<Info> List() {
    std::lock_guard lock(gMutex);
    BuildListLocked();
    return gList;
}

std::optional<Info> Find(uint16_t id) {
    std::lock_guard lock(gMutex);
    BuildListLocked();
    for (const auto& info : gList)
        if (info.id == id) return info;
    return std::nullopt;
}

void Unlock(uint16_t id) {
    std::lock_guard lock(gMutex);
    LoadEarnedLocked();
    if (gEarned.contains(id)) return;
    BuildListLocked();
    const auto known = std::find_if(gList.begin(), gList.end(), [&](const Info& info) { return info.id == id; });
    if (known == gList.end() && !gList.empty()) {
        LOG_WARNING("achievements: the game awarded unknown achievement {}", id);
        return;
    }
    gEarned[id] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    SaveEarnedLocked();
    gAnnouncements.push_back(id);
    gAnnouncementCount = uint32_t(gAnnouncements.size());
    LOG_NOTICE("achievements: unlocked {} ({}G) {}", id, known != gList.end() ? known->score : 0,
        known != gList.end() ? known->name : std::string{});
}

bool Earned(uint16_t id) {
    std::lock_guard lock(gMutex);
    LoadEarnedLocked();
    return gEarned.contains(id);
}

int64_t EarnedTime(uint16_t id) {
    std::lock_guard lock(gMutex);
    LoadEarnedLocked();
    const auto it = gEarned.find(id);
    return it == gEarned.end() ? 0 : it->second;
}

Icon IconOf(uint16_t id) {
    std::lock_guard lock(gMutex);
    if (const auto it = gIcons.find(id); it != gIcons.end()) return it->second;
    BuildListLocked();
    Icon icon;
    const auto info = std::find_if(gList.begin(), gList.end(), [&](const Info& i) { return i.id == id; });
    if (info != gList.end() && !info->iconPng.empty()) {
        SDL_IOStream* io = SDL_IOFromConstMem(info->iconPng.data(), info->iconPng.size());
        if (SDL_Surface* decoded = io ? SDL_LoadPNG_IO(io, true) : nullptr) {
            if (SDL_Surface* rgba = SDL_ConvertSurface(decoded, SDL_PIXELFORMAT_RGBA32)) {
                icon.width = uint32_t(rgba->w);
                icon.height = uint32_t(rgba->h);
                icon.pixels.resize(size_t(icon.width) * icon.height);
                for (uint32_t y = 0; y < icon.height; ++y)
                    std::memcpy(&icon.pixels[size_t(y) * icon.width], static_cast<const uint8_t*>(rgba->pixels) + size_t(y) * rgba->pitch, icon.width * 4);
                SDL_DestroySurface(rgba);
            }
            SDL_DestroySurface(decoded);
        }
    }
    return gIcons[id] = std::move(icon);
}

std::optional<uint16_t> NextAnnouncement() {
    if (!gAnnouncementCount.load(std::memory_order_relaxed) && !gTestAnnouncements) return std::nullopt;
    std::lock_guard lock(gMutex);
    QueueTestAnnouncementsLocked();
    if (gAnnouncements.empty()) return std::nullopt;
    const uint16_t id = gAnnouncements.front();
    gAnnouncements.pop_front();
    gAnnouncementCount = uint32_t(gAnnouncements.size());
    return id;
}

std::string UnlockedHeading() {
    if (const auto heading = PackTexts().find("heading"); heading != PackTexts().end()) return heading->second;
    switch (settings::GameLanguage()) {
    case 2: return "実績のロックを解除しました";
    case 3: return "Erfolg freigeschaltet";
    case 4: return "Succès déverrouillé";
    case 5: return "Logro desbloqueado";
    case 6: return "Obiettivo sbloccato";
    case 7: return "도전 과제 잠금 해제";
    case 8: return "成就已解鎖";
    case 9: return "成就已解锁";
    default: return "Achievement unlocked";
    }
}
}
