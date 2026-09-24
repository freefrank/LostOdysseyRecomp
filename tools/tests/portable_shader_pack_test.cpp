#include "gpu/shader/portable_shader_pack.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace p=xenos::portable_pack;
namespace fs=std::filesystem;
using Bytes=std::vector<uint8_t>;
int checks=0;
void Check(bool ok,const char* why) { ++checks; if(!ok) throw std::runtime_error(why); }
template<class F> void Reject(F&& f,const char* why) {
    bool rejected=false; try{f();}catch(const std::exception&){rejected=true;}
    Check(rejected,why);
}
void Put32(Bytes& b,size_t at,uint32_t n) { for(unsigned i=0;i<4;++i)b.at(at+i)=uint8_t(n>>(i*8)); }
void Put64(Bytes& b,size_t at,uint64_t n) { Put32(b,at,uint32_t(n));Put32(b,at+4,uint32_t(n>>32)); }
uint32_t U32(const Bytes& b,size_t at) { uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(b.at(at+i))<<(i*8);return v; }
uint64_t U64(const Bytes& b,size_t at) {return U32(b,at)|(uint64_t(U32(b,at+4))<<32);}
Bytes Read(const fs::path& path) {std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void Write(const fs::path& path,const Bytes& b) {std::ofstream f(path,std::ios::binary);f.write((const char*)b.data(),b.size());}
void RepairHeader(Bytes& b) {
    auto digest=xenos::resources::Sha256(std::span(b).first(128));
    std::copy(digest.begin(),digest.end(),b.end()-32);
}
void RepairIndex(Bytes& b) {
    auto digest=xenos::resources::Sha256(std::span(b).subspan(size_t(U64(b,48)),size_t(U64(b,56))));
    std::copy(digest.begin(),digest.end(),b.begin()+88);RepairHeader(b);
}
Bytes Shader(bool pixel,uint32_t generator=0,size_t nops=0) {
    // SPIR-V-shaped CPU framing fixture, not claimed to be driver-validated.
    std::vector<uint32_t> words={0x07230203,0x00010500,generator,16,0,
        (3u<<16)|14,0,1, (5u<<16)|15,pixel?4u:0u,1,0x6e69616d,0};
    words.insert(words.end(),nops,1u<<16); // OpNop
    Bytes b(words.size()*4);for(size_t i=0;i<words.size();++i)Put32(b,i*4,words[i]);return b;
}
int main(int argc,char** argv) try {
    auto root=fs::temp_directory_path()/ ("lo-portable-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    struct Cleanup {fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}} cleanup{root};
    Bytes xex={1,2,3,4};auto contract=p::Contract(23,"vulkan1.2;O3","guest","prelude","discovery",xex);
    auto file=root/"source.lospv";
    xenos::TranslatedShader vs;vs.hlsl=std::string(20000,'H');vs.errors="LOCAL_DIAGNOSTIC";
    vs.vertexFetchSlotMask[1]=0x80000001;vs.textureDimension[3]=2;vs.textureSlotMask=8;vs.usesRelativeConstants=true;
    xenos::TranslatedShader ps;ps.isPixelShader=true;ps.writesDepth=true;ps.colorTargetsWritten=5;
    ps.hlsl="PIXEL_HLSL_SHOULD_NOT_SHIP";
    auto vb=Shader(false),pb=Shader(true);
    p::Report report;
    {p::Writer w(file,contract,"producer-A");w.Add(30,vs,vb);w.Add(20,ps,pb);w.Add(21,ps,pb);w.OmitFailure(30,60);report=w.Finish();}
    Check(report.records==3 && report.uniqueBinaries==2 && report.failuresOmitted==1,"success-only dedup report");
    Check(report.binaryBytes==vb.size()+2*pb.size() && report.uniqueBinaryBytes==vb.size()+pb.size(),"byte accounting");
    Check(report.hlslBytesOmitted==vs.hlsl.size()+2*ps.hlsl.size()+30,"HLSL accounting");
    Check(report.fileBytes==fs::file_size(file),"actual file size reported");
    p::Reader reader(file,contract);Check(reader.PayloadReadBytes()==0,"index-only open must not read payload");
    Check(!reader.Get(true,999),"missing key stays retryable");
    auto a=reader.Get(false,30);Check(a && a->binary==vb && a->info.hlsl.empty() && a->info.errors.empty(),"byte-identical roundtrip without source");
    Check(a->info.vertexFetchSlotMask[1]==vs.vertexFetchSlotMask[1] && a->info.textureDimension[3]==2 && a->info.usesRelativeConstants,"metadata retained");
    auto readBytes=reader.PayloadReadBytes();Check(readBytes==report.compressedBytes,"one lazy block read");
    auto b=reader.Get(true,21);Check(b && b->binary==pb && b->info.writesDepth && b->info.colorTargetsWritten==5,"dedup alias metadata");
    Check(reader.PayloadReadBytes()==readBytes,"same block not reread");reader.VerifyAll();
    // Installation path and producer fingerprint do not participate in Contract.
    fs::create_directories(root/"different-install");auto moved=root/"different-install"/"release.lospv";fs::copy_file(file,moved);
    p::Reader relocated(moved,contract);Check(relocated.Get(true,20)->binary==pb,"relocated artifact");
    auto producerB=root/"producerB.lospv";{p::Writer w(producerB,contract,"different-OS-DXC");w.Add(30,vs,vb);w.Finish();}
    p::Reader differentProducer(producerB,contract);Check(differentProducer.Get(false,30)->binary==vb,"producer-independent acceptance");
    for(auto wrong:{p::Contract(24,"vulkan1.2;O3","guest","prelude","discovery",xex),
                   p::Contract(23,"vulkan1.2;O0","guest","prelude","discovery",xex),
                   p::Contract(23,"vulkan1.2;O3","other","prelude","discovery",xex),
                   p::Contract(23,"vulkan1.2;O3","guest","changed","discovery",xex),
                   p::Contract(23,"vulkan1.2;O3","guest","prelude","other",xex),
                   p::Contract(23,"vulkan1.2;O3","guest","prelude","discovery",Bytes{9}),
                   p::Contract(23,"vulkan1.2;O3","guest","prelude","discovery",xex,2)})
        Reject([&]{p::Reader r(file,wrong);},"contract mutation accepted");
    Reject([&]{p::Contract(23,"","guest","prelude","discovery",xex);},"empty contract accepted");
    const auto original=Read(file);auto bad=root/"bad.lospv";
    auto rejectFile=[&](Bytes bytes){Write(bad,bytes);Reject([&]{p::Reader r(bad,contract);r.VerifyAll();},"malformed file accepted");};
    for(size_t size:{size_t(0),size_t(12),size_t(127),size_t(128),original.size()-1}) {auto bytes=original;bytes.resize(size);rejectFile(bytes);}
    {auto bytes=original;bytes.push_back(0);rejectFile(bytes);}
    {auto bytes=original;bytes[0]^=1;RepairHeader(bytes);rejectFile(bytes);}
    {auto bytes=original;Put32(bytes,8,999);RepairHeader(bytes);rejectFile(bytes);}
    {auto bytes=original;Put32(bytes,72,UINT32_MAX);RepairHeader(bytes);rejectFile(bytes);}
    {auto bytes=original;Put64(bytes,48,UINT64_MAX);RepairHeader(bytes);rejectFile(bytes);}
    {auto bytes=original;Put64(bytes,56,UINT64_MAX);RepairHeader(bytes);rejectFile(bytes);}
    {auto bytes=original;bytes[84]=1;RepairHeader(bytes);rejectFile(bytes);}
    {auto bytes=original;bytes.at(size_t(U64(bytes,48)))^=1;rejectFile(bytes);}
    {auto bytes=original;bytes[128]^=1;rejectFile(bytes);}
    const auto index=size_t(U64(original,48));const auto entries=index+28+std::string("producer-A").size();
    {auto bytes=original;Put32(bytes,entries+8,16);RepairIndex(bytes);rejectFile(bytes);}
    {auto bytes=original;Put32(bytes,entries+72,UINT32_MAX);RepairIndex(bytes);rejectFile(bytes);}
    {auto bytes=original;Put64(bytes,entries+76,0);RepairIndex(bytes);rejectFile(bytes);}
    const auto blobs=entries+3*76,blocks=blobs+2*12;
    {auto bytes=original;Put32(bytes,blobs,99);RepairIndex(bytes);rejectFile(bytes);}
    {auto bytes=original;Put32(bytes,blobs+4,4);RepairIndex(bytes);rejectFile(bytes);}
    {auto bytes=original;Put32(bytes,blobs+8,p::MaxShaderBytes+4);RepairIndex(bytes);rejectFile(bytes);}
    {auto bytes=original;Put64(bytes,blocks,UINT64_MAX);RepairIndex(bytes);rejectFile(bytes);}
    {auto bytes=original;Put32(bytes,blocks+12,UINT32_MAX);RepairIndex(bytes);rejectFile(bytes);}
    {p::Writer w(file,contract,"producer-A");w.Add(1,ps,pb);}Check(Read(file)==original,"unfinished export clobbered published file");
    {p::Writer w(bad,contract,"producer-A");Reject([&]{w.Add(1,ps,vb);},"stage mismatch accepted");}
    {p::Writer w(bad,contract,"producer-A");auto v=vb;Put32(v,4,0x00010600);Reject([&]{w.Add(1,vs,v);},"Vulkan1.2 pack accepted SPIR-V1.6");}
    {p::Writer w(bad,contract,"producer-A");w.Add(1,vs,vb);Reject([&]{w.Add(1,vs,vb);},"duplicate key accepted");}
    {p::Writer w(bad,contract,"producer-A");w.OmitFailure(0,5);Reject([&]{w.Finish();},"empty success pack published");}
    auto big=root/"blocks.lospv";auto bigA=Shader(false,1,200000),bigB=Shader(false,2,200000);
    {p::Writer w(big,contract,"synthetic");w.Add(1,vs,bigA);w.Add(2,vs,bigB);w.Add(3,vs,bigA);report=w.Finish();}
    Check(report.blocks==2 && report.uniqueBinaries==2,"block/dedup accounting");
    Check(report.compressedBytes<report.uniqueBinaryBytes,"synthetic compression functional");
    p::Reader blocked(big,contract);Check(blocked.PayloadReadBytes()==0,"large pack lazy open");
    Check(blocked.Get(false,1)->binary==bigA,"first block");Check(blocked.Get(false,2)->binary==bigB,"second block");
    Check(blocked.Get(false,3)->binary==bigA,"evicted block alias");
    // Import is sequential by blob: preserve all original shader metadata,
    // aliases and omission totals, without repeatedly inflating old blocks.
    auto imported=root/"imported.lospv";
    p::Reader importSource(file,contract);
    {p::Writer w(imported,contract,"offline-merge");w.Import(importSource);w.Add(31,vs,bigA);report=w.Finish();}
    Check(report.records==4 && report.failuresOmitted==1,"import counts plus new key");
    Check(report.hlslBytesOmitted==p::Reader(file,contract).Info().hlslBytesOmitted+vs.hlsl.size(),"import omission accounting");
    Check(importSource.PayloadReadBytes()==p::Reader(file,contract).Info().compressedBytes,"import reads each block once");
    p::Reader merged(imported,contract);
    Check(merged.Get(false,30)->binary==vb && merged.Get(false,30)->info.vertexFetchSlotMask[1]==vs.vertexFetchSlotMask[1],"import retains old SPIR-V and metadata");
    Check(merged.Get(true,20)->binary==pb && merged.Get(true,21)->binary==pb &&
          merged.Get(true,21)->info.writesDepth,"import retains aliases and stage metadata");
    Check(merged.Get(false,31)->binary==bigA,"import adds new key without replacing old keys");
    p::Reader twoBlockSource(big,contract);
    auto twoBlockTarget=root/"two-block-import.lospv";
    {p::Writer w(twoBlockTarget,contract,"two-block-merge");w.Import(twoBlockSource);w.Finish();}
    Check(twoBlockSource.PayloadReadBytes()==p::Reader(big,contract).Info().compressedBytes,
          "import reads each compressed block once despite aliases");
    p::Reader twoBlockMerged(twoBlockTarget,contract);
    Check(twoBlockMerged.Get(false,1)->binary==bigA && twoBlockMerged.Get(false,2)->binary==bigB &&
          twoBlockMerged.Get(false,3)->binary==bigA,"import preserves multiple blocks and aliases");
    {p::Writer w(bad,p::Contract(24,"vulkan1.2;O3","guest","prelude","discovery",xex),"wrong");
     Reject([&]{w.Import(importSource);},"cross-contract import accepted");}
    std::atomic<bool> concurrentOk{true};std::vector<std::jthread> threads;
    for(int t=0;t<8;++t)threads.emplace_back([&]{try{for(int i=0;i<32;++i){auto r=reader.Get(true,20+(i%2));if(!r || r->binary!=pb)concurrentOk=false;}}catch(...){concurrentOk=false;}});
    threads.clear();Check(concurrentOk,"concurrent reader");
    Check(p::Reader::Inspect(big,true).fileBytes==fs::file_size(big),"offline verifier");
    if(argc>1) {fs::copy_file(big,argv[1],fs::copy_options::overwrite_existing);}
    std::cout<<"PASS "<<checks<<" portable-pack CPU checks (no game/DXC/GPU execution)\n";return 0;
} catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
