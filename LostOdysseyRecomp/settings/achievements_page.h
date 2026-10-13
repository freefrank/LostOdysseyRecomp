#pragma once
// The Achievements page (System → Achievements): the notification setting, a
// summary, then every achievement, earned ones first (newest first), then the
// locked ones in id order. menu.cpp owns the input and the snapshot; this keeps
// the data model and the icon conversion.
#include "menu_assets.h"
#include <achievements/achievements.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace settings::achievements_page
{
struct Item
{
    achievements::Info info;
    int64_t earnedTime = 0; // unix time, 0 while locked
    achievements::Icon icon;
    bool Earned() const { return earnedTime > 0; }
    // A locked secret hides its name, text and icon.
    bool Hidden() const { return info.secret && !Earned(); }
};
// The game's achievements and when they were earned. Icons are decoded only
// when asked for (the page), not for the System row's summary.
inline std::vector<Item> LoadAchievementItems(bool withIcons)
{
    std::vector<Item> items;
    for (auto &info : achievements::List())
    {
        Item item;
        item.earnedTime = achievements::EarnedTime(info.id);
        if (withIcons) item.icon = achievements::IconOf(info.id);
        item.info = std::move(info);
        items.push_back(std::move(item));
    }
    return items;
}
inline void Sort(std::vector<Item> &items)
{
    std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
        if (a.Earned() != b.Earned()) return a.Earned();
        if (a.Earned() && a.earnedTime != b.earnedTime) return a.earnedTime > b.earnedTime;
        return a.info.id < b.info.id;
    });
}
struct Summary
{
    int earned = 0, total = 0, earnedScore = 0, totalScore = 0;
};
inline Summary Summarize(const std::vector<Item> &items)
{
    Summary summary;
    for (const auto &item : items)
    {
        ++summary.total;
        summary.totalScore += item.info.score;
        if (item.Earned())
        {
            ++summary.earned;
            summary.earnedScore += item.info.score;
        }
    }
    return summary;
}
// YYYY-MM-DD in local time; empty for 0.
inline std::wstring FormatDate(int64_t unixTime)
{
    if (unixTime <= 0) return {};
    const std::time_t time = std::time_t(unixTime);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &time) != 0) return {};
#else
    if (!localtime_r(&time, &local)) return {};
#endif
    char text[16];
    std::snprintf(text, sizeof text, "%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    return std::wstring(text, text + std::char_traits<char>::length(text));
}
// The menu's straight-alpha BGRA from the achievement's RGBA (R in the low
// byte). Locked achievements are desaturated and darkened. Null without pixels.
inline std::shared_ptr<const menu_assets::Image> MakeIcon(const achievements::Icon &icon, bool earned)
{
    if (!icon.width || !icon.height || icon.pixels.size() < size_t(icon.width) * icon.height) return nullptr;
    auto image = std::make_shared<menu_assets::Image>();
    image->width = icon.width;
    image->height = icon.height;
    image->pixels.reserve(size_t(icon.width) * icon.height);
    for (size_t i = 0; i < size_t(icon.width) * icon.height; ++i)
    {
        const uint32_t p = icon.pixels[i];
        uint32_t r = p & 255, g = (p >> 8) & 255, b = (p >> 16) & 255;
        const uint32_t a = p >> 24;
        if (!earned)
        {
            const uint32_t gray = (r * 30 + g * 59 + b * 11) / 100 * 45 / 100;
            r = g = b = gray;
        }
        image->pixels.push_back((a << 24) | (r << 16) | (g << 8) | b);
    }
    return image;
}
} // namespace settings::achievements_page
