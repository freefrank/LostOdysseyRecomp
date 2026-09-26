#include <cassert>
#include <stdexcept>

#include "install/installer_ui.h"
#include "install/installer_navigation.h"

int main()
{
    using install::ui::Direction;
    install::ui::StickNavigation stick;
    // Explicit checks remain active in release builds, unlike assert.
    if (stick.Update(15000, 0, 0) != Direction::None) return 1;
    if (stick.Update(20000, 0, 10) != Direction::Right) return 2;
    if (stick.Update(20000, 0, 359) != Direction::None) return 3;
    if (stick.Update(20000, 0, 360) != Direction::Right) return 4;
    if (stick.Update(20000, 0, 479) != Direction::None) return 5;
    if (stick.Update(20000, 0, 480) != Direction::Right) return 6;
    if (stick.Update(0, 0, 490) != Direction::None) return 7;
    if (stick.Update(-32768, 0, 500) != Direction::Left) return 8;
    if (stick.Update(17000, -25000, 510) != Direction::Up) return 9;
    if (stick.Update(0, 25000, 520) != Direction::Down) return 10;
    if (stick.Update(0, 11000, 530) != Direction::None) return 11;
    if (stick.Update(0, 9000, 540) != Direction::None) return 12;
    if (stick.Update(0, 15000, 550) != Direction::None) return 13;
    if (stick.Update(0, 20000, 560) != Direction::Down) return 14;

    install::InstallResult discsOnly;
    discsOnly.discs.push_back(1);
    assert(install::ShouldPersistGamePath(discsOnly));

    install::InstallResult dlcOnly;
    dlcOnly.dlcImported.push_back("dlc");
    assert(!install::ShouldPersistGamePath(dlcOnly));

    install::InstallResult discsWithDlcFailure;
    discsWithDlcFailure.discs.push_back(2);
    discsWithDlcFailure.error = "DLC import failed";
    assert(install::ShouldPersistGamePath(discsWithDlcFailure));
    assert(!install::ShouldReportImportSuccess(discsWithDlcFailure, true));
    assert(!install::ShouldReportImportSuccess(discsOnly, false));
    assert(install::ShouldReportImportSuccess(discsOnly, true));
    assert(install::ShouldReportImportSuccess(dlcOnly, true));

    install::ContentScan mixed;
    mixed.discs.resize(4);
    mixed.packages.resize(2);
    assert(install::ReviewActionStart(mixed) == 6);

    // Scan A, switch to invalid B, then press Import: never reuse A.
    install::InstallerSessionState session;
    if (session.BeginImport()) return 15;
    session.BeginScan();
    session.FinishScan(mixed);
    if (!session.CanImport()) return 16;
    if (session.SelectedContent().discs.size() != 4 || session.SelectedContent().packages.size() != 2) return 24;
    if (!session.ToggleSelection(0) || !session.ToggleSelection(5)) return 25;
    if (session.SelectedContent().discs.size() != 3 || session.SelectedContent().packages.size() != 1) return 26;
    if (session.ToggleSelection(6)) return 27;
    for (size_t i = 1; i < 5; ++i) session.ToggleSelection(i);
    if (session.CanImport() || session.BeginImport()) return 28;
    session.BeginScan();
    if (session.BeginImport() || !session.scanResult.discs.empty()) return 17;
    session.FailScan("No supported sources in B");
    if (session.BeginImport() || !session.scanResult.packages.empty()) return 18;

    // Retry after cancellation must clear the result consumed by RunHost.
    session.BeginScan();
    session.FinishScan(mixed);
    if (!session.BeginImport()) return 19;
    session.userCancelled = true;
    if (!session.BeginImport() || session.userCancelled) return 20;
    session.installSuccess = true;
    if (!session.installSuccess || session.userCancelled) return 21;

    // Empty scans stay disabled, but DLC-only imports remain supported.
    session.BeginScan();
    session.FinishScan({});
    if (session.BeginImport()) return 22;
    install::ContentScan dlcScan;
    dlcScan.packages.resize(1);
    session.FinishScan(std::move(dlcScan));
    if (!session.BeginImport() || session.installSuccess) return 23;

    // The UI commit writes the effective root supplied by ReimportContent,
    // never the potentially nested path chosen in the destination browser.
    install::InstallResult committed;
    committed.discs = {2};
    committed.destination = (std::filesystem::path("selected") / "root").string();
    std::filesystem::path written;
    install::CommitImportedGamePath(committed, [&](const std::filesystem::path& root, std::string&) {
        written = root; return true;
    });
    if (written != std::filesystem::path(committed.destination)) return 29;
    committed.discs.clear(); // DLC-only must not rewrite the configured game path.
    written.clear();
    install::CommitImportedGamePath(committed, [&](const std::filesystem::path&, std::string&) {
        written = "unexpected"; return true;
    });
    if (!written.empty()) return 30;
    committed.discs = {1};
    try {
        install::CommitImportedGamePath(committed, [](const std::filesystem::path&, std::string& error) {
            error = "denied"; return false;
        });
        return 31;
    } catch (const install::Error& error) {
        if (std::string_view(error.what()).find("game-path.txt: denied") == std::string_view::npos) return 32;
    }

    // SDL_QUIT requests cancellation while the worker is still copying. The
    // final joined worker event decides whether it cancelled or committed.
    using Outcome = install::InstallerSessionState::ImportOutcome;
    session.userCancelled = true;
    if (session.FinishImport(committed, false, false) != Outcome::Complete ||
        session.userCancelled || !session.installSuccess) return 33;
    session.userCancelled = true;
    install::InstallResult cancelled; cancelled.cancelled = true;
    if (session.FinishImport(cancelled, true, true) != Outcome::Cancelled ||
        !session.userCancelled || session.installSuccess) return 34;
    install::InstallResult failed; failed.error = "commit failed and selected slots rolled back";
    if (session.FinishImport(failed, false, true) != Outcome::Failed ||
        session.userCancelled || session.installSuccess) return 35;

    // Close during a copy: request cancellation, join the worker, then consume
    // its final event. There must be no game-path write on cancellation.
    std::atomic<bool> closeRequested{false};
    install::InstallResult lastEvent;
    std::thread copying([&] {
        while (!closeRequested.load()) std::this_thread::yield();
        lastEvent.cancelled = true;
    });
    session.userCancelled = true;
    bool consumed = false;
    install::JoinWorkerAndConsume(copying, closeRequested, [&] {
        consumed = true;
        session.FinishImport(lastEvent, lastEvent.cancelled, true);
    });
    if (!consumed || !session.userCancelled || session.installSuccess || !written.empty()) return 37;

    // Close after the commit but before the completion event was read:
    // the committed result wins over the close request.
    closeRequested = false;
    std::atomic<bool> published{false};
    std::thread afterCommit([&] {
        install::CommitImportedGamePath(committed, [&](const std::filesystem::path& root, std::string&) {
            written = root; return true;
        });
        published = true;
        while (!closeRequested.load()) std::this_thread::yield();
        lastEvent = committed;
    });
    while (!published.load()) std::this_thread::yield();
    session.userCancelled = true;
    consumed = false;
    install::JoinWorkerAndConsume(afterCommit, closeRequested, [&] {
        consumed = true;
        session.FinishImport(lastEvent, false, false);
    });
    if (!consumed || session.userCancelled || !session.installSuccess ||
        written != std::filesystem::path(committed.destination)) return 38;

    for (uint32_t language = 0; language < 5; ++language)
        if (install::ReviewReplaceHint(language).empty()) return 36;

    return 0;
}
