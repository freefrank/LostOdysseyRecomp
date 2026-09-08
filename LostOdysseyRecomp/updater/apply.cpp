#include "update.h"

#include <algorithm>
#include <fstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace updater
{
namespace
{
struct AppliedFile
{
    std::filesystem::path target;
    std::filesystem::path backup;
    bool hadBackup = false;
    bool placed = false;
};

bool MoveReplace(const std::filesystem::path &source, const std::filesystem::path &destination,
                 std::string &error)
{
#ifdef _WIN32
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        error = "atomic file move failed with Win32 error " + std::to_string(GetLastError());
        return false;
    }
    return true;
#else
    std::error_code filesystemError;
    std::filesystem::rename(source, destination, filesystemError);
    if (filesystemError) { error = "atomic file move failed: " + filesystemError.message(); return false; }
    return true;
#endif
}

bool RemoveFile(const std::filesystem::path &path)
{
    std::error_code error;
    return !std::filesystem::exists(path, error) || std::filesystem::remove(path, error);
}
} // namespace

bool ApplyWithRollback(const ApplyPlan &plan, const ApplyOptions &options, std::string &error)
{
    if (!ValidateApplyPlan(plan, error)) return false;
    const auto rollbackRoot = plan.stageRoot.parent_path() / "rollback";
    std::error_code filesystemError;
    std::filesystem::remove_all(rollbackRoot, filesystemError);
    if (filesystemError) { error = "could not clear operation rollback directory"; return false; }
    std::filesystem::create_directories(rollbackRoot, filesystemError);
    if (filesystemError) { error = "could not create operation rollback directory"; return false; }
    std::vector<AppliedFile> applied;
    size_t replacementCount = 0;
    bool failed = false;
    for (const auto &file : plan.files)
    {
        AppliedFile state{plan.installRoot / file.path, rollbackRoot / file.path};
        const auto staged = plan.stageRoot / file.path;
        std::filesystem::create_directories(state.target.parent_path(), filesystemError);
        if (filesystemError) { error = "could not create target directory"; applied.push_back(state); failed = true; break; }
        if (std::filesystem::exists(state.target, filesystemError))
        {
            if (!std::filesystem::is_regular_file(state.target, filesystemError))
            {
                error = "update target is not a regular file";
                applied.push_back(state);
                failed = true;
                break;
            }
            std::filesystem::create_directories(state.backup.parent_path(), filesystemError);
            if (filesystemError || !MoveReplace(state.target, state.backup, error))
            {
                applied.push_back(state);
                failed = true;
                break;
            }
            state.hadBackup = true;
        }
        if (!MoveReplace(staged, state.target, error))
        {
            applied.push_back(state);
            failed = true;
            break;
        }
        state.placed = true;
        applied.push_back(state);
        ++replacementCount;
        if (options.failAfterReplacements && replacementCount >= options.failAfterReplacements)
        {
            error = "injected replacement failure";
            failed = true;
            break;
        }
    }
    if (!failed && applied.size() == plan.files.size())
        return true;
    std::string rollbackError;
    for (auto it = applied.rbegin(); it != applied.rend(); ++it)
    {
        if (it->placed && !RemoveFile(it->target)) rollbackError = "could not remove partially updated file";
        if (it->hadBackup)
        {
            std::string moveError;
            if (!MoveReplace(it->backup, it->target, moveError)) rollbackError = moveError;
        }
    }
    if (!rollbackError.empty()) error += "; rollback incomplete: " + rollbackError;
    return false;
}

bool RollbackInstalledFiles(const ApplyPlan &plan, std::string &error)
{
    const auto rollbackRoot = plan.stageRoot.parent_path() / "rollback";
    bool restored = true;
    for (auto it = plan.files.rbegin(); it != plan.files.rend(); ++it)
    {
        std::string pathError;
        if (!IsSafePayloadPath(it->path, pathError)) { error = pathError; return false; }
        const auto target = plan.installRoot / it->path;
        const auto backup = rollbackRoot / it->path;
        if (!RemoveFile(target)) { error = "could not remove updated file during rollback"; restored = false; }
        std::error_code filesystemError;
        if (std::filesystem::exists(backup, filesystemError))
        {
            std::string moveError;
            if (!MoveReplace(backup, target, moveError)) { error = moveError; restored = false; }
        }
    }
    return restored;
}
} // namespace updater
