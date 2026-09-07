#include "gpu/shader/resource_scan.h"
#include <chrono>
#include <iostream>

using namespace xenos::resources;
static void Require(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
static void Put(std::vector<uint8_t>& b, size_t offset, uint32_t value) {
    for (int i=0;i<4;++i) b[offset+i]=uint8_t(value >> (24-i*8));
}
static void PutLE(std::vector<uint8_t>& b, size_t offset, uint32_t value) {
    for (int i=0;i<4;++i) b[offset+i]=uint8_t(value >> (i*8));
}
static std::vector<uint8_t> LiteralCpx(std::span<const uint8_t> input) {
    std::vector<uint8_t> encoded(24+(input.size()*9+7)/8);
    encoded[0]='c'; encoded[1]='p'; encoded[2]='x'; encoded[4]=0x10; encoded[6]=1;
    PutLE(encoded,8,uint32_t(encoded.size())); PutLE(encoded,12,uint32_t(input.size())); PutLE(encoded,16,20);
    encoded[22]=uint8_t(input.size()-1); encoded[23]=uint8_t((input.size()-1)>>8);
    size_t bit=0;
    for (auto byte : input) {
        ++bit; // literal marker
        for (int i=7;i>=0;--i,++bit) encoded[24+bit/8]|=((byte>>i)&1)<<(7-bit%8);
    }
    return encoded;
}
int main(int argc, char** argv) {
    try {
        if (argc==4) {
            const auto started=std::chrono::steady_clock::now();
            const auto result=Scan(argv[1],argv[2],[](auto,auto){},
                std::string_view(argv[3])=="full" ? std::span<const IndexFile>{} : std::span<const IndexFile>{builtin::files});
            Require(result.error.empty(),result.error.c_str());
            std::cout << "shaders=" << result.shaders << " indexed=" << result.indexedFiles
                      << " scanned=" << result.scannedFiles << " bytes=" << result.bytesRead
                      << " reused=" << result.reused << " ms="
                      << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count() << '\n';
            return 0;
        }
        Require(argc==2,"supply an output directory");
        std::vector<uint8_t> container(120);
        Put(container,0,0x102a1100); Put(container,4,96); Put(container,8,24);
        Put(container,16,36); Put(container,24,64); Put(container,48,0xffff0300); Put(container,68,24);
        bool pixel=false;
        Require(Microcode(container,pixel).size()==24 && pixel,"valid pixel container");
        Require(Microcode(std::span(container).first(119),pixel).empty(),"reject truncated physical data");
        Put(container,64,0xfffffff0);
        Require(Microcode(container,pixel).empty(),"reject physical offset overflow"); Put(container,64,0);
        Put(container,48,0xfffe0300);
        Require(Microcode(container,pixel).empty(),"reject mismatched stage"); Put(container,48,0xffff0300);
        const auto root=fs::path(argv[1])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        fs::create_directories(root/"game");
        std::ofstream file(root/"game/test.fpd",std::ios::binary);
        std::vector<char> padding(4*1024*1024-8); file.write(padding.data(),padding.size());
        file.write(reinterpret_cast<char*>(container.data()),container.size());
        file.write(reinterpret_cast<char*>(container.data()),container.size()); file.close();
        const auto cache=root/"cache";
        const auto first=Scan(root/"game",cache,[](auto,auto){});
        Require(first.error.empty() && first.shaders==1 && !first.reused,"chunk boundary and deduplication");
        const auto second=Scan(root/"game",cache,[](auto,auto){});
        Require(second.error.empty() && second.reused && second.shaders==1,"manifest reuse");
        for (const auto& e:fs::directory_iterator(cache/"source")) std::ofstream(e.path()) << "broken";
        const auto repaired=Scan(root/"game",cache,[](auto,auto){});
        Require(repaired.error.empty() && !repaired.reused && repaired.shaders==1,"corrupt source recovery");
        uint64_t bytesRead=0;
        std::ifstream resource(root/"game/test.fpd",std::ios::binary);
        const auto fingerprint=Fingerprint(resource,fs::file_size(root/"game/test.fpd"),bytesRead);
        resource.close();
        IndexEntry entry{padding.size()+96,24,Hash(std::span(container).subspan(96)),true};
        IndexFile profile{"test.fpd",fs::file_size(root/"game/test.fpd"),fingerprint,{&entry,1}};
        const auto indexed=Scan(root/"game",root/"indexed",[](auto,auto){},{&profile,1});
        Require(indexed.error.empty() && indexed.shaders==1 && indexed.indexedFiles==1 && indexed.scannedFiles==0,
                "indexed cold extraction");
        Require(indexed.bytesRead<20000,"bounded indexed reads");
        entry.hash ^= 1;
        const auto badHash=Scan(root/"game",root/"bad-hash",[](auto,auto){},{&profile,1});
        Require(badHash.error.empty() && badHash.shaders==1 && badHash.scannedFiles==1 && !badHash.indexedFiles,
                "bad shader hash falls back");
        entry.hash ^= 1; profile.fingerprint ^= 1;
        const auto badProfile=Scan(root/"game",root/"bad-profile",[](auto,auto){},{&profile,1});
        Require(badProfile.error.empty() && badProfile.shaders==1 && badProfile.scannedFiles==1,"unknown layout fallback");
        profile.fingerprint ^= 1;
        fs::copy_file(root/"game/test.fpd",root/"game/unknown.fpd");
        const auto mixed=Scan(root/"game",root/"mixed",[](auto,auto){},{&profile,1});
        Require(mixed.error.empty() && mixed.shaders==1 && mixed.indexedFiles==1 && mixed.scannedFiles==1,
                "per-file fallback and shared deduplication");
        // Public synthetic fixture: literal-coded CPX hides the SDK container
        // from the naked scan. Two FPI extents carry identical compressed bytes.
        const auto compressed=LiteralCpx(container);
        const auto game=root/"cpx-game"; fs::create_directories(game);
        constexpr const char* archives[]={"LO.fpd","xenon_chr.fpd","xenon_event.fpd","xenon_field.fpd",
            "xenon_obj.fpd","xenon_scr.fpd","xenon_sys.fpd","xenon_vfx.fpd","xenon_world.fpd",
            "xenon_battle.fpd","xenon_loc.fpd","xenon_mov.fpd","xenon_snd.fpd"};
        for (const auto* name : archives) std::ofstream(game/name,std::ios::binary).close();
        std::vector<uint8_t> archive(8192);
        std::copy(compressed.begin(),compressed.end(),archive.begin()+2048);
        std::copy(compressed.begin(),compressed.end(),archive.begin()+6144);
        std::ofstream(game/"LO.fpd",std::ios::binary).write(reinterpret_cast<char*>(archive.data()),archive.size());
        std::vector<uint8_t> fpi(2048); fpi[12]=1; fpi[20]=1; fpi[21]=4; fpi[24]=1; fpi[26]=13;
        constexpr size_t entries=64+13*48;
        PutLE(fpi,28,2); PutLE(fpi,32,64); PutLE(fpi,36,entries); PutLE(fpi,40,entries+48);
        for (size_t i=0;i<13;++i) PutLE(fpi,64+i*48+4,uint32_t(entries+(i?48:0)-(64+i*48)));
        PutLE(fpi,entries+8,1); PutLE(fpi,entries+16,uint32_t(compressed.size()));
        PutLE(fpi,entries+24+8,3); PutLE(fpi,entries+24+16,uint32_t(compressed.size()));
        auto writeFpi=[&] { std::ofstream(game/"LO.fpi",std::ios::binary).write(reinterpret_cast<char*>(fpi.data()),fpi.size()); };
        writeFpi();
        const auto cpx=Scan(game,root/"cpx-cache",[](auto,auto){});
        Require(cpx.error.empty() && cpx.shaders==1 && cpx.cpxPackages==2 && cpx.decodedPackages==1 && cpx.decodedBytes==120,
            "FPI CPX discovery and exact-payload deduplication");
        const auto cpxWarm=Scan(game,root/"cpx-cache",[](auto,auto){});
        Require(cpxWarm.error.empty() && cpxWarm.reused && !cpxWarm.decodedPackages,"CPX manifest reuse");
        // Old discovery versions cannot hide newly supported compressed sources.
        const auto manifest=root/"cpx-cache/resources.manifest";
        { std::ifstream in(manifest,std::ios::binary); std::string text((std::istreambuf_iterator<char>(in)),{});
          in.close(); text.replace(0,text.find('\n'),"resource-scanner-v2"); std::ofstream(manifest,std::ios::binary)<<text; }
        const auto migrated=Scan(game,root/"cpx-cache",[](auto,auto){});
        Require(migrated.error.empty() && !migrated.reused && migrated.decodedPackages==1,"old discovery manifest invalidation");
        PutLE(fpi,entries+16,0xffffffff); writeFpi();
        const auto invalidFpi=Scan(game,root/"invalid-fpi",[](auto,auto){});
        Require(!invalidFpi.error.empty() && !fs::exists(root/"invalid-fpi/resources.manifest"),"invalid FPI never completes manifest");
        PutLE(fpi,entries+16,uint32_t(compressed.size())); writeFpi();
        archive[2048+16]=0xff;
        std::ofstream(game/"LO.fpd",std::ios::binary).write(reinterpret_cast<char*>(archive.data()),archive.size());
        const auto invalidCpx=Scan(game,root/"invalid-cpx",[](auto,auto){});
        Require(!invalidCpx.error.empty() && !fs::exists(root/"invalid-cpx/resources.manifest"),"malformed CPX never completes manifest");
        std::cout << "PASS: framing, stage, overflow, chunk boundary, dedup, reuse, recovery, index, bounded reads, hash/layout/mixed fallback, CPX/FPI extraction, migration, malformed input\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
