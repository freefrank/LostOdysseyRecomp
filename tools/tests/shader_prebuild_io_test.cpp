// Game-data-free integration fixtures for the trusted-import prebuild path.
#include "gpu/shader/resource_scan.h"
#include "gpu/shader/resource_variants.h"
#include "gpu/shader/source_store.h"
#include "gpu/shader/startup_cache.h"
#include <chrono>
#include <cstdio>
#include <thread>

namespace fs = std::filesystem;
namespace res = xenos::resources;
namespace sc = xenos::startup_cache;
using Bytes = std::vector<uint8_t>;
static void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static void Write(const fs::path& path, std::span<const uint8_t> bytes) {
    std::ofstream out(path, std::ios::binary);
    sc::WriteRaw(out, bytes);
    out.close(); Check(bool(out), "fixture write failed");
}
static Bytes Read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
static void PutBE(Bytes& bytes, size_t offset, uint32_t value) {
    for (unsigned i=0;i<4;++i) bytes.at(offset+i)=uint8_t(value >> (24-i*8));
}
static void Extraction(const fs::path& root) {
    fs::create_directories(root/"game");
    Bytes resource(1u<<20);
    PutBE(resource,0,0x102a1100); PutBE(resource,4,96); PutBE(resource,8,24);
    PutBE(resource,16,36); PutBE(resource,24,64); PutBE(resource,48,0xffff0300); PutBE(resource,68,24);
    for (unsigned i=96;i<120;++i) resource[i]=uint8_t(i);
    const auto code=std::span(resource).subspan(96,24);
    const auto path=root/"game/Resource.fpd";
    Write(path,resource);
    uint64_t probeBytes=0;
    std::ifstream in(path,std::ios::binary);
    const auto fingerprint=res::Fingerprint(in,resource.size(),probeBytes);
    const res::IndexEntry entry{96,24,res::Hash(code),true};
    const res::IndexFile profile{"Resource.fpd",resource.size(),fingerprint,std::span(&entry,1)};
    const auto noProgress=[](const res::ScanProgress&) {};
    // A regular file cannot contain exports or manifests. The memory path must
    // not touch that cache subtree at all, even when it is unusable for writes.
    const auto blocked=root/"not-a-cache-directory";
    Write(blocked,sc::Bytes("unchanged"));
    res::SourceStore memory;
    const res::SourceSink sink=[&](bool pixel,auto bytes) { memory.Add(pixel,bytes); };
    const auto fast=res::Scan(root/"game",blocked,noProgress,std::span(&profile,1),{},{},false,sink);
    Check(fast.error.empty() && fast.shaders==1 && fast.indexedFiles==1,"memory indexed extraction failed");
    Check(fast.bytesRead==code.size() && !fast.cacheBytesRead,"trusted extraction performed extra source/probe reads");
    Check(Read(blocked)==Bytes({'u','n','c','h','a','n','g','e','d'}),"prebuild touched cache subtree");
    Check(memory.Size()==1 && memory.Bytes()==code.size(),"source deduplication size");
    memory.Add(true,code);
    Check(memory.Size()==1 && memory.Bytes()==code.size(),"source was duplicated");
    const auto disk=res::Scan(root/"game",root/"legacy",noProgress,std::span(&profile,1),{},{});
    Check(disk.error.empty() && disk.bytesRead==probeBytes+code.size(),"legacy verification path changed");
    Check(Read(root/"legacy/source"/res::SourceName(true,code))==Bytes(code.begin(),code.end()),"legacy export changed");
    const auto reused=res::Scan(root/"game",root/"legacy",noProgress,std::span(&profile,1),{},{});
    Check(reused.reused && reused.cacheBytesRead==code.size(),"legacy manifest reuse changed");
    // Ambiguous layouts retain the distinguishing probes, even in trusted mode.
    auto other=profile;other.fingerprint^=1;
    const std::array<res::IndexFile,2> ambiguous{other,profile};
    uint64_t ambiguousBytes=0;std::set<std::string> names;
    Check(res::ExtractIndexed(path,blocked,ambiguous,names,ambiguousBytes,sink,true),"ambiguous profile not resolved");
    Check(ambiguousBytes==probeBytes+code.size(),"ambiguous layout skipped probes");
    // Trust must not permit unsafe indexed offsets or mismatching shader bytes.
    auto badEntry=entry;badEntry.offset=resource.size()-4;
    auto badProfile=profile;badProfile.entries=std::span(&badEntry,1);
    names.clear();uint64_t rejectedBytes=0;
    Check(!res::ExtractIndexed(path,blocked,std::span(&badProfile,1),names,rejectedBytes,sink,true),"out of range index accepted");
    Check(names.empty() && rejectedBytes==0,"unsafe index touched source data");
    badEntry=entry;badEntry.hash^=1;
    Check(!res::ExtractIndexed(path,blocked,std::span(&badProfile,1),names,rejectedBytes,sink,true),"wrong shader identity accepted");
    // Forced scans use the complete discovery path but still need no exports.
    res::SourceStore strictMemory;
    const auto strict=res::Scan(root/"game",blocked,noProgress,{},{},{},true,
        [&](bool pixel,auto bytes) { strictMemory.Add(pixel,bytes); });
    Check(strict.error.empty() && strict.scannedFiles==1 && strictMemory.Size()==1,"forced fallback scan failed");
    Check(strictMemory.Jobs()[0]->code==memory.Jobs()[0]->code,"memory extraction changed microcode");
    std::printf("PASS prebuild IO: trusted=%llu bytes, verified=%llu bytes, source writes=0\n",
        static_cast<unsigned long long>(fast.bytesRead),static_cast<unsigned long long>(disk.bytesRead));
}
static void Sources(const fs::path& root) {
    res::SourceStore store;
    const Bytes known(12,1),learned(24,2),corrupt(12,3);
    store.Add(false,known);
    fs::create_directories(root/"sources");
    // Even a corrupt legacy export must not be opened to override a known input.
    Write(root/"sources"/res::SourceName(false,known),sc::Bytes("bad"));
    Write(root/"sources"/res::SourceName(true,learned),learned);
    Write(root/"sources"/res::SourceName(true,corrupt),sc::Bytes("bad"));
    Check(store.ImportLearned(root/"sources")==1 && store.Size()==2,"learned cache migration");
    Bytes output;
    Check(store.Read(res::Hash(known),known.size(),false,output)==1 && output==known,"legacy data overrode imported source");
    Check(store.Read(res::Hash(learned),learned.size()+4,true,output)==-1,"wrong source length accepted");
    Check(store.Read(0,12,false,output)==0,"missing source synthesized");
    for (const size_t size : {size_t(0),size_t(11),size_t(13),res::SourceStore::MaxSourceBytes+1}) {
        bool rejected=false;
        try { store.Add(false,Bytes(size)); } catch (const std::runtime_error&) { rejected=true; }
        Check(rejected,"invalid source allocation accepted");
    }
    for (bool byteLimit : {false,true}) {
        res::SourceStore bounded(byteLimit ? known.size() : 1024, byteLimit ? 10 : 1);
        bounded.Add(false,known);bounded.Add(false,known);
        bool rejected=false;
        try { bounded.Add(true,learned); } catch (const std::runtime_error&) { rejected=true; }
        Check(rejected && bounded.Size()==1 && bounded.Bytes()==known.size(),"source budget was not atomic/bounded");
    }
    // Reader mode must work with an unusable disk directory and no file opens.
    size_t reads=0,saves=0;
    const res::variants::SourceReader missing=[&](auto,auto,bool,auto&) { ++reads;return 0; };
    const auto fixed=res::variants::GenerateFixedVariants(root/"does-not-exist",[&](auto,auto){++saves;},{},missing);
    Check(reads==std::size(res::variants::detail::sources) && !fixed.bytesRead && !saves,"memory fixed variant reader");
    const std::set<uint64_t> fixedHashes;
    reads=0;
    const auto linked=res::variants::GenerateLinkedVariants(root/"does-not-exist",[&](auto,auto){++saves;},{},missing,&fixedHashes);
    Check(reads==std::size(res::variants::detail::sources)+std::size(res::variants::detail::link::pixels),"linked variants regenerated fixed candidates");
    Check(!linked.bytesRead && !saves,"memory variants performed disk reads");
    // A reader must not authorize invalid bytes just by returning success.
    const res::variants::SourceReader wrong=[](auto,uint32_t size,bool,Bytes& bytes){ bytes.assign(size,0);return 1; };
    const auto invalid=res::variants::GenerateFixedVariants(root/"does-not-exist",[&](auto,auto){++saves;},{},wrong);
    Check(invalid.invalidBases==std::size(res::variants::detail::sources) && !saves,"reader bypassed source identity");
    std::puts("PASS bounded sources, learned-cache migration, memory variants, single fixed expansion");
}
static sc::Record Record(uint64_t hash,const std::string& common) {
    sc::Record record;record.hash=hash;
    auto& info=record.info;info.isPixelShader=true;info.textureSlotMask=7;info.colorTargetsWritten=1;
    info.vertexFetchSlotMask[1]=uint64_t(1)<<63;info.usesRelativeConstants=true;
    info.hlsl="prefix"+sc::Prelude(common,true)+"suffix";info.errors="fixture note";
    record.binary.resize(44);std::memcpy(record.binary.data(),"DXBC",4);
    auto put=[&](size_t offset,uint32_t value) { std::memcpy(record.binary.data()+offset,&value,4); };
    put(24,44);put(28,1);put(32,36);put(36,0x4c495844);put(40,0);
    return record;
}
static void Bundles(const fs::path& root) {
    const auto identity=xenos::cache::MakeIdentity(xenos::cache::Backend::D3D12,"compiler-A");
    const auto game=root/"nonexistent-game-root";
    const auto key=sc::RuntimeIdentity(game,identity,sc::Bytes("loaded-xex"));
    Check(!fs::exists(game),"identity accessed game data");
    std::vector<std::string> changed;
    auto other=identity;other.compiler="compiler-B";changed.push_back(sc::RuntimeIdentity(game,other,sc::Bytes("loaded-xex")));
    other=identity;++other.translatorVersion;changed.push_back(sc::RuntimeIdentity(game,other,sc::Bytes("loaded-xex")));
    other=identity;other.options+="changed";changed.push_back(sc::RuntimeIdentity(game,other,sc::Bytes("loaded-xex")));
    other=identity;other.variant="another-variant";changed.push_back(sc::RuntimeIdentity(game,other,sc::Bytes("loaded-xex")));
    other=xenos::cache::MakeIdentity(xenos::cache::Backend::Vulkan,"compiler-A");
    changed.push_back(sc::RuntimeIdentity(game,other,sc::Bytes("loaded-xex")));
    changed.push_back(sc::RuntimeIdentity(game,identity,sc::Bytes("different-xex")));
    changed.push_back(sc::RuntimeIdentity(game,identity,sc::Bytes("loaded-xex"),"different-discovery"));
    changed.push_back(sc::RuntimeIdentity(root/"another-game",identity,sc::Bytes("loaded-xex")));
    const std::string common="cbuffer XeConstants : register(b0, space0)\n"+std::string(32768,' ');
    const auto record=Record(1,common),second=Record(2,common);
    const auto path=root/"startup.bundle";
    {sc::Writer writer(path,common);writer.Add(record);writer.Add(second);writer.Finish(key);}
    const auto original=Read(path);
    for (const auto& expected : changed) {
        Check(expected!=key,"compiler/runtime identity did not invalidate");
        size_t consumed=0;
        const auto result=sc::Load(path,expected,identity.format,[&](auto&&) {++consumed;});
        Check(!result.ok && !consumed && !result.bytesRead,"foreign bundle reached the device consumer");
    }
    size_t consumed=0,pumps=0;std::vector<uint32_t> progress;
    auto load=[&] {
        return sc::LoadTransactional(path,key,identity,[&](sc::Record&& input) {
            ++consumed;
            Check(input.info.hlsl.empty(),"warm startup retained HLSL");
            Check(input.info.textureSlotMask==7 && input.info.colorTargetsWritten==1 &&
                input.info.vertexFetchSlotMask[1]==uint64_t(1)<<63 && input.info.usesRelativeConstants &&
                input.info.errors==record.info.errors && input.binary==record.binary,"metadata-only decode changed shader data");
            std::this_thread::sleep_for(std::chrono::milliseconds(18));
        },[&]{consumed=0;},[]{},[&]{++pumps;},[&](uint32_t done,uint32_t total){
            Check(total==2,"bundle progress total");progress.push_back(done);
        },false);
    };
    const auto valid=load();
    Check(valid.ok && consumed==2 && pumps>=3 && progress.front()==0 && progress.back()==2,"warm load did not pump progress");
    Check(valid.bytesRead==sc::Encode(record,common).size()+sc::Encode(second,common).size(),"bundle payload read more than once");
    auto damaged=original;damaged.back()^=1;Write(path,damaged);
    consumed=0;const auto failed=load();Check(!failed.ok && !consumed,"late footer error left installed shaders");
    Write(path,original);
    {sc::Writer writer(path,common);writer.Add(record);}
    Check(Read(path)==original,"abandoned writer replaced complete cache");
    Check(sc::Decode(sc::Encode(record,common),common,identity.format).info.hlsl==record.info.hlsl,"explicit HLSL decode lost capture text");
    std::puts("PASS runtime identity, single-pass bundle, metadata-only decode, event pump, rollback, atomic publication");
}
int main(int argc,char** argv) try {
    Check(argc==2,"usage: shader_prebuild_io_test <new-output-directory>");
    const auto root=fs::absolute(argv[1]);
    Check(!fs::exists(root),"fixture output already exists");fs::create_directories(root);
    Extraction(root);Sources(root);Bundles(root);
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr,"FAIL prebuild IO: %s\n",e.what());return 1;
}
