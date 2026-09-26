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
#include "install/import_crypto.h"

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

void RequireTreeEqual(const std::filesystem::path& expected, const std::filesystem::path& actual)
{
    for (const auto& entry : std::filesystem::recursive_directory_iterator(expected))
    {
        if (!entry.is_regular_file()) continue;
        auto relative = entry.path().lexically_relative(expected);
        auto candidate = actual / relative;
        Require(std::filesystem::is_regular_file(candidate), "missing installed file " + relative.string());
        Require(ReadBytes(entry.path()) == ReadBytes(candidate), "payload mismatch " + relative.string());
    }
}

// One STFS directory block and one payload block, with a valid hash table.
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
        const auto digest = install::crypto::ComputeSha1(bytes.data() + 0xb000 + block * 4096, 4096);
        const size_t record = 0xa000 + block * 24;
        std::copy(digest.begin(), digest.end(), bytes.begin() + record);
        bytes[record + 20] = 0x80;
        bytes[record + 21] = bytes[record + 22] = bytes[record + 23] = 0xff;
    }
    const auto digest = install::crypto::ComputeSha1(bytes.data() + 0xa000, 4096);
    std::copy(digest.begin(), digest.end(), bytes.begin() + 0x381);
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
    Require(install::ScanContent(trailingPath).packages.size() == 1, "trailing-separator DLC scan failed");
    std::cout << "[PASS] STFS open/write/flush/close failures, cleanup, retry and trailing-separator scan" << std::endl;
}

void RunExtractedDlcTest()
{
    const std::filesystem::path source("D:/Mihoyo/LostOdysseyRecomp-windows-x64/game/dlc");
    Require(std::filesystem::is_directory(source), "extracted DLC source is missing");

    auto scan = install::ScanContent(source);
    Require(scan.packages.size() == 3, "expected three extracted DLC packages");
    auto direct = install::ScanContent(scan.packages.front().path);
    Require(direct.packages.size() == 1, "direct package scan did not return one package");

    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
                      ("lo-extracted-dlc-test-" + std::to_string(unique));
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); } } cleanup{root};
    const auto destination = root / "game";
    auto result = install::InstallContent(scan, destination);
    Require(result.dlcImported.size() == 3, "initial DLC import did not publish three packages");
    for (const auto& package : scan.packages)
    {
        RequireTreeEqual(package.path, destination / "dlc" / package.contentId);
        std::cout << "[PASS] Exact payload and sidecars: " << package.displayName << '\n';
    }
    auto duplicate = install::InstallContent(scan, destination);
    Require(duplicate.dlcUnchanged.size() == 3 && duplicate.dlcImported.empty(), "duplicate import was not unchanged");

    const auto isolated = root / "isolated";
    std::filesystem::create_directories(isolated);
    const auto originalPackage = scan.packages.front().path;
    const auto isolatedPackage = isolated / originalPackage.filename();
    std::filesystem::copy(originalPackage, isolatedPackage, std::filesystem::copy_options::recursive);
    auto reject = [&](const std::string& label) {
        bool failed = false;
        try { auto rejected = install::ScanContent(isolated); failed = rejected.packages.empty() && !rejected.rejected.empty(); }
        catch (const install::Error&) { failed = true; }
        Require(failed, label + " was accepted");
    };
    std::filesystem::path payload;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(isolatedPackage))
        if (entry.is_regular_file() && entry.path().filename() != ".lo-content" && entry.path().filename() != ".lo-dlc-header" && entry.path().filename() != ".lo-dlc.json") { payload = entry.path(); break; }
    Require(!payload.empty(), "package payload is missing in source");
    { std::ofstream output(payload, std::ios::binary | std::ios::app); output.put('\x01'); }
    reject("mutated payload");
    std::filesystem::remove(payload);
    reject("missing payload");
    std::filesystem::remove_all(isolatedPackage);
    std::filesystem::copy(originalPackage, isolatedPackage, std::filesystem::copy_options::recursive);
    const auto manifestBytes = ReadBytes(isolatedPackage / ".lo-dlc.json");
    std::string manifestText(manifestBytes.begin(), manifestBytes.end());
    const auto pathStart = manifestText.find("\"path\": \"");
    Require(pathStart != std::string::npos, "fixture manifest lacks file path");
    manifestText.insert(pathStart + 9, "../");
    { std::ofstream manifest(isolatedPackage / ".lo-dlc.json", std::ios::binary | std::ios::trunc); manifest << manifestText; }
    reject("manifest traversal");

    { std::ofstream manifest(isolatedPackage / ".lo-dlc.json", std::ios::binary | std::ios::trunc); manifest.write(reinterpret_cast<const char*>(manifestBytes.data()), manifestBytes.size()); }
    auto isolatedScan = install::ScanContent(isolatedPackage);
    for (const auto& overlap : {isolatedPackage, isolatedPackage / "out", isolated})
    {
        bool refused = false;
        try { install::InstallContent(isolatedScan, overlap); }
        catch (const install::Error&) { refused = true; }
        Require(refused, "overlapping destination was accepted");
    }
    RequireTreeEqual(originalPackage, isolatedPackage);
    { std::ofstream manifest(isolatedPackage / ".lo-dlc.json", std::ios::binary | std::ios::app); manifest << '\n'; }
    auto conflict = install::ScanContent(std::vector<std::filesystem::path>{originalPackage, isolatedPackage});
    Require(conflict.packages.size() == 1 && conflict.rejected.size() == 1, "different extracted manifest was silently deduplicated");

    bool scanCancelled = false;
    try { install::ScanContent(source, [] { return true; }); }
    catch (const install::Error& error) { scanCancelled = error.cancelled(); }
    Require(scanCancelled, "scan cancellation was not reported");
    const auto cancelledDestination = root / "cancelled";
    bool copyCancelled = false;
    bool copying = false;
    try { install::InstallContent(direct, cancelledDestination,
        [&](uint64_t done, uint64_t, std::string_view) { if (done > 0) copying = true; }, [&] { return copying; }); }
    catch (const install::Error& error) { copyCancelled = error.cancelled(); }
    Require(copying && copyCancelled, "mid-copy cancellation was not reported");
    Require(!std::filesystem::exists(cancelledDestination / ".import.lock"), "cancellation left import lock");
    for (const auto& entry : std::filesystem::directory_iterator(cancelledDestination))
        Require(entry.path().filename().string().find(".dlc-import-") != 0, "cancellation left staging root");
    Require(std::filesystem::is_empty(cancelledDestination / "dlc"), "cancellation published a package");
    std::cout << "[PASS] Extracted DLC scan/import/duplicate/cancellation/error boundaries" << std::endl;
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
                     std::error_code ec; std::filesystem::remove_all(path, ec); install::ClearTestOverrides(); }
    } cleanup{root};
    std::filesystem::create_directories(root);
    static const uint32_t media[3] = {0, 0x39F7D748, 0x0EF8CEA8};
    for (uint32_t n = 1; n <= 2; ++n)
    {
        const auto xex = MakeXex(n, media[n]);
        install::SetTestSha256(n, install::crypto::Sha256Hex(xex.data(), xex.size()), false);
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
    if (argc > 1 && std::string(argv[1]) == "--extracted-dlc")
    {
        try { RunExtractedDlcTest(); return 0; }
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

    // 1. Set up test hashes for Asia edition
    static const uint32_t mediaMap[5] = {0, 0x39F7D748, 0x0EF8CEA8, 0x309E3386, 0x7B21A91D};
    for (uint32_t d = 1; d <= 4; ++d)
    {
        auto xex = MakeXex(d, mediaMap[d], 4, 4);
        std::string hash = install::crypto::Sha256Hex(xex.data(), xex.size());
        install::SetTestSha256(d, hash, false);
    }

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
    auto defaultDir = install::DefaultGameDirectory(tempDir / "bin");
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

    // 5c. Portable SHA-256 retains the standard vector used by import identity checks.
    assert(install::crypto::Sha256Hex("", 0) ==
           "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    std::cout << "[PASS] Portable SHA-256 vector" << std::endl;

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
    install::SetTestSha256(2, install::crypto::Sha256Hex(euXex2.data(), euXex2.size()), true);
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

    // 7c. Test isolated DLC installation and cancellation
    std::filesystem::path asiaDlcSrc = (std::filesystem::path("G:/ROMS/X360CH176/DLC") / (const char8_t*)u8"2号DLC：「獎勵物品二合一組」");
    if (std::filesystem::exists(asiaDlcSrc))
    {
        std::filesystem::path dlcTestDest = tempDir / "dlcTestGame";
        // Pre-create fake installed disc1 so dlc installer recognizes game root
        std::filesystem::create_directories(dlcTestDest / "disc1");
        {
            auto d1xex = MakeXex(1, mediaMap[1], 4, 4);
            std::ofstream out(dlcTestDest / "disc1" / "default.xex", std::ios::binary);
            out.write(reinterpret_cast<const char*>(d1xex.data()), d1xex.size());
        }

        auto dlcScan = install::ScanContent(asiaDlcSrc);
        assert(dlcScan.packages.size() == 1);
        auto pkg = dlcScan.packages[0];

        // First test cancellation rollback
        bool dlcCancelled = false;
        try
        {
            install::InstallContent(dlcScan, dlcTestDest, {}, []() { return true; });
        }
        catch (const install::Error& err)
        {
            assert(err.cancelled());
            dlcCancelled = true;
        }
        assert(dlcCancelled);
        assert(!std::filesystem::exists(dlcTestDest / "dlc" / pkg.contentId));
        assert(!std::filesystem::exists(dlcTestDest / ".import.lock"));

        // Now install cleanly
        auto res = install::InstallContent(dlcScan, dlcTestDest);
        assert(res.dlcImported.size() == 1);
        assert(res.dlcImported[0] == pkg.contentId);
        assert(std::filesystem::exists(dlcTestDest / "dlc" / pkg.contentId / ".lo-content"));
        assert(std::filesystem::exists(dlcTestDest / "dlc" / pkg.contentId / ".lo-dlc-header"));
        assert(std::filesystem::exists(dlcTestDest / "dlc" / pkg.contentId / ".lo-dlc.json"));

        // Verify re-importing unchanged DLC is recognized as unchanged
        auto res2 = install::InstallContent(dlcScan, dlcTestDest);
        assert(res2.dlcUnchanged.size() == 1);
        assert(res2.dlcUnchanged[0] == pkg.contentId);
        std::cout << "[PASS] Isolated DLC installation, rollback, and duplicate detection" << std::endl;
    }

    install::ClearTestOverrides();

    // 8. Test Real Sources Read-Only Verification (if present)
    std::cout << "\nVerifying real authoritative read-only sources:" << std::endl;

    // A. G:/ROMS/US (USA/Europe ISOs)
    std::filesystem::path usIsoPath("G:/ROMS/US");
    if (std::filesystem::exists(usIsoPath))
    {
        auto scan = install::ScanContent(usIsoPath);
        std::cout << "[REAL SOURCE] G:/ROMS/US - Discs found: " << scan.discs.size() << std::endl;
        assert(scan.discs.size() == 4);
        for (size_t i = 0; i < scan.discs.size(); ++i)
        {
            const auto& d = scan.discs[i];
            std::cout << "  Disc " << d.disc << " [" << (d.kind == install::Kind::Iso ? "ISO" : "Other") << "]: "
                      << d.path.filename().string() << "\n    Edition: " << d.edition
                      << ", Media: " << d.media << ", SHA256: " << d.sha256.substr(0, 16) << "..." << std::endl;
            assert(d.disc == static_cast<uint32_t>(i + 1));
            assert(d.edition == "usa-europe");
            assert(d.kind == install::Kind::Iso);
        }
        std::cout << "[PASS] Real USA/Europe ISOs verified successfully" << std::endl;
    }

    // B. G:/ROMS/X360CH176 (Asia GOD + DLC)
    std::filesystem::path asiaPath("G:/ROMS/X360CH176");
    if (std::filesystem::exists(asiaPath))
    {
        auto scan = install::ScanContent(asiaPath);
        std::cout << "[REAL SOURCE] G:/ROMS/X360CH176 - Discs found: " << scan.discs.size()
                  << ", Packages found: " << scan.packages.size()
                  << ", Rejected: " << scan.rejected.size() << std::endl;
        for (const auto& r : scan.rejected)
        {
            std::cout << "  REJECTED: " << r.first.string() << " -> " << r.second << std::endl;
        }
        assert(scan.discs.size() == 4);
        for (size_t i = 0; i < scan.discs.size(); ++i)
        {
            const auto& d = scan.discs[i];
            std::cout << "  Disc " << d.disc << " [" << (d.kind == install::Kind::God ? "GOD" : "Other") << "]: "
                      << d.path.filename().string() << "\n    Edition: " << d.edition
                      << ", Media: " << d.media << ", SHA256: " << d.sha256.substr(0, 16) << "..." << std::endl;
            assert(d.disc == static_cast<uint32_t>(i + 1));
            assert(d.edition == "asia");
            assert(d.kind == install::Kind::God);
        }

        assert(scan.packages.size() == 3);
        for (const auto& pkg : scan.packages)
        {
            std::cout << "  DLC Package: " << pkg.displayName << "\n    ID: " << pkg.contentId
                      << ", Files: " << pkg.files << ", Bytes: " << pkg.bytes
                      << ", SHA256: " << pkg.sourceSha256.substr(0, 16) << "..." << std::endl;
        }
        std::cout << "[PASS] Real Asia GOD + DLC verified successfully" << std::endl;
    }

    // C. D:/Mihoyo/LostOdysseyRecomp-windows-x64/game (Extracted dump)
    std::filesystem::path dumpPath("D:/Mihoyo/LostOdysseyRecomp-windows-x64/game");
    if (std::filesystem::exists(dumpPath))
    {
        auto scan = install::ScanContent(dumpPath);
        std::cout << "[REAL SOURCE] D:/Mihoyo/LostOdysseyRecomp-windows-x64/game - Discs found: "
                  << scan.discs.size() << std::endl;
        assert(scan.discs.size() == 4);
        for (size_t i = 0; i < scan.discs.size(); ++i)
        {
            const auto& d = scan.discs[i];
            std::cout << "  Disc " << d.disc << " [Folder]: " << d.path.filename().string()
                      << "\n    Edition: " << d.edition << ", Media: " << d.media
                      << ", SHA256: " << d.sha256.substr(0, 16) << "..." << std::endl;
            assert(d.disc == static_cast<uint32_t>(i + 1));
            assert(d.edition == "asia");
            assert(d.kind == install::Kind::Folder);
        }
        std::cout << "[PASS] Real Extracted Folder dump verified successfully" << std::endl;
    }

    std::cout << "\nALL LO_IMPORT_GAME_TEST CHECKS PASSED!" << std::endl;
    return 0;
}
