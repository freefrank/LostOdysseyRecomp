#include <os/log_file.h>
#include <os/logger.h>

#include <cerrno>
#include <algorithm>
#include <optional>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
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

        std::optional<std::string> LogTimestamp(const std::filesystem::path& path)
        {
            const auto name = path.filename().u8string();
            if (!name.starts_with(u8"runtime-") || !name.ends_with(u8".log") || name.size() <= 12)
                return std::nullopt;
            std::string digits;
            for (const auto digit : name.substr(8, name.size() - 12))
            {
                if (digit < u8'0' || digit > u8'9')
                    return std::nullopt;
                digits.push_back(static_cast<char>(digit));
            }
            // Compare decimal timestamps without overflow or lexical 9 > 10.
            const auto first = digits.find_first_not_of('0');
            return first == std::string::npos ? "0" : digits.substr(first);
        }

        void RemoveInactiveLog(const std::filesystem::path& path)
        {
#ifdef _WIN32
            // No sharing: even older loggers that allow FILE_SHARE_DELETE
            // block this open while either their CRT or emergency sink lives.
            // Mark deletion on this handle, avoiding a close/remove race.
            const HANDLE file = CreateFileW(path.c_str(), DELETE | FILE_READ_ATTRIBUTES,
                0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            if (file == INVALID_HANDLE_VALUE)
                return;
            BY_HANDLE_FILE_INFORMATION info{};
            if (GetFileInformationByHandle(file, &info) &&
                !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
            {
                FILE_DISPOSITION_INFO disposition{TRUE};
                SetFileInformationByHandle(file, FileDispositionInfo, &disposition, sizeof(disposition));
            }
            CloseHandle(file);
#else
            // Cooperating logger processes hold a shared lock for their life.
            const int file = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
            if (file < 0)
                return;
            struct stat opened{}, named{};
            if (flock(file, LOCK_EX | LOCK_NB) == 0 && fstat(file, &opened) == 0 &&
                S_ISREG(opened.st_mode) && lstat(path.c_str(), &named) == 0 &&
                opened.st_dev == named.st_dev && opened.st_ino == named.st_ino)
                unlink(path.c_str());
            close(file);
#endif
        }
    }

    bool OpenFile(const std::filesystem::path& path)
    {
        std::lock_guard lock(g_mutex);
        if (g_file)
            return false;

        std::error_code ec;
        auto absolutePath = std::filesystem::absolute(path, ec);
        if (ec)
            return false;
        absolutePath = absolutePath.lexically_normal();

#ifdef _WIN32
        FILE* file = _wfopen(absolutePath.c_str(), L"ab");
#else
        FILE* file = fopen(absolutePath.c_str(), "ab");
#endif
        if (!file)
            return false;

#ifndef _WIN32
        if (flock(fileno(file), LOCK_SH | LOCK_NB) != 0)
        {
            fclose(file);
            return false;
        }
#endif

        g_filePath = std::move(absolutePath);
        g_file = file;
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
                std::filesystem::path path;
                std::string timestamp;
            };
            std::vector<Candidate> candidates;
            std::filesystem::directory_iterator it(current.parent_path(), ec), end;
            for (; !ec && it != end; it.increment(ec))
            {
                if (it->path() == current)
                    continue;
                const auto timestamp = LogTimestamp(it->path());
                if (!timestamp)
                    continue;
                std::error_code statusError;
                if (std::filesystem::is_regular_file(it->symlink_status(statusError)) && !statusError)
                    candidates.push_back({it->path(), *timestamp});
            }
            // An incomplete inventory must not make a newer file look oldest.
            if (ec)
                return;
            std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
            {
                if (a.timestamp.size() != b.timestamp.size())
                    return a.timestamp.size() > b.timestamp.size();
                if (a.timestamp != b.timestamp)
                    return a.timestamp > b.timestamp;
                return a.path.native() > b.path.native();
            });
            // Current always consumes one slot, even after a wall-clock reset.
            for (size_t i = 2; i < candidates.size(); ++i)
                RemoveInactiveLog(candidates[i].path);
        }
        catch (...)
        {
            // Retention is optional; a cleanup failure must not disable logging.
        }
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
