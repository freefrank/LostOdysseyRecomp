// GPU-stopped dialog advice (#337, #533): which stops name AMD's driver
// upscaling and which offer DirectX 12 for the next start.
#include "gpu/device_loss_hint.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
bool Has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

using namespace gpu::device_loss_hint;

// #533: RX 6600, Vulkan from settings.ini, lost 6.7 s after the renderer went live.
Context AmdBoot()
{
    Context c;
    c.windows = true;
    c.vulkan = true;
    c.deviceLost = true;
    c.vendorId = AmdVendorId;
    c.settingsChoseVulkan = true;
    c.secondsLive = 6.7;
    c.gameplaySeen = false;
    return c;
}
} // namespace

int main()
{
    try
    {
        {
            const auto advice = Decide(AmdBoot());
            Check(advice.amdHint && advice.offerD3D12, "AMD boot loss: both the hint and the offer");
            const auto text = Text(advice);
            Check(text.rfind(StoppedText, 0) == 0, "the stopped text stays first");
            Check(Has(text, "FSR Upscaling / Radeon Super Resolution") && Has(text, "AMD Software"), "names AMD's driver upscaling");
            Check(Has(text, "Start with DirectX 12 next time? This sets graphics_backend=0 in settings.ini."),
                "asks about DirectX 12 and says what accepting writes");
            Check(!Has(text, "or switch to DirectX 12"), "the question replaces the by-hand instruction");
        }
        {
            // Breylus (#337): 283 s of pipeline compile, then lost 4.6 s after going live.
            auto c = AmdBoot();
            c.secondsLive = 4.6;
            Check(Decide(c).offerD3D12, "the clock starts when the renderer goes live, not at process start");
            c.secondsLive = EarlySeconds;
            Check(Decide(c).offerD3D12, "the early window includes its last second");
        }
        {
            auto c = AmdBoot();
            c.secondsLive = EarlySeconds + 0.1;
            const auto late = Decide(c);
            Check(late.amdHint && !late.offerD3D12, "late AMD loss: hint, no offer");
            const auto text = Text(late);
            Check(Has(text, "or switch to DirectX 12: set graphics_backend=0 in settings.ini."), "late AMD loss names DirectX 12 by hand");
            Check(!Has(text, "?"), "no question without the offer");
        }
        {
            auto c = AmdBoot();
            c.gameplaySeen = true;
            const auto advice = Decide(c);
            Check(advice.amdHint && !advice.offerD3D12, "a gameplay frame ends the boot window");
        }
        {
            auto c = AmdBoot();
            c.secondsLive = -1.0;
            Check(!Decide(c).offerD3D12, "unknown live time: no offer");
        }
        {
            auto c = AmdBoot();
            c.vendorId = 0x10DE;
            const auto advice = Decide(c);
            Check(!advice.amdHint && advice.offerD3D12, "NVIDIA boot loss: offer only");
            const auto text = Text(advice);
            Check(!Has(text, "AMD"), "no AMD text for NVIDIA");
            Check(Has(text, "Start with DirectX 12 next time?"), "other vendors still get the offer at boot");
            c.secondsLive = 600.0;
            Check(Text(Decide(c)) == StoppedText, "other vendors keep today's text after boot");
        }
        {
            auto c = AmdBoot();
            c.deviceLost = false; // e.g. -13 from our own submission checks
            const auto advice = Decide(c);
            Check(!advice.amdHint && !advice.offerD3D12 && Text(advice) == StoppedText, "only device loss gets advice");
        }
        {
            auto c = AmdBoot();
            c.vulkan = false;
            Check(Text(Decide(c)) == StoppedText, "DirectX 12 stops keep today's text");
        }
        {
            auto c = AmdBoot();
            c.windows = false;
            Check(Text(Decide(c)) == StoppedText, "no AMD Software or DirectX 12 outside Windows");
        }
        {
            auto c = AmdBoot();
            c.settingsChoseVulkan = false; // LO_GRAPHICS_API=vulkan, or a fallback from DirectX 12
            const auto advice = Decide(c);
            Check(advice.amdHint && !advice.offerD3D12, "no offer when settings.ini does not choose the backend");
        }
        std::cout << "PASS: GPU-stopped dialog advice\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
