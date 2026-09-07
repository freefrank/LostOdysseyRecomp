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
static std::vector<uint8_t> StoredCpx(std::span<const uint8_t> input) {
    const size_t blocks=(input.size()+65535)/65536, header=16+blocks*4;
    std::vector<uint8_t> encoded(header+blocks*4+input.size());
    encoded[0]='c';encoded[1]='p';encoded[2]='x';encoded[4]=0x10;
    encoded[6]=uint8_t(blocks);encoded[7]=uint8_t(blocks>>8);
    PutLE(encoded,8,uint32_t(encoded.size()));PutLE(encoded,12,uint32_t(input.size()));
    size_t offset=header;
    for (size_t i=0;i<blocks;++i) {
        PutLE(encoded,16+i*4,uint32_t(offset));
        const auto count=std::min<size_t>(65536,input.size()-i*65536);
        encoded[offset]=255;encoded[offset+2]=uint8_t(count-1);encoded[offset+3]=uint8_t((count-1)>>8);
        std::copy_n(input.begin()+i*65536,count,encoded.begin()+offset+4);offset+=4+count;
    }
    return encoded;
}
int main(int argc, char** argv) {
    try {
        if (argc==4) {
            const auto started=std::chrono::steady_clock::now();
            const auto result=Scan(argv[1],argv[2],[](const ScanProgress&){},
                std::string_view(argv[3])=="full" ? std::span<const IndexFile>{} : std::span<const IndexFile>{builtin::files},
                std::string_view(argv[3])=="full" ? std::span<const CpxIndexPackage>{} : std::span<const CpxIndexPackage>{builtin::cpxPackages});
            Require(result.error.empty(),result.error.c_str());
            std::cout << "shaders=" << result.shaders << " indexed=" << result.indexedFiles
                      << " scanned=" << result.scannedFiles << " bytes=" << result.bytesRead
                      << " cache_bytes=" << result.cacheBytesRead << " cpx=" << result.cpxPackages
                      << " decoded_packages=" << result.decodedPackages << " decoded_bytes=" << result.decodedBytes
                      << " indexed_packages=" << result.indexedPackages << " fallback_packages=" << result.fallbackPackages
                      << " duplicate_packages=" << result.duplicatePackages
                      << " reused=" << result.reused << " ms="
                      << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count() << '\n';
            return 0;
        }
        Require(argc==2,"supply an output directory");
        for (const auto& [message,expected]:std::vector<std::pair<std::string,std::string>>{
            {"","e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
            {"abc","ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
            {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq","248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
            {std::string(1000000,'a'),"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"}}) {
            const auto bytes=std::span(reinterpret_cast<const uint8_t*>(message.data()),message.size());
            Require(Sha256Hex(Sha256Portable(bytes))==expected,"portable SHA256 known vector");
            Require(Sha256Hex(Sha256(bytes))==expected,"accelerated SHA256 known vector");
        }
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
        const auto first=Scan(root/"game",cache,[](const ScanProgress&){});
        Require(first.error.empty() && first.shaders==1 && !first.reused,"chunk boundary and deduplication");
        const auto second=Scan(root/"game",cache,[](const ScanProgress&){});
        Require(second.error.empty() && second.reused && second.shaders==1,"manifest reuse");
        for (const auto& e:fs::directory_iterator(cache/"source")) std::ofstream(e.path()) << "broken";
        const auto repaired=Scan(root/"game",cache,[](const ScanProgress&){});
        Require(repaired.error.empty() && !repaired.reused && repaired.shaders==1,"corrupt source recovery");
        uint64_t bytesRead=0;
        std::ifstream resource(root/"game/test.fpd",std::ios::binary);
        const auto fingerprint=Fingerprint(resource,fs::file_size(root/"game/test.fpd"),bytesRead);
        resource.close();
        IndexEntry entry{padding.size()+96,24,Hash(std::span(container).subspan(96)),true};
        IndexFile profile{"test.fpd",fs::file_size(root/"game/test.fpd"),fingerprint,{&entry,1}};
        const auto indexed=Scan(root/"game",root/"indexed",[](const ScanProgress&){},{&profile,1});
        Require(indexed.error.empty() && indexed.shaders==1 && indexed.indexedFiles==1 && indexed.scannedFiles==0,
                "indexed cold extraction");
        Require(indexed.bytesRead<20000,"bounded indexed reads");
        entry.hash ^= 1;
        const auto badHash=Scan(root/"game",root/"bad-hash",[](const ScanProgress&){},{&profile,1});
        Require(badHash.error.empty() && badHash.shaders==1 && badHash.scannedFiles==1 && !badHash.indexedFiles,
                "bad shader hash falls back");
        entry.hash ^= 1; profile.fingerprint ^= 1;
        const auto badProfile=Scan(root/"game",root/"bad-profile",[](const ScanProgress&){},{&profile,1});
        Require(badProfile.error.empty() && badProfile.shaders==1 && badProfile.scannedFiles==1,"unknown layout fallback");
        profile.fingerprint ^= 1;
        fs::copy_file(root/"game/test.fpd",root/"game/unknown.fpd");
        const auto mixed=Scan(root/"game",root/"mixed",[](const ScanProgress&){},{&profile,1});
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
        const auto cpx=Scan(game,root/"cpx-cache",[](const ScanProgress&){});
        Require(cpx.error.empty() && cpx.shaders==1 && cpx.cpxPackages==2 && cpx.decodedPackages==1 && cpx.decodedBytes==120,
            "FPI CPX discovery and exact-payload deduplication");
        const auto cpxWarm=Scan(game,root/"cpx-cache",[](const ScanProgress&){});
        Require(cpxWarm.error.empty() && cpxWarm.reused && !cpxWarm.decodedPackages,"CPX manifest reuse");
        Require(cpxWarm.cacheBytesRead==24,"warm source verification bytes reported separately");
        auto locations=ContainerLocations(container);
        const auto packageDigest=Sha256Hex(Sha256(compressed));
        CpxIndexPackage cpxProfile{packageDigest,uint32_t(compressed.size()),120,locations};
        std::vector<ScanProgress> events;
        const auto knownCpx=Scan(game,root/"cpx-indexed",[&](const ScanProgress& p){events.push_back(p);},{},{&cpxProfile,1});
        Require(knownCpx.error.empty() && knownCpx.shaders==1 && knownCpx.indexedPackages==1 &&
            knownCpx.duplicatePackages==1 && !knownCpx.fallbackPackages,"known CPX identity and duplicate reuse");
        Require(std::any_of(events.begin(),events.end(),[](const auto& e){return e.stage==ScanStage::IndexedExtraction && e.unit==ScanUnit::Entries;}),
            "CPX progress explicitly uses entries");
        for (const auto& event:events) Require(event.completed<=event.total,"bounded progress");
        const auto fpiHash=Sha256Hex(Sha256(fpi));
        CpxIndexExtent directBindings[]={{2048,uint32_t(compressed.size()),0},{6144,uint32_t(compressed.size()),0}};
        CpxIndexArchive directArchive{fpiHash,"LO.fpd",archive.size(),directBindings};
        const auto direct=Scan(game,root/"cpx-direct",[](const ScanProgress&){},{},{&cpxProfile,1},{&directArchive,1});
        Require(direct.error.empty() && direct.shaders==1 && direct.indexedPackages==1 && direct.duplicatePackages==1 &&
            direct.bytesRead==archive.size()+fpi.size()*2+compressed.size(),"known layout reads one shader payload and skips duplicate");
        const auto strictDirect=Scan(game,root/"cpx-direct",[](const ScanProgress&){},{},{&cpxProfile,1},{&directArchive,1},true);
        Require(strictDirect.error.empty() && !strictDirect.reused && strictDirect.bytesRead==archive.size()+fpi.size()+2*compressed.size(),
            "strict mode has separate manifest and reads both full packages");
        const auto strictAgain=Scan(game,root/"cpx-direct",[](const ScanProgress&){},{},{&cpxProfile,1},{&directArchive,1},true);
        Require(strictAgain.error.empty() && !strictAgain.reused && strictAgain.bytesRead==strictDirect.bytesRead,
            "strict never reuses an earlier strict manifest");
        CpxIndexArchive editions[]={directArchive,directArchive};
        editions[0].fpiSha256="another-edition";
        const auto selectedEdition=Scan(game,root/"cpx-edition-select",[](const ScanProgress&){},{},{&cpxProfile,1},editions);
        Require(selectedEdition.error.empty() && selectedEdition.shaders==1 && selectedEdition.bytesRead==direct.bytesRead &&
            selectedEdition.indexedPackages==1 && !selectedEdition.fallbackPackages,
            "same archive name and size select the matching edition FPI automatically");
        directArchive.fpiSha256="unknown";
        const auto unknownDirect=Scan(game,root/"cpx-direct-unknown",[](const ScanProgress&){},{},{&cpxProfile,1},{&directArchive,1});
        Require(unknownDirect.error.empty() && unknownDirect.bytesRead==archive.size()+fpi.size()*2+2*compressed.size(),
            "unknown FPI digest disables direct bindings");
        directArchive.fpiSha256=fpiHash;
        locations[0].hash^=1;
        const auto badDirect=Scan(game,root/"cpx-direct-bad",[](const ScanProgress&){},{},{&cpxProfile,1},{&directArchive,1});
        Require(badDirect.error.empty() && badDirect.shaders==1 && badDirect.fallbackPackages==1 && !badDirect.indexedPackages,
            "sparse microcode validation failure falls back");
        locations[0].hash^=1;
        locations[0].hash^=1;
        const auto badCpxIndex=Scan(game,root/"cpx-bad-index",[](const ScanProgress&){},{},{&cpxProfile,1});
        Require(badCpxIndex.error.empty() && badCpxIndex.shaders==1 && badCpxIndex.fallbackPackages==1 &&
            !badCpxIndex.indexedPackages,"invalid indexed shader falls back to complete package");
        locations[0].hash^=1;
        // Cross-block shader bytes require two blocks, leaving the third untouched.
        std::vector<uint8_t> blockInput(3*65536);
        std::copy(container.begin(),container.end(),blockInput.begin()+65536-100);
        auto blockPackage=StoredCpx(blockInput);
        auto blockEntries=ContainerLocations(blockInput);
        const auto blockDigest=Sha256Hex(Sha256(blockPackage));
        CpxIndexPackage blockProfile{blockDigest,uint32_t(blockPackage.size()),uint32_t(blockInput.size()),blockEntries};
        fs::create_directories(root/"block-source");std::set<std::string> blockNames;uint64_t blockBytes=0;
        Require(ExtractCpxIndexed(blockPackage,blockProfile,root/"block-source",blockNames,blockBytes) &&
            blockNames.size()==1 && blockBytes==2*65536,"indexed cross-block extraction skips irrelevant block");
        const auto blockFile=root/"sparse-block.cpx";
        std::ofstream(blockFile,std::ios::binary).write(reinterpret_cast<const char*>(blockPackage.data()),blockPackage.size());
        std::ifstream sparseInput(blockFile,std::ios::binary); uint64_t sparseRead=0;
        blockNames.clear(); blockBytes=0;
        Require(ExtractCpxSparse(sparseInput,0,blockProfile,root/"block-source",blockNames,sparseRead,blockBytes) &&
            blockBytes==2*65536 && sparseRead==28+2*(65536+4),"sparse cross-block extraction reads exactly table and needed blocks");
        CpxIndexPackage noShaders{blockDigest,uint32_t(blockPackage.size()),uint32_t(blockInput.size()),{}};
        sparseRead=0;blockBytes=0;
        Require(ExtractCpxSparse(sparseInput,0,noShaders,root/"block-source",blockNames,sparseRead,blockBytes) &&
            !sparseRead && !blockBytes,"known empty package performs zero payload reads");
        blockEntries.push_back(blockEntries.front());blockEntries.back().hash^=1;blockProfile.entries=blockEntries;
        fs::create_directories(root/"invalid-block-source");blockNames.clear();blockBytes=0;
        Require(!ExtractCpxIndexed(blockPackage,blockProfile,root/"invalid-block-source",blockNames,blockBytes) &&
            blockNames.empty() && fs::is_empty(root/"invalid-block-source"),"failed last entry publishes no partial package");
        sparseRead=0;blockBytes=0;
        Require(!ExtractCpxSparse(sparseInput,0,blockProfile,root/"invalid-block-source",blockNames,sparseRead,blockBytes) &&
            blockNames.empty() && fs::is_empty(root/"invalid-block-source"),"sparse failed final entry publishes no partial sources");
        blockEntries.back()=blockEntries.front();blockEntries.back().offset=UINT64_MAX;blockProfile.entries=blockEntries;
        Require(!ExtractCpxIndexed(blockPackage,blockProfile,root/"invalid-block-source",blockNames,blockBytes),"reject indexed offset overflow");
        const auto emptyPackage=LiteralCpx(std::vector<uint8_t>(120));
        const auto emptyDigest=Sha256Hex(Sha256(emptyPackage));
        CpxIndexPackage mixedProfiles[]={cpxProfile,{emptyDigest,uint32_t(emptyPackage.size()),120,{}}};
        std::copy(emptyPackage.begin(),emptyPackage.end(),archive.begin()+6144);
        std::ofstream(game/"LO.fpd",std::ios::binary).write(reinterpret_cast<char*>(archive.data()),archive.size());
        const auto knownEmpty=Scan(game,root/"cpx-empty",[](const ScanProgress&){},{},mixedProfiles);
        Require(knownEmpty.error.empty() && knownEmpty.shaders==1 && knownEmpty.indexedPackages==2 &&
            knownEmpty.decodedPackages==1 && knownEmpty.decodedBytes==120,"known empty package skips decode");
        auto addedContainer=container;addedContainer[96]=1;
        const auto addedPackage=LiteralCpx(addedContainer);
        Require(addedPackage.size()==emptyPackage.size(),"same-size modification fixture");
        std::copy(addedPackage.begin(),addedPackage.end(),archive.begin()+6144);
        std::ofstream(game/"LO.fpd",std::ios::binary).write(reinterpret_cast<char*>(archive.data()),archive.size());
        const auto added=Scan(game,root/"cpx-added",[](const ScanProgress&){},{},mixedProfiles);
        Require(added.error.empty() && added.shaders==2 && added.indexedPackages==1 && added.fallbackPackages==1,
            "same-size changed empty package discovers newly added shader");
        std::copy(compressed.begin(),compressed.end(),archive.begin()+6144);
        std::ofstream(game/"LO.fpd",std::ios::binary).write(reinterpret_cast<char*>(archive.data()),archive.size());
        const auto indexedManifest=root/"cpx-indexed/resources.manifest";
        {std::ifstream in(indexedManifest,std::ios::binary);std::string text((std::istreambuf_iterator<char>(in)),{});
         in.close();text.resize(text.rfind("--complete--"));std::ofstream(indexedManifest,std::ios::binary)<<text;}
        const auto truncated=Scan(game,root/"cpx-indexed",[](const ScanProgress&){},{},{&cpxProfile,1});
        Require(truncated.error.empty() && !truncated.reused && truncated.shaders==1,"truncated manifest never hides missing sources");
        // An unread same-size known-empty package is intentionally trusted by
        // fast discovery; strict discovery remains available to detect additions.
        directBindings[1].package=1;
        std::copy(addedPackage.begin(),addedPackage.end(),archive.begin()+6144);
        std::ofstream(game/"LO.fpd",std::ios::binary).write(reinterpret_cast<char*>(archive.data()),archive.size());
        const auto trustEmpty=Scan(game,root/"direct-trust-empty",[](const ScanProgress&){},{},mixedProfiles,{&directArchive,1});
        Require(trustEmpty.error.empty() && trustEmpty.shaders==1 && trustEmpty.indexedPackages==2 &&
            trustEmpty.bytesRead==archive.size()+fpi.size()*2+compressed.size(),"fast known-empty binding skips same-size unread modifications");
        const auto strictEmpty=Scan(game,root/"direct-trust-empty",[](const ScanProgress&){},{},mixedProfiles,{&directArchive,1},true);
        Require(strictEmpty.error.empty() && !strictEmpty.reused && strictEmpty.shaders==2 && strictEmpty.fallbackPackages==1,
            "strict scan detects shader added inside formerly empty package");
        // A read shader block changed without changing layout: verify before
        // publication, then discover its new microcode through complete fallback.
        std::copy(addedPackage.begin(),addedPackage.end(),archive.begin()+2048);
        std::ofstream(game/"LO.fpd",std::ios::binary).write(reinterpret_cast<char*>(archive.data()),archive.size());
        const auto changedNeeded=Scan(game,root/"direct-changed-needed",[](const ScanProgress&){},{},mixedProfiles,{&directArchive,1});
        Require(changedNeeded.error.empty() && changedNeeded.shaders==1 && changedNeeded.fallbackPackages==1 &&
            !fs::exists(root/"direct-changed-needed/source"/SourceName(true,std::span(container).subspan(96,24))),
            "changed read microcode falls back without publishing stale indexed code");
        std::copy(compressed.begin(),compressed.end(),archive.begin()+2048);
        std::copy(compressed.begin(),compressed.end(),archive.begin()+6144);
        std::ofstream(game/"LO.fpd",std::ios::binary).write(reinterpret_cast<char*>(archive.data()),archive.size());
        directBindings[1].package=0;
        sparseInput.close();
        auto brokenBlock=blockPackage; PutLE(brokenBlock,20,33);
        std::ofstream(blockFile,std::ios::binary|std::ios::trunc).write(reinterpret_cast<const char*>(brokenBlock.data()),brokenBlock.size());
        sparseInput.open(blockFile,std::ios::binary);blockNames.clear();blockBytes=0;sparseRead=0;
        blockEntries.resize(1);blockProfile.entries=blockEntries;
        Require(!ExtractCpxSparse(sparseInput,0,blockProfile,root/"invalid-block-source",blockNames,sparseRead,blockBytes) &&
            blockNames.empty(),"truncated stored block rejects before memcpy");
        // Old discovery versions cannot hide newly supported compressed sources.
        const auto manifest=root/"cpx-cache/resources.manifest";
        { std::ifstream in(manifest,std::ios::binary); std::string text((std::istreambuf_iterator<char>(in)),{});
          in.close(); text.replace(0,text.find('\n'),"resource-scanner-v2"); std::ofstream(manifest,std::ios::binary)<<text; }
        const auto migrated=Scan(game,root/"cpx-cache",[](const ScanProgress&){});
        Require(migrated.error.empty() && !migrated.reused && migrated.decodedPackages==1,"old discovery manifest invalidation");
        PutLE(fpi,entries+16,0xffffffff); writeFpi();
        const auto invalidFpi=Scan(game,root/"invalid-fpi",[](const ScanProgress&){});
        Require(!invalidFpi.error.empty() && !fs::exists(root/"invalid-fpi/resources.manifest"),"invalid FPI never completes manifest");
        PutLE(fpi,entries+16,uint32_t(compressed.size())); writeFpi();
        archive[2048+16]=0xff;
        std::ofstream(game/"LO.fpd",std::ios::binary).write(reinterpret_cast<char*>(archive.data()),archive.size());
        const auto invalidCpx=Scan(game,root/"invalid-cpx",[](const ScanProgress&){});
        Require(!invalidCpx.error.empty() && !fs::exists(root/"invalid-cpx/resources.manifest"),"malformed CPX never completes manifest");
        std::cout << "PASS: framing, stage, overflow, chunk boundary, dedup, reuse, recovery, index, bounded reads, hash/layout/mixed fallback, CPX/FPI extraction, migration, malformed input, SHA256 vectors, CPX indexed/empty/modified fallback, cross-block decode, transactional extraction, manifest truncation, progress units, sparse layout reads, strict separation and unread-modification boundary\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
