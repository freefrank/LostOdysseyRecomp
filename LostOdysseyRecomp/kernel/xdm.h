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

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
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
#ifndef ERROR_INVALID_HANDLE
#define ERROR_INVALID_HANDLE       0x6
#endif
#ifndef ERROR_NO_MORE_FILES
#define ERROR_NO_MORE_FILES        0x12
#endif
#ifndef ERROR_WRITE_FAULT
#define ERROR_WRITE_FAULT          0x1D
#endif
#ifndef ERROR_INVALID_PARAMETER
#define ERROR_INVALID_PARAMETER    0x57
#endif
#ifndef ERROR_INSUFFICIENT_BUFFER
#define ERROR_INSUFFICIENT_BUFFER  0x7A
#endif
#ifndef ERROR_ALREADY_EXISTS
#define ERROR_ALREADY_EXISTS       0xB7
#endif
#ifndef ERROR_FUNCTION_FAILED
#define ERROR_FUNCTION_FAILED      0x65B
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

#include "dispatcher_wait.h"
#include "handle_table.h"
#include <memory>

struct KernelObject
{
    virtual ~KernelObject() = default;
    virtual kernel::wait::Target* WaitTarget() { return nullptr; }
    virtual uint32_t Wait(uint32_t timeout) {
        auto* target = WaitTarget();
        return target ? target->Wait(timeout) : STATUS_INVALID_HANDLE;
    }
};
// Function-local registry is intentionally process-lifetime: global guest heap
// teardown must not race registry destruction while detached guest workers exit.
kernel::HandleTable<KernelObject>& KernelHandles();

template<typename T, typename... Args>
inline std::shared_ptr<T> CreateKernelObject(Args&&... args)
{
    static_assert(std::is_base_of_v<KernelObject, T>);
    void* storage = g_userHeap.Alloc(sizeof(T));
    if (!storage) throw std::bad_alloc();
    T* raw;
    try { raw = new (storage) T(std::forward<Args>(args)...); }
    catch (...) { g_userHeap.Free(storage); throw; }
    std::shared_ptr<T> object(raw, [](T* p) { p->~T(); g_userHeap.Free(p); });
    KernelHandles().Insert(g_memory.MapVirtual(raw), object);
    return object;
}

template<typename T = KernelObject>
inline std::shared_ptr<T> GetKernelObject(uint32_t handle)
{
    return std::dynamic_pointer_cast<T>(KernelHandles().Acquire(handle));
}
uint32_t GetKernelHandle(KernelObject* obj);
template<class T> inline uint32_t GetKernelHandle(const std::shared_ptr<T>& obj) {
    return GetKernelHandle(obj.get());
}
bool DestroyKernelObject(uint32_t handle);
template<class T> inline bool DestroyKernelObject(const std::shared_ptr<T>& obj) {
    return DestroyKernelObject(GetKernelHandle(obj));
}
bool IsKernelObject(uint32_t handle);
uint32_t DuplicateKernelHandle(uint32_t source, bool closeSource);
uint32_t ReferenceKernelHandle(uint32_t handle);
void ReferenceKernelObject(uint32_t address);
void DereferenceKernelObject(uint32_t address);
extern Mutex g_kernelLock;

// Embedded dispatchers keep their canonical handle in the guest header; callers
// retain their own reference before dropping the attachment lock.
template<typename T>
inline std::shared_ptr<T> QueryKernelObject(XDISPATCHER_HEADER& header)
{
    std::lock_guard guard{ g_kernelLock };
    if (header.WaitListHead.Flink == OBJECT_SIGNATURE) {
        if (auto object = GetKernelObject<T>(header.WaitListHead.Blink)) return object;
    }
    auto object = CreateKernelObject<T>(reinterpret_cast<typename T::guest_type*>(&header));
    header.WaitListHead.Blink = GetKernelHandle(object);
    header.WaitListHead.Flink = OBJECT_SIGNATURE;
    return object;
}
template<typename T>
inline std::shared_ptr<T> TryQueryKernelObject(XDISPATCHER_HEADER& header)
{
    std::lock_guard guard{ g_kernelLock };
    return header.WaitListHead.Flink == OBJECT_SIGNATURE
        ? GetKernelObject<T>(header.WaitListHead.Blink) : nullptr;
}
