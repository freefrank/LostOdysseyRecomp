#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

// Opt-in upload of a filtered runtime log. At startup a background thread sends
// the previous session's [note], [error], [crash] and hang-watch lines, with
// user names in paths replaced, to lo.dotslash.pro. Windows only for now.
namespace os::log_collection
{
inline constexpr size_t MaxTextBytes = 60000;

// False where no uploader exists; the setting row is then shown disabled.
bool Supported();
// Reads log-collection.ini from the working directory.
void Initialize();
// -1 undecided, 0 declined, 1 enabled.
int Consent();
bool Enabled();
bool SetConsent(bool enabled);
const wchar_t* Label(uint32_t language);
const wchar_t* Message(uint32_t language);
void PromptFirstRun(uint32_t language);
// currentLog must be a default logs/runtime-<digits>.log sink. Never blocks and
// never joins: the detached thread owns everything it uses.
void StartUpload(const std::filesystem::path& currentLog);

struct Replacement
{
    std::string from, to;
};
// The lines that are sent, deduplicated, scrubbed and bounded to `limit` bytes
// of valid UTF-8 (head and tail kept when longer).
std::string Filter(std::string_view log, std::span<const Replacement> replacements, size_t limit = MaxTextBytes);
}
