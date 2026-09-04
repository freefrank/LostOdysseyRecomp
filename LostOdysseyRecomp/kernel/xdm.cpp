#include <stdafx.h>
#include "xdm.h"

Mutex g_kernelLock;

void DestroyKernelObject(KernelObject* obj)
{
    obj->~KernelObject();
    g_userHeap.Free(obj);
}

uint32_t GetKernelHandle(KernelObject* obj)
{
    assert(obj != GetInvalidKernelObject());
    return g_memory.MapVirtual(obj);
}

void DestroyKernelObject(uint32_t handle)
{
    DestroyKernelObject(GetKernelObject(handle));
}

bool IsKernelObject(uint32_t handle)
{
    // Host heap lives at 0x7C000000+, guest handles are pointers into it.
    return handle >= 0x7C000000 && handle < 0x7FC00000;
}

bool IsKernelObject(void* obj)
{
    return IsKernelObject(g_memory.MapVirtual(obj));
}

bool IsInvalidKernelObject(void* obj)
{
    return obj == GetInvalidKernelObject();
}
