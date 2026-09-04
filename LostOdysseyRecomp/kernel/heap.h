#pragma once

#include "mutex.h"

// Host-side allocator inside guest memory, used for kernel objects, thread
// blocks and anything the runtime hands to the game. The game's own heap runs
// through NtAllocateVirtualMemory instead (see memory.h).
struct Heap
{
    Mutex mutex;
    O1HeapInstance* heap{};

    void Init();

    void* Alloc(size_t size);
    void Free(void* ptr);
    size_t Size(void* ptr);

    template<typename T, typename... Args>
    T* Alloc(Args&&... args)
    {
        T* obj = (T*)Alloc(sizeof(T));
        new (obj) T(std::forward<Args>(args)...);
        return obj;
    }
};

extern Heap g_userHeap;
