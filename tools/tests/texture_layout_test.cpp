#include <gpu/texture_layout.h>
#include <cstdio>

int main()
{
    struct Case { unsigned width, height, mip, blockWidth, blockHeight, x, y; };
    // Xenia packed-tail placements, including the title's BC1 white texture
    // and tall BC3 gradient. Large base levels must remain at the origin.
    constexpr Case cases[] = {
        {4, 4, 0, 4, 4, 4, 0}, {4, 64, 0, 4, 4, 4, 0},
        {64, 4, 0, 4, 4, 0, 4}, {16, 16, 0, 1, 1, 16, 0},
        {32, 32, 0, 4, 4, 0, 0}, {256, 256, 0, 1, 1, 0, 0},
        {32, 32, 1, 4, 4, 4, 0}, {32, 32, 2, 4, 4, 2, 0},
        {32, 32, 3, 4, 4, 1, 0}, {32, 32, 4, 4, 4, 0, 2},
        {512, 256, 4, 4, 4, 0, 4}, {512, 256, 7, 4, 4, 4, 0},
        {1, 1, 0, 1, 1, 16, 0}, {17, 16, 0, 1, 1, 0, 16},
    };
    for (const auto& c : cases)
    {
        const auto offset = gpu::PackedMipOffset2D(c.width, c.height, c.mip, c.blockWidth, c.blockHeight);
        if (offset.x != c.x || offset.y != c.y)
        {
            std::printf("FAIL: %ux%u mip %u -> %u,%u (expected %u,%u)\n", c.width, c.height, c.mip, offset.x, offset.y, c.x, c.y);
            return 1;
        }
    }
    std::puts("PASS: packed mip block origins (small, rectangular, compressed and later levels)");
    return 0;
}
