// LoAssetExport: the runtime's --export-assets mode built without the game
// runtime (CMake LO_BUILD_MOD_TESTS). --game names the disc1 folder.
#include <modding/asset_export.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

int main(int argc, char **argv)
{
    std::filesystem::path game;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--game") == 0)
        {
            const std::string value = argv[i + 1];
            game = std::filesystem::path(std::u8string(value.begin(), value.end()));
        }
    const auto request = modding::asset_export::ParseArguments(argc, argv);
    if (!request)
    {
        std::fputs("usage: LoAssetExport --export-assets <dir> --game <disc1> "
                   "[--export-kinds textures,movies] [--export-filter <text>]\n", stderr);
        return 1;
    }
    return modding::asset_export::Run(*request, game);
}
