#include "gpu/shader/resource_scan.h"
#include <chrono>
#include <iostream>

using namespace xenos::resources;
static void Require(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
static void Put(std::vector<uint8_t>& b, size_t offset, uint32_t value) {
    for (int i=0;i<4;++i) b[offset+i]=uint8_t(value >> (24-i*8));
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
        std::cout << "PASS: framing, stage, overflow, chunk boundary, dedup, reuse, recovery, index, bounded reads, hash/layout/mixed fallback\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
