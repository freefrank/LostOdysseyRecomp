#include <os/log_file.h>
#include <os/logger.h>

#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

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

static void WriteFixtureFile(const fs::path& path)
{
    std::ofstream stream(path, std::ios::binary);
    stream << "retention fixture\n";
    Require(bool(stream), "create retention fixture file");
}

struct ActiveLog
{
#ifdef _WIN32
    HANDLE file = INVALID_HANDLE_VALUE;
    explicit ActiveLog(const fs::path& path)
    {
        // Match the older logger's permissive sharing, including delete. A
        // pruner must still notice this active append handle and leave it open.
        file = CreateFileW(path.c_str(), FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Require(file != INVALID_HANDLE_VALUE, "open another active log handle");
    }
    ~ActiveLog() { CloseHandle(file); }
#else
    int file = -1;
    explicit ActiveLog(const fs::path& path)
    {
        file = open(path.c_str(), O_WRONLY | O_APPEND | O_CLOEXEC);
        Require(file >= 0, "open another active log handle");
        if (flock(file, LOCK_SH | LOCK_NB) != 0)
        {
            close(file);
            throw std::runtime_error("lock another active log");
        }
    }
    ~ActiveLog() { close(file); }
#endif
};

static void CheckRetention(const fs::path& root)
{
    const auto directory = root / fs::path(u8"日志保留") / "logs";
    fs::create_directories(directory / "captures");
    const auto current = directory / "runtime-1.log";
    for (const auto name : {"runtime-1.log", "runtime-9.log", "runtime-10.log", "runtime-11.log",
                           "runtime.log", "runtime-.log", "runtime--3.log", "runtime-newest.log",
                           "runtime-12.log.bak", "unrelated.txt"})
        WriteFixtureFile(directory / name);
    WriteFixtureFile(directory / "captures" / "runtime.log");
    WriteFixtureFile(directory / "captures" / "runtime-99.log");
    fs::create_directory(directory / "runtime-999.log");
    // mtime changes on continued writes and file copies; filename timestamps
    // determine run order. Current is retained even after a wall-clock reset.
    fs::last_write_time(directory / "runtime-9.log", fs::file_time_type::clock::now() + std::chrono::hours(24));
    os::logger::PruneDefaultLogs(directory / "missing.log");
    os::logger::PruneDefaultLogs(directory / "runtime-123.log");
    os::logger::PruneDefaultLogs(directory / "runtime.log");
    Require(fs::exists(directory / "runtime-9.log"), "missing/non-default current path cannot trigger pruning");
    os::logger::PruneDefaultLogs(directory / "." / current.filename());
    Require(fs::exists(current), "current log survives even when its timestamp is oldest");
    Require(!fs::exists(directory / "runtime-9.log"), "numeric run timestamp outranks mtime and lexical sorting");
    for (const auto name : {"runtime-10.log", "runtime-11.log", "runtime.log", "runtime-.log", "runtime--3.log",
                           "runtime-newest.log", "runtime-12.log.bak", "unrelated.txt", "runtime-999.log"})
        Require(fs::exists(directory / name), "retention preserves newest logs and non-log files/directories");
    Require(fs::exists(directory / "captures" / "runtime.log") && fs::exists(directory / "captures" / "runtime-99.log"),
        "retention never enters capture subdirectories");

    const auto wide = root / "long-timestamps";
    fs::create_directory(wide);
    for (const auto name : {"runtime-1.log", "runtime-9.log", "runtime-99999999999999999999999999.log",
                           "runtime-100000000000000000000000000.log"})
        WriteFixtureFile(wide / name);
    os::logger::PruneDefaultLogs(wide / "runtime-1.log");
    Require(!fs::exists(wide / "runtime-9.log") && fs::exists(wide / "runtime-99999999999999999999999999.log") &&
        fs::exists(wide / "runtime-100000000000000000000000000.log"), "decimal timestamp ordering cannot overflow");

    const auto busy = root / "busy-logs";
    fs::create_directory(busy);
    for (const auto name : {"runtime-100.log", "runtime-99.log", "runtime-98.log", "runtime-97.log", "runtime-96.log", "runtime-95.log"})
        WriteFixtureFile(busy / name);
    {
        ActiveLog active(busy / "runtime-96.log");
#ifdef _WIN32
        struct ReadOnlyLog
        {
            fs::path path;
            ~ReadOnlyLog() { SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL); }
        } readOnly{busy / "runtime-97.log"};
        Require(SetFileAttributesW(readOnly.path.c_str(), FILE_ATTRIBUTE_READONLY) != FALSE,
            "create deletion failure fixture");
#endif
        os::logger::PruneDefaultLogs(busy / "runtime-100.log");
        Require(fs::exists(busy / "runtime-96.log"), "active log is not deleted despite permissive delete sharing");
#ifdef _WIN32
        Require(fs::exists(busy / "runtime-97.log"), "read-only deletion failure leaves source intact");
#endif
        Require(!fs::exists(busy / "runtime-95.log"), "busy or failed deletion does not prevent other eligible cleanup");
        Require(fs::exists(busy / "runtime-99.log") && fs::exists(busy / "runtime-98.log"),
            "busy old logs cannot evict newer retained logs");
    }
    os::logger::PruneDefaultLogs(busy / "runtime-100.log");
    Require(!fs::exists(busy / "runtime-96.log") && !fs::exists(busy / "runtime-97.log"),
        "later cleanup retries files once closed or writable");
    Require(std::distance(fs::directory_iterator(busy), fs::directory_iterator{}) == 3,
        "three logs remain after retry succeeds");
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
        CheckRetention(fixture.root);
        const auto destination = fixture.root / fs::path(u8"快照-\u00B4-\u2032.log");
        Require(os::logger::SnapshotFile(destination) == std::errc::bad_file_descriptor,
            "disabled or unopened logger reports missing log");
        Require(!fs::exists(destination), "missing logger does not create a false snapshot");

        const auto logDirectory = fixture.root / fs::path(u8"日志-\u00B4-\u2032");
        fs::create_directory(logDirectory);
        for (int i = 1; i <= 5; ++i)
            WriteFixtureFile(logDirectory / ("runtime-" + std::to_string(i) + ".log"));
        const auto source = logDirectory / fs::path(u8"运行.log");
        {
            std::ofstream stream(source, std::ios::binary);
            stream << "retained appended contents\n";
        }
        fs::current_path(fixture.root);
        Require(!os::logger::OpenFile("missing-parent/runtime.log"), "failed open reports failure");
        Require(os::logger::OpenFile(fs::relative(source)), "open Unicode relative log path");
        for (int i = 1; i <= 5; ++i)
            Require(fs::exists(logDirectory / ("runtime-" + std::to_string(i) + ".log")),
                "opening custom sink never prunes neighboring runtime logs");
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
        std::puts("current-process log snapshot and default retention checks passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "log capture fixture failed: %s\n", error.what());
        return 1;
    }
}
