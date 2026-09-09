#pragma once
#include <algorithm>
#include <cstdint>
#include <span>

namespace gpu {
// Match ReadRegister's zero-value MMIO fallback. Callers run on the command
// processor thread and keep the same register order; this is not a cache.
template<class MmioValue>
inline void CopyRegisterSnapshot(std::span<const uint32_t> registers,
    uint32_t first, std::span<uint32_t> output, const MmioValue* mmio)
{
    const size_t count = first < registers.size()
        ? std::min(output.size(), registers.size() - first) : 0;
    const auto* source = count ? registers.data() + first : nullptr;
    for (size_t i = 0; i < count; ++i)
        output[i] = source[i] ? source[i] : uint32_t(mmio[i]);
    std::fill(output.begin() + count, output.end(), 0);
}
}
