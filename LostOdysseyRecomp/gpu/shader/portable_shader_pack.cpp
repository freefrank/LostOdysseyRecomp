#include "portable_shader_pack.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <utility>
#include <zstd.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace xenos::portable_pack {
namespace {
constexpr size_t HeaderBytes = 128, FooterBytes = 40;
constexpr uint64_t MaxIndexBytes = 16u << 20;
constexpr std::string_view Magic = "LOSPVPK1", EndMagic = "LOSPVEND";
using Bytes = std::vector<uint8_t>;
using Key = std::pair<bool, uint64_t>;
[[noreturn]] void Bad(std::string_view reason) { throw std::runtime_error(std::string(reason)); }
std::span<const uint8_t> AsBytes(std::string_view s) {
    return {reinterpret_cast<const uint8_t*>(s.data()), s.size()};
}
struct Encode {
    Bytes b;
    void U32(uint32_t v) { for (unsigned i=0;i<4;++i) b.push_back(uint8_t(v>>(i*8))); }
    void U64(uint64_t v) { U32(uint32_t(v)); U32(uint32_t(v>>32)); }
    void Raw(std::span<const uint8_t> s) { b.insert(b.end(),s.begin(),s.end()); }
    void Text(std::string_view s) {
        if (s.size()>UINT32_MAX) Bad("text too long");
        U32(uint32_t(s.size())); Raw(AsBytes(s));
    }
};
struct Decode {
    std::span<const uint8_t> b;
    std::span<const uint8_t> Raw(size_t n) {
        if (n>b.size()) Bad("truncated pack field");
        auto s=b.first(n); b=b.subspan(n); return s;
    }
    uint32_t U32() {
        auto s=Raw(4); uint32_t v=0;
        for(unsigned i=0;i<4;++i) v|=uint32_t(s[i])<<(8*i);
        return v;
    }
    uint64_t U64() { auto lo=U32(); return uint64_t(lo)|(uint64_t(U32())<<32); }
    Digest Hash() { Digest h{}; auto s=Raw(h.size()); std::copy(s.begin(),s.end(),h.begin()); return h; }
    std::string Text(size_t max) {
        auto n=U32(); if(n>max) Bad("pack text exceeds limit");
        auto s=Raw(n); return {reinterpret_cast<const char*>(s.data()),s.size()};
    }
};
uint32_t Word(std::span<const uint8_t> b, size_t index) {
    const size_t p=index*4;
    return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;
}
// Structural checks, NOT a substitute for spirv-val and real driver testing.
void CheckSpirv(std::span<const uint8_t> b, bool pixel) {
    if(b.size()<20 || b.size()>MaxShaderBytes || b.size()%4) Bad("invalid SPIR-V size");
    const auto version=Word(b,1);
    if(Word(b,0)!=0x07230203 || version<0x00010000 || version>0x00010500 ||
       (version&0xFF) || !Word(b,3) || Word(b,4)) Bad("unsupported SPIR-V header");
    bool memory=false, entry=false;
    for(size_t i=5;i<b.size()/4;) {
        const auto op=Word(b,i), count=op>>16, code=op&0xFFFF;
        if(!count || count>b.size()/4-i) Bad("incomplete SPIR-V instruction");
        if(code==14 && count==3) memory=true;
        if(code==15 && count>=5 && Word(b,i+1)==(pixel?4u:0u)) {
            // OpEntryPoint execution model and exact NUL-terminated name.
            auto name=b.subspan((i+3)*4,(count-3)*4);
            if(name.size()>=5 && std::memcmp(name.data(),"main\0",5)==0) entry=true;
        }
        i+=count;
    }
    if(!memory || !entry) Bad("SPIR-V has no matching main entry point");
}
void Write(std::ostream& out, std::span<const uint8_t> b) {
    out.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));
    if(!out) Bad("pack write failed");
}
Bytes ReadAt(std::ifstream& in,uint64_t offset,size_t count) {
    in.clear(); in.seekg(std::streamoff(offset));
    if(!in) Bad("pack seek failed");
    Bytes b(count);
    if(!in.read(reinterpret_cast<char*>(b.data()),std::streamsize(count))) Bad("pack read incomplete");
    return b;
}
struct Entry { Key key; TranslatedShader info; uint32_t blob; };
struct Blob { uint32_t block,offset,size; };
struct Block { uint64_t offset; uint32_t compressed,raw; Digest digest; };
void EncodeEntry(Encode& e,const Entry& a) {
    const auto& i=a.info;
    e.U64(a.key.second);
    e.U32(unsigned(i.isPixelShader)|(unsigned(i.writesDepth)<<1)|
           (unsigned(i.usesPointSize)<<2)|(unsigned(i.usesRelativeConstants)<<3));
    e.U32(i.vertexFetchSlotsUsed); e.U64(i.vertexFetchSlotMask[0]); e.U64(i.vertexFetchSlotMask[1]);
    e.U32(i.textureSlotMask); e.Raw(i.textureDimension); e.U32(i.colorTargetsWritten); e.U32(a.blob);
}
Entry DecodeEntry(Decode& d) {
    Entry a{}; a.key.second=d.U64(); auto flags=d.U32();
    if(!a.key.second || flags>15) Bad("invalid pack shader identity/flags");
    auto& i=a.info; i.isPixelShader=bool(flags&1); a.key.first=i.isPixelShader;
    i.writesDepth=bool(flags&2); i.usesPointSize=bool(flags&4); i.usesRelativeConstants=bool(flags&8);
    i.vertexFetchSlotsUsed=d.U32(); i.vertexFetchSlotMask[0]=d.U64(); i.vertexFetchSlotMask[1]=d.U64();
    i.textureSlotMask=d.U32(); auto dims=d.Raw(32); std::copy(dims.begin(),dims.end(),i.textureDimension);
    i.colorTargetsWritten=d.U32(); a.blob=d.U32();
    if(i.colorTargetsWritten>15) Bad("invalid pack color metadata");
    return a;
}
struct Header {
    Report report;
    uint64_t indexOffset=0;
    Digest indexDigest{};
};
Bytes EncodeHeader(const Header& h) {
    Encode e; e.Raw(AsBytes(Magic)); e.U32(Schema); e.U32(HeaderBytes); e.Raw(h.report.contract);
    e.U64(h.indexOffset); e.U64(h.report.indexBytes); e.U64(h.report.fileBytes);
    e.U32(h.report.records); e.U32(h.report.uniqueBinaries); e.U32(h.report.blocks); e.U32(0);
    e.Raw(h.indexDigest); e.U64(0);
    if(e.b.size()!=HeaderBytes) Bad("internal pack header size");
    return std::move(e.b);
}
Header ReadHeader(std::ifstream& in,const Digest* expected) {
    in.seekg(0,std::ios::end); const auto end=in.tellg();
    if(end<0 || uint64_t(end)<HeaderBytes+FooterBytes || uint64_t(end)>MaxFileBytes) Bad("invalid pack file size");
    auto raw=ReadAt(in,0,HeaderBytes); Decode d{raw};
    auto magic=d.Raw(8);
    if(!std::equal(magic.begin(),magic.end(),Magic.begin()) || d.U32()!=Schema || d.U32()!=HeaderBytes)
        Bad("unsupported portable pack schema");
    Header h; auto& r=h.report; r.contract=d.Hash();
    if(expected && r.contract!=*expected) Bad("portable shader contract mismatch");
    h.indexOffset=d.U64(); r.indexBytes=d.U64(); r.fileBytes=d.U64();
    r.records=d.U32(); r.uniqueBinaries=d.U32(); r.blocks=d.U32();
    if(d.U32()) Bad("invalid pack reserved field");
    h.indexDigest=d.Hash(); if(d.U64()) Bad("invalid pack reserved field");
    if(r.fileBytes!=uint64_t(end) || !r.records || r.records>MaxRecords ||
       !r.uniqueBinaries || r.uniqueBinaries>r.records || !r.blocks || r.blocks>r.uniqueBinaries ||
       h.indexOffset<HeaderBytes || h.indexOffset>r.fileBytes-FooterBytes ||
       r.indexBytes>MaxIndexBytes || r.indexBytes!=r.fileBytes-FooterBytes-h.indexOffset)
        Bad("invalid pack index bounds/counts");
    auto foot=ReadAt(in,r.fileBytes-FooterBytes,FooterBytes);
    if(!std::equal(foot.begin(),foot.begin()+8,EndMagic.begin()) ||
       !std::equal(foot.begin()+8,foot.end(),resources::Sha256(raw).begin())) Bad("pack completion digest mismatch");
    return h;
}
std::filesystem::path TempPath(const std::filesystem::path& target) {
    static std::atomic<uint64_t> sequence{0};
    auto p=target;
    p += ".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(sequence++);
    return p;
}
void Publish(const std::filesystem::path& from,const std::filesystem::path& to) {
#ifdef _WIN32
    if(!MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING)) Bad("pack publish failed");
#else
    std::filesystem::rename(from,to);
#endif
}
} // namespace

Digest Contract(uint32_t version,std::string_view options,std::string_view variant,
    std::string_view common,std::string_view discovery,std::span<const uint8_t> xex,uint32_t layout) {
    if(!version || !layout || options.empty() || variant.empty() || common.empty() || discovery.empty() || xex.empty())
        Bad("incomplete portable shader contract");
    Encode e; e.Text("lo-portable-vulkan12-spv-contract"); e.U32(Schema); e.U32(layout); e.U32(version);
    e.Text(options); e.Text(variant); e.Raw(resources::Sha256(AsBytes(common)));
    e.Text(discovery); e.Raw(resources::Sha256(xex)); return resources::Sha256(e.b);
}
std::filesystem::path DefaultPath() {
    std::filesystem::path exe;
#ifdef _WIN32
    std::array<wchar_t,32768> p{}; const DWORD n=GetModuleFileNameW(nullptr,p.data(),DWORD(p.size()));
    if(!n || n>=p.size()) Bad("cannot resolve shader pack executable directory");
    exe=std::wstring(p.data(),n);
#elif defined(__APPLE__)
    uint32_t n=0; _NSGetExecutablePath(nullptr,&n); std::vector<char> p(n);
    if(_NSGetExecutablePath(p.data(),&n)!=0) Bad("cannot resolve executable directory");
    exe=std::filesystem::weakly_canonical(p.data());
#else
    std::array<char,8192> p{}; const auto n=readlink("/proc/self/exe",p.data(),p.size());
    if(n<=0 || size_t(n)>=p.size()) Bad("cannot resolve shader pack executable directory");
    exe=std::string(p.data(),size_t(n));
#endif
    return exe.parent_path()/"shaders"/FileName;
}

struct Writer::Impl {
    std::filesystem::path target,temp;
    std::ofstream out;
    Header header;
    std::vector<Entry> entries;
    std::vector<Blob> blobs;
    std::vector<Block> blocks;
    std::map<Digest,uint32_t> dedup;
    std::set<Key> keys;
    Bytes pending;
    int level;
    bool finished=false;
    Impl(const std::filesystem::path& p,Digest contract,std::string_view producer,int compression)
        :target(p),temp(TempPath(p)),level(compression) {
        if(producer.empty() || producer.size()>1024 || compression<1 || compression>19) Bad("invalid pack export options");
        if(!p.parent_path().empty()) std::filesystem::create_directories(p.parent_path());
        out.open(temp,std::ios::binary|std::ios::trunc);
        if(!out) Bad("cannot open portable pack output");
        try { Write(out,Bytes(HeaderBytes)); }
        catch(...) { out.close(); std::error_code ec; std::filesystem::remove(temp,ec); throw; }
        header.report.contract=contract; header.report.producer=producer;
    }
    ~Impl() { out.close(); std::error_code ec; std::filesystem::remove(temp,ec); }
    void Flush() {
        if(pending.empty()) return;
        Bytes compressed(ZSTD_compressBound(pending.size()));
        auto n=ZSTD_compress(compressed.data(),compressed.size(),pending.data(),pending.size(),level);
        if(ZSTD_isError(n)) Bad(ZSTD_getErrorName(n));
        compressed.resize(n); const auto offset=out.tellp();
        if(offset<0 || uint64_t(offset)+n>MaxFileBytes-MaxIndexBytes-FooterBytes) Bad("portable pack size limit");
        blocks.push_back({uint64_t(offset),uint32_t(n),uint32_t(pending.size()),resources::Sha256(compressed)});
        Write(out,compressed); header.report.compressedBytes+=n;
        pending.clear();
    }
};
Writer::Writer(const std::filesystem::path& p,Digest c,std::string_view producer,int level)
    :impl_(std::make_unique<Impl>(p,c,producer,level)) {}
Writer::~Writer()=default;
void Writer::Add(uint64_t hash,const TranslatedShader& info,std::span<const uint8_t> binary) {
    auto& w=*impl_;
    if(w.finished || !hash || w.entries.size()>=MaxRecords || info.colorTargetsWritten>15) Bad("invalid pack export record");
    CheckSpirv(binary,info.isPixelShader);
    if(!w.keys.emplace(info.isPixelShader,hash).second) Bad("duplicate portable shader key");
    const auto digest=resources::Sha256(binary);
    uint32_t blob=0;
    if(auto it=w.dedup.find(digest);it!=w.dedup.end()) {
        blob=it->second;
        if(w.blobs[blob].size!=binary.size()) Bad("conflicting binary digest");
    } else {
        if(w.header.report.uniqueBinaryBytes > MaxFileBytes-binary.size()) Bad("portable uncompressed corpus size limit");
        if(!w.pending.empty() && w.pending.size()+binary.size()>TargetBlockBytes) w.Flush();
        blob=uint32_t(w.blobs.size());
        w.blobs.push_back({uint32_t(w.blocks.size()),uint32_t(w.pending.size()),uint32_t(binary.size())});
        w.pending.insert(w.pending.end(),binary.begin(),binary.end());
        w.dedup.emplace(digest,blob); w.header.report.uniqueBinaryBytes+=binary.size();
    }
    Entry entry{{info.isPixelShader,hash},info,blob};
    // Do not retain a second copy of the HLSL corpus during packing.
    std::string{}.swap(entry.info.hlsl); std::string{}.swap(entry.info.errors);
    w.entries.push_back(std::move(entry));
    w.header.report.binaryBytes+=binary.size();
    w.header.report.hlslBytesOmitted+=info.hlsl.size();
    w.header.report.diagnosticBytesOmitted+=info.errors.size();
}
void Writer::OmitFailure(size_t hlsl,size_t diagnostic) {
    auto& w=*impl_;
    if(w.finished || w.header.report.failuresOmitted>=MaxRecords) Bad("invalid omitted failure count");
    ++w.header.report.failuresOmitted;
    w.header.report.hlslBytesOmitted+=hlsl; w.header.report.diagnosticBytesOmitted+=diagnostic;
}
Report Writer::Finish() {
    auto& w=*impl_;
    if(w.finished || w.entries.empty() || w.entries.size()+w.header.report.failuresOmitted>MaxRecords)
        Bad("incomplete portable pack export");
    w.Flush(); std::sort(w.entries.begin(),w.entries.end(),[](const auto& a,const auto& b){return a.key<b.key;});
    auto& r=w.header.report;
    r.records=uint32_t(w.entries.size()); r.uniqueBinaries=uint32_t(w.blobs.size()); r.blocks=uint32_t(w.blocks.size());
    Encode index; index.U32(Schema); index.U64(r.hlslBytesOmitted); index.U64(r.diagnosticBytesOmitted);
    index.U32(r.failuresOmitted); index.Text(r.producer);
    for(const auto& e:w.entries) EncodeEntry(index,e);
    for(const auto& b:w.blobs) { index.U32(b.block); index.U32(b.offset); index.U32(b.size); }
    for(const auto& b:w.blocks) { index.U64(b.offset); index.U32(b.compressed); index.U32(b.raw); index.Raw(b.digest); }
    if(index.b.size()>MaxIndexBytes) Bad("portable index size limit");
    auto pos=w.out.tellp(); if(pos<0) Bad("portable pack output position");
    w.header.indexOffset=uint64_t(pos); r.indexBytes=index.b.size();
    r.fileBytes=w.header.indexOffset+r.indexBytes+FooterBytes;
    if(r.fileBytes>MaxFileBytes) Bad("portable pack size limit");
    w.header.indexDigest=resources::Sha256(index.b); auto header=EncodeHeader(w.header);
    Write(w.out,index.b); Write(w.out,AsBytes(EndMagic)); Write(w.out,resources::Sha256(header));
    w.out.seekp(0); Write(w.out,header); w.out.close(); if(!w.out) Bad("portable pack close failed");
    Publish(w.temp,w.target); w.finished=true; return r;
}

struct Reader::Impl {
    std::ifstream in;
    Header header;
    std::vector<Entry> entries;
    std::vector<Blob> blobs;
    std::vector<Block> blocks;
    Bytes decoded;
    uint32_t cachedBlock=UINT32_MAX;
    std::atomic<uint64_t> payloadRead{0};
    std::mutex mutex;
    Impl(const std::filesystem::path& path,const Digest* expected) :in(path,std::ios::binary) {
        if(!in) Bad("portable pack missing");
        header=ReadHeader(in,expected); auto& r=header.report;
        auto bytes=ReadAt(in,header.indexOffset,size_t(r.indexBytes));
        if(resources::Sha256(bytes)!=header.indexDigest) Bad("portable pack index digest mismatch");
        Decode d{bytes}; if(d.U32()!=Schema) Bad("unsupported portable index");
        r.hlslBytesOmitted=d.U64(); r.diagnosticBytesOmitted=d.U64(); r.failuresOmitted=d.U32(); r.producer=d.Text(1024);
        if(r.producer.empty() || r.failuresOmitted>MaxRecords-r.records) Bad("invalid portable provenance");
        if(d.b.size()!=uint64_t(r.records)*76+uint64_t(r.uniqueBinaries)*12+uint64_t(r.blocks)*48)
            Bad("portable index length does not match counts");
        entries.reserve(r.records); blobs.reserve(r.uniqueBinaries); blocks.reserve(r.blocks);
        std::vector<bool> referenced(r.uniqueBinaries);
        for(uint32_t n=0;n<r.records;++n) {
            auto e=DecodeEntry(d);
            if(e.blob>=r.uniqueBinaries || (!entries.empty() && e.key<=entries.back().key)) Bad("invalid portable shader index");
            referenced[e.blob]=true; entries.push_back(std::move(e));
        }
        if(std::find(referenced.begin(),referenced.end(),false)!=referenced.end()) Bad("unreferenced portable binary");
        for(uint32_t n=0;n<r.uniqueBinaries;++n) {
            Blob b{d.U32(),d.U32(),d.U32()};
            if(b.block>=r.blocks || b.size<20 || b.size>MaxShaderBytes || b.size%4 || b.offset%4) Bad("invalid portable binary bounds");
            r.uniqueBinaryBytes+=b.size;
            if(r.uniqueBinaryBytes>MaxFileBytes) Bad("portable uncompressed corpus size limit");
            blobs.push_back(b);
        }
        uint64_t next=HeaderBytes;
        for(uint32_t n=0;n<r.blocks;++n) {
            Block b{d.U64(),d.U32(),d.U32(),d.Hash()};
            if(b.offset!=next || !b.raw || b.raw>MaxShaderBytes || !b.compressed ||
               b.compressed>ZSTD_compressBound(b.raw) || next>header.indexOffset || b.compressed>header.indexOffset-next)
                Bad("invalid portable block bounds");
            next+=b.compressed; r.compressedBytes+=b.compressed; blocks.push_back(b);
        }
        if(next!=header.indexOffset || !d.b.empty()) Bad("portable payload layout mismatch");
        // Unique blobs are tightly packed in block order; aliases live only in
        // the shader table. This rules out overlaps, gaps and unused blocks.
        uint32_t block=0,offset=0;
        for(const auto& b:blobs) {
            if(offset==blocks[block].raw) { ++block; offset=0; }
            if(block>=blocks.size() || b.block!=block || b.offset!=offset || b.size>blocks[block].raw-offset)
                Bad("portable binary overlap/gap");
            offset+=b.size;
        }
        if(block+1!=blocks.size() || offset!=blocks.back().raw) Bad("portable payload is not fully referenced");
        for(const auto& e:entries) r.binaryBytes+=blobs[e.blob].size;
    }
    void LoadBlock(uint32_t n) {
        if(cachedBlock==n) return;
        const auto& b=blocks.at(n);
        auto compressed=ReadAt(in,b.offset,b.compressed); payloadRead+=compressed.size();
        if(resources::Sha256(compressed)!=b.digest) Bad("portable block digest mismatch");
        if(ZSTD_getFrameContentSize(compressed.data(),compressed.size())!=b.raw ||
           ZSTD_findFrameCompressedSize(compressed.data(),compressed.size())!=compressed.size())
            Bad("portable compressed frame bounds mismatch");
        Bytes fresh(b.raw);
        auto size=ZSTD_decompress(fresh.data(),fresh.size(),compressed.data(),compressed.size());
        if(ZSTD_isError(size) || size!=b.raw) Bad("portable pack decompression failed");
        decoded.swap(fresh); cachedBlock=n;
    }
};
Reader::Reader(const std::filesystem::path& p,const Digest& expected):impl_(std::make_unique<Impl>(p,&expected)) {}
Reader::~Reader()=default;
const Report& Reader::Info() const { return impl_->header.report; }
uint64_t Reader::PayloadReadBytes() const { return impl_->payloadRead.load(); }
bool Reader::Contains(bool pixel,uint64_t hash) const {
    const auto& entries=impl_->entries; const Key key{pixel,hash};
    const auto it=std::lower_bound(entries.begin(),entries.end(),key,[](const Entry& e,const Key& k){return e.key<k;});
    return it!=entries.end() && it->key==key;
}
std::optional<Record> Reader::Get(bool pixel,uint64_t hash) {
    auto& r=*impl_; const Key key{pixel,hash};
    auto it=std::lower_bound(r.entries.begin(),r.entries.end(),key,[](const Entry& e,const Key& k){return e.key<k;});
    if(it==r.entries.end() || it->key!=key) return std::nullopt;
    const auto& b=r.blobs[it->blob]; Record result; result.hash=hash; result.info=it->info;
    { std::lock_guard lock(r.mutex); r.LoadBlock(b.block);
      result.binary.assign(r.decoded.begin()+b.offset,r.decoded.begin()+b.offset+b.size); }
    CheckSpirv(result.binary,pixel); return result;
}
void Reader::VerifyAll() {
    // In file/block order, so verification reads/decompresses each block once.
    auto& r=*impl_; std::vector<const Entry*> entries;
    for(const auto& e:r.entries) entries.push_back(&e);
    std::sort(entries.begin(),entries.end(),[&](auto a,auto b){return r.blobs[a->blob].block<r.blobs[b->blob].block;});
    for(const auto* e:entries) (void)Get(e->key.first,e->key.second);
}
void Writer::Import(Reader& source) {
    auto& w=*impl_; auto& r=*source.impl_;
    if(w.finished || !w.entries.empty() || w.header.report.failuresOmitted ||
       w.header.report.contract!=r.header.report.contract)
        Bad("incompatible or nonempty portable import");
    std::lock_guard lock(r.mutex);
    // Entry order in the index is by key, whereas payloads are stored by blob.
    // Import aliases together so each compressed block is read exactly once.
    std::vector<const Entry*> entries;
    entries.reserve(r.entries.size());
    for(const auto& e:r.entries) entries.push_back(&e);
    std::sort(entries.begin(),entries.end(),[&](auto a,auto b){return a->blob<b->blob;});
    for(const auto* e:entries) {
        const auto& blob=r.blobs[e->blob];
        r.LoadBlock(blob.block);
        const auto binary=std::span<const uint8_t>(r.decoded).subspan(blob.offset,blob.size);
        Add(e->key.second,e->info,binary);
    }
    w.header.report.failuresOmitted=r.header.report.failuresOmitted;
    w.header.report.hlslBytesOmitted+=r.header.report.hlslBytesOmitted;
    w.header.report.diagnosticBytesOmitted+=r.header.report.diagnosticBytesOmitted;
}
Report Reader::Inspect(const std::filesystem::path& p,bool verify) {
    // Explicitly offline: a self-reported contract is NEVER used by runtime.
    std::ifstream in(p,std::ios::binary); if(!in) Bad("portable pack missing");
    auto header=ReadHeader(in,nullptr); Reader reader(p,header.report.contract);
    if(verify) reader.VerifyAll();
    return reader.Info();
}
} // namespace xenos::portable_pack
