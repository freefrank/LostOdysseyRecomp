#include <SDL3/SDL.h>
#include <plume_render_interface.h>
#include <cstdio>
#include <cstring>

namespace plume { std::unique_ptr<RenderInterface> CreateVulkanInterface(RenderWindow); }

int main(int argc, char** argv)
{
    using namespace plume;
    const bool expectSdr = argc == 2 && std::strcmp(argv[1], "--expect-sdr") == 0;
    if (!SDL_Init(SDL_INIT_VIDEO)) { std::fprintf(stderr,"SDL: %s\n",SDL_GetError()); return 1; }
    SDL_Window* window=SDL_CreateWindow("HDR surface probe",320,180,
        SDL_WINDOW_VULKAN|SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);
    if (!window) { std::fprintf(stderr,"Window: %s\n",SDL_GetError()); SDL_Quit(); return 1; }
    bool pass=false;
    {
        auto api=CreateVulkanInterface(window);
        auto device=api ? api->createDevice() : nullptr;
        auto queue=device ? device->createCommandQueue(RenderCommandListType::DIRECT) : nullptr;
        if (queue) {
            std::printf("SDL backend=%s GPU=%s\n",SDL_GetCurrentVideoDriver(),device->getDescription().name.c_str());
            RenderSwapChainDesc desc(window,RenderFormat::R16G16B16A16_FLOAT,3);
            desc.outputMode=RenderOutputMode::HDR;
            auto swap=queue->createSwapChain(desc);
            if (swap && swap->resize() && !swap->isEmpty()) {
                const auto state=swap->getDisplayState();
                pass=!state.hdrStateKnown && (!expectSdr || state.encoding==RenderOutputEncoding::SDR);
                if (state.encoding==RenderOutputEncoding::SDR)
                    pass &= !state.hdrTransport && (swap->getFormat()==RenderFormat::R8G8B8A8_UNORM || swap->getFormat()==RenderFormat::B8G8R8A8_UNORM);
                else pass &= state.hdrTransport;
                std::printf("HDR requested: format=%u encoding=%u transport=%d physical_state_known=%d peak=%f\n",
                    unsigned(swap->getFormat()),unsigned(state.encoding),state.hdrTransport,state.hdrStateKnown,state.peakNits);
                SDL_SetWindowSize(window,480,270); SDL_PumpEvents();
                pass &= swap->resize() && !swap->isEmpty();
            }
        }
    }
    SDL_DestroyWindow(window); SDL_Quit();
    std::printf("Vulkan HDR negotiation, optional SDR fallback and resize: %s\n",pass?"PASS":"FAIL");
    return pass?0:1;
}
