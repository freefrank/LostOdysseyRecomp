#include <stdafx.h>
#include "xdm.h"
#include "function.h"
#include "io/file_system.h"
#include "io/io_diagnostics.h"

Mutex g_kernelLock;
kernel::HandleTable<KernelObject>& KernelHandles() {
    static auto* table = new kernel::HandleTable<KernelObject>;
    return *table;
}
bool DestroyKernelObject(uint32_t handle) {
    if (!io_diagnostics::Enabled()) return KernelHandles().Close(handle);
    io_diagnostics::RecordEvent(io_diagnostics::Stage::CloseBegin, "CloseHandle", nullptr, handle,
        nullptr, g_ppcContext ? g_ppcContext->r13.u32 : 0);
    std::shared_ptr<KernelObject> object;
    const bool closed = KernelHandles().Close(handle, &object);
    if (closed) FileSystem::TraceHandleClose(handle, object, true);
    return closed;
}
uint32_t GetKernelHandle(KernelObject* obj) { return obj ? g_memory.MapVirtual(obj) : 0; }
bool IsKernelObject(uint32_t handle) { return bool(KernelHandles().Acquire(handle)); }
uint32_t DuplicateKernelHandle(uint32_t source, bool closeSource) {
    auto* storage = g_userHeap.Alloc(sizeof(uint32_t));
    if (!storage) return 0;
    std::shared_ptr<void> token(storage, [](void* p) { g_userHeap.Free(p); });
    const auto handle = g_memory.MapVirtual(storage);
    const bool traceClose = closeSource && io_diagnostics::Enabled();
    std::shared_ptr<KernelObject> traced;
    if (traceClose) io_diagnostics::RecordEvent(io_diagnostics::Stage::CloseBegin, "DuplicateCloseSource",
        nullptr, source, nullptr, g_ppcContext ? g_ppcContext->r13.u32 : 0);
    const bool duplicated = KernelHandles().Duplicate(source, handle, std::move(token), closeSource,
        traceClose ? &traced : nullptr);
    if (traced && duplicated) FileSystem::TraceHandleClose(source, traced, true);
    return duplicated ? handle : 0;
}
uint32_t ReferenceKernelHandle(uint32_t handle) {
    auto object = KernelHandles().Acquire(handle);
    if (!object) return 0;
    const auto address = GetKernelHandle(object);
    KernelHandles().ReferenceObject(address, object);
    return address;
}
void ReferenceKernelObject(uint32_t address) {
    if (auto object = KernelHandles().AcquireObject(address)) KernelHandles().ReferenceObject(address, object);
}
void DereferenceKernelObject(uint32_t address) { KernelHandles().DereferenceObject(address); }
