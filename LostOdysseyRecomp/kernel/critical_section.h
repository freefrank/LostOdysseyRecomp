#pragma once

#include <atomic>
#include <bit>
#include <cstdint>

namespace GuestCriticalSection
{
// The SDK storage fields are native integers, but guest code reads these two
// words with lwz. Keep their memory representation big endian, including CAS.
inline uint32_t Encode(uint32_t value)
{
    if constexpr (std::endian::native == std::endian::little)
        return std::byteswap(value);
    return value;
}

inline void Increment(int32_t& recursion)
{
    recursion = std::bit_cast<int32_t>(Encode(Encode(std::bit_cast<uint32_t>(recursion)) + 1));
}

inline bool TryEnter(int32_t& recursion, uint32_t& owner, uint32_t thread)
{
    std::atomic_ref owningThread(owner);
    const uint32_t encodedThread = Encode(thread);
    uint32_t previousOwner = 0;
    if (owningThread.compare_exchange_strong(previousOwner, encodedThread) || previousOwner == encodedThread)
    {
        Increment(recursion);
        return true;
    }
    return false;
}

inline void Enter(int32_t& recursion, uint32_t& owner, uint32_t thread)
{
    std::atomic_ref owningThread(owner);
    const uint32_t encodedThread = Encode(thread);
    for (;;)
    {
        uint32_t previousOwner = 0;
        if (owningThread.compare_exchange_strong(previousOwner, encodedThread) || previousOwner == encodedThread)
        {
            Increment(recursion);
            return;
        }
        owningThread.wait(previousOwner);
    }
}

inline void Leave(int32_t& recursion, uint32_t& owner)
{
    const uint32_t remaining = Encode(std::bit_cast<uint32_t>(recursion)) - 1;
    recursion = std::bit_cast<int32_t>(Encode(remaining));
    if (remaining != 0)
        return;
    std::atomic_ref owningThread(owner);
    owningThread.store(0);
    owningThread.notify_one();
}
}
