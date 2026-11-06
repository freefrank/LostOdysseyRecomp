#pragma once
#include <cstdint>
#include <filesystem>
#include <string_view>

namespace install::compaction
{
enum class Mode { Skip, Ask, Run };
inline Mode StartupMode(bool automated, bool ask, std::string_view overrideValue)
{
    if (overrideValue == "0") return Mode::Skip;
    if (overrideValue == "1") return Mode::Run;
    return automated || !ask ? Mode::Skip : Mode::Ask;
}

enum class Action { None, Activate, Free, Later, Never, Previous, Next, Close };
enum class Choice { Free, Later, Never, Closed };
struct Offer
{
    Choice selected = Choice::Later; // Confirmation defaults to the non-destructive choice.
    bool finished = false;
    void Apply(Action action)
    {
        if (finished) return;
        if (action == Action::Previous) selected = Choice((int(selected) + 2) % 3);
        else if (action == Action::Next) selected = Choice((int(selected) + 1) % 3);
        else if (action != Action::None)
        {
            if (action == Action::Free) selected = Choice::Free;
            if (action == Action::Later) selected = Choice::Later;
            if (action == Action::Never) selected = Choice::Never;
            if (action == Action::Close) selected = Choice::Closed;
            finished = true;
        }
    }
};

// Called once before FileSystem::Init, while no disc archive is open.
void PrepareAtStartup(const std::filesystem::path& gameFolder);
}
