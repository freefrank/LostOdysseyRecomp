#include "recompiler.h"
#include <byteswap.h>

// Synthetic PPC switch using the same word compare, word index scaling and
// bctr lowering as the Uhra Council failure. All cases return distinct values.
static bool Generate(std::ofstream& output, bool increment)
{
    constexpr uint32_t base = 0x1000;
    constexpr uint32_t dispatch = base + 7 * 4;
    constexpr uint32_t firstCase = dispatch + 4;
    constexpr uint32_t defaultCase = firstCase + 8 * 8;
    std::vector<uint32_t> words{
        increment ? 0x39630001u : 0x7C6B1B78u, // addi r11,r3,1 / mr r11,r3
        0x2B0B0007, // cmplwi cr6,r11,7
        0x41990000 | (defaultCase - (base + 8)), // bgt cr6,default
        0x39800400, // li r12,0x400 (synthetic guest jump-table address)
        0x5560103A, // rlwinm r0,r11,2,0,29
        0x7C0C002E, // lwzx r0,r12,r0
        0x7C0903A6, // mtctr r0
        0x4E800420, // bctr
    };
    constexpr uint32_t values[]{13, 57, 22, 96, 5, 84, 41, 70};
    Recompiler recompiler;
    RecompilerSwitchTable table{11, {}};
    for (uint32_t i = 0; i < 8; ++i)
    {
        table.labels.push_back(firstCase + i * 8);
        words.push_back(0x38600000 | values[i]); // li r3,value
        words.push_back(0x4E800020); // blr
    }
    words.push_back(0x386000FF); // default: li r3,255
    words.push_back(0x4E800020);
    for (auto& word : words)
        word = ByteSwap(word);
    recompiler.image.Map(".text", base, uint32_t(words.size() * 4),
        SectionFlags_Code, reinterpret_cast<uint8_t*>(words.data()));
    recompiler.image.symbols.emplace(increment ? "switch_increment" : "switch_direct",
        base, words.size() * 4, Symbol_Function);
    recompiler.config.switchTables.emplace(dispatch, std::move(table));
    if (!recompiler.Recompile(Function(base, words.size() * 4)))
        return false;
    output << recompiler.out;
    return true;
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    std::ofstream output(argv[1], std::ios::binary);
    if (!output)
        return 2;
    output << "#define PPC_CONFIG_H_INCLUDED\n#include <ppc_context.h>\n";
    if (!Generate(output, false) || !Generate(output, true))
        return 1;
    return output.good() ? 0 : 2;
}
