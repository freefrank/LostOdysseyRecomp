#include "recompiler.h"
#include <byteswap.h>
#include <iostream>
#include <sstream>

// Each fixture line is a function name, skipLr flag and actual PPC words.
// Production decoding/generation handles complete bounded function bodies.
int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    std::ifstream input(argv[1]);
    std::ofstream output(argv[2], std::ios::binary);
    std::string line;
    while (std::getline(input, line))
    {
        std::istringstream stream(line);
        std::string name;
        unsigned skipLr;
        if (!(stream >> name >> skipLr)) return 3;
        std::vector<uint32_t> words;
        uint32_t word;
        while (stream >> std::hex >> word) words.push_back(ByteSwap(word));
        if (words.empty()) return 3;
        Recompiler recompiler;
        constexpr uint32_t pc = 0x1000;
        for (unsigned i = 0; i < words.size(); ++i)
        {
            ppc_insn insn{};
            if (ppc::Disassemble(&words[i], 4, pc + i * 4, insn) != 4 || !insn.opcode)
                return 4;
            std::cout << name << " " << std::hex << pc + i * 4 << " "
                      << ByteSwap(words[i]) << " " << insn.opcode->name << "\n";
        }
        recompiler.config.skipLr = skipLr != 0;
        recompiler.image.Map(".text", pc, uint32_t(words.size() * 4), SectionFlags_Code,
            reinterpret_cast<uint8_t*>(words.data()));
        recompiler.image.symbols.emplace(name, pc, words.size() * 4, Symbol_Function);
        if (!recompiler.Recompile(Function(pc, words.size() * 4))) return 5;
        output << recompiler.out;
    }
    return input.eof() && output.good() ? 0 : 6;
}
