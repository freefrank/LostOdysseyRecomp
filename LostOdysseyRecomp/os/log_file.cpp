#include <os/log_file.h>
#include <os/logger.h>

#include <cerrno>

namespace os::logger
{
    namespace
    {
        // Accessed only under g_mutex, together with the corresponding FILE*.
        std::filesystem::path g_filePath;
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

        g_filePath = std::move(absolutePath);
        g_file = file;
        return true;
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
