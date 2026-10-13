#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The game's Xbox 360 achievements. Names, texts, gamerscore and icons come
// from the title's own SPA (the XDBF resource in default.xex); unlocks arrive
// as XGIUserWriteAchievements messages and are kept in profile/achievements.ini.
namespace achievements {
struct Info {
    uint16_t id = 0;
    uint16_t score = 0;
    std::string name, unlockedText, lockedText; // UTF-8, in the game's language
    std::vector<uint8_t> iconPng;               // 64x64
    bool secret = false;                        // name and text hidden until earned
};
struct Icon {
    std::vector<uint32_t> pixels; // straight-alpha RGBA8, R in the low byte
    uint32_t width = 0, height = 0;
};

// XexLoader: the SPA resource bytes, copied out of the image.
void SetTitleData(std::vector<uint8_t> spa);
// All achievements in the game's language (English where a text is missing).
std::vector<Info> List();
std::optional<Info> Find(uint16_t id);
// Guest thread: the game awarded these. Already earned ones are ignored.
void Unlock(uint16_t id);
bool Earned(uint16_t id);
// Unix time it was earned; 0 while it is not.
int64_t EarnedTime(uint16_t id);
// The achievement's icon, decoded once (empty when the title data has none).
Icon IconOf(uint16_t id);
// Presentation thread: the next unlock to announce, oldest first.
std::optional<uint16_t> NextAnnouncement();
// "Achievement unlocked" in the game's language.
std::string UnlockedHeading();
}
