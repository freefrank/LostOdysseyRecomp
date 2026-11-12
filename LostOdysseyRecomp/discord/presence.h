#pragma once

#include <string>

namespace discord_rpc
{
enum class Mode { Exploring, Battle, Cutscene, Movie, Menu, Title };

struct Snapshot
{
    int disc = 0;
    std::string location;
    Mode mode = Mode::Exploring;

    bool operator==(const Snapshot&) const = default;
};

using LogCallback = void (*)(const char*);

#if defined(__ANDROID__) || defined(__SWITCH__) || defined(__NX__)
inline void Initialize(LogCallback = nullptr) {}
inline void Shutdown() {}
inline void SetEnabled(bool) {}
inline bool Enabled() noexcept { return false; }
inline void Publish(const Snapshot&) {}
#else
void Initialize(LogCallback log = nullptr);
void Shutdown();
void SetEnabled(bool enabled);
bool Enabled() noexcept;
// Never waits for the worker or Discord. A contended snapshot is simply dropped.
void Publish(const Snapshot& snapshot);
#endif
}
