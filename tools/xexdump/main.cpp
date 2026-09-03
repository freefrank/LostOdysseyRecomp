// Dump the decrypted + decompressed XEX image (as loaded in memory) to a flat file.
// Usage: xexdump <default.xex> <out.bin>
// Prints base/size/entry and section layout so scripts can map file offsets to addresses.
#include <file.h>
#include <image.h>
#include <cstdio>

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("Usage: xexdump <input XEX> <output image .bin>\n");
        return 1;
    }
    const auto file = LoadFile(argv[1]);
    if (file.empty())
    {
        printf("failed to read %s\n", argv[1]);
        return 1;
    }
    auto image = Image::ParseImage(file.data(), file.size());
    if (!image.data)
    {
        printf("failed to parse image\n");
        return 1;
    }
    printf("base=0x%zx size=0x%x entry=0x%zx\n", image.base, image.size, image.entry_point);
    for (const auto& s : image.sections)
        printf("section %-10s base=0x%zx size=0x%08x flags=%d\n", s.name.c_str(), s.base, s.size, (int)s.flags);

    FILE* out = fopen(argv[2], "wb");
    if (!out)
    {
        printf("failed to open %s\n", argv[2]);
        return 1;
    }
    fwrite(image.data.get(), 1, image.size, out);
    fclose(out);
    printf("wrote %u bytes to %s\n", image.size, argv[2]);
    return 0;
}
