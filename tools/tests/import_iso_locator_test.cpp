#include "install/import_image.h"

#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>

int main()
{
    namespace fs = std::filesystem;
    constexpr std::array<char, 20> magic{'M','I','C','R','O','S','O','F','T','*','X','B','O','X','*','M','E','D','I','A'};
    const auto root = fs::temp_directory_path() /
        ("lo-iso-locator-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(root);
    int failures = 0;
    auto check = [&](uint64_t base, const fs::path& path, bool unalignedFake) {
        std::array<char, 2048> descriptor{};
        std::memcpy(descriptor.data(), magic.data(), magic.size());
        std::memcpy(descriptor.data() + 0x7ec, magic.data(), magic.size());
        {
            std::ofstream output(path, std::ios::binary);
            if (unalignedFake)
            {
                output.seekp(0x10001);
                output.write(descriptor.data(), descriptor.size());
            }
            output.seekp(base + 0x10000);
            output.write(descriptor.data(), descriptor.size());
            output.close();
        }
        try
        {
            install::IsoImageReader reader(path);
            std::array<char, 2048> actual{};
            reader.Read(0x10000, actual.data(), actual.size());
            if (actual != descriptor || !reader.GetEntries().empty()) ++failures;
        }
        catch (const std::exception& error)
        {
            std::cerr << path.string() << ": " << error.what() << "\n";
            ++failures;
        }
    };
    check(0, root / "standard.iso", false);
    check(2048, root / fs::path(u8"中文填充.iso"), true);
    check(1024 * 1024 - 2048 - 0x10000, root / "chunk-boundary.iso", false);
    fs::remove_all(root);
    if (failures) std::cerr << failures << " ISO locator cases failed\n";
    return failures ? 1 : 0;
}
