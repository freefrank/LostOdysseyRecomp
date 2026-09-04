#include <stdafx.h>
#include "heap.h"
#include "memory.h"
#include <os/logger.h>

// Host heap occupies the top of the guest virtual region so it never collides
// with the game's own NtAllocateVirtualMemory requests (which grow upwards).
Heap g_userHeap;

constexpr uint32_t HOST_HEAP_BEGIN = 0x7C000000; // 60 MiB, keep in sync with IsKernelObject() and PageAllocator::Init()
constexpr uint32_t HOST_HEAP_END = 0x7FC00000;   // 0x7FC00000+ is MMIO (GPU registers at 0x7FC80000), must stay clear

void Heap::Init()
{
    heap = o1heapInit(g_memory.Translate(HOST_HEAP_BEGIN), HOST_HEAP_END - HOST_HEAP_BEGIN);
    assert(heap != nullptr);
    LOG_INFO("host heap at {} capacity {:#x}", (void*)heap, o1heapGetDiagnostics(heap).capacity);
}

void* Heap::Alloc(size_t size)
{
    std::lock_guard lock(mutex);
    void* ptr = o1heapAllocate(heap, std::max<size_t>(1, size));
    assert(ptr != nullptr && "host heap exhausted");
    return ptr;
}

void Heap::Free(void* ptr)
{
    if (ptr == nullptr)
        return;
    std::lock_guard lock(mutex);
    o1heapFree(heap, ptr);
}

size_t Heap::Size(void* ptr)
{
    if (ptr)
        return *((size_t*)ptr - 2) - O1HEAP_ALIGNMENT; // relies on the fragment header layout in o1heap.c

    return 0;
}
