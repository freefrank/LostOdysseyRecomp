#include <os/log_file.h>
#include <os/logger.h>
#include <os/shader_log.h>

#include <cerrno>
#include <atomic>
#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace os::logger
{
    namespace
    {
        // Accessed only under g_mutex, together with the corresponding FILE*.
        std::filesystem::path g_filePath;

        std::optional<std::string> LogTimestamp(const std::filesystem::path& path, bool allowShader = false)
        {
            const auto name = path.filename().u8string();
            const bool shader = allowShader && name.starts_with(u8"shader-") && name.ends_with(u8".jsonl");
            if ((!shader && (!name.starts_with(u8"runtime-") || !name.ends_with(u8".log"))) || name.size() <= (shader ? 13u : 12u))
                return std::nullopt;
            std::string digits;
            for (const auto digit : name.substr(shader ? 7 : 8, name.size() - (shader ? 13 : 12)))
            {
                if (digit < u8'0' || digit > u8'9')
                    return std::nullopt;
                digits.push_back(static_cast<char>(digit));
            }
            // Compare decimal timestamps without overflow or lexical 9 > 10.
            const auto first = digits.find_first_not_of('0');
            return first == std::string::npos ? "0" : digits.substr(first);
        }

        void RemoveInactiveLogGroup(const std::vector<std::filesystem::path>& paths)
        {
#ifdef _WIN32
            // No sharing: even older loggers that allow FILE_SHARE_DELETE
            // block this open while either their CRT or emergency sink lives.
            // Mark deletion on this handle, avoiding a close/remove race.
            struct Handles {
                std::vector<HANDLE> files;
                ~Handles() { for (auto file : files) CloseHandle(file); }
            } handles;
            for (const auto& path : paths)
            {
                const HANDLE file = CreateFileW(path.c_str(), DELETE | FILE_READ_ATTRIBUTES,
                    0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
                if (file == INVALID_HANDLE_VALUE) return;
                handles.files.push_back(file);
                BY_HANDLE_FILE_INFORMATION info{};
                if (!GetFileInformationByHandle(file, &info) ||
                    (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_READONLY))) return;
            }
            // Acquire every member first: an active shader log protects its
            // runtime companion and vice versa. Incomplete deletion is retried
            // as an orphan group on a later launch.
            for (const auto file : handles.files)
            {
                FILE_DISPOSITION_INFO disposition{TRUE};
                SetFileInformationByHandle(file, FileDispositionInfo, &disposition, sizeof(disposition));
            }
#else
            // Cooperating logger processes hold a shared lock for their life.
            struct Handles {
                std::vector<int> files;
                ~Handles() { for (auto file : files) close(file); }
            } handles;
            for (const auto& path : paths)
            {
                const int file = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
                if (file < 0) return;
                handles.files.push_back(file);
                struct stat opened{}, named{};
                if (flock(file, LOCK_EX | LOCK_NB) != 0 || fstat(file, &opened) != 0 ||
                    !S_ISREG(opened.st_mode) || lstat(path.c_str(), &named) != 0 ||
                    opened.st_dev != named.st_dev || opened.st_ino != named.st_ino) return;
            }
            for (const auto& path : paths) unlink(path.c_str());
#endif
        }
#ifdef _WIN32
        std::atomic<HANDLE> g_emergencyFile{INVALID_HANDLE_VALUE};
#else
        std::atomic<int> g_emergencyFile{-1};
#endif
        static_assert(decltype(g_emergencyFile)::is_always_lock_free);
    }

    bool OpenFile(const std::filesystem::path& path)
    {
        std::unique_lock lock(g_mutex);
        if (g_file)
            return false;

        std::error_code ec;
        auto absolutePath = std::filesystem::absolute(path, ec);
        if (ec)
            return false;
        absolutePath = absolutePath.lexically_normal();

#ifdef _WIN32
        // Both handles have append-only access: a concurrent normal log write
        // must not overwrite a crash record via a stale seek-to-end position.
        constexpr DWORD sharing = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
        HANDLE normal = CreateFileW(absolutePath.c_str(), FILE_APPEND_DATA,
            sharing, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (normal == INVALID_HANDLE_VALUE)
            return false;
        const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(normal), _O_WRONLY | _O_APPEND | _O_BINARY);
        if (fd == -1)
        {
            CloseHandle(normal);
            return false;
        }
        FILE* file = _fdopen(fd, "ab");
        if (!file)
        {
            _close(fd);
            return false;
        }
        HANDLE emergency = CreateFileW(absolutePath.c_str(), FILE_APPEND_DATA,
            sharing, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (emergency == INVALID_HANDLE_VALUE)
        {
            fclose(file);
            return false;
        }
#else
        FILE* file = fopen(absolutePath.c_str(), "ab");
        if (!file)
            return false;
        if (flock(fileno(file), LOCK_SH | LOCK_NB) != 0)
        {
            fclose(file);
            return false;
        }
        const int emergency = open(absolutePath.c_str(), O_WRONLY | O_APPEND | O_CLOEXEC);
        if (emergency == -1)
        {
            fclose(file);
            return false;
        }
#endif

        g_filePath = std::move(absolutePath);
        g_emergencyFile.store(emergency, std::memory_order_release);
        g_file = file;
        const auto runtimePath = g_filePath;
        lock.unlock();
        try { shaderlog::OpenForRuntime(runtimePath); }
        catch (const std::exception& e) { LOG_WARNING("could not initialize shader log: {}", e.what()); }
        return true;
    }

    void PruneDefaultLogs(const std::filesystem::path& currentLog) noexcept
    {
        try
        {
            std::error_code ec;
            const auto current = std::filesystem::absolute(currentLog, ec).lexically_normal();
            if (ec || !LogTimestamp(current) ||
                !std::filesystem::is_regular_file(std::filesystem::symlink_status(current, ec)) || ec)
                return;

            struct Candidate
            {
                std::string timestamp;
                std::vector<std::filesystem::path> paths;
            };
            std::map<std::string, Candidate> groups;
            const auto currentTimestamp = LogTimestamp(current);
            std::filesystem::directory_iterator it(current.parent_path(), ec), end;
            for (; !ec && it != end; it.increment(ec))
            {
                const auto timestamp = LogTimestamp(it->path(), true);
                if (!timestamp || timestamp == currentTimestamp)
                    continue;
                std::error_code statusError;
                if (std::filesystem::is_regular_file(it->symlink_status(statusError)) && !statusError)
                {
                    auto& group = groups[*timestamp];
                    group.timestamp = *timestamp;
                    group.paths.push_back(it->path());
                }
            }
            // An incomplete inventory must not make a newer file look oldest.
            if (ec)
                return;
            std::vector<Candidate> candidates;
            for (auto& [timestamp, group] : groups) candidates.push_back(std::move(group));
            std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
            {
                if (a.timestamp.size() != b.timestamp.size())
                    return a.timestamp.size() > b.timestamp.size();
                return a.timestamp > b.timestamp;
            });
            // Current always consumes one slot, even after a wall-clock reset.
            for (size_t i = 2; i < candidates.size(); ++i)
                RemoveInactiveLogGroup(candidates[i].paths);
        }
        catch (...)
        {
            // Retention is optional; a cleanup failure must not disable logging.
        }
    }

    void EmergencyWrite(const char* data, size_t size) noexcept
    {
        if (!data || !size)
            return;
#ifdef _WIN32
        const HANDLE outputs[] = {g_emergencyFile.load(std::memory_order_acquire), GetStdHandle(STD_ERROR_HANDLE)};
        for (unsigned index = 0; index < 2; ++index)
        {
            const HANDLE output = outputs[index];
            if (!output || output == INVALID_HANDLE_VALUE)
                continue;
            // A synchronous write to a full redirected pipe can hang forever.
            // Once a runtime sink exists, never let secondary pipe mirroring
            // prevent subsequent essential records from reaching that file.
            // Without a runtime sink, stderr remains a best-effort fallback.
            if (index == 1 && outputs[0] != INVALID_HANDLE_VALUE && GetFileType(output) == FILE_TYPE_PIPE)
                continue;
            size_t offset = 0;
            while (offset < size)
            {
                const DWORD count = static_cast<DWORD>((std::min)(size - offset, size_t(MAXDWORD)));
                DWORD written = 0;
                if (!WriteFile(output, data + offset, count, &written, nullptr) || !written)
                    break;
                offset += written;
            }
        }
#else
        const int outputs[] = {g_emergencyFile.load(std::memory_order_acquire), STDERR_FILENO};
        for (int output : outputs)
        {
            if (output < 0)
                continue;
            size_t offset = 0;
            while (offset < size)
            {
                const auto written = write(output, data + offset, size - offset);
                if (written < 0 && errno == EINTR)
                    continue;
                if (written <= 0)
                    break;
                offset += static_cast<size_t>(written);
            }
        }
#endif
    }

    std::error_code SnapshotFile(const std::filesystem::path& destination)
    {
        std::lock_guard lock(g_mutex);
        if (!g_file || g_filePath.empty())
            return std::make_error_code(std::errc::bad_file_descriptor);

        std::error_code ec;
        auto absoluteDestination = std::filesystem::absolute(destination, ec);
        if (ec)
            return ec;
        absoluteDestination = absoluteDestination.lexically_normal();
        if (absoluteDestination == g_filePath)
            return std::make_error_code(std::errc::invalid_argument);

        // copy_file(overwrite_existing) does not reliably reject a source alias
        // on every implementation. Check file identity before allowing a write.
        if (std::filesystem::equivalent(g_filePath, absoluteDestination, ec))
            return std::make_error_code(std::errc::invalid_argument);
        if (ec && ec != std::errc::no_such_file_or_directory)
            return ec;

        errno = 0;
        if (fflush(g_file) != 0)
            return errno ? std::error_code(errno, std::generic_category())
                : std::make_error_code(std::errc::io_error);

        ec.clear();
        const bool copied = std::filesystem::copy_file(g_filePath, absoluteDestination,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
            return ec;
        return copied ? std::error_code{} : std::make_error_code(std::errc::io_error);
    }
}
