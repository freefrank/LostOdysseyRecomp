#include <os/capture_archive.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <cstdlib>
extern char** environ;
#endif
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

void Write(const fs::path& file, const std::string& value)
{
    fs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out << value;
    out.close();
    Check(!out.fail(), "fixture write failed");
}

std::string Read(const fs::path& file)
{
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

#ifdef _WIN32
constexpr auto suffix = ".zip";
int wmain(int argc, wchar_t** argv)
#else
constexpr auto suffix = ".tar.gz";
int main(int argc, char** argv)
#endif
{
    try
    {
        Check(argc == 2, "supply a new fixture output directory");
        const auto root = fs::absolute(fs::path(argv[1]));
        Check(!fs::exists(root), "fixture output must not already exist");
        fs::create_directories(root / "captures");
        auto first = root / "captures" / fs::path(u8"render-\u54c8\u54c8-'`$-1");
        std::string payload(8 * 1024 * 1024, '\0');
        for (size_t i = 0; i < payload.size(); ++i) payload[i] = char((i * 131 + i / 251) & 255);
        Write(first / "frame-01" / "payload.bin", payload);
        Write(first / "runtime.log", "closed snapshot\n");
        auto future = os::StartCaptureArchive(first);
        size_t progress = 0;
        while (future.wait_for(0ms) != std::future_status::ready)
        {
            ++progress;
            std::this_thread::sleep_for(1ms);
        }
        const auto saved = future.get();
        Check(progress > 0, "caller did not progress while archive was in flight");
        Check(saved.saved && !saved.error && !saved.cleanupError, "successful archive failed");
        Check(fs::is_regular_file(saved.archive) && !fs::exists(first), "successful publication left source");
        Check(!fs::exists(fs::path(saved.archive).concat(".partial")), "successful publication left partial ZIP");

        const auto collision = root / "captures" / "render-collision";
        Write(collision / "data.txt", "keep source");
        Write(fs::path(collision).concat(suffix), "keep existing ZIP");
        const auto conflict = os::StartCaptureArchive(collision).get();
        Check(!conflict.saved && conflict.error, "existing archive must fail");
        Check(Read(collision / "data.txt") == "keep source", "collision changed source");
        Check(Read(conflict.archive) == "keep existing ZIP", "collision replaced archive");

        const auto partial = root / "captures" / "render-partial";
        Write(partial / "data.txt", "keep source");
        Write(fs::path(partial).concat(std::string(suffix) + ".partial"), "keep existing partial");
        Check(!os::StartCaptureArchive(partial).get().saved, "partial collision must fail");
        Check(Read(fs::path(partial).concat(std::string(suffix) + ".partial")) == "keep existing partial", "deleted unrelated partial");

        #ifdef _WIN32
        const auto locked = root / "captures" / "render-locked";
        Write(locked / "data.txt", "keep locked source");
        auto handle = CreateFileW((locked / "data.txt").c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        Check(handle != INVALID_HANDLE_VALUE, "fixture lock failed");
        const auto failed = os::StartCaptureArchive(locked).get();
        CloseHandle(handle);
        Check(!failed.saved && failed.error, "unreadable source did not fail compression");
        Check(Read(locked / "data.txt") == "keep locked source", "failed compression deleted source");
        Check(!fs::exists(failed.archive) && !fs::exists(fs::path(failed.archive).concat(".partial")), "failure left partial ZIP");

        const auto cleanup = root / "captures" / "render-cleanup";
        Write(cleanup / "data.txt", "readable but cannot delete");
        handle = CreateFileW((cleanup / "data.txt").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        Check(handle != INVALID_HANDLE_VALUE, "fixture delete lock failed");
        const auto kept = os::StartCaptureArchive(cleanup).get();
        CloseHandle(handle);
        Check(kept.saved && kept.cleanupError, "cleanup failure not distinguished from ZIP failure");
        Check(fs::exists(kept.archive) && Read(cleanup / "data.txt") == "readable but cannot delete", "cleanup lost recovery data");

        #else
        const auto extracted = root / "extracted";
        fs::create_directories(extracted);
        auto archivePath = saved.archive.string();
        auto extractPath = extracted.string();
        char* args[] = {const_cast<char*>("tar"), const_cast<char*>("-xzf"), archivePath.data(),
            const_cast<char*>("-C"), extractPath.data(), nullptr};
        pid_t child;
        Check(posix_spawnp(&child, "tar", nullptr, nullptr, args, environ) == 0, "extraction spawn failed");
        int status = 0;
        Check(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0, "extraction failed");
        Check(Read(extracted / "frame-01" / "payload.bin") == payload, "archive binary content changed");
        Check(Read(extracted / "runtime.log") == "closed snapshot\n", "archive log content changed");

        const auto missing = root / "captures" / "render-missing-tar";
        Write(missing / "data.txt", "keep source");
        const std::string oldPath = std::getenv("PATH");
        setenv("PATH", "/nonexistent-capture-test", 1);
        const auto failed = os::StartCaptureArchive(missing).get();
        setenv("PATH", oldPath.c_str(), 1);
        Check(!failed.saved && failed.error, "missing compressor succeeded");
        Check(Read(missing / "data.txt") == "keep source", "failed compression removed source");
        Check(!fs::exists(failed.archive) && !fs::exists(fs::path(failed.archive).concat(".partial")), "failure left output");

        const auto unreadable = root / "captures" / "render-unreadable";
        Write(unreadable / "data.txt", "keep unreadable source");
        fs::permissions(unreadable / "data.txt", fs::perms::none);
        const auto compressionFailure = os::StartCaptureArchive(unreadable).get();
        fs::permissions(unreadable / "data.txt", fs::perms::owner_read | fs::perms::owner_write);
        Check(!compressionFailure.saved && compressionFailure.error, "unreadable source compression succeeded");
        Check(Read(unreadable / "data.txt") == "keep unreadable source", "compression failure lost source");
        Check(!fs::exists(compressionFailure.archive) && !fs::exists(fs::path(compressionFailure.archive).concat(".partial")), "compression failure left output");

        const auto linkTarget = root / "external";
        Write(linkTarget / "data.txt", "keep external");
        const auto linked = root / "captures" / "render-symlink";
        fs::create_directory_symlink(linkTarget, linked);
        Check(!os::StartCaptureArchive(linked).get().saved && Read(linkTarget / "data.txt") == "keep external", "accepted symlink cleanup root");

        const auto raced = root / "captures" / "render-raced";
        Write(raced / "data.txt", "keep source");
        const auto racedResult = os::StartCaptureArchive(raced, [&](const fs::path& path) {
            Write(fs::path(path).concat(suffix), "concurrent archive");
        }).get();
        Check(!racedResult.saved && racedResult.error, "publication overwrote concurrent archive");
        Check(Read(racedResult.archive) == "concurrent archive" && fs::exists(raced / "data.txt"), "publication collision lost data");
        #endif

        const auto prepared = root / "captures" / "render-prepared";
        Write(prepared / "data.txt", "prepare source");
        const auto caller = std::this_thread::get_id();
        bool preparedOnWorker = false;
        const auto preparedResult = os::StartCaptureArchive(prepared, [&](const fs::path& path) {
            preparedOnWorker = std::this_thread::get_id() != caller;
            Write(path / "prepared.txt", "worker data");
        }).get();
        Check(preparedOnWorker && preparedResult.saved, "prepare did not run on archive worker");

        // Dropping the final future (normal shutdown) joins the worker.
        const auto shutdown = root / "captures" / "render-shutdown";
        Write(shutdown / "data.txt", "wait for shutdown");
        { auto pending = os::StartCaptureArchive(shutdown); }
        Check(!fs::exists(shutdown) && fs::exists(fs::path(shutdown).concat(suffix)), "shutdown abandoned archive");

        const auto outside = root / "render-outside";
        Write(outside / "data.txt", "not a capture child");
        Check(!os::StartCaptureArchive(outside).get().saved && fs::exists(outside / "data.txt"), "accepted unsafe cleanup root");
        std::cout << "background archive checks passed; caller progress ticks=" << progress << '\n';
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "capture archive test failed: " << e.what() << '\n';
        return 1;
    }
}
