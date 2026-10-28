#include <stdafx.h>
#include <kernel/guest_address_space.h>
#include <os/logger.h>
#if LO_PLATFORM_SWITCH
#include <os/switch_platform.h>
#endif
#include <atomic>

// Entering the city of Uhra, sub_82B6E908 reads a byte through the pointer at
// offset 0 of its object (r3), and that pointer leads into physical memory
// the game never allocated (0xA11C40FC on two consoles, two game editions).
// The desktop builds map the whole guest window and read zeros there; the
// Switch backs only allocated pages, so the read faulted. The page behind the
// pointer is now backed with zeros before the original runs, which gives the
// same result as the desktop builds.

extern "C" PPC_FUNC(__imp__sub_82B6E908);

#if LO_PLATFORM_SWITCH
namespace
{
// Written synchronously (an error): the run that needs it may end right after.
void LogBackingFailure(const char* what, uint32_t address)
{
    static std::atomic<uint32_t> logged{0};
    if (logged.fetch_add(1, std::memory_order_relaxed) >= 4)
        return;
    const auto failure = GuestAddressSpace::GetFailureInfo();
    LOG_ERROR("guest memory: sub_82B6E908 {} {:#x} is unallocated and could not be backed ({} failed: error {:#x}, "
              "address {:#x}, size {:#x}, offset {:#x}, view {}); skipping the function. committed {} MiB, {}",
        what, address, GuestAddressSpace::FailureOperationName(failure.operation), failure.error, failure.address,
        failure.size, failure.offset, failure.viewIndex, GuestAddressSpace::CommittedBytes() >> 20,
        os::switch_platform::MemorySummary());
    if (GuestAddressSpace::LastRepair().count != 0)
        GuestAddressSpace::LogRepair("sub_82B6E908 (failed repair)");
}
}

void GuestAddressSpace::LogRepair(const char* who)
{
    static std::atomic<uint32_t> logged{0};
    if (logged.fetch_add(1, std::memory_order_relaxed) >= 16)
        return;
    const auto repair = GuestAddressSpace::LastRepair();
    LOG_ERROR("guest memory: {} read committed address {:#x} but the kernel had nothing mapped there "
              "(type {:#x} attribute {:#x} permission {:#x}, block {:#x}+{:#x}; C alias type {:#x} permission {:#x}; "
              "source {:#x} type {:#x} permission {:#x}); mapped {:#x}+{:#x} again: result {:#x}, repair {}; chunk now: {}",
        who, repair.address, repair.type, repair.attribute, repair.permission, repair.regionStart, repair.regionSize,
        repair.aliasType, repair.aliasPermission, repair.source, repair.sourceType, repair.sourcePermission,
        repair.mappedStart, repair.mappedSize, repair.result, repair.count, repair.layout);
}
#endif

PPC_FUNC(sub_82B6E908)
{
#if LO_PLATFORM_SWITCH
    using GuestAddressSpace::Backing;
    const uint32_t object = ctx.r3.u32;
    if (object >= 0x1000)
    {
        const auto objectBacking = GuestAddressSpace::EnsureBacked(object, 32);
        if (objectBacking == Backing::Failed)
        {
            LogBackingFailure("object", object);
            return;
        }
        if (objectBacking == Backing::Repaired)
            GuestAddressSpace::LogRepair("sub_82B6E908 object");
        const uint32_t pointer = PPC_LOAD_U32(object);
        static std::atomic<uint32_t> logged{0};
        const auto backing = pointer >= 0x1000 ? GuestAddressSpace::EnsureBacked(pointer, 32) : Backing::AlreadyBacked;
        if (backing == Backing::Failed)
        {
            // The PC reads zeros there; without a page to read, leave the result (a clamped byte sum
            // at object+22) as it was rather than fault.
            LogBackingFailure("pointer", pointer);
            return;
        }
        if (backing == Backing::NewlyBacked && logged.fetch_add(1, std::memory_order_relaxed) < 8)
            LOG_WARNING("guest memory: sub_82B6E908 object {:#x} points at unallocated {:#x}: backed with zeros, as on PC",
                        object, pointer);
        if (backing == Backing::Repaired)
            GuestAddressSpace::LogRepair("sub_82B6E908");
    }
#endif
    __imp__sub_82B6E908(ctx, base);
}
