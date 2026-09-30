#include <cassert>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "install/import_game.h"

namespace
{
void Require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error("[extracted-dlc] " + message);
}

std::vector<uint8_t> ReadBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    Require(static_cast<bool>(input), "could not open " + path.string());
    return {std::istreambuf_iterator<char>(input), {}};
}

// One STFS directory block and one payload block, with allocation-chain metadata.
void WriteTinyStfs(const std::filesystem::path& path)
{
    std::vector<uint8_t> bytes(0xd000, 0);
    auto be32 = [&](size_t offset, uint32_t value) {
        for (int i = 0; i < 4; ++i) bytes[offset + i] = static_cast<uint8_t>(value >> (24 - i * 8));
    };
    std::memcpy(bytes.data(), "LIVE", 4);
    bytes[0x32c] = 1;
    be32(0x340, 0xa000);
    be32(0x344, 2);
    be32(0x360, 0x4D5307FA);
    bytes[0x379] = 0x24;
    bytes[0x37b] = 1; // one hash table copy
    bytes[0x37c] = 1; // one directory block, starting at block zero
    be32(0x395, 2);
    bytes[0x412] = 'T';
    const size_t directory = 0xb000;
    std::memcpy(bytes.data() + directory, "payload.bin", 11);
    bytes[directory + 40] = 11;
    bytes[directory + 41] = bytes[directory + 44] = 1;
    bytes[directory + 47] = 1;
    bytes[directory + 50] = bytes[directory + 51] = 0xff;
    be32(directory + 52, 4);
    std::memcpy(bytes.data() + 0xc000, "data", 4);
    for (size_t block = 0; block < 2; ++block)
    {
        const size_t record = 0xa000 + block * 24;
        bytes[record + 20] = 0x80;
        bytes[record + 21] = bytes[record + 22] = bytes[record + 23] = 0xff;
    }
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void RunDlcIoTest()
{
    const auto root = std::filesystem::temp_directory_path() /
        ("lo-dlc-io-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { install::SetTestDlcWriteFailure({}, {}); std::error_code ec; std::filesystem::remove_all(path, ec); }
    } cleanup{root};
    std::filesystem::create_directories(root);
    WriteTinyStfs(root / "source.stfs");
    const auto scan = install::ScanContent(root / "source.stfs");
    Require(scan.packages.size() == 1, "tiny STFS fixture was rejected");
    const auto destination = root / "game";
    const auto installed = destination / "dlc" / scan.packages.front().contentId;
    for (const auto* filename : {"payload.bin", ".lo-content", ".lo-dlc-header", ".lo-dlc.json"})
    {
        for (const auto* stage : {"open", "write", "flush", "close"})
        {
            install::SetTestDlcWriteFailure(filename, stage);
            bool failed = false, completed = false;
            try {
                install::InstallContent(scan, destination, [&](uint64_t, uint64_t, std::string_view label) {
                    if (label == "Import complete") completed = true;
                });
            }
            catch (const install::Error& error) {
                failed = std::string(error.what()).find(std::string("DLC ") + stage + " failed") != std::string::npos;
            }
            Require(failed && !completed, std::string(filename) + " " + stage + " failure was not reported");
            Require(!std::filesystem::exists(installed), "I/O failure published DLC");
            Require(!std::filesystem::exists(destination / ".import.lock"), "I/O failure retained lock");
            for (const auto& entry : std::filesystem::directory_iterator(destination))
                Require(entry.path().filename().string().find(".dlc-import-") != 0, "I/O failure retained staging");
        }
    }
    install::SetTestDlcWriteFailure({}, {});
    Require(install::InstallContent(scan, destination).dlcImported.size() == 1, "retry after I/O failures did not succeed");
    Require(ReadBytes(installed / "payload.bin") == std::vector<uint8_t>({'d', 'a', 't', 'a'}), "installed payload changed");
    // lexically_normal retains the final separator; scanning must still terminate.
    const auto trailingPath = installed / "";
    const auto extracted = install::ScanContent(trailingPath);
    Require(extracted.packages.size() == 1, "trailing-separator DLC scan failed");
    const auto extractedDestination = root / "extracted-game";
    Require(install::InstallContent(extracted, extractedDestination).dlcImported.size() == 1,
            "extracted DLC import failed");
    Require(ReadBytes(extractedDestination / "dlc" / extracted.packages.front().contentId / "payload.bin") ==
            std::vector<uint8_t>({'d', 'a', 't', 'a'}), "extracted payload changed");
    Require(install::InstallContent(extracted, extractedDestination).dlcUnchanged.size() == 1,
            "extracted DLC duplicate was not recognized");
    std::cout << "[PASS] STFS I/O failures and synthetic extracted DLC import" << std::endl;
}

std::vector<uint8_t> MakeXex(uint32_t disc = 1, uint32_t media = 0x39F7D748, uint32_t version = 4, uint32_t base = 4)
{
    std::vector<uint8_t> data(128, 0);
    std::memcpy(data.data(), "XEX2", 4);

    auto writeBeU32 = [&](size_t offset, uint32_t val) {
        data[offset] = static_cast<uint8_t>(val >> 24);
        data[offset + 1] = static_cast<uint8_t>(val >> 16);
        data[offset + 2] = static_cast<uint8_t>(val >> 8);
        data[offset + 3] = static_cast<uint8_t>(val);
    };

    writeBeU32(20, 1); // 1 optional header
    writeBeU32(24, 0x40006); // key
    writeBeU32(28, 32); // offset

    writeBeU32(32, media);
    writeBeU32(36, version);
    writeBeU32(40, base);
    writeBeU32(44, 0x4D5307FA); // title ID
    data[48] = 2;
    data[49] = 0;
    data[50] = static_cast<uint8_t>(disc);
    data[51] = 4; // 4 discs
    return data;
}

void WriteDiscFiles(const std::filesystem::path& dir, uint32_t disc, bool includeXex = true)
{
    std::filesystem::create_directories(dir);
    if (includeXex)
    {
        static const uint32_t mediaMap[5] = {0, 0x39F7D748, 0x0EF8CEA8, 0x309E3386, 0x7B21A91D};
        auto xex = MakeXex(disc, mediaMap[disc], 4, 4);
        std::ofstream xexOut(dir / "default.xex", std::ios::binary);
        xexOut.write(reinterpret_cast<const char*>(xex.data()), xex.size());
    }

    static const char* requiredNames[] = {
        "lo.fpd", "lo.fpi", "xenon_battle.fpd", "xenon_chr.fpd", "xenon_event.fpd",
        "xenon_field.fpd", "xenon_loc.fpd", "xenon_mov.fpd", "xenon_obj.fpd",
        "xenon_scr.fpd", "xenon_snd.fpd", "xenon_sys.fpd", "xenon_vfx.fpd", "xenon_world.fpd"
    };

    for (const char* name : requiredNames)
    {
        std::ofstream out(dir / name, std::ios::binary);
        std::string dummy = std::string(name) + " payload";
        out.write(dummy.data(), dummy.size());
    }
}

void RunDiscIoTest(const std::filesystem::path& source, const std::filesystem::path& root)
{
    const auto destination = root / "disc-io-game";
    for (const auto* filename : {"lo.fpd", "import-info.json"})
    {
        for (const auto* stage : {"open", "write", "flush", "close"})
        {
            install::SetTestDiscWriteFailure(filename, stage);
            bool failed = false;
            try { install::InstallDiscs(source, destination); }
            catch (const install::Error& error) {
                failed = std::string(error.what()).find(std::string("Disc ") + stage + " failed") != std::string::npos;
            }
            Require(failed, std::string(filename) + " " + stage + " failure was not reported");
            Require(!std::filesystem::exists(destination / "disc1"), "I/O failure published disc");
            Require(!std::filesystem::exists(destination / ".import.lock"), "I/O failure retained lock");
            for (const auto& entry : std::filesystem::directory_iterator(destination))
                Require(entry.path().filename().string().find(".import-staging-") != 0, "I/O failure retained staging");
        }
    }
    install::SetTestDiscWriteFailure({}, {});
    Require(install::InstallDiscs(source, destination).size() == 1, "retry after disc I/O failures did not succeed");
    Require(std::filesystem::exists(destination / "disc1" / "lo.fpd"), "retry did not publish disc");
    std::cout << "[PASS] Disc open/write/flush/close failures abort publication and allow retry" << std::endl;
}

void RunReimportTest()
{
    const auto root = std::filesystem::temp_directory_path() /
        ("lo-reimport-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { install::SetTestPublishFailure({}, {}); install::SetTestDlcWriteFailure({}, {});
                     std::error_code ec; std::filesystem::remove_all(path, ec); }
    } cleanup{root};
    std::filesystem::create_directories(root);
    for (uint32_t n = 1; n <= 2; ++n)
    {
        WriteDiscFiles(root / "sources" / ("disc" + std::to_string(n)), n);
    }
    WriteTinyStfs(root / "sources" / "source.stfs");
    const auto selection = install::ScanContent(std::vector<std::filesystem::path>{
        root / "sources" / "disc1", root / "sources" / "source.stfs"});
    Require(selection.discs.size() == 1 && selection.packages.size() == 1, "combined source scan failed");
    const auto dest = root / "game";
    const auto oldDisc = dest / "disc1", untouched = dest / "disc2";
    const auto oldDlc = dest / "dlc" / selection.packages.front().contentId;
    std::filesystem::create_directories(dest);
    std::filesystem::copy(root / "sources" / "disc1", oldDisc, std::filesystem::copy_options::recursive);
    std::filesystem::copy(root / "sources" / "disc2", untouched, std::filesystem::copy_options::recursive);
    install::ContentScan dlcOnly; dlcOnly.packages = selection.packages;
    install::InstallContent(dlcOnly, dest);
    auto marker = [](const std::filesystem::path& path) { std::ofstream(path) << "old"; };
    marker(oldDisc / "sentinel"); marker(oldDlc / "sentinel");
    auto oldState = [&] {
        Require(ReadBytes(oldDisc / "sentinel") == std::vector<uint8_t>({'o', 'l', 'd'}) &&
                ReadBytes(oldDlc / "sentinel") == std::vector<uint8_t>({'o', 'l', 'd'}), "old slots were not restored");
        Require(!std::filesystem::exists(oldDisc / "lo.fpd"), "damaged old disc was replaced on failure");
    };
    marker(untouched / "retained"); marker(dest / "save-sentinel");
    std::filesystem::create_directories(dest / ".import-staging-stale");
    marker(dest / ".import-staging-stale" / "keep");
    // A retained disc can have damaged resources; its compatible XEX is enough.
    std::filesystem::remove(untouched / "lo.fpd");
    // A selected old disc can be damaged and must not be revalidated.
    std::filesystem::remove(oldDisc / "lo.fpd");
    oldState();
    auto expectFailure = [&](const std::string& label, const install::Commit& cb = {}) {
        bool failed = false;
        try { install::ReimportContent(selection, dest, {}, {}, cb); }
        catch (const std::exception&) { failed = true; }
        Require(failed, label + " did not fail");
        oldState();
        Require(std::filesystem::exists(untouched / "retained"), label + " changed retained disc");
        Require(!std::filesystem::exists(dest / ".import.lock"), label + " left lock");
    };
    install::SetTestDlcWriteFailure("payload.bin", "write");
    expectFailure("DLC preparation failure");
    install::SetTestDlcWriteFailure({}, {});
    install::SetTestPublishFailure(selection.packages.front().contentId, "publish");
    expectFailure("DLC publish failure");
    install::SetTestPublishFailure({}, {});
    expectFailure("commit failure", [](const install::InstallResult&) { throw std::runtime_error("path persistence failed"); });
    bool nonStdRethrown = false;
    try { install::ReimportContent(selection, dest, {}, {}, [](const install::InstallResult&) { throw 1; }); }
    catch (int value) { nonStdRethrown = value == 1; }
    Require(nonStdRethrown, "non-standard callback failure was not rethrown");
    oldState();
    Require(!std::filesystem::exists(dest / ".import.lock"), "non-standard callback failure left lock");
    bool cancelOnPublish = false;
    bool cancelled = false;
    try { install::ReimportContent(selection, dest, [&](uint64_t, uint64_t, std::string_view label) {
            if (label == "payload.bin") cancelOnPublish = true;
        }, [&] { return cancelOnPublish; }); }
    catch (const install::Error& error) { cancelled = error.cancelled(); }
    Require(cancelled, "cancellation before commit did not abort");
    oldState();

    // Deliberately fail restoration; the old DLC remains in an explicitly
    // reported staging/old/dlc/<id> directory for manual recovery.
    install::SetTestPublishFailure(selection.packages.front().contentId, "rollback-old");
    bool preserved = false;
    try { install::ReimportContent(selection, dest, {}, {}, [](const install::InstallResult&) { throw std::runtime_error("abort"); }); }
    catch (const install::Error& error)
    {
        auto message = std::string(error.what());
        for (const auto& entry : std::filesystem::directory_iterator(dest))
        {
            if (entry.path().filename().string().starts_with(".import-staging-") &&
                std::filesystem::exists(entry.path() / "old" / "dlc" / selection.packages.front().contentId / "sentinel"))
                preserved = message.find((entry.path() / "old" / "dlc" / selection.packages.front().contentId).string()) != std::string::npos;
        }
    }
    Require(preserved, "rollback failure did not preserve/report old backup");
    install::SetTestPublishFailure({}, {});

    // A selected old disc may have lost its XEX; the destination is explicitly
    // the installation root, independent of that selected slot's health.
    std::filesystem::remove(oldDisc / "default.xex");
    auto successful = install::ReimportContent(selection, dest, {}, {}, [&](const install::InstallResult& result) {
        Require(result.destination == std::filesystem::absolute(dest).lexically_normal().string(), "callback received wrong root");
        Require(std::filesystem::exists(oldDisc / "lo.fpd") && std::filesystem::exists(oldDlc / "payload.bin"), "callback called before all publish");
    });
    Require(successful.discs == std::vector<int>{1} && successful.dlcImported.size() == 1, "selected slots missing in result");
    Require(!std::filesystem::exists(oldDisc / "sentinel") && !std::filesystem::exists(oldDlc / "sentinel"), "selected slots not replaced");
    Require(std::filesystem::exists(untouched / "retained") && std::filesystem::exists(dest / "save-sentinel"), "unselected files were changed");
    Require(!std::filesystem::exists(dest / ".import.lock"), "successful reimport retained lock");
    Require(std::filesystem::exists(dest / ".import-staging-stale" / "keep"), "unowned staging was removed");
    marker(oldDisc / "replace-again");
    install::ContentScan discOnly; discOnly.discs = selection.discs;
    install::SetTestPublishFailure("staging", "cleanup");
    auto cleanupWarning = install::ReimportContent(discOnly, dest);
    install::SetTestPublishFailure({}, {});
    Require(!cleanupWarning.warning.empty() && !std::filesystem::exists(oldDisc / "replace-again"),
        "post-commit cleanup failure incorrectly failed or reverted import");
    bool retainedBackup = false;
    for (const auto& entry : std::filesystem::directory_iterator(dest))
        if (entry.path().filename().string().starts_with(".import-staging-") &&
            std::filesystem::exists(entry.path() / "old" / "disc1" / "replace-again")) retainedBackup = true;
    Require(retainedBackup, "cleanup warning lost old backup");

    const char* oldProfile = std::getenv("LO_PROFILE_DIR");
    const std::string previousProfile = oldProfile ? oldProfile : "";
    const bool hadProfile = oldProfile != nullptr;
    const auto profileWithinSlot = oldDisc / "profile";
    std::filesystem::create_directories(profileWithinSlot);
#ifdef _WIN32
    _putenv_s("LO_PROFILE_DIR", profileWithinSlot.string().c_str());
#else
    setenv("LO_PROFILE_DIR", profileWithinSlot.string().c_str(), 1);
#endif
    bool protectedProfile = false;
    try { install::ReimportContent(selection, dest); }
    catch (const install::Error& error) { protectedProfile = std::string(error.what()).find("profile") != std::string::npos; }
#ifdef _WIN32
    _putenv_s("LO_PROFILE_DIR", hadProfile ? previousProfile.c_str() : "");
#else
    if (hadProfile) setenv("LO_PROFILE_DIR", previousProfile.c_str(), 1);
    else unsetenv("LO_PROFILE_DIR");
#endif
    Require(protectedProfile && std::filesystem::exists(profileWithinSlot), "actual profile root inside selected disc was replaced");

    // In the opposite direction, an entire installation may itself be rooted
    // inside the active profile; neither nested slot may be replaced.
    marker(dest / "nested-profile-sentinel");
#ifdef _WIN32
    _putenv_s("LO_PROFILE_DIR", dest.string().c_str());
#else
    setenv("LO_PROFILE_DIR", dest.string().c_str(), 1);
#endif
    bool protectedNested = false;
    try { install::ReimportContent(selection, dest); }
    catch (const install::Error& error) { protectedNested = std::string(error.what()).find("profile") != std::string::npos; }
#ifdef _WIN32
    _putenv_s("LO_PROFILE_DIR", hadProfile ? previousProfile.c_str() : "");
#else
    if (hadProfile) setenv("LO_PROFILE_DIR", previousProfile.c_str(), 1);
    else unsetenv("LO_PROFILE_DIR");
#endif
    Require(protectedNested && std::filesystem::exists(dest / "nested-profile-sentinel") &&
            std::filesystem::exists(oldDisc / "default.xex"), "selected slot inside active profile root was replaced");

    // A root directory named disc1 must remain the root. The parent and all
    // unselected siblings are untouched even if disc1 is selected for repair.
    const auto namedRoot = root / "named" / "disc1";
    std::filesystem::create_directories(namedRoot);
    std::filesystem::copy(root / "sources" / "disc1", namedRoot / "disc1", std::filesystem::copy_options::recursive);
    std::filesystem::copy(root / "sources" / "disc2", namedRoot / "disc2", std::filesystem::copy_options::recursive);
    marker(namedRoot / "disc1" / "old-selected");
    marker(namedRoot / "disc2" / "retained");
    marker(root / "named" / "parent-sentinel");
    auto namedResult = install::ReimportContent(discOnly, namedRoot);
    Require(namedResult.destination == std::filesystem::absolute(namedRoot).lexically_normal().string() &&
            std::filesystem::exists(namedRoot / "disc1" / "default.xex") &&
            !std::filesystem::exists(namedRoot / "disc1" / "old-selected") &&
            std::filesystem::exists(namedRoot / "disc2" / "retained") &&
            std::filesystem::exists(root / "named" / "parent-sentinel"), "disc1-named installation root or retained content changed");

    bool calledOnDlcOnly = false;
    auto dlcWithoutDisc = install::ReimportContent(dlcOnly, root / "dlc-only", {}, {},
        [&](const install::InstallResult&) { calledOnDlcOnly = true; });
    Require(!calledOnDlcOnly && !dlcWithoutDisc.warning.empty(), "DLC-only new destination was offered as game default");
    Require(std::filesystem::exists(root / "dlc-only" / "dlc" / selection.packages.front().contentId / "payload.bin"),
        "DLC-only import failed");

    // The source may be a file or a directory; both overlap directions must be rejected.
    for (const auto& source : {root / "sources" / "disc1", root / "sources" / "source.stfs"})
    {
        auto nested = source == root / "sources" / "source.stfs" ? dlcOnly : selection;
        if (nested.discs.size()) nested.discs.front().path = dest / "disc1";
        else nested.packages.front().path = dest / "source.stfs";
        bool refused = false;
        try { install::ReimportContent(nested, dest); } catch (const install::Error&) { refused = true; }
        Require(refused, "overlapping file/directory source accepted");
    }
    bool flatRejected = false;
    std::filesystem::copy_file(root / "sources" / "disc1" / "default.xex", dest / "default.xex");
    try { install::ReimportContent(selection, dest); } catch (const install::Error&) { flatRejected = true; }
    Require(flatRejected, "flat default.xex destination accepted for disc replacement");
    std::cout << "[PASS] selective reimport, preparation/publish/callback rollback, retained backups and overlap" << std::endl;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc > 1 && std::string(argv[1]) == "--reimport")
    {
        try { RunReimportTest(); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << std::endl; return 1; }
    }
    if (argc > 1 && std::string(argv[1]) == "--dlc-io")
    {
        try { RunDlcIoTest(); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << std::endl; return 1; }
    }
    std::cout << "Starting LoImportGameTest..." << std::endl;
    RunDlcIoTest();

    const auto uniqueId = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path tempDir = std::filesystem::temp_directory_path() /
                                     ("lo-import-test-" + std::to_string(uniqueId));
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    struct TempCleanup {
        std::filesystem::path p;
        ~TempCleanup() { std::error_code ec; std::filesystem::remove_all(p, ec); }
    } cleanup{tempDir};

    // 1. Recognize the four Asia discs by execution metadata.
    static const uint32_t mediaMap[5] = {0, 0x39F7D748, 0x0EF8CEA8, 0x309E3386, 0x7B21A91D};

    // 2. Test standard folder import with authentic per-disc files
    std::filesystem::path sourceStandard = tempDir / "standard";
    for (uint32_t d = 1; d <= 4; ++d)
    {
        WriteDiscFiles(sourceStandard / ("disc" + std::to_string(d)), d, true);
    }

    auto disc1Scan = install::ScanSource(sourceStandard / "disc1");
    assert(disc1Scan.discs.size() == 1);
    assert(disc1Scan.discs[0].disc == 1);
    assert(disc1Scan.discs[0].edition == "asia");
    std::cout << "[PASS] Standard single-disc scan" << std::endl;

    auto allScan = install::ScanSource(sourceStandard);
    assert(allScan.discs.size() == 4);
    for (size_t i = 0; i < 4; ++i)
    {
        assert(allScan.discs[i].disc == static_cast<uint32_t>(i + 1));
        assert(allScan.discs[i].edition == "asia");
    }
    std::cout << "[PASS] Standard 4-disc set scan" << std::endl;

    RunDiscIoTest(sourceStandard / "disc1", tempDir);

    // 3. Test authentic per-disc identity enforcement:
    // Reject fabrication where discs share a single top-level XEX or fabricate disc numbers without authentic per-disc identity
    std::filesystem::path fakeLayoutDir = tempDir / "fakeLayout";
    std::filesystem::create_directories(fakeLayoutDir);
    auto topXex = MakeXex(1, mediaMap[1], 4, 4);
    {
        std::ofstream topXexOut(fakeLayoutDir / "default.xex", std::ios::binary);
        topXexOut.write(reinterpret_cast<const char*>(topXex.data()), topXex.size());
    }
    for (uint32_t d = 1; d <= 4; ++d)
    {
        // Missing per-disc default.xex
        WriteDiscFiles(fakeLayoutDir / ("disc" + std::to_string(d)), d, false);
    }

    bool fakeRejected = false;
    try
    {
        // When scanning fake layout, the top folder only has Disc 1 XEX without required disc files at top level
        install::ScanSource(fakeLayoutDir);
    }
    catch (const install::Error& err)
    {
        fakeRejected = true;
    }
    assert(fakeRejected && "Should not fabricate disc identities from missing per-disc XEX");
    std::cout << "[PASS] Rejection of fabricated disc layout" << std::endl;

    // 4. Test transactional installation of standard 4-disc set
    std::filesystem::path destGame = tempDir / "destGame";
    uint64_t progressReported = 0;
    std::vector<int> installed;

    try
    {
        installed = install::InstallDiscs(sourceStandard, destGame,
            [&](uint64_t done, uint64_t total, std::string_view label) {
                progressReported = done;
                (void)total;
                (void)label;
            });
    }
    catch (const std::exception& ex)
    {
        std::cerr << "InstallDiscs exception: " << ex.what() << std::endl;
        assert(false);
    }
    std::cout << "InstallDiscs returned " << installed.size() << " discs" << std::endl;

    assert(installed.size() == 4);
    assert(installed[0] == 1 && installed[1] == 2 && installed[2] == 3 && installed[3] == 4);
    assert(progressReported > 0);

    // Verify destination files exist
    for (uint32_t d = 1; d <= 4; ++d)
    {
        assert(std::filesystem::exists(destGame / ("disc" + std::to_string(d)) / "default.xex"));
        assert(std::filesystem::exists(destGame / ("disc" + std::to_string(d)) / "lo.fpd"));
        assert(std::filesystem::exists(destGame / ("disc" + std::to_string(d)) / "import-info.json"));
    }
    assert(!std::filesystem::exists(destGame / ".import.lock"));
    std::cout << "[PASS] Transactional installation of 4 discs" << std::endl;

    // 5. Test DefaultGameDirectory and WriteGamePath
    auto defaultDir = install::DefaultGameDirectory(tempDir);
    assert(defaultDir == (tempDir / "game").lexically_normal());

    std::string writeErr;
    bool writeOk = install::WriteGamePath(tempDir, destGame, writeErr);
    assert(writeOk);
    assert(std::filesystem::exists(tempDir / "game-path.txt"));
    std::cout << "[PASS] Game path persistence" << std::endl;

    // 5b. A pre-existing lock must reject a second importer without touching the destination.
    std::filesystem::path lockedDest = tempDir / "lockedDest";
    std::filesystem::create_directories(lockedDest);
    {
        std::ofstream lock(lockedDest / ".import.lock", std::ios::binary);
        lock << "active";
    }
    bool lockRejected = false;
    try
    {
        install::InstallDiscs(sourceStandard, lockedDest);
    }
    catch (const install::Error& err)
    {
        lockRejected = std::string(err.what()).find("Another import owns") != std::string::npos;
    }
    assert(lockRejected);
    assert(std::filesystem::exists(lockedDest / ".import.lock"));
    std::cout << "[PASS] Import lock exclusivity" << std::endl;

    // 6. Test cancellation during scan/install
    bool cancelled = false;
    try
    {
        install::ScanSource(sourceStandard, []() { return true; });
        assert(false && "Should have thrown Cancelled Error");
    }
    catch (const install::Error& err)
    {
        assert(err.cancelled());
        cancelled = true;
    }
    assert(cancelled);
    std::cout << "[PASS] Cancellation handling" << std::endl;

    // 7. Test cancellation rollback during install
    std::filesystem::path cancelDest = tempDir / "cancelDest";
    bool installCancelled = false;
    try
    {
        install::InstallDiscs(sourceStandard, cancelDest, {}, []() { return true; });
    }
    catch (const install::Error& err)
    {
        assert(err.cancelled());
        installCancelled = true;
    }
    assert(installCancelled);
    // Verify rollback: no published discs or locks remain
    assert(!std::filesystem::exists(cancelDest / "disc1"));
    assert(!std::filesystem::exists(cancelDest / ".import.lock"));
    std::cout << "[PASS] Installation rollback on cancellation" << std::endl;

    // 7b. Test mixed edition rejection in scan and install
    std::filesystem::path mixedSource = tempDir / "mixedSource";
    std::filesystem::create_directories(mixedSource / "disc1");
    std::filesystem::create_directories(mixedSource / "disc2");
    WriteDiscFiles(mixedSource / "disc1", 1, true); // Asia Disc 1
    // Write USA/Europe Disc 2
    static const uint32_t euMediaMap[5] = {0, 0x368DE6DD, 0x1888BE4E, 0x6DD59D08, 0x0C0E80B5};
    auto euXex2 = MakeXex(2, euMediaMap[2], 3, 3);
    {
        std::ofstream out(mixedSource / "disc2" / "default.xex", std::ios::binary);
        out.write(reinterpret_cast<const char*>(euXex2.data()), euXex2.size());
    }
    static const char* reqFiles[] = {
        "lo.fpd", "lo.fpi", "xenon_battle.fpd", "xenon_chr.fpd", "xenon_event.fpd",
        "xenon_field.fpd", "xenon_loc.fpd", "xenon_mov.fpd", "xenon_obj.fpd",
        "xenon_scr.fpd", "xenon_snd.fpd", "xenon_sys.fpd", "xenon_vfx.fpd", "xenon_world.fpd"
    };
    for (const char* name : reqFiles)
    {
        std::ofstream out(mixedSource / "disc2" / name, std::ios::binary);
        out.write("payload", 7);
    }

    bool mixedRejected = false;
    try
    {
        install::ScanContent(mixedSource);
    }
    catch (const install::Error& err)
    {
        mixedRejected = (std::string(err.what()).find("Cannot mix") != std::string::npos);
    }
    assert(mixedRejected && "Mixed editions must be rejected");
    std::cout << "[PASS] Rejection of mixed editions" << std::endl;

    std::cout << "\nALL LO_IMPORT_GAME_TEST CHECKS PASSED!" << std::endl;
    return 0;
}
