#include "merge.h"
#include "gpu/shader/portable_shader_contract.h"
#include "gpu/shader/dxc_compiler.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
namespace fs = std::filesystem;
namespace pack = xenos::portable_pack;
using Bytes = std::vector<uint8_t>;

struct Candidate {
    bool include = false, pixel = false;
    uint64_t hash = 0;
    fs::path source;
    std::string provenance, status;
    xenos::TranslatedShader info;
    Bytes binary;
};

fs::path Path(const char* text) { return fs::path(reinterpret_cast<const char8_t*>(text)); }

std::string Hex(uint64_t hash) {
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}

Bytes Read(const fs::path& path, size_t limit) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    const auto size = in.tellg();
    if(size < 0 || uint64_t(size) > limit) throw std::runtime_error("source missing or too large: " + path.string());
    Bytes data(static_cast<size_t>(size));
    in.seekg(0);
    if(!in.read(reinterpret_cast<char*>(data.data()),size)) throw std::runtime_error("source read incomplete: " + path.string());
    return data;
}

uint64_t SourceHash(const Bytes& bytes) {
    uint64_t hash=0xcbf29ce484222325ull;
    for(uint8_t byte:bytes) { hash^=byte; hash*=0x100000001b3ull; }
    return hash;
}

std::vector<Candidate> Manifest(const fs::path& path) {
    if(fs::file_size(path)>256u*1024) throw std::runtime_error("merge manifest too large");
    std::ifstream input(path, std::ios::binary);
    if(!input) throw std::runtime_error("cannot open merge manifest");
    std::string line;
    if(!std::getline(input,line)) throw std::runtime_error("missing merge manifest header");
    if(!line.empty() && line.back()=='\r') line.pop_back();
    if(line!="action\tstage\thash\tsource\tprovenance")
        throw std::runtime_error("manifest header must be: action<TAB>stage<TAB>hash<TAB>source<TAB>provenance");
    std::vector<Candidate> candidates;
    std::set<std::pair<bool,uint64_t>> keys;
    std::map<fs::path,uint64_t> sources;
    while(std::getline(input,line)) {
        if(!line.empty() && line.back()=='\r') line.pop_back();
        if(line.empty()) continue;
        if(line.size()>4096) throw std::runtime_error("merge manifest row too long");
        std::array<std::string,5> field;
        size_t pos=0;
        for(size_t n=0;n<4;++n) {
            const auto end=line.find('\t',pos);
            if(end==std::string::npos) throw std::runtime_error("incomplete manifest row");
            field[n]=line.substr(pos,end-pos); pos=end+1;
        }
        field[4]=line.substr(pos);
        if(field[4].find('\t')!=std::string::npos || field[4].empty() ||
           std::any_of(field.begin(),field.end(),[](const auto& text) {
               return std::any_of(text.begin(),text.end(),[](unsigned char c) {return c<32 && c!='\t';});
           }) ||
           (field[0]!="include" && field[0]!="exclude") ||
           (field[1]!="vs" && field[1]!="ps") || field[2].size()!=16)
            throw std::runtime_error("invalid manifest row");
        uint64_t hash=0;
        for(char c:field[2]) {
            if(!((c>='0' && c<='9') || (c>='a' && c<='f')))
                throw std::runtime_error("manifest hash must be 16 lowercase hex digits");
            hash=(hash<<4)|uint64_t(c>='a' ? c-'a'+10 : c-'0');
        }
        if(!hash) throw std::runtime_error("zero shader hash");
        Candidate item;
        item.include=field[0]=="include"; item.pixel=field[1]=="ps";
        item.hash=hash; item.provenance=field[4];
        if(!keys.emplace(item.pixel,hash).second) throw std::runtime_error("duplicate manifest shader key");
        if(item.include) {
            if(field[3].empty() || field[3]=="-") throw std::runtime_error("included source path missing");
            item.source=fs::absolute(path.parent_path()/Path(field[3].c_str())).lexically_normal();
            if(auto [it,inserted]=sources.emplace(item.source,hash); !inserted && it->second!=hash)
                throw std::runtime_error("source path assigned conflicting hashes");
        } else if(field[3]!="-") throw std::runtime_error("excluded source must be '-'");
        candidates.push_back(std::move(item));
    }
    if(!input.eof()) throw std::runtime_error("manifest read failed");
    return candidates;
}

bool SamePath(const fs::path& left,const fs::path& right) {
    if(fs::weakly_canonical(left)==fs::weakly_canonical(right)) return true;
    std::error_code ec;
    return fs::equivalent(left,right,ec) && !ec;
}

void CheckOutputs(const fs::path& destination,const fs::path& reportPath,
                  const fs::path& baseline,const fs::path& image,const fs::path& manifest,
                  const std::vector<Candidate>& candidates) {
    for(const auto& output:{destination,reportPath}) {
        for(const auto& input:{baseline,image,manifest})
            if(SamePath(output,input)) throw std::runtime_error("merge output aliases an input: " + output.string());
        for(const auto& item:candidates)
            if(item.include && SamePath(output,item.source))
                throw std::runtime_error("merge output aliases a source: " + output.string());
        if(fs::exists(output)) throw std::runtime_error("merge output already exists: " + output.string());
    }
}

struct Staging {
    fs::path directory;
    explicit Staging(const fs::path& parent) {
        static std::atomic<uint64_t> sequence{0};
        for(unsigned i=0;i<32;++i) {
            auto candidate=parent/(".lo-pack-merge-"+
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+
                std::to_string(sequence++));
            std::error_code ec;
            if(fs::create_directory(candidate,ec)) {directory=std::move(candidate);return;}
            if(ec && ec!=std::errc::file_exists) throw fs::filesystem_error("merge staging directory",candidate,ec);
        }
        throw std::runtime_error("cannot reserve merge staging directory");
    }
    ~Staging() {std::error_code ec;fs::remove_all(directory,ec);}
    Staging(const Staging&)=delete;
    Staging& operator=(const Staging&)=delete;
};

void Publish(const fs::path& staged,const fs::path& output) {
#ifdef _WIN32
    if(!MoveFileExW(staged.c_str(),output.c_str(),0))
        throw std::runtime_error("merge publication failed");
#else
    // Same parent filesystem; link refuses to replace another writer's output.
    fs::create_hard_link(staged,output);
#endif
}

void Report(const fs::path& path,const pack::Report& result, const std::vector<Candidate>& entries,
            const pack::Report& baseline, const fs::path& baselinePath, std::string_view compiler) {
    // path is inside this invocation's exclusively created staging directory.
    std::ofstream out(path, std::ios::binary);
    if(!out) throw std::runtime_error("cannot write merge report");
    size_t added=0,skipped=0,excluded=0;
    for(const auto& item:entries) {
        if(item.status=="added") ++added;
        else if(item.status=="skipped_existing") ++skipped;
        else ++excluded;
    }
    const auto baselineUtf8=fs::weakly_canonical(baselinePath).u8string();
    out << "{\n  \"schema\": 1,\n  \"contract\": " << std::quoted(xenos::resources::Sha256Hex(result.contract))
        << ",\n  \"baseline_path\": " << std::quoted(std::string(baselineUtf8.begin(),baselineUtf8.end()))
        << ",\n  \"baseline_producer\": " << std::quoted(baseline.producer)
        << ",\n  \"baseline_records\": " << baseline.records << ",\n  \"records\": " << result.records
        << ",\n  \"added\": " << added << ",\n  \"skipped_existing\": " << skipped
        << ",\n  \"excluded\": " << excluded << ",\n  \"failures_omitted\": " << result.failuresOmitted
        << ",\n  \"producer\": " << std::quoted(result.producer)
        << ",\n  \"compiler\": " << std::quoted(std::string(compiler))
        << ",\n  \"compile_options\": " << std::quoted(xenos::cache::DefaultOptions(xenos::cache::Backend::Vulkan))
        << ",\n  \"candidates\": [\n";
    for(size_t i=0;i<entries.size();++i) {
        const auto& item=entries[i];
        const auto source=item.source.u8string();
        out << "    {\"stage\": \"" << (item.pixel?"ps":"vs") << "\", \"hash\": " << std::quoted(Hex(item.hash))
            << ", \"source\": " << std::quoted(std::string(source.begin(),source.end()))
            << ", \"provenance\": " << std::quoted(item.provenance)
            << ", \"status\": " << std::quoted(item.status) << "}" << (i+1==entries.size()?"\n":",\n");
    }
    out << "  ]\n}\n";
    out.close(); if(!out) throw std::runtime_error("merge report write failed");
}
} // namespace

int Merge(int argc,char** argv) {
    if(argc!=6) {
        std::cerr << "Usage: LoShaderPackTool merge baseline.lospv decrypted-image.bin manifest.tsv output-dir\n"
                     "TSV header: action<TAB>stage<TAB>hash<TAB>source<TAB>provenance\n"
                     "Rows: include|exclude, vs|ps, 16 lowercase hex digits, source path relative to manifest (or '-' for exclude), provenance.\n";
        return 2;
    }
    const fs::path baseline=Path(argv[2]), imagePath=Path(argv[3]), manifest=Path(argv[4]);
    const fs::path destination=Path(argv[5])/pack::FileName;
    const auto reportPath=destination.parent_path()/"merge-report.json";
    auto candidates=Manifest(manifest);
    CheckOutputs(destination,reportPath,baseline,imagePath,manifest,candidates);
    std::ifstream input(imagePath,std::ios::binary);
    Bytes image(pack::RuntimeXexBytes);
    if(!input.read(reinterpret_cast<char*>(image.data()),std::streamsize(image.size())))
        throw std::runtime_error("missing/short decrypted runtime image (use xexdump output)");
    auto contract=pack::RuntimeContract(image);
    // Same audited xexdump/loaded-guest image exception as verify-runtime.
    if(xenos::resources::Sha256Hex(contract)=="d5a2fab10441a46444b6b41ffcb4f1ba562bea75668a7b445fd43688aec67507") {
        const auto stored=pack::Reader::Inspect(baseline).contract;
        if(xenos::resources::Sha256Hex(stored)=="f6fd1179b50f6ff9b63d6be84c662d1337af6b7dfa78865a9a6c025509c9b77f")
            contract=stored;
    }
    pack::Reader reader(baseline,contract);
    std::vector<uint32_t> words;
    size_t added=0;
    for(auto& item:candidates) {
        if(!item.include) {item.status="excluded";continue;}
        const Bytes source=Read(item.source,262144);
        if(source.size()<12 || source.size()%4 || SourceHash(source)!=item.hash)
            throw std::runtime_error("invalid source size or renderer-byte FNV-1a hash: " + item.source.string());
        if(reader.Contains(item.pixel,item.hash)) {item.status="skipped_existing";continue;}
        words.resize(source.size()/4);
        for(size_t i=0;i<words.size();++i)
            words[i]=(uint32_t(source[i*4])<<24)|(uint32_t(source[i*4+1])<<16)|
                (uint32_t(source[i*4+2])<<8)|source[i*4+3];
        item.info=xenos::TranslateShader(words.data(),uint32_t(words.size()),item.pixel);
        if(!item.info.errors.empty()) throw std::runtime_error("translation failed for " + Hex(item.hash) + ": " + item.info.errors);
        auto compiled=xenos::CompileHlsl(item.info.hlsl,"main",item.pixel?"ps_6_0":"vs_6_0",xenos::ShaderBinaryFormat::Spirv);
        if(!compiled.ok || compiled.bytecode.empty())
            throw std::runtime_error("SPIR-V compilation failed for " + Hex(item.hash) + ": " + compiled.errors);
        item.binary=std::move(compiled.bytecode);
        item.status="added"; ++added;
    }
    // Do not touch final paths until BOTH staged outputs have been completed.
    fs::create_directories(destination.parent_path());
    Staging staging(destination.parent_path());
    const auto stagedPack=staging.directory/pack::FileName;
    const auto stagedReport=staging.directory/"merge-report.json";
    if(!added) {
        fs::copy_file(baseline,stagedPack);
        Report(stagedReport,reader.Info(),candidates,reader.Info(),baseline,"");
    } else {
        const auto compiler=xenos::DxcIdentity();
        if(compiler.empty()) throw std::runtime_error("cannot certify merge compiler identity");
        pack::Writer writer(stagedPack,contract,"merged:"+compiler);
        writer.Import(reader);
        for(auto& item:candidates) if(item.status=="added") writer.Add(item.hash,item.info,item.binary);
        const auto result=writer.Finish();
        if(result.records!=reader.Info().records+added) throw std::runtime_error("merged record count mismatch");
        Report(stagedReport,result,candidates,reader.Info(),baseline,compiler);
    }
    CheckOutputs(destination,reportPath,baseline,imagePath,manifest,candidates);
    bool publishedPack=false;
    try {
        Publish(stagedPack,destination); publishedPack=true;
        // Narrow failure injection exercises rollback after the first publication.
        if(const char* fail=std::getenv("LO_PACK_MERGE_TEST_FAIL_REPORT_PUBLISH"); fail && std::string_view(fail)=="1")
            throw std::runtime_error("injected merge report publication failure");
        Publish(stagedReport,reportPath);
    } catch(...) {
        if(publishedPack) {std::error_code ec;fs::remove(destination,ec);}
        throw;
    }
    std::cout << "merge: baseline=" << reader.Info().records << " added=" << added
              << " output=" << destination.string() << "\n";
    return 0;
}
