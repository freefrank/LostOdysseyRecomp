#pragma once

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "import_game.h"

namespace install
{
inline bool ShouldPersistGamePath(const InstallResult& result)
{
    return !result.discs.empty();
}

// ReimportContent invokes this before disc/DLC backups are removed. A failed
// atomic path write throws so its selected slots are restored by the importer.
template<class WritePath>
void CommitImportedGamePath(const InstallResult& result, WritePath&& writePath)
{
    if (!ShouldPersistGamePath(result)) return;
    std::string error;
    if (!writePath(std::filesystem::path(result.destination), error))
        throw Error("Failed to update game-path.txt: " + error);
}

inline std::string_view ReviewReplaceHint(uint32_t language)
{
    constexpr std::string_view hints[] = {
        "Selected disc numbers and DLC IDs replace existing content. Unselected content and saves stay intact.",
        "僅替換所選光碟編號與 DLC ID 的現有內容；未選內容與存檔保留。",
        "選択したディスク番号とDLC IDの既存データを置き換えます。未選択のデータとセーブは残ります。",
        "선택한 디스크 번호와 DLC ID의 기존 내용만 교체합니다. 선택하지 않은 내용과 저장 파일은 유지됩니다.",
        "仅替换所选光盘编号与 DLC ID 的现有内容；未选内容和存档保留。"
    };
    return hints[language < 5 ? language : 0];
}

inline bool ShouldReportImportSuccess(const InstallResult& result, bool pathSaved)
{
    return !result.cancelled && result.error.empty() && pathSaved;
}

template<class Consume>
void JoinWorkerAndConsume(std::thread& worker, std::atomic<bool>& cancelRequested, Consume&& consume)
{
    if (!worker.joinable()) return;
    cancelRequested = true;
    worker.join();
    consume();
}

inline int ReviewActionStart(const ContentScan& scan)
{
    return static_cast<int>(scan.discs.size() + scan.packages.size());
}

// Shared by the SDL controller and its transition tests. A failed or pending
// scan must never leave an earlier source eligible for import.
struct InstallerSessionState
{
    ContentScan scanResult;
    std::vector<bool> selected;
    std::string scanError;
    bool userCancelled = false;
    bool installSuccess = false;

    enum class ImportOutcome { Complete, Cancelled, Failed };

    void BeginScan()
    {
        scanResult = {};
        selected.clear();
        scanError.clear();
        scanReady = false;
    }

    void FinishScan(ContentScan scan)
    {
        scanResult = std::move(scan);
        selected.assign(ReviewActionStart(scanResult), true);
        scanError.clear();
        scanReady = true;
    }

    void FailScan(std::string error)
    {
        BeginScan();
        scanError = std::move(error);
    }

    bool CanImport() const
    {
        return scanReady && scanError.empty() &&
               std::any_of(selected.begin(), selected.end(), [](bool checked) { return checked; });
    }

    bool ToggleSelection(size_t index)
    {
        if (!scanReady || index >= selected.size()) return false;
        selected[index] = !selected[index];
        return true;
    }

    ContentScan SelectedContent() const
    {
        ContentScan selection;
        for (size_t i = 0; i < scanResult.discs.size(); ++i)
            if (i < selected.size() && selected[i]) selection.discs.push_back(scanResult.discs[i]);
        for (size_t i = 0; i < scanResult.packages.size(); ++i)
            if (size_t index = scanResult.discs.size() + i; index < selected.size() && selected[index])
                selection.packages.push_back(scanResult.packages[i]);
        return selection;
    }

    ImportOutcome FinishImport(const InstallResult& result, bool cancelled, bool failed)
    {
        installSuccess = !failed && !cancelled && ShouldReportImportSuccess(result, true);
        userCancelled = !installSuccess && (cancelled || result.cancelled);
        return installSuccess ? ImportOutcome::Complete : userCancelled ? ImportOutcome::Cancelled : ImportOutcome::Failed;
    }

    bool BeginImport()
    {
        if (!CanImport()) return false;
        userCancelled = false;
        installSuccess = false;
        return true;
    }

private:
    bool scanReady = false;
};

struct InstallerResult
{
    bool success = false;
    bool cancelled = false;
    std::filesystem::path destination;
    std::string error;
};

// Shows the self-drawn SDL2 installer window.
// Returns success status, whether user cancelled, and the selected/installed destination.
InstallerResult ShowInstallerUI(const std::filesystem::path& executableDirectory,
                                const std::filesystem::path& initialSource = {},
                                const std::filesystem::path& initialDest = {});
}
