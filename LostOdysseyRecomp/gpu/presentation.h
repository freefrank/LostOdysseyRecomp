#pragma once
#include <cstdint>
#include <memory>
namespace plume
{
struct RenderDevice;
struct RenderCommandList;
struct RenderTexture;
} // namespace plume
namespace gpu
{
// Owned by the presentation thread. Resources stay alive until the present fence.
class Presentation
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    Presentation();
    ~Presentation();
    bool Init(plume::RenderDevice *device);
    void Draw(plume::RenderCommandList *commands, plume::RenderTexture *source, plume::RenderTexture *target,
              uint32_t sourceWidth, uint32_t sourceHeight, uint32_t outputWidth, uint32_t outputHeight, bool antialias);
};
} // namespace gpu
