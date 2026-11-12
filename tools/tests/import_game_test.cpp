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
#include "os/user_paths.h"
#include "gpu/shader/resource_cpx_index_sha256.h"

#ifndef _WIN32
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace
{
// A live importer's .import.lock: the file exists and, outside Windows, is flocked.
struct HeldLock
{
    explicit HeldLock(const std::filesystem::path& path)
    {
#ifdef _WIN32
        std::ofstream(path, std::ios::binary) << "active";
#else
        fd = open(path.c_str(), O_WRONLY | O_CREAT, 0600);
        flock(fd, LOCK_EX);
#endif
    }
    ~HeldLock()
    {
#ifndef _WIN32
        close(fd);
#endif
    }
    int fd = -1;
};

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
    std::vector<uint8_t> bytes(0x20000, 0);
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
    bytes[0x414] = '"';
    bytes[0x416] = '\\';
    bytes[0x418] = 'N';
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
    const std::string millionA(1000000, 'a');
    xenos::resources::Sha256Incremental incremental;
    const auto message = std::span(reinterpret_cast<const uint8_t*>(millionA.data()), millionA.size());
    for (size_t offset = 0; offset < message.size();)
    {
        const auto take = std::min<size_t>(1 + offset % 65537, message.size() - offset);
        incremental.Update(message.subspan(offset, take));
        offset += take;
    }
    Require(xenos::resources::Sha256Hex(incremental.Finalize()) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
            "incremental SHA256 changed a known multi-block digest");

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
    for (const auto* filename : {"payload.bin", "source.stfs"})
    {
        install::SetTestDlcWriteFailure(filename, "checksum");
        bool failed = false;
        try { install::InstallContent(scan, destination); }
        catch (const install::Error& error) {
            failed = std::string(error.what()).find("DLC checksum read failed") != std::string::npos;
        }
        Require(failed, std::string(filename) + " checksum failure was not reported");
        Require(!std::filesystem::exists(installed), "checksum failure published DLC");
        Require(!std::filesystem::exists(destination / ".import.lock"), "checksum failure retained lock");
        for (const auto& entry : std::filesystem::directory_iterator(destination))
            Require(entry.path().filename().string().find(".import-staging-") != 0,
                    "checksum failure retained staging");
    }
    install::SetTestDlcWriteFailure({}, {});
    bool hashingStarted = false, cancelled = false;
    try {
        install::InstallContent(scan, destination,
            [&](uint64_t, uint64_t, std::string_view label) { if (label == "payload.bin") hashingStarted = true; },
            [&] { return hashingStarted; });
    }
    catch (const install::Error& error) { cancelled = error.cancelled(); }
    Require(cancelled && !std::filesystem::exists(installed), "checksum cancellation published DLC");
    Require(!std::filesystem::exists(destination / ".import.lock"), "checksum cancellation retained lock");
    for (const auto& entry : std::filesystem::directory_iterator(destination))
        Require(entry.path().filename().string().find(".import-staging-") != 0,
                "checksum cancellation retained staging");
    Require(install::InstallContent(scan, destination).dlcImported.size() == 1, "retry after I/O failures did not succeed");
    Require(ReadBytes(installed / "payload.bin") == std::vector<uint8_t>({'d', 'a', 't', 'a'}), "installed payload changed");
    // The runtime (kernel/dlc_content.cpp) ignores a DLC without these digests.
    {
        const auto bytes = ReadBytes(installed / ".lo-dlc.json");
        const std::string manifest(bytes.begin(), bytes.end());
        const auto source = ReadBytes(root / "source.stfs");
        const auto sourceDigest = xenos::resources::Sha256Hex(xenos::resources::Sha256(source));
        Require(manifest.find("\"source_sha256\": \"" + sourceDigest + "\"") != std::string::npos,
                "DLC manifest source digest is missing or wrong");
        Require(manifest.find(R"("display_name": "T\"\\N")") != std::string::npos,
                "DLC manifest display name was not escaped");
        Require(manifest.find(R"("sha256": "3a6eb0790f39ac87c94f3856b2dd2c5d110e6811602261a9a923d3bb23adc8b7")") != std::string::npos,
                "DLC manifest payload digest is missing or wrong");
    }
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

#if defined(__linux__) && !defined(__ANDROID__)
// A complete synthetic disc, including a resource above the 4 GiB boundary.
// Sparse padding exercises 64-bit ISO reads without real game data or a large copy.
void WriteLinuxTestIso(const std::filesystem::path& path)
{
    constexpr uint64_t sector = 2048;
    std::vector<std::pair<std::string, std::vector<uint8_t>>> files{
        {"default.xex", MakeXex()},
    };
    for (const char* name : {"lo.fpd", "lo.fpi", "xenon_battle.fpd", "xenon_chr.fpd", "xenon_event.fpd",
             "xenon_field.fpd", "xenon_loc.fpd", "xenon_mov.fpd", "xenon_obj.fpd", "xenon_scr.fpd",
             "xenon_snd.fpd", "xenon_sys.fpd", "xenon_vfx.fpd", "xenon_world.fpd"})
        files.push_back({name, {'p', 'a', 'y', 'l', 'o', 'a', 'd'}});
    auto le32 = [](uint8_t* p, uint32_t n) {
        for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(n >> (8 * i));
    };
    std::array<uint8_t, sector> descriptor{}, directory{};
    constexpr const char* magic = "MICROSOFT*XBOX*MEDIA";
    std::memcpy(descriptor.data(), magic, 20);
    std::memcpy(descriptor.data() + 0x7ec, magic, 20);
    le32(descriptor.data() + 20, 33);
    le32(descriptor.data() + 24, sector);
    std::ofstream out(path, std::ios::binary);
    out.seekp(0x10000);
    out.write(reinterpret_cast<const char*>(descriptor.data()), descriptor.size());
    size_t offset = 0;
    for (size_t i = 0; i < files.size(); ++i)
    {
        const auto& [name, data] = files[i];
        const size_t next = (offset + 14 + name.size() + 3) & ~size_t{3};
        if (i + 1 < files.size())
        {
            directory[offset + 2] = static_cast<uint8_t>(next / 4);
            directory[offset + 3] = static_cast<uint8_t>((next / 4) >> 8);
        }
        const uint32_t fileSector = i + 1 == files.size() ? 4ULL * 1024 * 1024 * 1024 / sector + 1 : 34 + i;
        le32(directory.data() + offset + 4, fileSector);
        le32(directory.data() + offset + 8, data.size());
        directory[offset + 13] = static_cast<uint8_t>(name.size());
        std::memcpy(directory.data() + offset + 14, name.data(), name.size());
        out.seekp(static_cast<uint64_t>(fileSector) * sector);
        out.write(reinterpret_cast<const char*>(data.data()), data.size());
        offset = next;
    }
    out.seekp(33 * sector);
    out.write(reinterpret_cast<const char*>(directory.data()), directory.size());
    out.close();
    Require(static_cast<bool>(out), "could not write synthetic ISO");
}

// Run only inside an isolated Linux root with /home -> /var/home, e.g. bwrap.
// The ordinary test suite never changes the machine's /home layout.
void RunLinuxHomeTest()
{
    namespace fs = std::filesystem;
    Require(fs::is_symlink("/home") && fs::canonical("/home") == "/var/home",
            "--linux-home requires an isolated /home -> /var/home root");
    const auto root = fs::path("/home/x") / ("lo-linux-import-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        fs::path path;
        ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); }
    } cleanup{root};
    fs::create_directories(root / "source");
    const auto iso = root / "source" / "disc1.iso";
    WriteLinuxTestIso(iso);
    const auto scan = install::ScanContent(iso);
    Require(scan.discs.size() == 1, "synthetic ISO under /home did not scan");
    const auto canonicalRoot = fs::canonical(root);
    Require(install::InstallContent(scan, canonicalRoot / "canonical-game").discs == std::vector<int>{1},
            "canonical /var/home import failed");
    std::cout << "[PASS] canonical destination and sparse ISO reads above 4 GiB" << std::endl;
    os::user_paths::g_usePortableLayout = false;
    const auto destination = install::DefaultGameDirectory("/app/bin");
    Require(destination.string().starts_with("/home/x/"), "default HOME alias was not retained");
    Require(install::InstallContent(scan, destination).discs == std::vector<int>{1},
            "default destination under system /home alias failed");
    Require(ReadBytes(destination / "disc1" / "xenon_world.fpd") ==
            std::vector<uint8_t>({'p', 'a', 'y', 'l', 'o', 'a', 'd'}), "large-offset payload changed");
    Require(install::ReimportContent(scan, destination).discs == std::vector<int>{1},
            "reimport under system /home alias failed");
    std::cout << "[PASS] HOME alias default destination, ISO extraction and reimport" << std::endl;
    auto rejected = [&](const fs::path& dest, std::string_view expected) {
        bool failed = false;
        try { install::InstallContent(scan, dest); }
        catch (const install::Error& error) { failed = std::string(error.what()).find(expected) != std::string::npos; }
        Require(failed, "path protection did not reject " + dest.string());
    };
    rejected(canonicalRoot / "source", "Source and destination must be separate folders");
    const auto canonicalScan = install::ScanContent(canonicalRoot / "source" / "disc1.iso");
    bool overlap = false;
    try { install::InstallContent(canonicalScan, root / "source"); }
    catch (const install::Error& error) { overlap = std::string(error.what()).find("Source and destination must be separate folders") != std::string::npos; }
    Require(overlap, "source/destination overlap through /home alias was accepted");
    fs::create_directory_symlink(canonicalRoot, root / "user-link");
    rejected(root / "user-link" / "game", "Destination ancestors must not contain links or junctions");
    fs::create_directory_symlink(canonicalRoot, root / "linked-game");
    rejected(root / "linked-game", "Destination ancestors must not contain links or junctions");
    fs::create_directories(root / "linked-slot");
    fs::create_directory_symlink(canonicalRoot / "canonical-game" / "disc1", root / "linked-slot" / "disc1");
    bool slotRejected = false;
    try { install::ReimportContent(scan, root / "linked-slot"); }
    catch (const install::Error& error) { slotRejected = std::string(error.what()).find("Import target is a link") != std::string::npos; }
    Require(slotRejected, "linked disc slot was accepted");
    std::cout << "[PASS] arbitrary links, linked slots and canonical/alias overlap remain rejected" << std::endl;
    fs::create_directories("/run/user/1000/doc/test-id");
    WriteLinuxTestIso("/run/user/1000/doc/test-id/disc1.iso");
    const auto portalScan = install::ScanContent("/run/user/1000/doc/test-id/disc1.iso");
    Require(portalScan.discs.size() == 1 &&
            install::InstallContent(portalScan, canonicalRoot / "portal-game").discs == std::vector<int>{1},
            "single ISO at a portal-shaped path failed");
    std::cout << "[PASS] portal-shaped path imports one disc without neighboring discs (ordinary filesystem)" << std::endl;
}

void RunLinuxOtherHomeTest()
{
    namespace fs = std::filesystem;
    const auto source = fs::path("/var/home/x/source");
    WriteDiscFiles(source, 1);
    const auto scan = install::ScanContent(source);
    bool rejected = false;
    try { install::InstallContent(scan, "/home/x/game"); }
    catch (const install::Error& error) {
        rejected = std::string(error.what()) == "Destination ancestors must not contain links or junctions";
    }
    Require(rejected, "an unsupported /home link was accepted");
    std::cout << "[PASS] unsupported /home links remain rejected" << std::endl;
}
#endif

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

    // Empty disc slot folders (made by hand or by the Android app) hold no
    // disc: a partial import neither validates them nor refuses to fill them.
    for (const bool reimport : {true, false})
    {
        const auto prepared = root / (reimport ? "prepared-reimport" : "prepared-install");
        for (uint32_t n = 1; n <= 4; ++n) std::filesystem::create_directories(prepared / ("disc" + std::to_string(n)));
        const auto result = reimport ? install::ReimportContent(discOnly, prepared) : install::InstallContent(discOnly, prepared);
        Require(result.discs == std::vector<int>{1} && std::filesystem::exists(prepared / "disc1" / "default.xex") &&
                std::filesystem::is_empty(prepared / "disc2"),
                std::string("import into empty slot folders failed (") + (reimport ? "reimport" : "install") + ")");
    }
    std::cout << "[PASS] selective reimport, preparation/publish/callback rollback, retained backups and overlap" << std::endl;
}

bool SameFile(const std::filesystem::path& a, const std::filesystem::path& b)
{
    std::error_code ec;
    return std::filesystem::equivalent(a, b, ec);
}

std::string ReadText(const std::filesystem::path& path)
{
    const auto bytes = ReadBytes(path);
    return {bytes.begin(), bytes.end()};
}

void RunCompactionEstimateTest()
{
    const auto root = std::filesystem::temp_directory_path() /
        ("lo-compaction-estimate-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    } cleanup{root};
    for (uint32_t n = 1; n <= 4; ++n)
    {
        const auto disc = root / ("disc" + std::to_string(n));
        std::filesystem::create_directories(disc);
        std::ofstream(disc / "same.fpd", std::ios::binary) << "same";
        std::ofstream(disc / "ignored.bin", std::ios::binary) << "not an archive";
        std::ofstream(disc / "different-size.fpd", std::ios::binary) << std::string(n, 'x');
    }
    Require(install::EstimateCompactGameDataBytes(root) == 12,
            "estimate did not count three duplicate archive copies or counted other sizes/files");
    Require(install::EstimateCompactGameDataBytes(root / "disc1") == 12,
            "estimate did not accept a disc folder");

    std::filesystem::remove(root / "disc4" / "same.fpd");
    std::filesystem::create_hard_link(root / "disc3" / "same.fpd", root / "disc4" / "same.fpd");
    Require(install::EstimateCompactGameDataBytes(root) == 8,
            "estimate counted a mixed group's linked copies as separate storage");
    std::filesystem::create_hard_link(root / "disc2" / "same.fpd", root / "outside.fpd");
    Require(install::EstimateCompactGameDataBytes(root) == 4,
            "estimate counted storage retained by an external hard link");
    std::filesystem::remove(root / "outside.fpd");
    for (uint32_t n = 2; n <= 4; ++n)
    {
        const auto path = root / ("disc" + std::to_string(n)) / "same.fpd";
        std::filesystem::remove(path);
        std::filesystem::create_hard_link(root / "disc1" / "same.fpd", path);
    }
    Require(install::EstimateCompactGameDataBytes(root) == 0,
            "estimate counted already linked archives");

    std::filesystem::remove_all(root / "disc2");
    std::filesystem::remove_all(root / "disc3");
    std::filesystem::remove(root / "disc4" / "same.fpd");
    std::ofstream(root / "disc4" / "same.fpd", std::ios::binary) << "same";
    Require(install::EstimateCompactGameDataBytes(root) == 4,
            "missing middle discs prevented detection on retained discs");
    std::filesystem::remove_all(root / "disc4");
    Require(install::EstimateCompactGameDataBytes(root) == 0 &&
            install::EstimateCompactGameDataBytes(root / "missing") == 0,
            "missing discs or installation contributed to the estimate");
    Require(!std::filesystem::exists(root / ".import.lock"), "read-only estimate created an import lock");
    std::cout << "[PASS] compaction estimate: copies, mixed/all hard links, external links, sizes, missing discs" << std::endl;
}

void RunGameDataSharingProbeTest()
{
    const auto root = std::filesystem::temp_directory_path() /
        ("lo-sharing-probe-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { install::SetTestShareFailure({}); std::error_code ec; std::filesystem::remove_all(path, ec); }
    } cleanup{root};
    for (const auto disc : {"disc1", "disc4"})
    {
        std::filesystem::create_directories(root / disc);
        std::ofstream(root / disc / "same.fpd", std::ios::binary) << "same";
    }
    const auto estimate = install::EstimateCompactGameDataBytes(root);
    Require(estimate == 4, "probe fixture did not contain a duplicate archive");
    const auto checkUnchanged = [&] {
        size_t entries = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        {
            ++entries;
            Require(entry.path().filename() == "disc1" || entry.path().filename() == "disc4" ||
                    entry.path().filename() == "same.fpd", "sharing probe left a temporary file or directory");
        }
        Require(entries == 4 && ReadText(root / "disc1/same.fpd") == "same" &&
                ReadText(root / "disc4/same.fpd") == "same", "sharing probe changed an archive");
        Require(install::EstimateCompactGameDataBytes(root) == estimate &&
                !SameFile(root / "disc1/same.fpd", root / "disc4/same.fpd"),
                "sharing probe compacted archives or changed their estimate");
    };
    install::SetTestShareFailure("no-clone");
    Require(install::CanShareGameData(root), "hard-link-capable storage failed the sharing probe");
    checkUnchanged();
    Require(install::CanShareGameData(root / "disc4"), "sharing probe did not accept a disc folder");
    checkUnchanged();
    install::SetTestShareFailure("link");
    Require(!install::CanShareGameData(root), "unsupported sharing passed the probe");
    checkUnchanged();
    install::SetTestShareFailure("rename");
    Require(!install::CanShareGameData(root), "interrupted replacement passed the probe");
    checkUnchanged();
    install::SetTestShareFailure({});
    Require(!install::CanShareGameData(root / "missing"), "missing installation passed the sharing probe");
    std::filesystem::remove_all(root / "disc1");
    std::filesystem::remove_all(root / "disc4");
    Require(!install::CanShareGameData(root) && std::filesystem::is_empty(root),
            "sharing probe created missing disc directories");
    std::cout << "[PASS] sharing probe: hard links, unsupported sharing, interrupted replacement, cleanup, missing discs" << std::endl;
}

// Archives identical on several discs are stored once, by import or by compaction;
// different archives, indexes and failed or interrupted links keep plain copies.
void RunSharedArchiveTest()
{
    const auto root = std::filesystem::temp_directory_path() /
        ("lo-shared-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { install::SetTestShareFailure({}); std::error_code ec; std::filesystem::remove_all(path, ec); }
    } cleanup{root};
    // Hard links make sharing observable; copy-on-write clones are checked at the end.
    install::SetTestShareFailure("no-clone");
    for (uint32_t n = 1; n <= 2; ++n)
        WriteDiscFiles(root / "sources" / ("disc" + std::to_string(n)), n);
    {
        std::ofstream field(root / "sources" / "disc2" / "xenon_field.fpd", std::ios::binary | std::ios::trunc);
        field << "xenon_field.fpd payload, disc 2";
    }
    uint64_t sharedSize = 0;
    for (const auto& entry : std::filesystem::directory_iterator(root / "sources" / "disc1"))
        if (entry.path().extension() == ".fpd" && entry.path().filename() != "xenon_field.fpd")
            sharedSize += entry.file_size();
    const auto both = install::ScanContent(root / "sources");
    auto checkLinked = [&](const std::filesystem::path& game, bool linked, const std::string& what) {
        for (const auto& entry : std::filesystem::directory_iterator(game / "disc1"))
        {
            const auto name = entry.path().filename();
            const auto other = game / "disc2" / name;
            if (!std::filesystem::exists(other)) continue;
            const bool expect = linked && entry.path().extension() == ".fpd" && name != "xenon_field.fpd";
            Require(SameFile(entry.path(), other) == expect, what + ": " + name.string() + " sharing is wrong");
            if (name != "xenon_field.fpd" && name != "default.xex" && name != "import-info.json")
                Require(ReadText(other) == name.string() + " payload", what + ": " + name.string() + " content changed");
        }
        Require(ReadText(game / "disc2" / "xenon_field.fpd") == "xenon_field.fpd payload, disc 2", what + ": distinct archive changed");
        for (const auto& entry : std::filesystem::recursive_directory_iterator(game))
            Require(entry.path().extension() != ".lo-share", what + ": temporary left behind");
    };

    // Import links identical archives across the discs of one import.
    const auto linked = root / "linked";
    const auto imported = install::InstallContent(both, linked);
    Require(imported.sharedBytes == sharedSize, "import did not share the identical archives");
    checkLinked(linked, true, "import");

    // A disc imported later links to a retained disc's archives.
    const auto later = root / "later";
    install::InstallContent(install::ScanContent(root / "sources" / "disc1"), later);
    Require(install::InstallContent(install::ScanContent(root / "sources" / "disc2"), later).sharedBytes == sharedSize,
            "later disc did not share the retained disc's archives");
    checkLinked(later, true, "later import");

    // Where links fail, the import keeps plain copies and still succeeds.
    install::SetTestShareFailure("link");
    const auto copies = root / "copies";
    Require(install::InstallContent(both, copies).sharedBytes == 0, "failed links were counted");
    install::SetTestShareFailure("no-clone");
    checkLinked(copies, false, "link failure");
    std::filesystem::copy(copies, root / "copies-interrupted", std::filesystem::copy_options::recursive);

    // Compaction verifies digests and replaces later copies; a second run has nothing to do.
    auto compacted = install::CompactGameData(copies);
    Require(compacted.replaced == 12 && compacted.freedBytes == sharedSize && compacted.sharedBytes == sharedSize &&
            compacted.failed == 0, "compaction did not replace the identical copies");
    checkLinked(copies, true, "compaction");
    compacted = install::CompactGameData(copies / "disc1");
    Require(compacted.replaced == 0 && compacted.freedBytes == 0 && compacted.sharedBytes == sharedSize,
            "second compaction changed a compacted folder");

    // An interruption between link and rename leaves every copy intact; the next run cleans up.
    const auto interrupted = root / "copies-interrupted";
    install::SetTestShareFailure("rename");
    bool stopped = false;
    try { install::CompactGameData(interrupted); } catch (const install::Error&) { stopped = true; }
    install::SetTestShareFailure("no-clone");
    Require(stopped && !std::filesystem::exists(interrupted / ".import.lock"), "interrupted compaction kept its lock");
    bool temporary = false;
    for (const auto& entry : std::filesystem::directory_iterator(interrupted / "disc2"))
        temporary |= entry.path().extension() == ".lo-share";
    Require(temporary, "interruption did not leave its temporary");
    for (const auto& entry : std::filesystem::directory_iterator(interrupted / "disc2"))
        if (entry.path().extension() == ".fpd")
            Require(!SameFile(entry.path(), interrupted / "disc1" / entry.path().filename()), "interrupted rename replaced a copy");
    Require(install::CompactGameData(interrupted).replaced == 12, "compaction after an interruption did not finish");
    checkLinked(interrupted, true, "after interruption");

    // Compaction shares the import lock.
    bool locked = false;
    {
        HeldLock held(copies / ".import.lock");
        try { install::CompactGameData(copies); } catch (const install::Error&) { locked = true; }
    }
    Require(locked, "compaction ignored the import lock");

    // With clones allowed (APFS, btrfs, XFS clone; elsewhere links) the shared copies keep their content.
    install::SetTestShareFailure({});
    const auto any = root / "any";
    Require(install::InstallContent(both, any).sharedBytes == sharedSize, "import with clones allowed did not share");
    for (const auto& entry : std::filesystem::directory_iterator(any / "disc2"))
        if (entry.path().extension() == ".fpd" && entry.path().filename() != "xenon_field.fpd")
            Require(ReadText(entry.path()) == ReadText(any / "disc1" / entry.path().filename()), "shared copy content changed");
    std::cout << "[PASS] shared archives: import, later disc, link failure, compaction, interrupted rename, lock, clones" << std::endl;
}
} // namespace

int main(int argc, char** argv)
{
#if defined(__linux__) && !defined(__ANDROID__)
    if (argc > 1 && std::string(argv[1]) == "--linux-home")
    {
        try { RunLinuxHomeTest(); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << std::endl; return 1; }
    }
    if (argc > 1 && std::string(argv[1]) == "--linux-other-home")
    {
        try { RunLinuxOtherHomeTest(); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << std::endl; return 1; }
    }
#endif
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
    if (argc > 1 && std::string(argv[1]) == "--shared")
    {
        try { RunCompactionEstimateTest(); RunGameDataSharingProbeTest(); RunSharedArchiveTest(); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << std::endl; return 1; }
    }
    std::cout << "Starting LoImportGameTest..." << std::endl;
    RunDlcIoTest();
    RunCompactionEstimateTest();
    RunGameDataSharingProbeTest();
    RunSharedArchiveTest();

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
    auto storedGamePath = [&]() {
        std::ifstream written(tempDir / "game-path.txt", std::ios::binary);
        std::string stored;
        std::getline(written, stored);
        return stored;
    };
    // Inside the executable's folder the path is relative, so a moved folder keeps it.
    assert(storedGamePath() == "destGame");
    assert(install::WriteGamePath(tempDir, tempDir.parent_path() / "outside-game", writeErr));
    assert(std::filesystem::path(storedGamePath()).is_absolute());
    std::cout << "[PASS] Game path persistence" << std::endl;

    // 5b. A pre-existing lock must reject a second importer without touching the destination.
    std::filesystem::path lockedDest = tempDir / "lockedDest";
    std::filesystem::create_directories(lockedDest);
    bool lockRejected = false;
    {
        HeldLock held(lockedDest / ".import.lock");
        try
        {
            install::InstallDiscs(sourceStandard, lockedDest);
        }
        catch (const install::Error& err)
        {
            lockRejected = std::string(err.what()).find("Another import owns") != std::string::npos;
        }
    }
    assert(lockRejected);
    assert(std::filesystem::exists(lockedDest / ".import.lock"));
#ifndef _WIN32
    // A lock file whose owner was killed holds no flock; the next importer takes it over.
    assert(install::InstallDiscs(sourceStandard, lockedDest).size() == 4);
    assert(!std::filesystem::exists(lockedDest / ".import.lock"));
#endif
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
