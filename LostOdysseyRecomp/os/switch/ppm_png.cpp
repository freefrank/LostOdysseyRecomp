// The debug server's "shot" as a PNG (ppm_png.h). No game code: tools/switch/debug_server_host_test.cpp
// builds it on a computer.
#include "ppm_png.h"

#include <miniz.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>

namespace os::debug_server
{
bool PpmToPng(const std::string& path, std::string& png, std::string& why)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return why = "cannot open " + path, false;
    unsigned width = 0, height = 0, maxValue = 0;
    if (std::fscanf(f, "P6 %u %u %u", &width, &height, &maxValue) != 3 || maxValue != 255 || std::fgetc(f) == EOF ||
        !width || !height || width > 8192 || height > 8192)
    {
        std::fclose(f);
        return why = "not an 8-bit PPM: " + path, false;
    }
    auto* compressor = static_cast<tdefl_compressor*>(std::malloc(sizeof(tdefl_compressor)));
    if (!compressor)
    {
        std::fclose(f);
        return why = "no memory for the PNG compressor", false;
    }
    std::string idat;
    const auto put = [](const void* data, int length, void* user) -> mz_bool {
        try
        {
            static_cast<std::string*>(user)->append(static_cast<const char*>(data), size_t(length));
            return MZ_TRUE;
        }
        catch (const std::bad_alloc&)
        {
            return MZ_FALSE;
        }
    };
    // Level 3: a few times smaller than the PPM, fast on the console's CPU.
    tdefl_init(compressor, put, &idat, int(tdefl_create_comp_flags_from_zip_params(3, 15, MZ_DEFAULT_STRATEGY)));
    // Each row with PNG's "Up" filter (the byte minus the one above it): a frame's rows are alike, so the
    // differences compress far better than the pixels, for one subtraction per byte.
    const size_t stride = size_t(width) * 3;
    std::string previous(stride, '\0'), current(stride, '\0'), row(stride + 1, '\2');
    bool good = true;
    for (unsigned y = 0; y < height && good; y++)
    {
        good = std::fread(current.data(), 1, stride, f) == stride;
        for (size_t i = 0; i < stride; i++)
            row[i + 1] = char(uint8_t(current[i]) - uint8_t(previous[i]));
        good = good && tdefl_compress_buffer(compressor, row.data(), row.size(), TDEFL_NO_FLUSH) == TDEFL_STATUS_OKAY;
        previous.swap(current);
    }
    good = good && tdefl_compress_buffer(compressor, nullptr, 0, TDEFL_FINISH) == TDEFL_STATUS_DONE;
    std::free(compressor);
    std::fclose(f);
    if (!good)
        return why = "PNG compression failed (out of memory, or a short file)", false;
    const auto chunk = [&png](const char* type, const std::string& data) {
        const uint32_t length = uint32_t(data.size());
        const char size[4] = { char(length >> 24), char(length >> 16), char(length >> 8), char(length) };
        png.append(size, 4).append(type, 4).append(data);
        mz_ulong crc = mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const unsigned char*>(type), 4);
        crc = mz_crc32(crc, reinterpret_cast<const unsigned char*>(data.data()), data.size());
        const char sum[4] = { char(crc >> 24), char(crc >> 16), char(crc >> 8), char(crc) };
        png.append(sum, 4);
    };
    std::string header(13, '\0');
    for (int i = 0; i < 4; i++)
    {
        header[i] = char(width >> (24 - 8 * i));
        header[4 + i] = char(height >> (24 - 8 * i));
    }
    header[8] = 8; // bits per channel
    header[9] = 2; // RGB
    png.assign("\x89PNG\r\n\x1a\n", 8);
    chunk("IHDR", header);
    chunk("IDAT", idat);
    chunk("IEND", {});
    return true;
}
}
