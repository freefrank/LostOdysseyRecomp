#include <FidelityFX/host/ffx_fsr3upscaler.h>
#include <FidelityFX/host/backends/dx12/ffx_dx12.h>

bool Dx12SdkLinkCheck() {
    auto volatile backend = &ffxGetInterfaceDX12;
    return backend && ffxGetScratchMemorySizeDX12(FFX_FSR3UPSCALER_CONTEXT_COUNT);
}
