#include "gpu/shader/portable_shader_contract.h"
#include "gpu/shader/portable_shader_pack_location.h"
#include "merge.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>

// The runtime contract hashes the image as parsed, before import binding,
// which is what xexdump writes, so this is the value the game computes.
static std::vector<uint8_t> ReadRuntimeImage(const char* argument) {
    std::ifstream in(std::filesystem::path(reinterpret_cast<const char8_t*>(argument)), std::ios::binary);
    std::vector<uint8_t> image(xenos::portable_pack::RuntimeXexBytes);
    if (!in.read(reinterpret_cast<char*>(image.data()), std::streamsize(image.size())))
        throw std::runtime_error("missing/short decrypted runtime image (use xexdump output)");
    return image;
}

int main(int argc,char** argv) try {
    const auto command = argc > 1 ? std::string_view(argv[1]) : std::string_view{};
    if (command == "merge") return Merge(argc, argv);
    if (command == "contract" && argc == 3) {
        // The contract each renderer's startup download looks up in index.json.
        // "vulkan" also covers Android and Metal on macOS.
        namespace pack = xenos::portable_pack;
        const auto image = ReadRuntimeImage(argv[2]);
        const char* separator = "{\n";
        for (const auto flavor : {pack::Flavor::Vulkan, pack::Flavor::D3D12}) {
            std::cout << separator << "  \"" << pack::FlavorName(flavor) << "\": \""
                      << xenos::resources::Sha256Hex(pack::FlavorContract(image, flavor)) << "\"";
            separator = ",\n";
        }
        std::cout << "\n}\n";
        return 0;
    }
    const bool runtime = command == "verify-runtime";
    if ((runtime ? argc != 4 : argc != 3) ||
        (command != "inspect" && command != "verify" && !runtime)) {
        std::cerr << "Usage: LoShaderPackTool <inspect|verify> pack.lospv|pack.lospd\n"
                     "       LoShaderPackTool verify-runtime pack.lospv|pack.lospd xexdump-image.bin\n"
                     "       LoShaderPackTool contract xexdump-image.bin\n"
                     "       LoShaderPackTool merge baseline.lospv xexdump-image.bin manifest.tsv output-dir\n";
        return 2;
    }
    const bool verified = command != "inspect";
    const auto path = std::filesystem::path(reinterpret_cast<const char8_t*>(argv[2]));
    xenos::portable_pack::Report r;
    if (runtime) {
        const auto format = xenos::portable_pack::Reader::Inspect(path).format;
        const auto image = ReadRuntimeImage(argv[3]);
        const auto backend = format == xenos::portable_pack::PackFormat::Dxil ?
            xenos::cache::Backend::D3D12 : xenos::cache::Backend::Vulkan;
        const auto identity = xenos::cache::MakeIdentity(backend, "", xenos::cache::UboConstantsRequested());
        const auto expected = xenos::portable_pack::RuntimeContract(image, identity, format);
        xenos::portable_pack::Reader reader(path, expected, format);
        reader.VerifyAll();
        r = reader.Info();
    } else r = xenos::portable_pack::Reader::Inspect(path, verified);
    std::cout<<"{\n  \"schema\": "<<xenos::portable_pack::Schema
        <<",\n  \"format\": \""<<(r.format == xenos::portable_pack::PackFormat::Spirv ? "spirv" : "dxil")<<"\""
        <<",\n  \"contract\": \""<<xenos::resources::Sha256Hex(r.contract)<<"\""
        <<",\n  \"records\": "<<r.records<<",\n  \"unique_binaries\": "<<r.uniqueBinaries
        <<",\n  \"blocks\": "<<r.blocks<<",\n  \"failures_omitted\": "<<r.failuresOmitted
        <<",\n  \"reconstructed_hlsl_bytes_omitted\": "<<r.hlslBytesOmitted
        <<",\n  \"diagnostic_bytes_omitted\": "<<r.diagnosticBytesOmitted
        <<",\n  \"binary_bytes_before_dedup\": "<<r.binaryBytes
        <<",\n  \"binary_bytes_after_dedup\": "<<r.uniqueBinaryBytes
        <<",\n  \"compressed_payload_bytes\": "<<r.compressedBytes
        <<",\n  \"index_bytes\": "<<r.indexBytes
        <<",\n  \"file_bytes\": "<<r.fileBytes
        <<",\n  \"file_mib\": "<<std::fixed<<std::setprecision(3)<<double(r.fileBytes)/1048576.0
        <<",\n  \"all_payloads_verified\": "<<(verified?"true":"false")
        <<",\n  \"runtime_compatibility_verified\": "<<(runtime?"true":"false")<<"\n}\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<"shader pack: "<<e.what()<<'\n';return 1;}
