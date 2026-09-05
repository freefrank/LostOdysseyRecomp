#include <stdafx.h>
#include <kernel/guest_printf.h>

namespace
{
alignas(16) uint8_t guest[4096]{};
constexpr uint32_t FormatAddress = 0x100;
constexpr uint32_t WideString = 0x400;
constexpr uint32_t NarrowString = 0x500;
constexpr uint32_t Arguments = 0x600;
unsigned failures = 0;
unsigned checks = 0;

void StoreWide(uint32_t address, const char* text)
{
    auto* output = reinterpret_cast<be<uint16_t>*>(guest + address);
    do { *output++ = uint8_t(*text); } while (*text++);
}

void Check(const char* format, bool wide, uint32_t argument, const char* expected)
{
    if (wide) StoreWide(FormatAddress, format);
    else memcpy(guest + FormatAddress, format, strlen(format) + 1);
    *reinterpret_cast<be<uint64_t>*>(guest + Arguments) = argument;
    const auto* encodedFormat = reinterpret_cast<const char*>(guest + FormatAddress);

    // Exercise both exported formatter entry points. The game uses the va_list
    // variant after saving r5 into a big-endian eight-byte register home slot.
    PPCContext ctx{};
    ctx.r5.u64 = argument;
    const std::string results[] = {
        GuestFormatVaList(guest, encodedFormat, Arguments, wide),
        GuestFormat(ctx, guest, encodedFormat, 2, wide)
    };
    for (unsigned i = 0; i < 2; ++i)
    {
        ++checks;
        if (results[i] != expected)
        {
            ++failures;
            fprintf(stderr, "FAIL %s %s %s: got '%s', expected '%s'\n",
                i ? "register" : "va_list", wide ? "wide" : "narrow",
                format, results[i].c_str(), expected);
        }
    }
}
}

int main()
{
    StoreWide(WideString, "int");
    memcpy(guest + NarrowString, "int", 4);

    // Actual resource formats in the battle ring UI. An empty language suffix
    // produces a missing package and the game's fatal dirty-disc path.
    Check("RPMenuResBattle_%s.feelring", true, WideString,
        "RPMenuResBattle_int.feelring");
    Check("RPMenuResBattle_%s.BATTLE-LOG", true, WideString,
        "RPMenuResBattle_int.BATTLE-LOG");

    // Expected argument encodings follow Xenia's xboxkrnl_strings.cc rules:
    // default width follows the function, S inverts it, h/l/w override it.
    for (bool wide : {false, true})
    {
        Check("%s", wide, wide ? WideString : NarrowString, "int");
        Check("%S", wide, wide ? NarrowString : WideString, "int");
        for (const char* format : {"%hs", "%hS"})
            Check(format, wide, NarrowString, "int");
        for (const char* format : {"%ls", "%lS", "%ws", "%wS"})
            Check(format, wide, WideString, "int");
        Check("[%5.2s]", wide, wide ? WideString : NarrowString, "[   in]");
        Check("%s", wide, 0, "(null)");
    }
    printf("Guest printf: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
