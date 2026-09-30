#include "capture_archive.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace os
{
    namespace
    {
#ifdef _WIN32
        struct Handle
        {
            HANDLE value = nullptr;
            ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
        };

        std::error_code WindowsError() { return {int(GetLastError()), std::system_category()}; }

        bool IsPlainDirectory(const std::filesystem::path& directory)
        {
            const auto attributes = GetFileAttributesW(directory.c_str());
            return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
                !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
        }
#endif

        CaptureArchiveResult Archive(std::filesystem::path directory, std::function<void(const std::filesystem::path&)> prepare)
        {
            CaptureArchiveResult result;
            result.directory = std::move(directory);
            result.archive = result.directory;
#ifdef __linux__
            result.archive += ".tar.gz";
#else
            result.archive += ".zip";
#endif
#ifdef _WIN32
            auto temporary = result.archive;
            temporary += L".partial";
            bool ownsTemporary = false;
            try
            {
                // Cleanup is limited to this exact completed capture child.
                const auto parent = std::filesystem::canonical(result.directory.parent_path());
                const auto expected = parent / result.directory.filename();
                if (parent.filename() != L"captures" ||
                    !result.directory.filename().wstring().starts_with(L"render-") ||
                    !IsPlainDirectory(result.directory) || std::filesystem::canonical(result.directory) != expected)
                    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
                if (std::filesystem::exists(result.archive) || std::filesystem::exists(temporary))
                    throw std::system_error(std::make_error_code(std::errc::file_exists));

                if (prepare) prepare(result.directory);

                // PowerShell single-quoted literals escape only apostrophes.
                // Use the system executable directly, with no shell expansion.
                const auto literal = [](const std::wstring& value) {
                    std::wstring quoted = L"'";
                    for (auto c : value) { quoted += c; if (c == L'\'') quoted += c; }
                    return quoted + L"'";
                };
                wchar_t systemDirectory[MAX_PATH]{};
                if (!GetSystemDirectoryW(systemDirectory, MAX_PATH)) throw std::system_error(WindowsError());
                const auto executable = std::filesystem::path(systemDirectory) / L"WindowsPowerShell/v1.0/powershell.exe";
                std::wstring command = L"\"" + executable.wstring() + L"\" -NoLogo -NoProfile -NonInteractive -Command \"$ErrorActionPreference='Stop'; Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::CreateFromDirectory(" +
                    literal(result.directory.wstring()) + L"," + literal(temporary.wstring()) + L",[System.IO.Compression.CompressionLevel]::Optimal,$false)\"";

                // An abrupt game exit must not leave an orphan compressor.
                // Start suspended so it belongs to the job before writing files.
                Handle job{CreateJobObjectW(nullptr, nullptr)};
                if (!job.value) throw std::system_error(WindowsError());
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
                limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                if (!SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
                    throw std::system_error(WindowsError());
                STARTUPINFOW startup{}; startup.cb = sizeof(startup);
                PROCESS_INFORMATION process{};
                if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                    CREATE_NO_WINDOW | CREATE_SUSPENDED | BELOW_NORMAL_PRIORITY_CLASS, nullptr, nullptr, &startup, &process))
                    throw std::system_error(WindowsError());
                Handle processHandle{process.hProcess}, threadHandle{process.hThread};
                if (!AssignProcessToJobObject(job.value, process.hProcess))
                {
                    const auto error = WindowsError();
                    TerminateProcess(process.hProcess, 1);
                    WaitForSingleObject(process.hProcess, INFINITE);
                    throw std::system_error(error);
                }
                ownsTemporary = true;
                if (ResumeThread(process.hThread) == DWORD(-1)) throw std::system_error(WindowsError());
                const auto wait = WaitForSingleObject(process.hProcess, 180000);
                if (wait != WAIT_OBJECT_0)
                {
                    TerminateJobObject(job.value, 1);
                    WaitForSingleObject(process.hProcess, INFINITE);
                    throw std::system_error(std::make_error_code(std::errc::timed_out));
                }
                DWORD code = 1;
                if (!GetExitCodeProcess(process.hProcess, &code)) throw std::system_error(WindowsError());
                if (code != 0) throw std::system_error(std::make_error_code(std::errc::io_error));
                std::filesystem::rename(temporary, result.archive);
                result.saved = true;
                // Recheck the resolved target immediately before recursive removal.
                if (!IsPlainDirectory(result.directory) || std::filesystem::canonical(result.directory) != expected)
                    result.cleanupError = std::make_error_code(std::errc::invalid_argument);
                else
                    std::filesystem::remove_all(result.directory, result.cleanupError);
            }
            catch (const std::system_error& e)
            {
                (result.saved ? result.cleanupError : result.error) = e.code();
            }
            catch (...)
            {
                (result.saved ? result.cleanupError : result.error) = std::make_error_code(std::errc::io_error);
            }
            if (ownsTemporary && !result.saved)
            {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
            }
#elif defined(__linux__)
            auto temporary = result.archive;
            temporary += ".partial";
            bool ownsTemporary = false;
            int output = -1;
            try
            {
                const auto parent = std::filesystem::canonical(result.directory.parent_path());
                const auto expected = parent / result.directory.filename();
                const auto plainDirectory = [&] {
                    return std::filesystem::is_directory(std::filesystem::symlink_status(result.directory)) &&
                        std::filesystem::canonical(result.directory) == expected;
                };
                if (parent.filename() != "captures" ||
                    !result.directory.filename().string().starts_with("render-") || !plainDirectory())
                    throw std::system_error(std::make_error_code(std::errc::invalid_argument));
                const auto exists = [](const std::filesystem::path& path) {
                    return std::filesystem::exists(std::filesystem::symlink_status(path));
                };
                if (exists(result.archive) || exists(temporary))
                    throw std::system_error(std::make_error_code(std::errc::file_exists));
                if (prepare) prepare(result.directory);

                // Reserve our partial exclusively, then give tar its output descriptor.
                // Paths are argv entries; neither capture names nor filenames enter a shell.
                output = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
                if (output < 0) throw std::system_error(errno, std::generic_category());
                ownsTemporary = true;
                posix_spawn_file_actions_t actions;
                int error = posix_spawn_file_actions_init(&actions);
                if (error) throw std::system_error(error, std::generic_category());
                error = posix_spawn_file_actions_adddup2(&actions, output, STDOUT_FILENO);
                auto source = result.directory.string();
                char* arguments[] = {const_cast<char*>("tar"), const_cast<char*>("-czf"),
                    const_cast<char*>("-"), const_cast<char*>("-C"), source.data(), const_cast<char*>("."), nullptr};
                pid_t child = -1;
                if (!error) error = posix_spawnp(&child, "tar", &actions, nullptr, arguments, environ);
                posix_spawn_file_actions_destroy(&actions);
                if (error) throw std::system_error(error, std::generic_category());
                close(output);
                output = -1;
                int status = 0;
                pid_t waited;
                do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
                if (waited < 0) throw std::system_error(errno, std::generic_category());
                if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
                    throw std::system_error(std::make_error_code(std::errc::io_error));
                // link publishes atomically without replacing an archive created meanwhile.
                if (link(temporary.c_str(), result.archive.c_str()) != 0)
                    throw std::system_error(errno, std::generic_category());
                result.saved = true;
                std::filesystem::remove(temporary);
                if (!plainDirectory())
                    result.cleanupError = std::make_error_code(std::errc::invalid_argument);
                else
                    std::filesystem::remove_all(result.directory, result.cleanupError);
            }
            catch (const std::system_error& e)
            {
                (result.saved ? result.cleanupError : result.error) = e.code();
            }
            catch (...)
            {
                (result.saved ? result.cleanupError : result.error) = std::make_error_code(std::errc::io_error);
            }
            if (output >= 0) close(output);
            if (ownsTemporary)
            {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
            }
#else
            result.error = std::make_error_code(std::errc::operation_not_supported);
#endif
            return result;
        }
    }

    std::future<CaptureArchiveResult> StartCaptureArchive(std::filesystem::path directory,
        std::function<void(const std::filesystem::path&)> prepare)
    {
        return std::async(std::launch::async, Archive, std::filesystem::absolute(directory).lexically_normal(), std::move(prepare));
    }
}
