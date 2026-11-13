#include "recompiler.h"
#include <byteswap.h>
#include <array>

// Reduced PPC fixture: select the original seven-way switch directly through
// r11 instead of duplicating its floating-point classification prelude. The
// table and mask/return tail match the published Android translation.
int main(int argc, char** argv)
{
    if (argc < 4 || argc > 5) return 2;
    constexpr uint32_t start = 0x82F80F48, dispatch = 0x82F80FEC, end = 0x82F81050;
    Recompiler r;
    r.config.Load(argv[1]);
    const auto declared = r.config.functions;
    const auto tables = r.config.switchTables;
    r.config = {}; // no game-wide helpers or hooks in the reduced image
    for (uint32_t entry : {start, 0x82F81038u, 0x82F81044u})
        if (const auto it = declared.find(entry); it != declared.end())
            r.config.functions.emplace(*it);
    if (const auto it = tables.find(dispatch); it != tables.end())
        r.config.switchTables.emplace(*it);
    if (argc == 5)
    {
        const std::string_view mode = argv[4];
        if (mode != "--without-table" && mode != "--without-fix") return 2;
        r.config.switchTables.erase(dispatch);
        if (mode == "--without-fix") r.config.functions.erase(start);
    }

    std::array<uint32_t, (end - start) / 4> code;
    code.fill(0x60000000); // synthetic NOP padding replaces the classification prelude
    const auto put = [&](uint32_t pc, uint32_t word) { code.at((pc - start) / 4) = word; };
    put(start, 0x39400000);      // li r10,0 (the helper's initial return value)
    put(start + 4, 0x3D8082F8); // lis r12,0x82f8
    put(start + 8, 0x618C0FF0); // ori r12,r12,0xff0
    put(start + 12, 0x5560103A);// slwi r0,r11,2
    put(start + 16, 0x7C0C002E);// lwzx r0,r12,r0
    put(start + 20, 0x7C0903A6);// mtctr r0
    put(dispatch, 0x4E800420); // bctr: production metadata must lower this to local labels
    constexpr std::array<uint32_t, 7> labels{
        0x82F8100C, 0x82F81014, 0x82F8101C, 0x82F81024, 0x82F8102C, 0x82F81034, 0x82F81040};
    for (uint32_t i = 0; i < labels.size(); ++i) put(0x82F80FF0 + i * 4, labels[i]);
    constexpr uint32_t masks[]{0x28, 0x18, 0x30, 8, 0x10, 0x20};
    for (uint32_t i = 0; i < 6; ++i)
    {
        put(labels[i], 0x706B0000 | masks[i]); // andi. r11,r3,mask
        if (i < 5) put(labels[i] + 4, 0x48000000 | (0x82F81038 - (labels[i] + 4)));
    }
    put(0x82F81038, 0x2F8B0000); // cmpwi cr7,r11,0
    put(0x82F8103C, 0x409E0008); // bne cr7,0x82f81044
    put(0x82F81040, 0x39400001); // li r10,1 -- NOT a complete standalone function
    put(0x82F81044, 0x7D435378); // mr r3,r10
    put(0x82F81048, 0x4E800020); // blr
    for (auto& word : code) word = ByteSwap(word);
    uint8_t empty = 0;
    r.image.Map(".pdata", 0x8221F600, 0, 0, &empty);
    r.image.Map(".text", start, sizeof(code), SectionFlags_Code, reinterpret_cast<uint8_t*>(code.data()));
    r.Analyse();
    r.config.directoryPath.clear();
    r.config.outDirectoryPath = std::filesystem::absolute(argv[3]).string();
    std::filesystem::create_directories(r.config.outDirectoryPath);
    r.Recompile(argv[2]);
    return 0;
}
