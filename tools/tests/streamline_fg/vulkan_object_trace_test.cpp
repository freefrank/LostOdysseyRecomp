#include <gpu/vulkan_object_trace.h>
#include <cstdio>
#include <cstring>

// Run in separate processes: Enabled() intentionally caches the startup option.
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const bool expected = std::strcmp(argv[1], "enabled") == 0;
    unsigned checks = 0;
    const auto check = [&](bool value) { ++checks; return value; };
    if (!check(gpu::vk_object_trace::Enabled() == expected)) return 1;
    if (!check(gpu::vk_object_trace::Id(uint64_t{0x123456789abcdef0ull}) == 0x123456789abcdef0ull)) return 1;
    int object = 0;
    if (!check(gpu::vk_object_trace::Id(&object) == static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&object)))) return 1;
    if (!check(gpu::vk_object_trace::Id(static_cast<int*>(nullptr)) == 0)) return 1;
    for (unsigned i = 0; i < 8192; ++i)
        if (!check(gpu::vk_object_trace::Permit() == expected)) return 1;
    for (unsigned i = 0; i < 32; ++i)
        if (!check(!gpu::vk_object_trace::Permit())) return 1;
    std::printf("PASS: %u trace option/handle/budget checks (CPU helper only)\n", checks);
}
