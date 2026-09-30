#include <plume_render_interface.h>
#ifndef _WIN32
namespace plume { std::unique_ptr<RenderInterface> CreateD3D12Interface() { return {}; } }
#endif
// Telemetry is deliberately disabled in offline rendering fixtures.
#include <gpu/temporal_collection_gpu.h>
namespace gpu::taa_collection {
bool WantSparse() { return false; }
uint64_t ConsentEpoch() noexcept { return 0; }
void SubmitSparse(SparseFrame) {}
}
