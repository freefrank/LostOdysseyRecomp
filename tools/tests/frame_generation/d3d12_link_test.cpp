#include "frame_generation/d3d12.h"
#include <cstdio>
int main() {
    std::string reason;
    framegen::Config config{framegen::Provider::Dlss,framegen::Mode::Fixed,1,0};
#ifdef FRAMEGEN_WITH_DLSS
    if (framegen::CreateDlssD3D12(nullptr,nullptr,config,L"missing-runtime",reason)) return 1;
#endif
#ifdef FRAMEGEN_WITH_FSR
    config.provider=framegen::Provider::Fsr;
    if (framegen::CreateFsrD3D12(nullptr,nullptr,config,L"missing-runtime.dll",reason)) return 2;
#endif
    std::puts("D3D12 adapter link/invalid-device smoke passed (no GPU or SDK runtime execution)");
}
