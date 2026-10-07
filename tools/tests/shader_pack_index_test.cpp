// The startup shader-pack download: index parsing, selection, asset names,
// URLs and the per-contract decline record. No network or window.
#include "updater/shader_pack_index.h"
#include "updater/update.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
int checks = 0;
void Check(bool condition, const char *message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

const std::string ContractA(64, 'a');
const std::string ContractB(64, 'b');
const std::string Sha(64, 'c');

std::string Entry(const std::string &renderer, const std::string &contract, const std::string &file,
                  const std::string &size = "180527457", const std::string &sha = Sha)
{
    return "{\"renderer\":\"" + renderer + "\",\"contract\":\"" + contract + "\",\"file\":\"" + file +
           "\",\"size\":" + size + ",\"sha256\":\"" + sha + "\"}";
}

std::string Index(const std::string &packs, const std::string &schema = "1")
{
    return "{\"schema\":" + schema + ",\"packs\":[" + packs + "]}";
}

bool Rejects(const std::string &text, const std::string &expected)
{
    std::string error;
    return !updater::shader_pack::ParseIndex(text, error) && error.find(expected) != std::string::npos;
}
} // namespace

int main()
{
    try
    {
        using namespace updater::shader_pack;
        std::string error;
        const auto index = ParseIndex(Index(Entry("vulkan", ContractA, "portable_vk-aaaaaaaaaaaaaaaa.lospv") + "," +
                                            Entry("d3d12", ContractA, "portable_dx12-aaaaaaaaaaaaaaaa.lospd", "80462294") + "," +
                                            Entry("metal", ContractB, "portable_metal-bbbbbbbbbbbbbbbb.lospv") + "," +
                                            "{\"renderer\":\"webgpu\",\"anything\":true}"),
                                      error);
        Check(index && index->size() == 3, "valid index parses; unknown renderers are left for newer runtimes");
        const auto *vulkan = Select(*index, "vulkan", ContractA);
        Check(vulkan && vulkan->file == "portable_vk-aaaaaaaaaaaaaaaa.lospv" && vulkan->size == 180527457 &&
                  vulkan->sha256 == Sha,
              "selection returns the entry for the renderer and contract");
        Check(Select(*index, "d3d12", ContractA) && Select(*index, "d3d12", ContractA)->size == 80462294,
              "the same contract digest selects per renderer");
        Check(!Select(*index, "vulkan", ContractB) && !Select(*index, "metal", ContractA),
              "another contract or renderer selects nothing");

        // The recipe corpus is listed like a pack. Runtimes before it skip the entry
        // as an unknown renderer (the "webgpu" case above), so packs still select.
        Check(LowerHex64(CorpusContract), "corpus contract is a lowercase SHA-256");
        const auto withCorpus = ParseIndex(Index(Entry("vulkan", ContractA, "portable_vk-aaaaaaaaaaaaaaaa.lospv") + "," +
                                                 Entry(std::string(CorpusRenderer), std::string(CorpusContract),
                                                       "pipelines_corpus-0123456789abcdef.bin", "409184")),
                                           error);
        const auto *corpus = withCorpus ? Select(*withCorpus, CorpusRenderer, CorpusContract) : nullptr;
        Check(corpus && corpus->file == "pipelines_corpus-0123456789abcdef.bin" && corpus->size == 409184 &&
                  Select(*withCorpus, "vulkan", ContractA),
              "the corpus entry parses and selects beside the packs");
        Check(withCorpus && !Select(*withCorpus, CorpusRenderer, ContractA),
              "a corpus of another recipe format selects nothing");
        Check(Rejects(Index(Entry(std::string(CorpusRenderer), std::string(CorpusContract), "../c.bin")), "malformed"),
              "a malformed corpus entry is rejected like a pack");

        Check(Rejects(Index(Entry("vulkan", ContractA, "a.lospv") + "," + Entry("vulkan", ContractA, "b.lospv")), "twice"),
              "duplicate renderer and contract is rejected");
        Check(Rejects(Index(Entry("vulkan", std::string(64, 'A'), "a.lospv")), "malformed"),
              "contract must be lowercase hex");
        Check(Rejects(Index(Entry("vulkan", ContractA.substr(1), "a.lospv")), "malformed"), "contract must be 64 digits");
        Check(Rejects(Index(Entry("vulkan", ContractA, "a.lospv", "100", std::string(63, 'c'))), "malformed"),
              "sha256 must be 64 digits");
        Check(Rejects(Index(Entry("vulkan", ContractA, "a.lospv", "0")), "malformed"), "empty pack is rejected");
        Check(Rejects(Index(Entry("vulkan", ContractA, "a.lospv", "-5")), "malformed"), "negative size is rejected");
        Check(Rejects(Index(Entry("vulkan", ContractA, "a.lospv", std::to_string(MaxPackBytes + 1))), "malformed"),
              "oversized pack is rejected");
        for (const char *file : {"../a.lospv", "dir/a.lospv", "dir\\\\a.lospv", ".hidden", "", "a b.lospv",
                                 "https:x"})
            Check(Rejects(Index(Entry("vulkan", ContractA, file)), "malformed"), "unsafe asset name is rejected");
        Check(Rejects(Index(Entry("vulkan", ContractA, "a.lospv"), "2"), "schema 2"), "newer schema is reported");
        Check(Rejects("{\"packs\":[]}", "invalid schema") && Rejects("[]", "invalid schema"),
              "missing schema is rejected");
        Check(Rejects("{\"schema\":1,\"packs\":[{\"contract\":\"x\"}]}", "no renderer"), "entry without renderer");
        Check(Rejects("{", "JSON"), "invalid JSON is reported");
        const auto empty = ParseIndex(Index(""), error);
        Check(empty && empty->empty(), "an index with no packs is valid and selects nothing");

        Check(SafeAssetName("portable_vk-4e123a08c0ffee12.lospv") && !SafeAssetName(std::string(129, 'a')) &&
                  !SafeAssetName("a..b") && !SafeAssetName("a%2fb"),
              "asset names are plain file names");
        Check(LowerHex64(ContractA) && !LowerHex64(ContractA + "a") && !LowerHex64(std::string(64, 'g')),
              "hex digest check");

        const auto url = IndexUrl(nullptr);
        Check(url == std::string("https://github.com/") + updater::kReleaseRepository +
                         "/releases/download/shader-packs/index.json",
              "default index lives on the shader-packs release of the update repository");
        Check(IndexUrl("") == url, "an empty override keeps the default");
        Check(IndexUrl("http://127.0.0.1:8765/packs/index.json") == "http://127.0.0.1:8765/packs/index.json",
              "a test override is used verbatim");
        Check(AssetUrl(url, "portable_vk-a.lospv") == std::string("https://github.com/") + updater::kReleaseRepository +
                                                          "/releases/download/shader-packs/portable_vk-a.lospv",
              "assets are fetched beside the index");
        Check(AssetUrl("http://127.0.0.1:8765/index.json", "x.lospd") == "http://127.0.0.1:8765/x.lospd",
              "assets beside an override index");

        const auto root = std::filesystem::temp_directory_path() /
                          ("lo-shader-pack-index-" +
                           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const auto record = root / "shaders" / "declined-downloads.txt";
        Check(!Declined(record, ContractA), "no record means nothing was declined");
        Check(RecordDecline(record, ContractA, error) && Declined(record, ContractA) && !Declined(record, ContractB),
              "a decline is remembered per contract");
        Check(RecordDecline(record, ContractA, error), "recording the same decline again succeeds");
        {
            std::ofstream append(record, std::ios::app | std::ios::binary);
            append << ContractB << "\r\n";
        }
        Check(Declined(record, ContractB), "a CRLF line still matches");
        std::ifstream lines(record);
        int count = 0;
        for (std::string line; std::getline(lines, line);) ++count;
        lines.close();
        Check(count == 2, "a repeated decline does not grow the record");
        std::filesystem::remove_all(root);

        std::cout << "PASS: " << checks << " checks; index parse/select, asset names, URLs, decline record\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
