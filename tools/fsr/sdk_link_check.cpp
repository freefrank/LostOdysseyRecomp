// Link-only SDK check: no Vulkan instance, device, window, or GPU work.
#include <FidelityFX/host/ffx_fsr3upscaler.h>
#include <FidelityFX/host/backends/vk/ffx_vk.h>
bool Dx12SdkLinkCheck();

int main()
{
    auto volatile backend = &ffxGetInterfaceVK;
    auto volatile dispatch = &ffxFsr3UpscalerContextDispatch;
#if defined(LO_HAS_FSR_D3D12) && LO_HAS_FSR_D3D12
    if (!Dx12SdkLinkCheck()) return 1;
#endif
    return backend && dispatch && ffxFsr3UpscalerGetEffectVersion() == FFX_SDK_MAKE_VERSION(3, 1, 4) ? 0 : 1;
}
