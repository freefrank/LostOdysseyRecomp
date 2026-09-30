#include "install/import_game.h"
#include "install/import_image.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>

int main()
{
    try
    {
        const auto source = install::ScanContent(std::filesystem::path("G:/ROMS/X360CH176"));
        const std::filesystem::path dump("D:/Mihoyo/LostOdysseyRecomp-windows-x64/game");
        uint64_t checked = 0;
        for (const auto& disc : source.discs)
        {
            install::GodImageReader reader(disc.path);
            for (const auto& entry : reader.GetEntries())
            {
                const auto file = dump / ("disc" + std::to_string(disc.disc)) / entry.name;
                if (!std::filesystem::is_regular_file(file)) continue;
                if (std::filesystem::file_size(file) != entry.size)
                    throw install::Error("Size mismatch: " + entry.name);
                std::ifstream input(file, std::ios::binary);
                const uint64_t take = std::min<uint64_t>(entry.size, 4096);
                for (uint64_t offset : {uint64_t(0), entry.size / 2 - std::min(entry.size / 2, take / 2), entry.size - take})
                {
                    std::array<char, 4096> original{}, extracted{};
                    reader.Read(entry.offset + offset, original.data(), static_cast<size_t>(take));
                    input.clear();
                    input.seekg(offset);
                    input.read(extracted.data(), static_cast<std::streamsize>(take));
                    if (input.gcount() != static_cast<std::streamsize>(take) || original != extracted)
                        throw install::Error("Byte mismatch: " + entry.name);
                    checked += take;
                }
            }
            std::cout << "PASS GOD/dump disc=" << disc.disc << " edition=" << disc.edition << '\n';
        }
        if (source.discs.size() != 4 || checked == 0) throw install::Error("Incomplete source coverage");
        std::cout << "PASS bounded comparison bytes=" << checked << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
