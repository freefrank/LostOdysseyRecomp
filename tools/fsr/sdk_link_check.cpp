// Link-only SDK check: no Vulkan instance, device, window, or GPU work.
#include <FidelityFX/host/ffx_fsr3upscaler.h>
#include <FidelityFX/host/backends/vk/ffx_vk.h>

int main()
{
    auto volatile backend = &ffxGetInterfaceVK;
    auto volatile dispatch = &ffxFsr3UpscalerContextDispatch;
    return backend && dispatch && ffxFsr3UpscalerGetEffectVersion() == FFX_SDK_MAKE_VERSION(3, 1, 4) ? 0 : 1;
}
