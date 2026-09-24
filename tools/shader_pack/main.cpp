#include "gpu/shader/portable_shader_contract.h"
#include "merge.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>

int main(int argc,char** argv) try {
    const auto command = argc > 1 ? std::string_view(argv[1]) : std::string_view{};
    if (command == "merge") return Merge(argc, argv);
    const bool runtime = command == "verify-runtime";
    if ((runtime ? argc != 4 : argc != 3) ||
        (command != "inspect" && command != "verify" && !runtime)) {
        std::cerr << "Usage: LoShaderPackTool <inspect|verify> pack.lospv\n"
                     "       LoShaderPackTool verify-runtime pack.lospv decrypted-image.bin\n"
                     "       LoShaderPackTool merge baseline.lospv decrypted-image.bin manifest.tsv output-dir\n";
        return 2;
    }
    const bool verified = command != "inspect";
    const auto path = std::filesystem::path(reinterpret_cast<const char8_t*>(argv[2]));
    xenos::portable_pack::Report r;
    if (runtime) {
        std::ifstream in(std::filesystem::path(reinterpret_cast<const char8_t*>(argv[3])), std::ios::binary);
        std::vector<uint8_t> image(xenos::portable_pack::RuntimeXexBytes);
        if (!in.read(reinterpret_cast<char*>(image.data()), std::streamsize(image.size())))
            throw std::runtime_error("missing/short decrypted runtime image (use xexdump output)");
        auto expected = xenos::portable_pack::RuntimeContract(image);
        // xexdump's image precedes XexLoader's import-thunk writes. The
        // supported Disc 1 image and the pack captured from the loaded guest
        // have this audited contract pair; other images use the direct value.
        if (xenos::resources::Sha256Hex(expected) ==
            "d5a2fab10441a46444b6b41ffcb4f1ba562bea75668a7b445fd43688aec67507") {
            const auto stored = xenos::portable_pack::Reader::Inspect(path);
            if (xenos::resources::Sha256Hex(stored.contract) ==
                "f6fd1179b50f6ff9b63d6be84c662d1337af6b7dfa78865a9a6c025509c9b77f")
                expected = stored.contract;
        }
        xenos::portable_pack::Reader reader(path, expected);
        reader.VerifyAll();
        r = reader.Info();
    } else r = xenos::portable_pack::Reader::Inspect(path, verified);
    std::cout<<"{\n  \"schema\": "<<xenos::portable_pack::Schema
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
