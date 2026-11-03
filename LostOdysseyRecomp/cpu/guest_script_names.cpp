// The script dispatcher's lookups of an object by name, without the recompiled call chain around them. sub_8229C490
// takes a record whose +0 is a kind and +4 an 8-bit name, widens the name (sub_8284BF18 -> strlen sub_8229C538 ->
// sub_8229C560 -> RtlMultiByteToUnicodeN), finds the object of that name (sub_8229C638), checks it against a global
// list (sub_8229C6E8) and returns the object's +68. sub_8229C638 makes the name with FName::Init(FNAME_Find, split)
// and scans an array of objects (world +80 +60 +0, array at +696, count at +700) for the first one with that name.
// In a busy street the byte loops, the import bridge and the scan were about 1% of Guest Main; the scan itself is
// bound by its loads, so the gain is the call chain around it.
// sub_8229C490 runs here with the game's loads, stores and calls: FName::Init is still the game's (or its cache in
// guest_names.cpp), called with the registers and stack the guest gives it, and every register it leaves in the
// context is the one the recompiled chain leaves, so callers see no difference. Names of 64 or more bytes (an error
// path in sub_8229C560) go to the game's code, and a list global not created yet to the game's sub_8229C6E8, which
// makes it. sub_8229C638's other callers keep the recompiled code: alone, its native scan was no faster.
// LO_NATIVE_SCRIPT_NAMES=0 keeps the recompiled code; 2 runs both on each call and logs the first differences.
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <cstdlib>
#include <cstring>

extern "C" PPC_FUNC(__imp__sub_8229C490);

namespace
{
    constexpr uint32_t kWorld = 0x83318744;
    constexpr uint32_t kListOwner = 0x83318754;      // sub_8229C768's global, made on first use
    constexpr uint32_t kObjects = 696;
    constexpr uint32_t kObjectCount = 700;
    constexpr uint32_t kObjectNameIndex = 60;
    constexpr uint32_t kObjectNameNumber = 64;
    constexpr uint32_t kObjectChain = 52;            // sub_8229C6E8: first link at +52, next at +60
    constexpr uint32_t kChainNext = 60;
    constexpr uint32_t kResult = 68;
    constexpr uint32_t kFlags = 96;
    constexpr uint32_t kReferencedFlag = 0x10000000;
    constexpr uint32_t kMaxChars = 64;               // sub_8284BF18's buffer, in characters
    constexpr uint32_t kStackChecked = 1024;         // mode 2: the stack below r1 compared

    // The guest frames, as the recompiled functions make them.
    constexpr uint32_t kFrame490 = 224, kBuffer490 = 80;
    constexpr uint32_t kFrame638 = 96, kName638 = 80;
    constexpr uint32_t kFrame8284BF18 = 112, kFrame8229C560 = 96, kFrame6E8 = 96, kFrame768 = 96;
    constexpr uint32_t kReturn8229C4E0 = 0x8229C4E0, kReturn8229C4EC = 0x8229C4EC;
    constexpr uint32_t kReturn8229C65C = 0x8229C65C, kReturn8284BF5C = 0x8284BF5C, kReturn8229C708 = 0x8229C708;

    // 0: recompiled code. 1: native. 2: both, compared.
    int NativeScriptNamesMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_SCRIPT_NAMES");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    uint32_t LoadU32(uint8_t* base, uint32_t address)
    {
        return ByteSwap(*reinterpret_cast<uint32_t*>(base + address));
    }

    void StoreU32(uint8_t* base, uint32_t address, uint32_t value)
    {
        *reinterpret_cast<uint32_t*>(base + address) = ByteSwap(value);
    }

    void StoreU16(uint8_t* base, uint32_t address, uint16_t value)
    {
        *reinterpret_cast<uint16_t*>(base + address) = ByteSwap(value);
    }

    // `std r31,-16(r1)` of a function whose r31 the recompiled code keeps as a zeroed local.
    void StoreZero64(uint8_t* base, uint32_t address)
    {
        std::memset(base + address, 0, 8);
    }

    // A guest frame's back chain and saved return address, so stack walks through the calls below see the guest's frames.
    void PushFrame(uint8_t* base, uint32_t stack, uint32_t size, uint32_t returnAddress)
    {
        StoreU32(base, stack - 8, returnAddress);
        StoreU32(base, stack - size, stack);
    }

    // sub_8229C638: r3 = the name as a wide string. Returns the object in r3 (0 if none) and leaves r5-r10 as the guest
    // does; r4, r11 and r12 are whatever FName::Init leaves.
    void FindObjectByName(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t stack = ctx.r1.u32;
        const uint64_t returnAddress = ctx.lr;
        const uint32_t frame = stack - kFrame638;
        PushFrame(base, stack, kFrame638, uint32_t(returnAddress));

        ctx.r4.u64 = ctx.r3.u64;
        ctx.r7.s64 = 1;
        ctx.r6.s64 = 1;
        ctx.r5.s64 = 0;
        ctx.r3.u64 = frame + kName638;
        ctx.r1.u64 = frame;
        ctx.lr = kReturn8229C65C;
        sub_82296D30(ctx, base);

        // The guest's r5-r9 as the scan leaves them: FName::Init's until the scan loads its own.
        uint64_t r5 = ctx.r5.u64, r6 = ctx.r6.u64, r7 = ctx.r7.u64, r9 = ctx.r9.u64;
        const uint32_t list = LoadU32(base, LoadU32(base, LoadU32(base, LoadU32(base, kWorld) + 80) + 60));
        const uint32_t count = LoadU32(base, list + kObjectCount);
        uint32_t found = 0;
        int32_t index = 0;
        if (int32_t(count) > 0)
        {
            r9 = LoadU32(base, list + kObjects);
            r7 = LoadU32(base, frame + kName638 + 4);
            r6 = LoadU32(base, frame + kName638);
            // r5 is the last word the guest's loop loads: the name of the last object it passes, or its number when
            // the name matched.
            const uint32_t name = uint32_t(r6), number = uint32_t(r7);
            const uint32_t array = uint32_t(r9);
            for (const int32_t total = int32_t(count); index < total; ++index)
            {
                const uint32_t object = LoadU32(base, array + uint32_t(index) * 4);
                if (object == 0)
                    continue;
                r5 = LoadU32(base, object + kObjectNameIndex);
                if (uint32_t(r5) != name)
                    continue;
                r5 = LoadU32(base, object + kObjectNameNumber);
                if (uint32_t(r5) == number)
                {
                    found = object;
                    break;
                }
            }
            r9 = array + uint32_t(index) * 4;
        }

        ctx.r1.u64 = stack;
        ctx.r3.u64 = found;
        ctx.r5.u64 = r5;
        ctx.r6.u64 = r6;
        ctx.r7.u64 = r7;
        ctx.r8.u64 = count;
        ctx.r9.u64 = r9;
        ctx.r10.s64 = index;
        ctx.lr = returnAddress;
    }

    // sub_8229C490 when it runs here; false when the game's code must run it (nothing done yet).
    bool ObjectResultByName(PPCContext& ctx, uint8_t* base, uint32_t& flagsAddress)
    {
        const uint32_t record = ctx.r3.u32;
        const uint32_t kind = LoadU32(base, record);
        const uint32_t stack = ctx.r1.u32;
        const uint64_t returnAddress = ctx.lr;
        const uint32_t source = record + 4;
        const uint32_t length = kind == 0 ? 0 : uint32_t(std::strlen(reinterpret_cast<const char*>(base + source)));
        if (int32_t(length + 1) < 0 || int32_t(length + 1) > int32_t(kMaxChars))
            return false;

        // Every stack word the guest's chain writes, in its order (frames below are re-used by the later calls).
        const uint32_t frame = stack - kFrame490;
        PushFrame(base, stack, kFrame490, uint32_t(returnAddress));
        StoreZero64(base, stack - 16);
        if (kind == 0)
        {
            ctx.r3.s64 = 0;
            return true;
        }

        // sub_8284BF18 into the frame's buffer: its frame, a terminator, sub_8229C560's frame, then
        // RtlMultiByteToUnicodeN's byte-to-char copy of the name and its terminator.
        const uint32_t buffer = frame + kBuffer490;
        const uint32_t frame8284BF18 = frame - kFrame8284BF18;
        StoreU32(base, frame8284BF18, frame);
        StoreU16(base, buffer, 0);
        PushFrame(base, frame8284BF18, kFrame8229C560, kReturn8284BF5C);
        StoreZero64(base, frame8284BF18 - 16);
        const uint8_t* name = base + source;
        for (uint32_t i = 0; i <= length; ++i)
            StoreU16(base, buffer + i * 2, name[i]);

        // The registers that chain leaves: r4 = 128 (the buffer's bytes), r5 = 0, r6 and r9 the name, r7 its size,
        // r8 = 64, r10 = the terminator, r11 the kind, r12 the return address sub_8229C560 saved.
        ctx.r4.s64 = kMaxChars * 2;
        ctx.r5.s64 = 0;
        ctx.r6.u64 = source;
        ctx.r7.u64 = length + 1;
        ctx.r8.s64 = kMaxChars;
        ctx.r9.u64 = source;
        ctx.r10.s64 = 0;
        ctx.r11.u64 = kind;
        ctx.r12.u64 = kReturn8284BF5C;

        ctx.r1.u64 = frame;
        ctx.r3.u64 = buffer;
        ctx.lr = kReturn8229C4E0;
        FindObjectByName(ctx, base);
        uint32_t object = ctx.r3.u32;
        if (object == 0)
        {
            ctx.r1.u64 = stack;
            ctx.lr = returnAddress;
            return true;
        }

        // sub_8229C6E8: the object, if the list owner is on its chain (or there is no owner).
        ctx.r11.u64 = kind;
        const uint32_t owner = LoadU32(base, kListOwner);
        if (owner == 0)
        {
            ctx.lr = kReturn8229C4EC;
            sub_8229C6E8(ctx, base);
            object = ctx.r3.u32;
        }
        else
        {
            // sub_8229C6E8's frame, then sub_8229C768's, as they write them.
            PushFrame(base, frame, kFrame6E8, kReturn8229C4EC);
            StoreZero64(base, frame - 16);
            const uint32_t frame6E8 = frame - kFrame6E8;
            PushFrame(base, frame6E8, kFrame768, kReturn8229C708);
            StoreZero64(base, frame6E8 - 16);
            uint32_t link = LoadU32(base, object + kObjectChain);
            while (link != 0 && link != owner)
                link = LoadU32(base, link + kChainNext);
            if (link == 0)
                object = 0;
        }

        if (kind - 1 <= 2 && LoadU32(base, object + kResult) == 0)
        {
            const uint32_t flags = LoadU32(base, object + kFlags);
            ctx.r10.u64 = flags & kReferencedFlag;
            if ((flags & kReferencedFlag) == 0)
            {
                flagsAddress = object + kFlags;
                StoreU32(base, object + kFlags, flags | kReferencedFlag);
            }
        }
        ctx.r3.u64 = LoadU32(base, object + kResult);
        ctx.r1.u64 = stack;
        ctx.lr = returnAddress;
        return true;
    }

    const char* const kRegisterNames[] = { "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12" };

    // Mode 2: what the native code left against what the recompiled code leaves (r3-r12, r1, lr, the flag word and
    // the stack below r1; `stackDifference` is the first differing byte's offset below r1, 0 if none).
    void CheckRegisters(const char* name, const PPCContext& native, const PPCContext& guest, uint32_t record,
        uint32_t nativeFlags, uint32_t guestFlags, uint32_t stackDifference)
    {
        static uint32_t calls, differences;
        const uint64_t a[] = { native.r3.u64, native.r4.u64, native.r5.u64, native.r6.u64, native.r7.u64,
            native.r8.u64, native.r9.u64, native.r10.u64, native.r11.u64, native.r12.u64 };
        const uint64_t b[] = { guest.r3.u64, guest.r4.u64, guest.r5.u64, guest.r6.u64, guest.r7.u64,
            guest.r8.u64, guest.r9.u64, guest.r10.u64, guest.r11.u64, guest.r12.u64 };
        ++calls;
        bool same = native.r1.u64 == guest.r1.u64 && native.lr == guest.lr && nativeFlags == guestFlags && stackDifference == 0;
        for (int i = 0; i < 10 && same; ++i)
            same = uint32_t(a[i]) == uint32_t(b[i]);
        if (!same && ++differences <= 20)
        {
            for (int i = 0; i < 10; ++i)
                if (uint32_t(a[i]) != uint32_t(b[i]))
                    LOG_ERROR("[script names] {} differs for {:08X}: {} native {:08X}, guest {:08X}", name, record,
                        kRegisterNames[i], uint32_t(a[i]), uint32_t(b[i]));
            if (native.r1.u64 != guest.r1.u64 || native.lr != guest.lr || nativeFlags != guestFlags)
                LOG_ERROR("[script names] {} differs for {:08X}: r1 {:08X}/{:08X} lr {:08X}/{:08X} flags {:08X}/{:08X}",
                    name, record, native.r1.u32, guest.r1.u32, uint32_t(native.lr), uint32_t(guest.lr), nativeFlags, guestFlags);
            if (stackDifference != 0)
                LOG_ERROR("[script names] {} differs for {:08X}: stack at r1-{}", name, record, stackDifference);
        }
        if ((calls & 0xFFFFF) == 0)
            LOG_ERROR("[script names] {} checked {} calls, {} differ", name, calls, differences);
    }
}

PPC_FUNC(sub_8229C490)
{
    const int mode = NativeScriptNamesMode();
    if (mode == 0)
    {
        __imp__sub_8229C490(ctx, base);
        return;
    }
    if (mode == 2)
    {
        // FName::Init's cache (guest_names.cpp) decides which registers the call leaves, and a first run warms it for
        // the second; so the native code runs once to warm it, is undone, runs again, is undone, and then the game's
        // code runs from the same state. Before the owner list exists the native run would make it for the guest's;
        // the game's code then runs alone.
        const PPCContext before = ctx;
        const uint32_t low = before.r1.u32 - kStackChecked;
        uint8_t stackBefore[kStackChecked], stackNative[kStackChecked];
        std::memcpy(stackBefore, base + low, kStackChecked);
        uint32_t flagsAddress = 0;
        if (LoadU32(base, kListOwner) == 0 || !ObjectResultByName(ctx, base, flagsAddress))
        {
            ctx = before;
            __imp__sub_8229C490(ctx, base);
            return;
        }
        // Undo the native flag store and stack writes, so the next run decides them again.
        auto undo = [&]()
            {
                if (flagsAddress != 0)
                    StoreU32(base, flagsAddress, LoadU32(base, flagsAddress) & ~kReferencedFlag);
                std::memcpy(base + low, stackBefore, kStackChecked);
                ctx = before;
            };
        undo();
        flagsAddress = 0;
        ObjectResultByName(ctx, base, flagsAddress);
        const PPCContext native = ctx;
        const uint32_t nativeFlags = flagsAddress != 0 ? LoadU32(base, flagsAddress) : 0;
        std::memcpy(stackNative, base + low, kStackChecked);
        undo();
        __imp__sub_8229C490(ctx, base);
        const uint32_t guestFlags = flagsAddress != 0 ? LoadU32(base, flagsAddress) : 0;
        uint32_t stackDifference = 0;
        for (uint32_t i = kStackChecked; i-- > 0;)
            if (stackNative[i] != base[low + i])
            {
                stackDifference = kStackChecked - i;
                break;
            }
        CheckRegisters("sub_8229C490", native, ctx, before.r3.u32, nativeFlags, guestFlags, stackDifference);
        return;
    }

    uint32_t flagsAddress = 0;
    const uint32_t record = ctx.r3.u32;
    if (!ObjectResultByName(ctx, base, flagsAddress))
    {
        ctx.r3.u64 = record;
        __imp__sub_8229C490(ctx, base);
    }
}
