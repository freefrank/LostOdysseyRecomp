#pragma once

#include "ppc_context.h"
#include <kernel/memory.h>

// Scoped allocation on the current guest stack. Local variable use only.
template<typename T, bool Init = true>
class guest_stack_var
{
    uint32_t m_ptr = 0;
    uint32_t m_oldStackPtr = 0;

    void AllocGuestStackMemory()
    {
        auto ctx = GetPPCContext();
        m_oldStackPtr = ctx->r1.u32;
        m_ptr = (ctx->r1.u32 - sizeof(T)) & ~(std::max<uint32_t>(alignof(T), 8) - 1);
        ctx->r1.u32 = m_ptr;
    }

public:
    T* get() { return reinterpret_cast<T*>(g_memory.Translate(m_ptr)); }
    const T* get() const { return reinterpret_cast<const T*>(g_memory.Translate(m_ptr)); }

    template<typename... Args>
    guest_stack_var(Args&&... args)
    {
        AllocGuestStackMemory();
        if (Init)
            new (get()) T(std::forward<Args>(args)...);
    }

    guest_stack_var(const guest_stack_var&) = delete;
    guest_stack_var& operator=(const guest_stack_var&) = delete;

    ~guest_stack_var()
    {
        get()->~T();
        auto ctx = GetPPCContext();
        assert(ctx->r1.u32 == m_ptr);
        ctx->r1.u32 = m_oldStackPtr;
    }

    operator T* () { return get(); }
    T* operator->() { return get(); }
    T& operator*() { return *get(); }
};
