#pragma once

#include <os/logger.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#ifdef _WIN32
#include <share.h>
#else
#include <sys/file.h>
#include <unistd.h>
#endif

// Shader diagnostics use the runtime logger's monotonic clock and thread tags.
// This header is also used by small shader tools without the game log_file.cpp.
namespace os::shaderlog
{
    enum class HashNamespace { None, RendererByteFnv, CommandWordFnv, HlslSourceSha256, BuiltinKeyFnv };

    inline const char* NamespaceName(HashNamespace value)
    {
        switch (value)
        {
        case HashNamespace::RendererByteFnv: return "renderer-byte-fnv1a64";
        case HashNamespace::CommandWordFnv: return "command-processor-word-fnv1a64";
        case HashNamespace::HlslSourceSha256: return "hlsl-source-sha256";
        case HashNamespace::BuiltinKeyFnv: return "builtin-key-byte-fnv1a64";
        default: return "none";
        }
    }

    inline std::string JsonString(std::string_view value)
    {
        std::string out = "\"";
        for (size_t i = 0; i < value.size(); ++i)
        {
            const auto c = static_cast<unsigned char>(value[i]);
            if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
            else if (c < 0x20) out += fmt::format("\\u{:04x}", c);
            else if (c < 0x80) out += char(c);
            else
            {
                // DXC normally returns UTF-8. Preserve valid UTF-8, but escape
                // each malformed byte rather than producing unreadable JSONL.
                const size_t length = c >= 0xC2 && c <= 0xDF ? 2 : c >= 0xE0 && c <= 0xEF ? 3 : c >= 0xF0 && c <= 0xF4 ? 4 : 0;
                bool valid = length && i + length <= value.size();
                for (size_t j = 1; valid && j < length; ++j)
                    valid = (static_cast<unsigned char>(value[i + j]) & 0xC0) == 0x80;
                if (valid && length >= 3)
                {
                    const auto next = static_cast<unsigned char>(value[i + 1]);
                    valid = !(c == 0xE0 && next < 0xA0) && !(c == 0xED && next >= 0xA0) &&
                        !(c == 0xF0 && next < 0x90) && !(c == 0xF4 && next >= 0x90);
                }
                if (valid) { out.append(value.substr(i, length)); i += length - 1; }
                else out += fmt::format("\\u{:04x}", c);
            }
        }
        return out + '"';
    }

    inline std::string Utf8(const std::filesystem::path& path)
    {
        const auto text = path.u8string();
        return {text.begin(), text.end()};
    }

    class Sink
    {
        std::mutex mutex_;
        FILE* file_ = nullptr;
        std::filesystem::path path_;
        std::string session_;
        uint64_t sequence_ = 0;
        size_t pending_ = 0;
        double lastFlush_ = 0;
        std::error_code error_;
        char buffer_[64 * 1024]{};

        std::error_code FlushLocked()
        {
            if (error_) return error_;
            if (!file_) return std::make_error_code(std::errc::bad_file_descriptor);
            errno = 0;
            if (fflush(file_) != 0)
                error_ = errno ? std::error_code(errno, std::generic_category()) : std::make_error_code(std::errc::io_error);
            pending_ = 0;
            lastFlush_ = logger::ElapsedSeconds();
            return error_;
        }

    public:
        ~Sink() { Close(); }
        // Open once; never truncate a caller's existing file. The runtime path
        // is recorded in a session event by OpenForRuntime below.
        bool Open(const std::filesystem::path& path, std::string session)
        {
            std::lock_guard lock(mutex_);
            if (file_) return false;
            std::error_code ec;
            auto absolute = std::filesystem::absolute(path, ec).lexically_normal();
            if (ec) return false;
#ifdef _WIN32
            // Permit F1/read-only diagnostics while excluding another writer.
            // _wfopen_s uses secure sharing and can reject live snapshots.
            file_ = _wfsopen(absolute.c_str(), L"ab", _SH_DENYWR);
            if (!file_) return false;
#else
            file_ = fopen(absolute.c_str(), "ab");
            if (!file_) return false;
            if (flock(fileno(file_), LOCK_SH | LOCK_NB) != 0) { fclose(file_); file_ = nullptr; return false; }
#endif
            setvbuf(file_, buffer_, _IOFBF, sizeof(buffer_));
            path_ = std::move(absolute);
            session_ = std::move(session);
            error_.clear();
            sequence_ = pending_ = 0;
            lastFlush_ = logger::ElapsedSeconds();
            return true;
        }

        bool IsOpen()
        {
            std::lock_guard lock(mutex_);
            return file_ && !error_;
        }

        std::filesystem::path Path()
        {
            std::lock_guard lock(mutex_);
            return path_;
        }

        // Callers emit shader creation/cache events and explicitly enabled
        // traces, never a new unconditional per-draw event. Ordinary writes use
        // CRT buffering; flush at 128 events or the next event after one second.
        // Warnings/errors, snapshots and normal exit flush immediately.
        uint64_t Write(LogType type, std::string_view category, HashNamespace hashNamespace, std::string_view detail)
        {
            std::lock_guard lock(mutex_);
            if (!file_ || error_) return 0;
            const double elapsed = logger::ElapsedSeconds();
            const uint64_t id = ++sequence_;
            const auto line = fmt::format(
                "{{\"schema\":1,\"session\":{},\"sequence\":{},\"elapsed_seconds\":{:.6f},\"thread\":{},\"level\":{},\"category\":{},\"hash_namespace\":{},\"detail\":{}}}\n",
                JsonString(session_), id, elapsed, logger::ThreadTag(),
                JsonString(type == LogType::Error ? "error" : type == LogType::Warning ? "warning" : "info"),
                JsonString(category), JsonString(NamespaceName(hashNamespace)), JsonString(detail));
            errno = 0;
            if (fwrite(line.data(), 1, line.size(), file_) != line.size())
            {
                error_ = errno ? std::error_code(errno, std::generic_category()) : std::make_error_code(std::errc::io_error);
                return 0;
            }
            if (++pending_ >= 128 || elapsed - lastFlush_ >= 1 || type == LogType::Warning || type == LogType::Error)
                if (FlushLocked()) return 0;
            return id;
        }

        std::error_code Flush()
        {
            std::lock_guard lock(mutex_);
            return FlushLocked();
        }

        std::error_code Close()
        {
            std::lock_guard lock(mutex_);
            if (!file_) return error_;
            FlushLocked();
            errno = 0;
            if (fclose(file_) != 0 && !error_)
                error_ = errno ? std::error_code(errno, std::generic_category()) : std::make_error_code(std::errc::io_error);
            file_ = nullptr;
            return error_;
        }

        std::error_code Snapshot(const std::filesystem::path& destination)
        {
            std::lock_guard lock(mutex_);
            if (!file_) return std::make_error_code(std::errc::bad_file_descriptor);
            std::error_code ec;
            const auto target = std::filesystem::absolute(destination, ec).lexically_normal();
            if (ec) return ec;
            if (target == path_ || std::filesystem::equivalent(path_, target, ec))
                return std::make_error_code(std::errc::invalid_argument);
            if (ec && ec != std::errc::no_such_file_or_directory) return ec;
            if (const auto flush = FlushLocked()) return flush;
            ec.clear();
            const bool copied = std::filesystem::copy_file(path_, target, std::filesystem::copy_options::overwrite_existing, ec);
            return ec ? ec : copied ? std::error_code{} : std::make_error_code(std::errc::io_error);
        }
    };

    inline Sink& Current() { static Sink sink; return sink; }

    // The game's normal window/guest exit paths use std::_Exit, which skips
    // destructors. Close before those calls; later racing events fall back to
    // the immediately flushed runtime logger rather than buffering new bytes.
    inline void CloseForExit()
    {
        if (const auto error = Current().Close())
            LOG_WARNING("shader log close failed: {}", error.message());
    }

    template<typename... Args>
    inline void Log(LogType type, std::string_view category, HashNamespace hashNamespace,
        fmt::format_string<Args...> format, Args&&... args)
    {
        const auto detail = fmt::format(format, std::forward<Args>(args)...);
        const auto sequence = Current().Write(type, category, hashNamespace, detail);
        if (!sequence)
        {
            // Disabled/unavailable/failed shader sinks must not hide failures.
            logger::Log(type, nullptr, "shader [{}; {}]: {}", category, NamespaceName(hashNamespace), detail);
        }
        else if (type == LogType::Warning || type == LogType::Error)
        {
            // Identity is at the start of callers' messages. Keep the first line
            // here; full compiler diagnostics remain in the referenced event.
            const auto end = detail.find_first_of("\r\n");
            logger::Log(type, nullptr, "shader event {} [{}; {}]: {} (details: {})", sequence,
                category, NamespaceName(hashNamespace), detail.substr(0, end), Utf8(Current().Path()));
        }
    }

    inline void OpenForRuntime(const std::filesystem::path& runtimePath)
    {
#ifdef _WIN32
        const wchar_t* configured = _wgetenv(L"LO_SHADER_LOG_FILE");
        if (configured && std::wstring_view(configured) == L"0") return;
#else
        const char* configured = std::getenv("LO_SHADER_LOG_FILE");
        if (configured && std::string_view(configured) == "0") return;
#endif
        const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto name = Utf8(runtimePath.filename());
        std::string session = std::to_string(ticks);
        if (name.starts_with("runtime-") && name.ends_with(".log"))
        {
            const auto stamp = name.substr(8, name.size() - 12);
            if (!stamp.empty() && stamp.find_first_not_of("0123456789") == std::string::npos) session = stamp;
        }
        const auto path = configured ? std::filesystem::path(configured) :
            runtimePath.parent_path() / ("shader-" + session + ".jsonl");
        std::error_code ec;
        // Prevent a custom shader sink from corrupting the runtime log.
        const auto absolute = std::filesystem::absolute(path, ec).lexically_normal();
        if (ec || absolute == runtimePath || std::filesystem::equivalent(runtimePath, absolute, ec))
        {
            LOG_WARNING("shader log path aliases runtime log or is invalid: {}", Utf8(path));
            return;
        }
        ec.clear();
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
        if (ec || !Current().Open(path, session))
        {
            LOG_WARNING("could not open shader log: {}", Utf8(path));
            return;
        }
        LOG_INFO("shader log: {} (session {}; shared runtime elapsed_seconds clock)", Utf8(Current().Path()), session);
        Log(LogType::Info, "session", HashNamespace::None, "runtime_log={} unix_microseconds={} hash namespaces are distinct; no implicit mapping",
            Utf8(runtimePath), ticks);
    }

    inline void CaptureSnapshot(const std::filesystem::path& directory, uint64_t frame)
    {
        try
        {
            const auto error = Current().Snapshot(directory / "shader.jsonl");
            std::ofstream status(directory / "shader-log-status.txt");
            status << "frame=" << frame << '\n';
            if (error)
            {
                std::error_code ignored;
                std::filesystem::remove(directory / "shader.jsonl", ignored);
                status << "status=unavailable\nreason=" << error.message() << '\n';
                LOG_WARNING("render capture shader log unavailable: {}", error.message());
            }
            else status << "status=included\nfile=shader.jsonl\nsource=" << Utf8(Current().Path()) <<
                "\nscope=Current process shader log; flushed after frame export before ZIP creation.\n";
            status.close();
            if (status.fail()) LOG_WARNING("render capture: could not write shader log status");
        }
        catch (const std::exception& e) { LOG_WARNING("render capture shader log: {}", e.what()); }
    }
}

#define SHADER_LOG_INFO(category, space, ...) os::shaderlog::Log(LogType::Info, category, os::shaderlog::HashNamespace::space, __VA_ARGS__)
#define SHADER_LOG_WARNING(category, space, ...) os::shaderlog::Log(LogType::Warning, category, os::shaderlog::HashNamespace::space, __VA_ARGS__)
#define SHADER_LOG_ERROR(category, space, ...) os::shaderlog::Log(LogType::Error, category, os::shaderlog::HashNamespace::space, __VA_ARGS__)
