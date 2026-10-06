#include "gpu/pipeline_cache.h"

#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace cache = gpu::pipeline_cache;
namespace fs = std::filesystem;

static void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static std::vector<uint8_t> Read(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    Check(bool(input), "open fixture input");
    return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
}

static void RawWrite(const fs::path& path, std::span<const uint8_t> bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    output.close();
    Check(bool(output), "write fixture bytes");
}

static void HeaderChecksum(std::vector<uint8_t>& bytes)
{
    cache::detail::Put64(bytes.data() + 40, cache::detail::Hash(std::span(bytes).first(40)));
}

static void PayloadChecksum(std::vector<uint8_t>& bytes)
{
    cache::detail::Put64(bytes.data() + 32, cache::detail::Hash(std::span(bytes).subspan(cache::kHeaderBytes)));
    HeaderChecksum(bytes);
}

static cache::Key SampleKey()
{
    cache::Key key;
    key.vs = 0x0123456789ABCDEFULL;
    key.ps = 0xFEDCBA9876543210ULL;
    key.blend = 0x10203040;
    key.depthControl = 0xAABBCCDD;
    key.modeCull = 0x3807;
    key.colorMask = 0xB;
    key.prim = 4;
    key.rtFormat = 7;
    key.depthFormat = 9;
    key.stencilRefMask = 0x123456;
    key.stencilRefMaskBack = 0x654321;
    key.depthBias = -137;
    key.slopeBias = std::bit_cast<uint32_t>(-0.25f);
    return key;
}

static bool ValidateEnums(const cache::Key& key)
{
    return (key.prim == 4 || key.prim == 8) && key.rtFormat == 7 &&
        (key.depthFormat == 0 || key.depthFormat == 9);
}

static std::vector<cache::Key> Keys(const std::vector<cache::Record>& records)
{
    std::vector<cache::Key> keys;
    for (const auto& record : records) keys.push_back(record.key);
    return keys;
}

static std::vector<cache::Record> Records(std::span<const cache::Key> keys)
{
    std::vector<cache::Record> records;
    for (const auto& key : keys) records.push_back({ key, {} });
    return records;
}

// A version 1 file (72-byte records, no scenes) as written before Normalize.
static void WriteV1(const fs::path& path, std::span<const cache::Key> keys, uint32_t shaderVersion, uint32_t recipeVersion)
{
    std::vector<uint8_t> bytes(cache::kHeaderBytes + keys.size() * cache::kRecordBytesV1, 0);
    for (size_t i = 0; i < keys.size(); ++i)
    {
        uint8_t* out = bytes.data() + cache::kHeaderBytes + i * cache::kRecordBytesV1;
        const auto encoded = cache::detail::Encode(keys[i]);
        std::copy(encoded.begin(), encoded.end(), out);
        cache::detail::Put64(out + cache::kKeyBytes, cache::detail::Hash(encoded));
    }
    std::copy(cache::detail::kMagic.begin(), cache::detail::kMagic.end(), bytes.begin());
    cache::detail::Put32(bytes.data() + 8, 1);
    cache::detail::Put32(bytes.data() + 12, shaderVersion);
    cache::detail::Put32(bytes.data() + 16, recipeVersion);
    cache::detail::Put32(bytes.data() + 20, uint32_t(cache::kRecordBytesV1));
    cache::detail::Put32(bytes.data() + 24, uint32_t(keys.size()));
    PayloadChecksum(bytes);
    RawWrite(path, bytes);
}

static void NormalizationTests(const fs::path& root)
{
    // Stencil, Z function, early Z, offset enables and blend without a color
    // mask are not pipeline state; the Z write bit and real stencil state are.
    cache::Key raw{};
    raw.vs = 1;
    raw.depthControl = 0x1234567C; // Z and stencil off, Z write on
    raw.modeCull = 0x3805;
    raw.colorMask = 0;
    raw.blend = 0x10203040;
    raw.prim = 13;
    raw.depthFormat = 9;
    raw.stencilRefMask = 0x123456;
    raw.stencilRefMaskBack = 0x654321;
    raw.slopeBias = 0x80000000;
    const auto norm = cache::Normalize(raw);
    Check(norm.depthControl == 0x4 && norm.modeCull == 0x5 && norm.blend == cache::kNoBlend && norm.prim == 4 &&
        !norm.stencilRefMask && !norm.stencilRefMaskBack && !norm.slopeBias, "irrelevant state cleared");
    Check(cache::Normalize(norm) == norm && cache::IsValid(norm), "normalization idempotent and valid");

    auto stencil = raw;
    stencil.depthControl = 0x12345603; // Z, stencil, front face only
    stencil.colorMask = 0xF;
    stencil.depthBias = -5;
    const auto front = cache::Normalize(stencil);
    Check(front.depthControl == 0x00045603 && front.stencilRefMask == 0x123456 && !front.stencilRefMaskBack &&
        front.depthBias == -5 && front.blend == (0x10203040u & 0x1FFF1FFFu), "front stencil and bias kept");
    stencil.depthControl |= 0x80;
    Check(cache::Normalize(stencil).stencilRefMaskBack == 0x654321, "back stencil kept when enabled");
    stencil.depthFormat = 0;
    Check(cache::Normalize(stencil).depthControl == 0 && !cache::Normalize(stencil).depthBias, "no depth target");

    // Version 1 recipes load through Normalize and merge into one record.
    const fs::path v1 = root / "v1.bin";
    auto other = raw;
    other.blend = 0x01010101; // differs only in ignored state
    const cache::Key v1Keys[] = { raw, other };
    WriteV1(v1, v1Keys, 29, 1);
    Check(cache::Load(v1, 29, 2).status == cache::Status::Incompatible, "old recipe version needs opt-in");
    auto loaded = cache::Load(v1, 29, 2, {}, 1);
    Check(loaded.status == cache::Status::Loaded && loaded.migrated && loaded.duplicates == 1 &&
        Keys(loaded.records) == std::vector<cache::Key>{ norm }, "version 1 migration");
    Check(cache::Load(v1, cache::kAnyShaderVersion, 2, {}, 1).status == cache::Status::Loaded, "any shader version");

    // Scenes: overflow collapses to "many"; duplicates merge on write.
    cache::Scenes scenes;
    for (uint32_t id = 1; id <= cache::kSceneSlots; ++id) Check(scenes.Add(cache::SceneTag(cache::kSceneMap, id)), "scene added");
    Check(!scenes.Add(cache::SceneTag(cache::kSceneMap, 1)) && !scenes.Many(), "scene present");
    Check(scenes.Add(cache::SceneTag(cache::kSceneBattle, 7)) && scenes.Many() && scenes.Count() > cache::kSceneSlots, "scene overflow");
    const fs::path tagged = root / "tagged.bin";
    cache::Record a{ norm, {} }, b{ norm, {} };
    a.scenes.Add(cache::SceneTag(cache::kSceneMap, 20));
    b.scenes.Add(cache::SceneTag(cache::kSceneBattle, 3));
    const cache::Record pair[] = { a, b };
    const auto written = cache::Write(tagged, pair, 29, 2);
    loaded = cache::Load(tagged, 29, 2);
    Check(written.ok && written.duplicates == 1 && loaded.status == cache::Status::Loaded && !loaded.migrated &&
        loaded.records.size() == 1 && loaded.records[0].scenes.Contains(cache::SceneTag(cache::kSceneMap, 20)) &&
        loaded.records[0].scenes.Contains(cache::SceneTag(cache::kSceneBattle, 3)), "scene round trip");
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "usage: pipeline_cache_test <scratch-directory>");
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const fs::path root = fs::absolute(fs::path(argv[1])) / ("run-" + std::to_string(stamp));
        fs::create_directories(root);
        const fs::path path = root / fs::path(L"recipes-\u4e2d\u6587.bin");
        const fs::path badPath = root / "invalid.bin";
        constexpr uint32_t shaderVersion = 29, recipeVersion = 3;
        const auto key = SampleKey();
        auto second = key;
        second.ps = 0; // A depth-only PSO legitimately has no pixel shader.
        second.depthBias = std::numeric_limits<int32_t>::min();
        second.slopeBias = std::bit_cast<uint32_t>(-0.0f);
        std::vector<cache::Key> keys{ key, second, key };
        const auto records = Records(keys);

        Check(cache::Load(path, shaderVersion, recipeVersion).status == cache::Status::Missing, "missing file");
        const auto written = cache::Write(path, records, shaderVersion, recipeVersion, ValidateEnums);
        Check(written.ok && written.written == 2 && written.duplicates == 1, "write deduplication");
        const auto original = Read(path);
        Check(original.size() == cache::kHeaderBytes + 2 * cache::kRecordBytes, "explicit framing");
        Check(original[cache::kHeaderBytes] == 0xEF && original[cache::kHeaderBytes + 7] == 1,
            "little-endian shader hash");
        Check(original[cache::kHeaderBytes + 56] == 0x77 && original[cache::kHeaderBytes + 59] == 0xFF,
            "little-endian signed bias bits");
        auto loaded = cache::Load(path, shaderVersion, recipeVersion, ValidateEnums);
        Check(loaded.status == cache::Status::Loaded && Keys(loaded.records) == std::vector<cache::Key>{ key, second }, "key round trip");
        Check(cache::Write(path, records, shaderVersion, recipeVersion, ValidateEnums).ok && Read(path) == original,
            "deterministic replacement");
        Check(cache::KeyHash{}(key) == static_cast<size_t>(cache::detail::Hash(cache::detail::Encode(key))),
            "stable key hash");
        auto distinct = key;
        distinct.slopeBias ^= 0x80000000;
        Check(distinct != key && cache::detail::Encode(distinct) != cache::detail::Encode(key), "exact slope sign equality");
        distinct = key;
        ++distinct.stencilRefMaskBack;
        Check(distinct != key, "back stencil mask equality");
        Check(cache::Load(path, shaderVersion + 1, recipeVersion).status == cache::Status::Incompatible, "shader version");
        Check(cache::Load(path, shaderVersion, recipeVersion + 1).status == cache::Status::Incompatible, "recipe version");

        // Corrupt every byte independently, and every possible truncation of a
        // two-record file. No valid prefix is returned from an invalid file.
        for (size_t i = 0; i < original.size(); ++i)
        {
            auto changed = original;
            changed[i] ^= 0x80;
            RawWrite(badPath, changed);
            loaded = cache::Load(badPath, shaderVersion, recipeVersion, ValidateEnums);
            Check(loaded.status != cache::Status::Loaded && loaded.records.empty(), "single-byte corruption rejected");
            RawWrite(badPath, std::span(original).first(i));
            loaded = cache::Load(badPath, shaderVersion, recipeVersion, ValidateEnums);
            Check(loaded.status != cache::Status::Loaded && loaded.records.empty(), "truncation rejected");
        }
        auto changed = original;
        changed.push_back(0);
        RawWrite(badPath, changed);
        Check(cache::Load(badPath, shaderVersion, recipeVersion).status == cache::Status::Invalid, "trailing data rejected");

        changed = original;
        cache::detail::Put32(changed.data() + 8, cache::kFileVersion + 1);
        HeaderChecksum(changed);
        RawWrite(badPath, changed);
        Check(cache::Load(badPath, shaderVersion, recipeVersion).status == cache::Status::Incompatible, "file version rejected");
        for (const size_t offset : { size_t(20), size_t(24), size_t(28) })
        {
            changed = original;
            cache::detail::Put32(changed.data() + offset, 0xFFFFFFFF);
            HeaderChecksum(changed);
            RawWrite(badPath, changed);
            Check(cache::Load(badPath, shaderVersion, recipeVersion).status == cache::Status::Invalid, "framing bounds rejected");
        }

        // Recompute outer checksums: the per-record checksum must still detect
        // damaged records, and semantic validation must reject damaged enums.
        changed = original;
        changed[cache::kHeaderBytes + 1] ^= 1;
        PayloadChecksum(changed);
        RawWrite(badPath, changed);
        loaded = cache::Load(badPath, shaderVersion, recipeVersion);
        Check(loaded.status == cache::Status::Invalid && loaded.error == "recipe record checksum mismatch", "per-record checksum");

        for (unsigned scenario = 0; scenario < 9; ++scenario)
        {
            auto invalid = key;
            switch (scenario)
            {
            case 0: invalid.flags = 1; break;
            case 1: invalid.colorMask = 0x10; break;
            case 2: invalid.modeCull = 8; break;
            case 3: invalid.stencilRefMask = 0x1000000; break;
            case 4: invalid.stencilRefMaskBack = 0x1000000; break;
            case 5: invalid.slopeBias = 0x7F800000; break;
            case 6: invalid.slopeBias = 0x7FC00001; break;
            case 7: invalid.prim = 31; break;
            case 8: invalid.rtFormat = 0xFFFFFFFF; break;
            }
            const cache::Record invalidRecord{ invalid, {} };
            Check(!cache::Write(path, { &invalidRecord, 1 }, shaderVersion, recipeVersion, ValidateEnums).ok && Read(path) == original,
                "invalid write preserves destination");
            changed = original;
            const auto encoded = cache::detail::Encode(invalid);
            std::copy(encoded.begin(), encoded.end(), changed.begin() + cache::kHeaderBytes);
            cache::detail::Put64(changed.data() + cache::kHeaderBytes + cache::kKeyBytes + cache::kSceneBytes,
                cache::detail::Hash({ changed.data() + cache::kHeaderBytes, cache::kKeyBytes + cache::kSceneBytes }));
            PayloadChecksum(changed);
            RawWrite(badPath, changed);
            loaded = cache::Load(badPath, shaderVersion, recipeVersion, ValidateEnums);
            Check(loaded.status == cache::Status::Invalid && loaded.records.empty(), "invalid semantic state rejected");
        }

        changed = original;
        changed.insert(changed.end(), original.begin() + cache::kHeaderBytes,
            original.begin() + cache::kHeaderBytes + cache::kRecordBytes);
        cache::detail::Put32(changed.data() + 24, 3);
        PayloadChecksum(changed);
        RawWrite(badPath, changed);
        loaded = cache::Load(badPath, shaderVersion, recipeVersion, ValidateEnums);
        Check(loaded.status == cache::Status::Loaded && loaded.records.size() == 2 && loaded.duplicates == 1, "read deduplication");

        const cache::Validator throwing = [](const cache::Key&) -> bool { throw std::runtime_error("fixture failure"); };
        Check(!cache::Write(path, records, shaderVersion, recipeVersion, throwing).ok && Read(path) == original,
            "validator exception preserves destination");
        Check(cache::Load(path, shaderVersion, recipeVersion, throwing).records.empty(), "validator exception returns no recipes");

#ifdef _WIN32
        // Deny DELETE sharing so Windows rejects the final atomic replacement
        // after the temporary file has already been fully written and closed.
        HANDLE locked = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Check(locked != INVALID_HANDLE_VALUE, "lock existing destination");
        const cache::Record secondRecord{ second, {} };
        const auto lockedResult = cache::Write(path, { &secondRecord, 1 }, shaderVersion, recipeVersion, ValidateEnums);
        CloseHandle(locked);
        Check(!lockedResult.ok && Read(path) == original, "failed replacement preserves destination");
#endif
        Check(!cache::Write(path / "child.bin", records, shaderVersion, recipeVersion).ok && Read(path) == original,
            "invalid parent preserves destination");
        for (const auto& entry : fs::directory_iterator(root))
            Check(entry.path().filename().u8string().find(u8".tmp-") == std::u8string::npos, "temporary files cleaned");

        const fs::path maxPath = root / "bounded.bin";
        std::vector<cache::Key> maxKeys(cache::kMaxRecords, key);
        for (size_t i = 0; i < maxKeys.size(); ++i) maxKeys[i].vs += i;
        Check(cache::Write(maxPath, Records(maxKeys), shaderVersion, recipeVersion, ValidateEnums).ok, "record cap accepted");
        loaded = cache::Load(maxPath, shaderVersion, recipeVersion, ValidateEnums);
        Check(loaded.status == cache::Status::Loaded && Keys(loaded.records) == maxKeys, "record cap round trip");
        maxKeys.push_back(second);
        Check(!cache::Write(path, Records(maxKeys), shaderVersion, recipeVersion).ok && Read(path) == original, "record cap overflow preserves destination");

        Check(cache::Write(maxPath, std::span<const cache::Record>{}, shaderVersion, recipeVersion).ok, "empty cache replacement");
        loaded = cache::Load(maxPath, shaderVersion, recipeVersion);
        Check(loaded.status == cache::Status::Loaded && loaded.records.empty(), "empty cache round trip");
        NormalizationTests(root);
        std::cout << "pipeline cache tests passed: round trip, exact keys, dedup, versions, normalization, v1 migration, scenes, "
            << original.size() << " byte corruptions + truncations, state validation, atomic failure, cap="
            << cache::kMaxRecords << "\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "pipeline cache test failed: " << e.what() << '\n';
        return 1;
    }
}
