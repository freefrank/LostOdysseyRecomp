#include <stdafx.h>
#include <gpu/guest_writes.h>

// Vertex and index buffer writes noted where the title's D3D runtime finishes
// them, so the renderer's caches look at a source again only after it changed
// (gpu/guest_writes.h). Resource header: +0 Common (type in bits 0-3: 1 vertex,
// 2 index), +0x18 address, +0x1C size. A vertex buffer keeps its fetch
// constant there: the address with the fetch type in bits 0-1, then the size
// in dwords from bit 2.

extern "C" PPC_FUNC(__imp__sub_823F1458);
extern "C" PPC_FUNC(__imp__sub_82BE0F90);

namespace
{
// The address may be in a physical view or already physical.
void NoteBuffer(uint32_t resource, uint8_t* base)
{
    using namespace gpu::guest_writes;
    switch (PPC_LOAD_U32(resource) & 0xF)
    {
    case 1:
        NotePhysical(PhysicalOf(PPC_LOAD_U32(resource + 0x18) & ~3u), PPC_LOAD_U32(resource + 0x1C) & 0x03FFFFFCu);
        break;
    case 2:
        NotePhysical(PhysicalOf(PPC_LOAD_U32(resource + 0x18)), PPC_LOAD_U32(resource + 0x1C));
        break;
    }
}
}

// D3D resource Unlock (r3 resource). Every write through a Lock (sub_823F0D18)
// ends here; the only other unlock, inlined in 8270EA78, follows a read-only lock.
PPC_FUNC(sub_823F1458)
{
    const uint32_t resource = ctx.r3.u32;
    __imp__sub_823F1458(ctx, base);
    if (resource) NoteBuffer(resource, base);
}

// XGOffsetResourceAddress (r3 resource, r4 base). RHICreateVertexBuffer
// (sub_82760288) and RHICreateIndexBuffer (sub_82749328) point new buffers at
// resource array memory the package loader filled without a Lock.
PPC_FUNC(sub_82BE0F90)
{
    const uint32_t resource = ctx.r3.u32;
    __imp__sub_82BE0F90(ctx, base);
    if (resource) NoteBuffer(resource, base);
}

// After the device's command buffer setup (sub_827B6E48, hooked in
// gpu/frame_plan.cpp): its first and last dword at device +0x3A28 and +0x3A2C.
// DrawVerticesUP (823E9990) and DrawIndexedVerticesUP (823CDB98) copy their
// vertices and indices into it for every draw.
void gpu::guest_writes::CommandBufferSetUp(uint32_t device, uint8_t* base)
{
    if (!device) return;
    const uint32_t first = PPC_LOAD_U32(device + 0x3A28), last = PPC_LOAD_U32(device + 0x3A2C);
    if (first && last >= first) {
        const uint32_t physical = PhysicalOf(first);
        SetCommandBuffer(physical, last - first + 4);
        // Anything cached from earlier use of this memory is stale.
        NotePhysical(physical, last - first + 4);
    }
}
