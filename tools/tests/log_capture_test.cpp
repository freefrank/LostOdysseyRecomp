#include <os/log_file.h>
#include <os/logger.h>

#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

static void Require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}

static std::string ReadFile(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    Require(stream.is_open(), "open fixture output");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

struct Fixture
{
    fs::path originalDirectory = fs::current_path();
    fs::path root = fs::temp_directory_path() / ("lo-log-capture-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));

    Fixture()
    {
        // Only a directory created by this fixture is eligible for cleanup.
        Require(fs::create_directory(root), "create unique fixture directory");
    }

    ~Fixture()
    {
        {
            std::lock_guard lock(os::logger::g_mutex);
            if (os::logger::g_file)
            {
                fclose(os::logger::g_file);
                os::logger::g_file = nullptr;
            }
        }
        std::error_code ec;
        fs::current_path(originalDirectory, ec);
        fs::remove_all(root, ec);
    }
};

static size_t CheckConcurrentLines(const std::string& snapshot, const std::string& baseline)
{
    Require(snapshot.starts_with(baseline), "snapshot retains complete earlier log contents");
    Require(!snapshot.empty() && snapshot.back() == '\n', "snapshot ends at a complete line");
    std::istringstream lines(snapshot.substr(baseline.size()));
    size_t count = 0;
    for (std::string line; std::getline(lines, line); ++count)
    {
        const std::string suffix = " [info]  capture-record " + std::to_string(count);
        Require(line.starts_with("[") && line.ends_with(suffix),
            "concurrent snapshot contains ordered whole logger records");
    }
    return count;
}

int main()
{
    try
    {
        Fixture fixture;
        const auto destination = fixture.root / fs::path(u8"快照-\u00B4-\u2032.log");
        Require(os::logger::SnapshotFile(destination) == std::errc::bad_file_descriptor,
            "disabled or unopened logger reports missing log");
        Require(!fs::exists(destination), "missing logger does not create a false snapshot");

        const auto logDirectory = fixture.root / fs::path(u8"日志-\u00B4-\u2032");
        fs::create_directory(logDirectory);
        const auto source = logDirectory / fs::path(u8"运行.log");
        {
            std::ofstream stream(source, std::ios::binary);
            stream << "retained appended contents\n";
        }
        fs::current_path(fixture.root);
        Require(!os::logger::OpenFile("missing-parent/runtime.log"), "failed open reports failure");
        Require(os::logger::OpenFile(fs::relative(source)), "open Unicode relative log path");
        fs::current_path(fixture.originalDirectory);
        LOG_INFO("first snapshot marker");

        const auto unrelated = logDirectory / "runtime-newest.log";
        {
            std::ofstream stream(unrelated, std::ios::binary);
            stream << "unrelated newer process\n";
        }
        fs::last_write_time(unrelated, fs::file_time_type::clock::now() + std::chrono::hours(1));
        Require(!os::logger::OpenFile(unrelated), "second open cannot replace the active process log");
        Require(!os::logger::SnapshotFile(destination), "snapshot succeeds while logger stays open");
        const auto firstSnapshot = ReadFile(destination);
        Require(firstSnapshot == ReadFile(source), "snapshot copies complete active log bytes");
        Require(firstSnapshot.starts_with("retained appended contents\n"), "logger retains append mode");
        Require(firstSnapshot.find("first snapshot marker\n") != std::string::npos, "latest message included");
        Require(firstSnapshot.find("unrelated newer process") == std::string::npos,
            "newer unrelated log excluded despite changed working directory");

        Require(bool(os::logger::SnapshotFile(fixture.root / "missing-parent" / "runtime.log")),
            "invalid destination reports failure");
        Require(bool(os::logger::SnapshotFile(source)), "snapshot cannot overwrite its source");
        Require(bool(os::logger::SnapshotFile(source.parent_path() / "." / source.filename())),
            "normalized source alias cannot overwrite the active log");
        const auto sourceAlias = logDirectory / "runtime-hardlink.log";
        fs::create_hard_link(source, sourceAlias);
        Require(bool(os::logger::SnapshotFile(sourceAlias)), "hardlink alias cannot overwrite the active log");
        Require(ReadFile(source) == firstSnapshot, "rejected aliases preserve all source bytes");
        LOG_INFO("logging after failed snapshot");
        Require(!os::logger::SnapshotFile(destination), "snapshot can be repeated after destination failure");
        const auto baseline = ReadFile(destination);
        Require(baseline == ReadFile(source) && baseline.find("logging after failed snapshot\n") != std::string::npos,
            "failed snapshot preserves source and subsequent logging");

        std::jthread writer([]
        {
            for (size_t i = 0; i < 40; ++i)
            {
                LOG_INFO("capture-record {}", i);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
        size_t previousCount = 0;
        for (int i = 0; i < 16; ++i)
        {
            Require(!os::logger::SnapshotFile(destination), "snapshot during concurrent logging");
            const size_t count = CheckConcurrentLines(ReadFile(destination), baseline);
            Require(count >= previousCount, "repeated snapshots retain earlier concurrent records");
            previousCount = count;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        writer.join();
        Require(!os::logger::SnapshotFile(destination), "final concurrent snapshot");
        Require(CheckConcurrentLines(ReadFile(destination), baseline) == 40, "all concurrent records retained");
        Require(ReadFile(destination) == ReadFile(source), "final snapshot matches complete source");
        std::puts("current-process log snapshot checks passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "log capture fixture failed: %s\n", error.what());
        return 1;
    }
}
