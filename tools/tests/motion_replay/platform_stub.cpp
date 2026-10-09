#include <plume_render_interface.h>
#ifndef _WIN32
namespace plume { std::unique_ptr<RenderInterface> CreateD3D12Interface() { return {}; } }
#endif
