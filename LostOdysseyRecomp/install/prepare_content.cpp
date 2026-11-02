#include "prepare_content.h"
#include <algorithm>
#include <csignal>
#include <iostream>
#include <string_view>

namespace install::prepare_content
{
namespace
{
volatile std::sig_atomic_t interrupted = 0;
void Interrupt(int) { interrupted = 1; }
struct Signals
{
    using Handler = void (*)(int);
    Handler interrupt, terminate;
    Signals()
    {
        interrupted = 0;
        interrupt = std::signal(SIGINT, Interrupt);
        terminate = std::signal(SIGTERM, Interrupt);
    }
    ~Signals()
    {
        if (interrupt != SIG_ERR) std::signal(SIGINT, interrupt);
        if (terminate != SIG_ERR) std::signal(SIGTERM, terminate);
    }
};
}
std::optional<Request> ParseArguments(int argc, char* const argv[])
{
    bool requested = false;
    for (int i = 1; i < argc; ++i)
        requested |= std::string_view(argv[i]) == "--prepare-content";
    if (!requested) return {};

    Request request;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view argument(argv[i]);
        if (argument == "--prepare-content") continue;
        if (argument != "--source" && argument != "--destination")
            throw Error("Unknown content preparation argument: " + std::string(argument));
        if (++i == argc || std::string_view(argv[i]).starts_with("--") || !*argv[i])
            throw Error("Missing path after " + std::string(argument));
        const auto path = std::filesystem::absolute(std::filesystem::u8path(argv[i])).lexically_normal();
        if (argument == "--source") request.sources.push_back(path);
        else
        {
            if (!request.destination.empty()) throw Error("Specify --destination only once");
            request.destination = path;
        }
    }
    if (request.sources.empty() || request.destination.empty())
        throw Error("Usage: --prepare-content --source <folder-or-disc> [--source <path> ...] --destination <game-folder>");
    return request;
}

int Run(const Request& request, const std::filesystem::path& executableDirectory,
        std::ostream& output, std::ostream& errors, const Cancelled& cancelled)
{
    Signals signals;
    const auto stopped = [&] { return interrupted != 0 || (cancelled && cancelled()); };
    try
    {
        if (request.sources.empty() || request.destination.empty()) throw Error("Source and destination are required");
        output << "progress 0 Checking Lost Odyssey discs and DLC\n" << std::flush;
        const auto scan = ScanContent(request.sources, stopped);
        // An unattended setup must not silently skip a broken selected disc or DLC.
        if (!scan.rejected.empty())
            throw Error("A source could not be imported: " + scan.rejected.front().second);
        if (std::none_of(scan.discs.begin(), scan.discs.end(), [](const DiscInfo& disc) { return disc.disc == 1; }))
        {
            if (!std::filesystem::is_regular_file(request.destination / "disc1/default.xex"))
                throw Error("Disc 1 is required to prepare a playable installation");
            const auto existing = ScanSource(request.destination / "disc1", stopped);
            if (existing.discs.size() != 1 || existing.discs.front().disc != 1)
                throw Error("Disc 1 is required to prepare a playable installation");
        }
        const auto commit = [&](const InstallResult&) {
            if (stopped()) throw Error("Content preparation cancelled", true);
            std::string error;
            if (!WriteGamePath(executableDirectory, request.destination, error)) throw Error(error);
        };
        // Selecting an already imported installation only adopts its validated path.
        std::error_code ec;
        if (request.sources.size() == 1 &&
            std::filesystem::equivalent(request.sources.front(), request.destination, ec) && !ec)
        {
            commit({});
        }
        else
        {
            double last = 0;
            const auto progress = [&](uint64_t done, uint64_t total, std::string_view label) {
                const double fraction = total ? std::clamp(double(done) / double(total), 0.0, 1.0) : 0.0;
                last = std::max(last, 0.05 + fraction * 0.90);
                std::string text(label);
                std::replace(text.begin(), text.end(), '\n', ' ');
                std::replace(text.begin(), text.end(), '\r', ' ');
                output << "progress " << last << ' ' << text << '\n' << std::flush;
            };
            const auto result = ReimportContent(scan, request.destination, progress, stopped, commit);
            if (!result.warning.empty()) errors << result.warning << '\n';
        }
        output << "progress 1 Content ready\n" << std::flush;
        return 0;
    }
    catch (const Error& error)
    {
        errors << error.what() << '\n';
        return error.cancelled() ? 3 : 1;
    }
    catch (const std::exception& error)
    {
        errors << error.what() << '\n';
        return 1;
    }
}
}
