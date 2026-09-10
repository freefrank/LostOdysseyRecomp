#include "capture_archive.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
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
            result.archive += ".zip";
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
                const auto wait = WaitForSingleObject(process.hProcess, 60000);
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
