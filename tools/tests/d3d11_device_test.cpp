#include "gpu/d3d11_probe.h"
#include <cstdio>
#include <cstring>

int main()
{
    char text[192]{};
    gpu::d3d11::ProbeResult failed{};
    failed.hr = E_FAIL;
    gpu::d3d11::Format(text, sizeof(text), failed);
    if (std::strncmp(text, "failed HRESULT=", 15) != 0)
    {
        std::fprintf(stderr, "Format(failed) produced '%s'\n", text);
        return 1;
    }

    const auto probe = gpu::d3d11::ProbeHardware();
    gpu::d3d11::Format(text, sizeof(text), probe);
    if (FAILED(probe.hr))
    {
        std::fprintf(stderr, "D3D11 probe %s\n", text);
        return 1;
    }
    if (std::strncmp(text, "ok feature_level=", 17) != 0)
    {
        std::fprintf(stderr, "Format(ok) produced '%s'\n", text);
        return 1;
    }
    std::printf("PASS: D3D11 probe %s\n", text);
    return 0;
}
