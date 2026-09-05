#include <kernel/guest_address_space.h>
#include <cstdio>
#include <cstdint>
#include <initializer_list>

int main()
{
    // Repeat to exercise release and reuse, including the preferred host base.
    for (int iteration = 0; iteration < 2; ++iteration)
    {
        uint8_t* base = GuestAddressSpace::Allocate();
        if (!base)
        {
            std::fprintf(stderr, "Guest address space allocation failed\n");
            return 1;
        }
        auto word = [base](uint32_t address) -> volatile uint32_t& {
            return *reinterpret_cast<volatile uint32_t*>(base + address);
        };
        bool ok = true;
        for (uint32_t offset : {0u, 0x1000u, 0x1FFFCu, 0x1234560u, 0x1FFFFFFCu})
        {
            word(0xA0000000u + offset) = 0x12345678;
            ok &= word(0xC0000000u + offset) == 0x12345678;
            word(0xC0000000u + offset) = 0;
            ok &= word(0xA0000000u + offset) == 0;
            if (offset >= 0x1000)
            {
                word(0xE0000000u + offset - 0x1000) = 0xABCDEF01;
                ok &= word(0xA0000000u + offset) == 0xABCDEF01;
                ok &= word(0xC0000000u + offset) == 0xABCDEF01;
            }
        }
        // A virtual page must not accidentally alias the physical page.
        word(0x1000) = 0x87654321;
        word(0xA0001000) = 0xDEADBEEF;
        ok &= word(0x1000) == 0x87654321;

        // Occlusion-query round trip: CPU initializes through C, GPU writes
        // END through A, CPU subtracts BEGIN from END through C.
        constexpr uint32_t query = 0x01002000;
        word(0xC0000000 + query + 0x30) = 100;
        word(0xC0000000 + query + 0x10) = 0xFFFFFFFF;
        ok &= word(0xA0000000 + query + 0x10) == 0xFFFFFFFF;
        word(0xA0000000 + query + 0x10) = 65636;
        ok &= word(0xC0000000 + query + 0x10) - word(0xC0000000 + query + 0x30) == 65536;
        GuestAddressSpace::Release(base);
        if (!ok)
        {
            std::fprintf(stderr, "Physical alias coherence failed\n");
            return 1;
        }
    }
    std::puts("PASS: A/C coherence, E offset, virtual isolation, query round trip, release/reallocate");
    return 0;
}
