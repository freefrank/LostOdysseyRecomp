#include "gpu/shader_source_capture.h"
#include "os/capture_archive.h"
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>

namespace {
std::atomic<bool> countAllocations{false};
std::atomic<size_t> allocationCount{0};
}
void* operator new(size_t size) {
    if (countAllocations.load(std::memory_order_relaxed)) allocationCount.fetch_add(1, std::memory_order_relaxed);
    if (auto* result = std::malloc(size ? size : 1)) return result;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, size_t) noexcept { std::free(value); }
void operator delete[](void* value, size_t) noexcept { std::free(value); }

static std::string Read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    assert(in);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

int main(int argc, char** argv) {
    using gpu::shader_source_capture::Capture;
    assert(argc == 2);
    const auto root = std::filesystem::absolute(argv[1]);
    assert(!std::filesystem::exists(root));
    std::filesystem::create_directories(root);
    const uint32_t words[]{0x01020304,0xaabbccdd,0x99887766};
    const auto raw = std::span(reinterpret_cast<const uint8_t*>(words), sizeof(words));
    const auto hash = Capture::Hash(raw);
    auto capture = std::make_shared<Capture>();

    countAllocations = true;
    for (uint64_t frame = 101; frame <= 103; ++frame) {
        capture->Observe(true, hash, words, 3, frame);
        capture->Observe(false, hash, words, 3, frame);
        capture->MarkHlsl(true, hash, false);
        capture->NotePixelNotBound();
    }
    countAllocations = false;
    assert(allocationCount == 0);
    assert(capture->Count() == 2);
    assert(capture->At(0).firstFrame == 101 && capture->At(0).lastFrame == 103);
    assert(capture->At(0).vertex && !capture->At(1).vertex);
    assert(std::memcmp(capture->Bytes(0).data(), words, sizeof(words)) == 0);
    capture->Write(root / "roundtrip");
    size_t binaryFiles = 0;
    for (const auto& file : std::filesystem::directory_iterator(root / "roundtrip/shaders/source")) {
        if (file.path().extension() != ".bin") continue;
        ++binaryFiles;
        assert(Read(file.path()) == std::string(reinterpret_cast<const char*>(words), sizeof(words)));
    }
    assert(binaryFiles == 2);
    const auto manifest = Read(root / "roundtrip/shaders/source/manifest.json");
    assert(manifest.find("\"complete\":true") != std::string::npos);
    assert(manifest.find("hlsl_unavailable") != std::string::npos);
    assert(manifest.find("\"pixel_not_bound_draws\":3") != std::string::npos);
    assert(manifest.find(xenos::resources::Sha256Hex(xenos::resources::Sha256(raw))) != std::string::npos);

    Capture invalid(6, sizeof(words));
    countAllocations = true;
    invalid.Observe(true, hash, words, 3, 1);
    invalid.Observe(true, 1, nullptr, 3, 1);
    invalid.Observe(true, 2, words, Capture::MaxProgramBytes / 4 + 1, 1);
    invalid.Observe(false, hash, words, 3, 1);
    const uint32_t changed[]{1,2,3};
    invalid.Observe(true, hash, changed, 3, 2);
    countAllocations = false;
    assert(allocationCount == 0);
    assert(invalid.At(0).status == Capture::Status::HashConflict);
    assert(invalid.At(1).status == Capture::Status::Missing);
    assert(invalid.At(2).status == Capture::Status::Oversize);
    assert(invalid.At(3).status == Capture::Status::ByteCapacity);
    invalid.Write(root / "invalid");
    const auto invalidManifest = Read(root / "invalid/shaders/source/manifest.json");
    for (const auto reason : {"hash_conflict", "missing_microcode", "program_exceeds_64kib", "byte_capacity_exhausted", "\"complete\":false"})
        assert(invalidManifest.find(reason) != std::string::npos);

    Capture limited(1, sizeof(words));
    limited.Observe(true, hash, words, 3, 1);
    limited.Observe(false, hash, words, 3, 1);
    assert(limited.DroppedRecords() == 1);
    limited.Reset();
    assert(limited.Count() == 0 && limited.DroppedRecords() == 0);
    limited.Observe(false, hash, words, 3, 4);
    assert(limited.Count() == 1 && !limited.At(0).vertex);

    Capture badHash;
    badHash.Observe(true, hash ^ 1, words, 3, 1);
    badHash.Write(root / "bad-hash");
    assert(Read(root / "bad-hash/shaders/source/manifest.json").find("hash_mismatch") != std::string::npos);

    std::filesystem::create_directories(root / "write-failure");
    std::ofstream(root / "write-failure/shaders") << "blocks directory creation";
    bool writeFailed = false;
    try { capture->Write(root / "write-failure"); }
    catch (const std::system_error&) { writeFailed = true; }
    assert(writeFailed);

#ifdef _WIN32
    const auto partial = root / "captures/render-partial";
    std::filesystem::create_directories(partial);
    const auto caller = std::this_thread::get_id();
    std::thread::id worker;
    auto future = os::StartCaptureArchive(partial, [capture, &worker](const std::filesystem::path& directory) {
        worker = std::this_thread::get_id();
        capture->Write(directory);
        throw std::system_error(std::make_error_code(std::errc::io_error));
    });
    const auto result = future.get();
    assert(worker != caller);
    assert(!result.saved && result.error == std::errc::io_error);
    assert(std::filesystem::exists(partial / "shaders/source/manifest.json"));
    assert(!std::filesystem::exists(result.archive));
#endif
    std::cout << "shader source capture: roundtrip/stages/3 frames/warm HLSL/missing/oversize/capacity/conflict/SHA/write failure/background failure passed; Observe allocations=" << allocationCount << '\n';
}
