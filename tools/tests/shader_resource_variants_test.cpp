#include "gpu/shader/resource_variants.h"
#include <chrono>
#include <iostream>
#include <map>

namespace variants = xenos::resources::variants;
namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;
static void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static void Write(const fs::path& path, std::span<const uint8_t> data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
    Require(bool(out), "fixture write failed");
}
static Bytes Read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    Require(bool(in) && in.tellg() >= 0 && in.tellg() <= 262144, "invalid reference input");
    Bytes data(static_cast<size_t>(in.tellg()));
    in.seekg(0);
    Require(bool(in.read(reinterpret_cast<char*>(data.data()), std::streamsize(data.size()))), "reference read failed");
    return data;
}
int main(int argc, char** argv) {
    try {
        Require(argc == 2 || argc == 4 || argc == 5, "usage: shader_resource_variants_test scratch [original-source reference-354 [reference-linked]]");
        const auto scratch = fs::path(argv[1]) / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        fs::create_directories(scratch / "empty");
        size_t calls = 0;
        const auto save = [&](auto, auto) { ++calls; };
        const auto empty = variants::GenerateFixedVariants(scratch / "empty", save);
        Require(!calls && !empty.generated && empty.missingBases == 77 && !empty.invalidBases, "empty input must not synthesize shaders");
        const auto stopped = variants::GenerateFixedVariants(scratch / "empty", save, [] { return true; });
        Require(stopped.cancelled && !stopped.generated && !stopped.bytesRead && !calls, "early cancellation");
        bool rejected = false;
        try { variants::GenerateFixedVariants(scratch / "empty", {}); }
        catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected, "missing save callback must be rejected");
        fs::create_directories(scratch / "invalid");
        for (size_t i = 0; i < 3; ++i) {
            const auto& source = variants::detail::sources[i];
            // Correct length / too short / too long, all with wrong bytes.
            const size_t length = i == 0 ? source.size : i == 1 ? source.size - 1 : source.size + 1;
            Write(scratch / "invalid" / variants::detail::Name(source.hash), Bytes(length));
        }
        Write(scratch / "invalid" / "vs_0000000000000000.bin", Bytes(12));
        const auto invalid = variants::GenerateFixedVariants(scratch / "invalid", save);
        Require(!calls && !invalid.generated && invalid.invalidBases == 3 && invalid.missingBases == 74,
                "wrong hash and malformed lengths must not generate; unrelated files ignored");
        Require(invalid.bytesRead == variants::detail::sources[0].size, "malformed lengths rejected before allocation/read");
        const auto linkedEmpty = variants::GenerateLinkedVariants(scratch / "empty", save);
        Require(!linkedEmpty.generated && linkedEmpty.missingBases == 77 && linkedEmpty.missingPixelSources == 86,
                "empty input cannot generate linked shaders");
        const auto linkedStopped = variants::GenerateLinkedVariants(scratch / "empty", save, [] { return true; });
        Require(linkedStopped.cancelled && !linkedStopped.bytesRead && !calls, "linked early cancellation");
        std::cout << "PASS: empty input, cancellation, callback validation, wrong hash, short/long input, unknown names\n";
        if (argc == 2) return 0;

        const auto sourceDirectory = fs::path(argv[2]), references = fs::path(argv[3]);
        std::map<uint64_t, Bytes> generated;
        const auto full = variants::GenerateFixedVariants(sourceDirectory, [&](uint64_t hash, std::span<const uint8_t> data) {
            Require(variants::detail::Hash(data) == hash, "callback hash must describe exact output bytes");
            Require(generated.emplace(hash, Bytes(data.begin(), data.end())).second, "duplicate generated output");
        });
        Require(full.verifiedBases == 77 && !full.missingBases && !full.invalidBases && full.generated == 354,
                "original source set must generate exactly 354 candidates");
        // Generation is complete before any reference bytes are read.
        size_t referenceCount = 0;
        for (const auto& entry : fs::directory_iterator(references)) {
            if (entry.is_regular_file() && entry.path().extension() == ".bin") ++referenceCount;
        }
        Require(referenceCount == 354, "reference set must contain exactly 354 candidates");
        fs::create_directories(scratch / "generated");
        for (const auto& [hash, data] : generated) {
            const auto name = variants::detail::Name(hash);
            Require(Read(references / name) == data, "generated candidate differs from independently derived reference");
            Write(scratch / "generated" / name, data);
        }
        fs::create_directories(scratch / "one-source");
        const auto& first = variants::detail::sources[0];
        const auto name = variants::detail::Name(first.hash);
        auto code = Read(sourceDirectory / name);
        Write(scratch / "one-source" / name, code);
        calls = 0;
        const auto one = variants::GenerateFixedVariants(scratch / "one-source", save);
        Require(one.verifiedBases == 1 && one.missingBases == 76 && calls > 0 && calls < 354, "partial original source coverage");
        Require(Read(scratch / "one-source" / name) == code, "generation must leave input unchanged");
        calls = 0;
        const auto partial = variants::GenerateFixedVariants(scratch / "one-source", save, [&] { return calls != 0; });
        Require(partial.cancelled && partial.generated == 1 && calls == 1, "cancellation between outputs");
        rejected = false;
        try { variants::GenerateFixedVariants(scratch / "one-source", [](auto, auto) { throw std::runtime_error("save failed"); }); }
        catch (const std::runtime_error& error) { rejected = std::string(error.what()) == "save failed"; }
        Require(rejected, "save failures must propagate to the caller");
        code[0] ^= 1;
        Write(scratch / "one-source" / name, code);
        calls = 0;
        const auto corrupt = variants::GenerateFixedVariants(scratch / "one-source", save);
        Require(corrupt.invalidBases == 1 && !corrupt.generated && !calls, "one-byte corruption cannot use source metadata");
        std::cout << "PASS: 77 original sources -> 354 distinct outputs, all byte-identical to reference; partial coverage, input preservation, mid-run cancellation, save failure, corruption\n"
                  << "bytesRead=" << full.bytesRead << " generated=" << (scratch / "generated").string() << '\n';
        if (argc == 5) {
            const auto started = std::chrono::steady_clock::now();
            std::map<uint64_t, Bytes> linked;
            const auto expanded = variants::GenerateLinkedVariants(sourceDirectory, [&](uint64_t hash, std::span<const uint8_t> data) {
                Require(!generated.contains(hash), "linked API must exclude fixed candidates");
                Require(linked.emplace(hash, Bytes(data.begin(), data.end())).second, "duplicate linked output");
            });
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
            Require(expanded.verifiedBases == 77 && expanded.verifiedPixelSources == 86 &&
                    !expanded.invalidBases && !expanded.invalidPixelSources && expanded.generated == 1891,
                    "original source signatures must generate exactly 1891 additional linked candidates");
            // Compare only after generation is finished.
            fs::create_directories(scratch / "linked-generated");
            for (const auto& [hash, data] : linked) {
                Require(Read(fs::path(argv[4]) / variants::detail::Name(hash)) == data, "SDK linked bytes differ from independent reference");
                Write(scratch / "linked-generated" / variants::detail::Name(hash), data);
            }
            calls = 0;
            const auto linkedPartial = variants::GenerateLinkedVariants(sourceDirectory, save, [&] { return calls != 0; });
            Require(linkedPartial.cancelled && calls == 1 && linkedPartial.generated == 1, "linked cancellation between outputs");
            rejected = false;
            try { variants::GenerateLinkedVariants(sourceDirectory, [](auto, auto) { throw std::runtime_error("linked save failed"); }); }
            catch (const std::runtime_error& error) { rejected = std::string(error.what()) == "linked save failed"; }
            Require(rejected, "linked save failure must propagate");
            fs::create_directories(scratch / "no-pixels");
            for (const auto& source : variants::detail::sources) {
                const auto inputName = variants::detail::Name(source.hash);
                Write(scratch / "no-pixels" / inputName, Read(sourceDirectory / inputName));
            }
            const auto noPixels = variants::GenerateLinkedVariants(scratch / "no-pixels", [](auto, auto) {});
            Require(noPixels.missingPixelSources == 86 && noPixels.generated < expanded.generated,
                    "missing PS source must not authorize representative PS link plans");
            const auto& pixel = variants::detail::link::pixels[0];
            auto pixelName = variants::detail::Name(pixel.hash); pixelName[0] = 'p';
            Write(scratch / "no-pixels" / pixelName, Bytes(pixel.size));
            const auto badPixel = variants::GenerateLinkedVariants(scratch / "no-pixels", [](auto, auto) {});
            Require(badPixel.invalidPixelSources == 1 && badPixel.generated == noPixels.generated,
                    "corrupt PS source cannot authorize link metadata");
            std::cout << "PASS: 1891 additional SDK linked outputs byte-identical, PS hash gating, linked cancellation/save failure; total=2245 ms="
                      << elapsed << " bytesRead=" << expanded.bytesRead << '\n';
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
