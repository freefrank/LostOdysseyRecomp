// Shortcut/VDF tests keep artwork generation as a boundary. The independent
// steam_artwork target exercises the real lookup, composition and fallback.
#include <os/steam_artwork.h>
#include <fstream>
#include <stdexcept>

namespace steam_artwork
{
Artwork Generate(const std::filesystem::path &, const std::filesystem::path &directory, const TextureReader &)
{
    constexpr std::array<const char *, 5> names{"portrait.png", "header.png", "hero.png", "logo.png", "icon.png"};
    Artwork artwork;
    for (size_t i = 0; i < names.size(); ++i)
    {
        std::ifstream input(directory / names[i], std::ios::binary);
        if (!input) throw std::runtime_error("artwork fixture missing");
        artwork.pngs[i] = {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }
    artwork.source = "Steam artwork fixture boundary";
    return artwork;
}
}
