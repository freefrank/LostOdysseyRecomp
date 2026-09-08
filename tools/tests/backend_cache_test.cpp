#include "gpu/shader/binary_cache.h"
#include "gpu/shader/startup_cache.h"
#include "gpu/shader/dxc_compiler.h"
#include <cstdio>
#include <cstdlib>

namespace cache = xenos::cache;
namespace sc = xenos::startup_cache;
namespace fs = std::filesystem;
static unsigned checks=0;
static void Check(bool value, const char* expression, int line) {
    ++checks;
    if (!value) { std::fprintf(stderr,"FAIL line %d: %s\n",line,expression); std::exit(1); }
}
#define CHECK(...) Check((__VA_ARGS__), #__VA_ARGS__, __LINE__)
static std::vector<uint8_t> Read(const fs::path& path) {
    std::ifstream in(path,std::ios::binary); return {std::istreambuf_iterator<char>(in),{}};
}
static void Write(const fs::path& path,std::span<const uint8_t> bytes) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc); sc::WriteRaw(out,bytes);
}
static uint32_t Word(std::span<const uint8_t> bytes,size_t offset) {
    uint32_t result; std::memcpy(&result,bytes.data()+offset,4); return result;
}

int main(int argc,char** argv) {
    if(argc==2 && std::string_view(argv[1])=="--framing-only") {
        constexpr auto chunks=cache::MaxContainerChunks;
        const size_t tableEnd=32+chunks*4;
        std::vector<uint8_t> bytes(tableEnd+chunks*8);
        auto word=[&](size_t offset,uint32_t value) {std::memcpy(bytes.data()+offset,&value,4);};
        std::memcpy(bytes.data(),"DXBC",4);word(24,uint32_t(bytes.size()));word(28,chunks);
        for(uint32_t i=0;i<chunks;++i) {word(32+i*4,uint32_t(tableEnd+i*8));word(tableEnd+i*8,0x4c495844);}
        CHECK(cache::CompleteContainer(bytes));
        // Reverse-order chunks remain valid; they need not be stored in table order.
        for(uint32_t i=0;i<chunks;++i) word(32+i*4,uint32_t(tableEnd+(chunks-1-i)*8));
        CHECK(cache::CompleteContainer(bytes));
        word(32,Word(bytes,36));CHECK(!cache::CompleteContainer(bytes));
        word(28,chunks+1);CHECK(!cache::CompleteContainer(bytes));
        word(28,1);word(32,32);CHECK(!cache::CompleteContainer(bytes));
        std::printf("PASS bounded framing: %u checks; maximum/reversed chunks, overlap, excessive count and table overlap; no DXC calls\n",checks);
        return 0;
    }
    if(argc<2) {std::fputs("usage: LoBackendCacheTest <isolated-new-directory> [--restart|--builtins-only]\n",stderr);return 2;}
    const fs::path root=fs::absolute(fs::u8path(argv[1]));
    const bool restart=argc>2 && std::string_view(argv[2])=="--restart";
    const bool builtinsOnly=argc>2 && std::string_view(argv[2])=="--builtins-only";
    if(!restart && fs::exists(root)) {std::fputs("initial directory must be new\n",stderr);return 2;}
    fs::create_directories(root);
    const auto configured=(root/"builtins").string();
    // Production reads CRT getenv: updating only Win32's environment leaves
    // the CRT's already-initialized copy unchanged in this same process.
    CHECK(_putenv_s("LO_SHADER_CACHE_DIR",configured.c_str())==0);
    CHECK(std::string(std::getenv("LO_SHADER_CACHE_DIR"))==configured);
    CHECK(xenos::DxcAvailable() && !xenos::DxcIdentity().empty());
    const std::string source="float4 main() : SV_Target { return float4(0.125, 0.25, 0.5, 1.0); }";
    std::array<std::vector<uint8_t>,2> binaries;
    for(size_t index=0;index<2;++index) {
        const auto format=index ? xenos::ShaderBinaryFormat::Spirv : xenos::ShaderBinaryFormat::Dxil;
        const auto before=xenos::GetDxcStatistics().calls;
        auto compiled=xenos::CompileCachedHlsl(source,"main","ps_6_0",format);
        CHECK(compiled.ok && !compiled.bytecode.empty());
        CHECK(xenos::GetDxcStatistics().calls==before+(restart ? 0 : 1));
        binaries[index]=compiled.bytecode;
        if(!restart) {
            auto warm=xenos::CompileCachedHlsl(source,"main","ps_6_0",format);
            CHECK(warm.ok && warm.bytecode==compiled.bytecode);
            CHECK(xenos::GetDxcStatistics().calls==before+1);
        }
    }
    CHECK(fs::is_directory(root/"builtins/builtin"));
    CHECK(std::distance(fs::directory_iterator(root/"builtins/builtin"),fs::directory_iterator{})==2);
    if(restart) {
        CHECK(xenos::GetDxcStatistics().calls==0);
        std::printf("PASS restart: %u checks; DXIL and SPIR-V builtin cache hits, 0 actual DXC calls\n",checks);
        return 0;
    }
    if(builtinsOnly) {
        CHECK(xenos::GetDxcStatistics().calls==2);
        std::printf("PASS isolated cold/warm builtins: %u checks; DXIL and SPIR-V compiled once each, warm 0 additional DXC calls; compiler %s\n",checks,xenos::DxcIdentity().c_str());
        return 0;
    }

    auto dxil=cache::MakeIdentity(cache::Backend::D3D12,xenos::DxcIdentity());
    auto spirv=cache::MakeIdentity(cache::Backend::Vulkan,xenos::DxcIdentity());
    auto dxbc=cache::MakeIdentity(cache::Backend::D3D11,"reserved-fixture-compiler");
    CHECK(cache::RuntimeSupported(dxil) && cache::RuntimeSupported(spirv));
    CHECK(cache::ValidIdentity(dxbc) && !cache::RuntimeSupported(dxbc));
    CHECK(cache::CompleteBinary(binaries[0],cache::Format::Dxil));
    CHECK(cache::CompleteBinary(binaries[1],cache::Format::Spirv));
    CHECK(!cache::CompleteBinary(binaries[0],cache::Format::Dxbc));
    CHECK(!cache::CompleteBinary(binaries[0],cache::Format::Spirv));
    CHECK(!cache::CompleteBinary(binaries[1],cache::Format::Dxil));
    // An SM5 SHDR chunk and a DXIL chunk share the DXBC outer magic; the
    // runtime must distinguish their format without relying on extensions.
    auto reserved=binaries[0]; bool replaced=false;
    for(uint32_t i=0;i<Word(reserved,28);++i) {
        const auto offset=Word(reserved,32+i*4);
        if(Word(reserved,offset)==0x4c495844) {std::memcpy(reserved.data()+offset,"SHDR",4);replaced=true;}
    }
    CHECK(replaced && cache::CompleteBinary(reserved,cache::Format::Dxbc));
    CHECK(!cache::CompleteBinary(reserved,cache::Format::Dxil));
    fs::create_directories(root/"cache");fs::create_directories(root/"game");
    constexpr uint64_t hash=0x1248abcd76543210ULL;
    const auto path=root/"cache"/cache::FileName(true,hash,dxil);
    bool present=true;
    CHECK(cache::ReadBinary(path,true,hash,dxil,&present).empty() && !present);
    CHECK(cache::WriteBinary(path,true,hash,dxil,binaries[0]));
    CHECK(cache::ReadBinary(path,true,hash,dxil,&present)==binaries[0] && present);
    const auto original=Read(path);
    CHECK(!cache::WriteBinary(path,true,hash,dxil,reserved));
    CHECK(Read(path)==original);
    CHECK(cache::ReadBinary(path,false,hash,dxil).empty());
    CHECK(cache::ReadBinary(path,true,hash+1,dxil).empty());

    std::vector<cache::Identity> changes{spirv,dxbc};
    auto changed=dxil;changed.compiler+="-other-validator";changes.push_back(changed);
    changed=dxil;++changed.translatorVersion;changes.push_back(changed);
    changed=dxil;changed.options+=";Zi";changes.push_back(changed);
    changed=dxil;changed.variant="linked-vs-variant-2";changes.push_back(changed);
    changed=dxil;changed.format=cache::Format::Dxbc;changes.push_back(changed);
    changed=dxil;changed.backend=static_cast<cache::Backend>(99);changes.push_back(changed);
    changed=dxil;changed.compiler.clear();changes.push_back(changed);
    const auto xex=sc::Bytes("synthetic-xex");
    const auto snapshot=sc::Snapshot(root/"game",root/"cache",dxil,xex);
    CHECK(snapshot==sc::Snapshot(root/"game",root/"cache",false,dxil.compiler,xex));
    CHECK(sc::Snapshot(root/"game",root/"cache",spirv,xex)==
        sc::Snapshot(root/"game",root/"cache",true,spirv.compiler,xex));
    sc::Record record;record.hash=hash;record.info.isPixelShader=true;record.info.hlsl=source;record.binary=binaries[0];
    const auto bundle=root/"legacy-compatible.bundle";
    {sc::Writer writer(bundle,"");writer.Add(record);writer.Finish(snapshot);}
    unsigned consumed=0;
    auto load=[&](const std::string& identity,const cache::Identity& backend) {
        return sc::LoadTransactional(bundle,identity,backend,[&](sc::Record&& r) {
            CHECK(r.binary==binaries[0]);++consumed;
        },[]{},[]{ });
    };
    CHECK(load(snapshot,dxil).ok && consumed==1);
    const auto failed=root/"negative.failed";
    const auto failureKey=sc::FailureKey(source,dxil,true);
    CHECK(sc::WriteFailure(failed,failureKey,"deterministic fixture rejection",true));
    for(const auto& identity:changes) {
        CHECK(cache::FileName(true,hash,identity)!=path.filename().string());
        // Exact same file path deliberately bypasses filename isolation.
        CHECK(cache::ReadBinary(path,true,hash,identity).empty());
        CHECK(sc::ReadFailure(failed,sc::FailureKey(source,identity,true)).empty());
        if(cache::ValidIdentity(identity)) {
            const auto updated=sc::Snapshot(root/"game",root/"cache",identity,xex);
            CHECK(snapshot!=updated);
            CHECK(!load(updated,identity).ok && consumed==1);
        } else CHECK(!load(snapshot,identity).ok && consumed==1);
    }
    for(const size_t offset:{size_t(0),size_t(8),size_t(72),size_t(136),original.size()-1}) {
        auto corrupt=original;corrupt[offset]^=1;Write(path,corrupt);
        CHECK(cache::ReadBinary(path,true,hash,dxil).empty());
    }
    for(const size_t length:{size_t(0),size_t(8),cache::BinaryHeaderSize-1,original.size()-1}) {
        Write(path,{original.data(),length});CHECK(cache::ReadBinary(path,true,hash,dxil).empty());
    }
    auto trailing=original;trailing.push_back(0);Write(path,trailing);
    CHECK(cache::ReadBinary(path,true,hash,dxil).empty());
    // Legacy raw successes have no compiler provenance and cannot re-enter
    // through fallback after a bundle identity mismatch.
    Write(path,binaries[0]);CHECK(cache::ReadBinary(path,true,hash,dxil).empty());
    Write(path,original);CHECK(cache::ReadBinary(path,true,hash,dxil)==binaries[0]);
    const auto reservePath=root/"cache"/cache::FileName(true,hash,dxbc);
    CHECK(reservePath.extension()==".dxbc");
    CHECK(cache::WriteBinary(reservePath,true,hash,dxbc,reserved));
    CHECK(cache::ReadBinary(reservePath,true,hash,dxbc)==reserved);
    CHECK(cache::ReadBinary(reservePath,true,hash,dxil).empty());
    CHECK(!cache::RuntimeSupported(dxbc));
    CHECK(xenos::GetDxcStatistics().calls==2);
    std::printf("PASS cache isolation: %u checks; typed identities, cross-format rejection, corruption/truncation, legacy bundle reuse, compiler/options/variant changes, cold/warm builtin cache; 2 actual DXC calls\n",checks);
}
