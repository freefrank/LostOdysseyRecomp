#include "gpu/shader/startup_cache.h"
#include "gpu/shader/dxc_compiler.h"
#include <cassert>
#include <cstdio>
#include <map>

namespace sc = xenos::startup_cache;
namespace fs = std::filesystem;
static std::vector<uint8_t> Read(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};
}
static void Write(const fs::path& path,std::span<const uint8_t> bytes) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);sc::WriteRaw(out,bytes);
}
int main(int argc,char** argv) {
    const fs::path root=argc>1 ? fs::u8path(argv[1]) : fs::temp_directory_path()/"lo-startup-cache-test";
    fs::create_directories(root/"game/disc1");fs::create_directories(root/"cache/source");
    const auto file=root/"startup.bundle";
    const std::string common="cbuffer XeConstants : register(b0, space0)\n"+std::string(8192,' ');
    const std::string identity(64,'a');
    sc::Record record;
    record.hash=0x1234567812345678;auto& info=record.info;
    info.isPixelShader=true;info.vertexFetchSlotsUsed=7;info.vertexFetchSlotMask[0]=0x100000004;
    info.vertexFetchSlotMask[1]=0x8000000000000000;info.textureSlotMask=0x90000001;
    for(int i=0;i<32;++i) info.textureDimension[i]=uint8_t(i%4);
    info.writesDepth=true;info.colorTargetsWritten=15;info.usesPointSize=true;info.usesRelativeConstants=true;
    info.hlsl="#define XE_PIXEL_SHADER 1\n"+sc::Prelude(common,true)+"\nfloat4 main() {}";info.errors="translation note";
    record.binary.resize(44);std::memcpy(record.binary.data(),"DXBC",4);
    auto word=[&](size_t offset,uint32_t value) {std::memcpy(record.binary.data()+offset,&value,4);};
    word(24,44);word(28,1);word(32,36);word(36,0x4c495844);word(40,0);
    const auto encoded=sc::Encode(record,common);
    assert(encoded.size()<1024);
    const auto restored=sc::Decode(encoded,common,false);
    assert(sc::Encode(restored,common)==encoded && restored.info.hlsl==info.hlsl);
    auto vertex=record;vertex.info.isPixelShader=false;vertex.info.hlsl="#define XE_SAMPLE x\n"+common+"VS tail";
    assert(sc::Encode(vertex,common).size()<1024);
    assert(sc::Decode(sc::Encode(vertex,common),common,false).info.hlsl==vertex.info.hlsl);
    auto spirv=record;std::array<uint32_t,12> spv{0x07230203,0x10000,0,2,0,3u<<16|14,0,1,4u<<16|15,0,1,0};
    spirv.binary.assign(reinterpret_cast<uint8_t*>(spv.data()),reinterpret_cast<uint8_t*>(spv.data()+spv.size()));
    assert(sc::Decode(sc::Encode(spirv,common),common,true).binary==spirv.binary);
    auto negative=record;negative.hash++;negative.binary.clear();negative.failure="shader.hlsl:1:1: error: undefined identifier\n";
    {sc::Writer writer(file,common);writer.Add(record);writer.Add(negative);writer.Finish(identity);}
    const auto original=Read(file);
    for(int pass=0;pass<2;++pass) {
        int calls=0;auto result=sc::Load(file,identity,false,[&](sc::Record&& r) {
            assert(r.info.hlsl==info.hlsl);++calls;
        });assert(result.ok && calls==2 && result.records==2);
    }
    auto rejected=[&](std::vector<uint8_t> bytes) {
        Write(file,bytes);int callbacks=0;
        const auto result=sc::Load(file,identity,false,[&](auto&&) {++callbacks;});
        assert(!result.ok && callbacks==0);
    };
    // Shared prelude, header count, field length, record, footer and truncation.
    for(const size_t offset : {size_t(16),size_t(96),size_t(96+common.size()),original.size()-42,original.size()-1}) {
        auto bytes=original;bytes[offset]^=0xff;rejected(bytes);
    }
    for(const size_t length : {size_t(0),size_t(15),size_t(90),original.size()-1})
        rejected({original.begin(),original.begin()+length});
    Write(file,original);
    int ignored=0;assert(!sc::Load(file,std::string(64,'b'),false,[&](auto&&) {++ignored;}).ok && ignored==0);
    assert(!sc::Load(file,identity,true,[&](auto&&) {++ignored;}).ok && ignored==0);
    // No partial renderer state survives a pass-two consumer/device error, or
    // a resource change detected after otherwise successful installation.
    for(bool postFailure : {false,true}) {
        std::map<uint64_t,sc::Record> state;int consumed=0;bool rolledBack=false;
        const auto result=sc::LoadTransactional(file,identity,false,[&](sc::Record&& r) {
            state.emplace(r.hash,std::move(r));
            if(++consumed==2 && !postFailure) throw std::runtime_error("injected module creation failure");
        },[&] {state.clear();rolledBack=true;},[&] {
            if(postFailure) throw std::runtime_error("injected concurrent input change");
        });
        assert(!result.ok && rolledBack && state.empty() && consumed==2);
    }
    // Abandoned write cannot replace the last complete cache.
    {sc::Writer writer(file,common);writer.Add(record);}
    assert(Read(file)==original);
    const auto failFile=root/"shader.failed";
    const auto key=sc::FailureKey(info.hlsl,"compiler-a",true,false);
    assert(!sc::WriteFailure(failFile,key,"transient",false));
    assert(sc::WriteFailure(failFile,key,negative.failure,true));
    assert(sc::ReadFailure(failFile,key)==negative.failure);
    for(const auto changed : {sc::FailureKey(info.hlsl+"x","compiler-a",true,false),
        sc::FailureKey(info.hlsl,"compiler-b",true,false),sc::FailureKey(info.hlsl,"compiler-a",true,true),
        sc::FailureKey(info.hlsl,"compiler-a",false,false)}) assert(sc::ReadFailure(failFile,changed).empty());
    auto damaged=Read(failFile);damaged.back()^=1;Write(failFile,damaged);assert(sc::ReadFailure(failFile,key).empty());
    Write(failFile,sc::Bytes("truncated"));assert(sc::ReadFailure(failFile,key).empty());
    const auto game=root/"game/disc1",cache=root/"cache";
    Write(game/"LO.FPI",sc::Bytes("index"));Write(game/"Resource.FPD",sc::Bytes("resource"));
    Write(cache/"source/vs_1.bin",sc::Bytes("source"));
    auto snapshot=[&] {return sc::Snapshot(game,cache,false,"compiler",sc::Bytes("xex"));};
    const auto before=snapshot();assert(before==snapshot());
    Write(game/"Resource.FPD",sc::Bytes("resource changed"));assert(before!=snapshot());
    auto current=snapshot();Write(cache/"source/vs_2.bin",sc::Bytes("added source"));assert(current!=snapshot());
    current=snapshot();Write(cache/"new.dxil",record.binary);assert(current!=snapshot());
    current=snapshot();fs::last_write_time(cache/"new.dxil",fs::last_write_time(cache/"new.dxil")+std::chrono::seconds(2));assert(current!=snapshot());
    current=snapshot();fs::create_directories(root/"game/disc2");Write(root/"game/disc2/new.fpd",sc::Bytes("disc"));assert(current!=snapshot());
    assert(snapshot()!=sc::Snapshot(game,cache,true,"compiler",sc::Bytes("xex")));
    assert(snapshot()!=sc::Snapshot(game,cache,false,"compiler2",sc::Bytes("xex")));
    assert(snapshot()!=sc::Snapshot(game,cache,false,"compiler",sc::Bytes("changed xex")));
    // One real deterministic DXC rejection, then reuse without another call;
    // changed source succeeds and is never treated as a cached rejection.
    assert(xenos::DxcAvailable() && !xenos::DxcIdentity().empty());
    const std::string bad="float4 main() : SV_Target { return nonexistent_value; }";
    auto rejectedCompile=xenos::CompileHlsl(bad,"main","ps_6_0");
    assert(!rejectedCompile.ok && rejectedCompile.deterministicFailure);
    const auto actualKey=sc::FailureKey(bad,xenos::DxcIdentity(),true,false);
    assert(sc::WriteFailure(failFile,actualKey,rejectedCompile.errors,rejectedCompile.deterministicFailure));
    const auto counts=xenos::GetDxcStatistics();
    assert(!sc::ReadFailure(failFile,actualKey).empty());
    assert(xenos::GetDxcStatistics().calls==counts.calls);
    auto success=xenos::CompileHlsl("float4 main() : SV_Target { return 1; }","main","ps_6_0");
    assert(success.ok && !success.deterministicFailure && xenos::GetDxcStatistics().calls==counts.calls+1);
    std::puts("PASS startup cache: full metadata/HLSL, DXIL/SPIR-V, digest/bounds, transactional rollback, atomic publication, metadata invalidation, deterministic failure reuse");
}
