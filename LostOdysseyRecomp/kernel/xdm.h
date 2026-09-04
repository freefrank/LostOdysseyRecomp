#pragma once

#include "heap.h"
#include "memory.h"

#define OBJECT_SIGNATURE           (uint32_t)'XBOX'
#define GUEST_INVALID_HANDLE_VALUE 0xFFFFFFFF

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS             0x00000000
#endif
#ifndef STATUS_WAIT_0
#define STATUS_WAIT_0              0x00000000
#endif
#ifndef STATUS_ABANDONED_WAIT_0
#define STATUS_ABANDONED_WAIT_0    0x00000080
#endif
#ifndef STATUS_USER_APC
#define STATUS_USER_APC            0x000000C0
#endif
#ifndef STATUS_TIMEOUT
#define STATUS_TIMEOUT             0x00000102
#endif
#ifndef STATUS_PENDING
#define STATUS_PENDING             0x00000103
#endif
#ifndef STATUS_END_OF_FILE
#define STATUS_END_OF_FILE         0xC0000011
#endif
#ifndef STATUS_NO_MORE_FILES
#define STATUS_NO_MORE_FILES       0x80000006
#endif
#ifndef STATUS_INVALID_HANDLE
#define STATUS_INVALID_HANDLE      0xC0000008
#endif
#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER   0xC000000D
#endif
#ifndef STATUS_NO_SUCH_FILE
#define STATUS_NO_SUCH_FILE        0xC000000F
#endif
#ifndef STATUS_ACCESS_DENIED
#define STATUS_ACCESS_DENIED       0xC0000022
#endif
#ifndef STATUS_OBJECT_NAME_NOT_FOUND
#define STATUS_OBJECT_NAME_NOT_FOUND 0xC0000034
#endif
#ifndef STATUS_OBJECT_PATH_NOT_FOUND
#define STATUS_OBJECT_PATH_NOT_FOUND 0xC000003A
#endif
#ifndef STATUS_NOT_IMPLEMENTED
#define STATUS_NOT_IMPLEMENTED     0xC0000002
#endif
#ifndef STATUS_NO_MEMORY
#define STATUS_NO_MEMORY           0xC0000017
#endif
#ifndef STATUS_FAIL_CHECK
#define STATUS_FAIL_CHECK          0xC0000229
#endif
#ifndef STATUS_NOT_SUPPORTED
#define STATUS_NOT_SUPPORTED       0xC00000BB
#endif
#ifndef STATUS_INVALID_DEVICE_REQUEST
#define STATUS_INVALID_DEVICE_REQUEST 0xC0000010
#endif

#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS              0x0
#endif
#ifndef ERROR_PATH_NOT_FOUND
#define ERROR_PATH_NOT_FOUND       0x3
#endif
#ifndef ERROR_ACCESS_DENIED
#define ERROR_ACCESS_DENIED        0x5
#endif
#ifndef ERROR_NO_MORE_FILES
#define ERROR_NO_MORE_FILES        0x12
#endif
#ifndef ERROR_NOT_SUPPORTED
#define ERROR_NOT_SUPPORTED        0x32
#endif
#ifndef ERROR_BAD_ARGUMENTS
#define ERROR_BAD_ARGUMENTS        0xA0
#endif
#ifndef ERROR_DEVICE_NOT_CONNECTED
#define ERROR_DEVICE_NOT_CONNECTED 0x48F
#endif
#ifndef ERROR_NO_SUCH_USER
#define ERROR_NO_SUCH_USER         0x525
#endif
#ifndef ERROR_IO_PENDING
#define ERROR_IO_PENDING           0x3E5
#endif

#ifndef INFINITE
#define INFINITE 0xFFFFFFFF
#endif
#ifndef PAGE_READWRITE
#define PAGE_READWRITE 0x04
#endif

struct KernelObject
{
    virtual ~KernelObject() {}

    virtual uint32_t Wait(uint32_t timeout)
    {
        assert(false && "Wait not implemented for this kernel object.");
        return STATUS_TIMEOUT;
    }
};

template<typename T, typename... Args>
inline T* CreateKernelObject(Args&&... args)
{
    static_assert(std::is_base_of_v<KernelObject, T>);
    return g_userHeap.Alloc<T>(std::forward<Args>(args)...);
}

template<typename T = KernelObject>
inline T* GetKernelObject(uint32_t handle)
{
    assert(handle != GUEST_INVALID_HANDLE_VALUE);
    return reinterpret_cast<T*>(g_memory.Translate(handle));
}

uint32_t GetKernelHandle(KernelObject* obj);

void DestroyKernelObject(KernelObject* obj);
void DestroyKernelObject(uint32_t handle);

bool IsKernelObject(uint32_t handle);
bool IsKernelObject(void* obj);
bool IsInvalidKernelObject(void* obj);

template<typename T = void>
inline T* GetInvalidKernelObject()
{
    return reinterpret_cast<T*>(g_memory.Translate(GUEST_INVALID_HANDLE_VALUE));
}

extern Mutex g_kernelLock;

// Lazily attach a host object to a guest dispatcher header (events,
// semaphores, mutants, critical sections). The guest never touches
// WaitListHead so it doubles as our signature + back-pointer.
template<typename T>
inline T* QueryKernelObject(XDISPATCHER_HEADER& header)
{
    std::lock_guard guard{ g_kernelLock };
    if (header.WaitListHead.Flink != OBJECT_SIGNATURE)
    {
        header.WaitListHead.Flink = OBJECT_SIGNATURE;
        auto* obj = CreateKernelObject<T>(reinterpret_cast<typename T::guest_type*>(&header));
        header.WaitListHead.Blink = g_memory.MapVirtual(obj);
        return obj;
    }

    return static_cast<T*>(g_memory.Translate(header.WaitListHead.Blink.get()));
}

template<typename T>
inline T* TryQueryKernelObject(XDISPATCHER_HEADER& header)
{
    if (header.WaitListHead.Flink != OBJECT_SIGNATURE)
        return nullptr;

    return static_cast<T*>(g_memory.Translate(header.WaitListHead.Blink.get()));
}
