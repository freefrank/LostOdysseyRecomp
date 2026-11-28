#include <os/platform.h>
#if LO_PLATFORM_APPLE
#include "guest_address_space_layout.h"
#include <cerrno>
#include <csignal>
#include <mach/mach.h>
#if LO_PLATFORM_IOS
// The iOS SDK has no mach_vm.h; vm_remap takes the same 64-bit arguments there.
// The 4 GiB reservation below needs the extended virtual addressing entitlement.
#include <mach/vm_map.h>
#else
#include <mach/mach_vm.h>
#endif
#include <sys/mman.h>
#include <unistd.h>

// macOS backend. Apple Silicon uses 16 KiB pages, so a mapping's host address
// and backing offset must both be 16 KiB aligned. The A and C views alias the
// physical backing exactly, but the E view (A shifted by 4 KiB) cannot be
// expressed with page mappings at any base. The runtime never hands out E
// addresses (physical allocations come from A; MmGetPhysicalAddress applies
// the shift arithmetically), so E stays inaccessible and a fault probe reports
// any guest access to it. Whether the game itself uses E decides whether a
// translation layer is needed.
namespace GuestAddressSpace
{
static uint8_t* s_probeBase = nullptr;
static struct sigaction s_previousSegv{};
static struct sigaction s_previousBus{};

static void WriteHex(char* out, uint64_t value, int digits)
{
    for (int i = digits - 1; i >= 0; --i, value >>= 4)
        out[i] = "0123456789ABCDEF"[value & 0xF];
}

static uint64_t FaultPc(const void* context)
{
    const auto* machine = static_cast<const ucontext_t*>(context)->uc_mcontext;
    if (!machine)
        return 0;
#if defined(__aarch64__)
    return uint64_t(__darwin_arm_thread_state64_get_pc(machine->__ss));
#else
    return uint64_t(machine->__ss.__rip);
#endif
}

// Async-signal-safe: a fixed buffer, write(2) and sigaction(2) only.
static void ProbeHandler(int signal, siginfo_t* info, void* context)
{
    const auto address = reinterpret_cast<uintptr_t>(info->si_addr);
    const auto begin = reinterpret_cast<uintptr_t>(s_probeBase) + kStarts[3];
    if (s_probeBase && address >= begin && address < begin + kSizes[3])
    {
        static constexpr char kGuest[] = "guest memory: access to unsupported E view at guest 0x";
        static constexpr char kPc[] = ", host pc 0x";
        char message[sizeof(kGuest) - 1 + 8 + sizeof(kPc) - 1 + 16 + 1];
        char* out = message;
        for (char c : kGuest) if (c) *out++ = c;
        WriteHex(out, uint32_t(address - reinterpret_cast<uintptr_t>(s_probeBase)), 8);
        out += 8;
        for (char c : kPc) if (c) *out++ = c;
        WriteHex(out, FaultPc(context), 16);
        out += 16;
        *out++ = '\n';
        (void)write(STDERR_FILENO, message, size_t(out - message));
        // Return into the default action so the process still crashes.
        struct sigaction fallback{};
        fallback.sa_handler = SIG_DFL;
        sigaction(signal, &fallback, nullptr);
        return;
    }
    // Not ours: reinstate the previous disposition; the faulting instruction
    // re-executes and is delivered to it.
    sigaction(signal, signal == SIGBUS ? &s_previousBus : &s_previousSegv, nullptr);
}

static void InstallProbe(uint8_t* base)
{
    s_probeBase = base;
    struct sigaction action{};
    action.sa_sigaction = ProbeHandler;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    // PROT_NONE faults arrive as SIGBUS on macOS; handle SIGSEGV as well.
    sigaction(SIGBUS, &action, &s_previousBus);
    sigaction(SIGSEGV, &action, &s_previousSegv);
}

uint8_t* Allocate()
{
    ClearFailure();
    // Reserve the whole 4 GiB guest space inaccessible. The kernel places it
    // anywhere: the executable itself occupies 0x100000000 on macOS, and no
    // runtime code depends on a particular base.
    auto* base = static_cast<uint8_t*>(mmap(nullptr, kSize, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0));
    if (base == MAP_FAILED)
    {
        RecordFailure(FailureOperation::ReserveAny, uint32_t(errno), -1, nullptr, kSize);
        return nullptr;
    }

    // The virtual view has no aliases; private anonymous memory suffices.
    if (mmap(base + kStarts[0], kSizes[0], PROT_READ | PROT_WRITE,
             MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0) == MAP_FAILED)
    {
        RecordFailure(FailureOperation::MapView, uint32_t(errno), 0, base + kStarts[0], kSizes[0], kOffsets[0]);
        munmap(base, kSize);
        return nullptr;
    }

    // The A view is the physical backing store; C is a shared Mach VM alias
    // of it, so a write through either is visible through the other.
    if (mmap(base + kStarts[1], kSizes[1], PROT_READ | PROT_WRITE,
             MAP_ANON | MAP_SHARED | MAP_FIXED, -1, 0) == MAP_FAILED)
    {
        RecordFailure(FailureOperation::CreateBacking, uint32_t(errno), 1, base + kStarts[1], kSizes[1], kOffsets[1]);
        munmap(base, kSize);
        return nullptr;
    }
#if LO_PLATFORM_IOS
    using RemapAddress = vm_address_t;
    static_assert(sizeof(vm_address_t) == 8, "vm_remap needs 64-bit addresses");
#define LO_VM_REMAP vm_remap
#else
    using RemapAddress = mach_vm_address_t;
#define LO_VM_REMAP mach_vm_remap
#endif
    auto alias = RemapAddress(base + kStarts[2]);
    vm_prot_t current = VM_PROT_NONE, maximum = VM_PROT_NONE;
    const kern_return_t remapped = LO_VM_REMAP(mach_task_self(), &alias, kSizes[2], 0,
        VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, mach_task_self(), RemapAddress(base + kStarts[1]),
        FALSE, &current, &maximum, VM_INHERIT_NONE);
#undef LO_VM_REMAP
    if (remapped != KERN_SUCCESS || alias != RemapAddress(base + kStarts[2]))
    {
        RecordFailure(FailureOperation::MapView, uint32_t(remapped), 2, base + kStarts[2], kSizes[2], kOffsets[2]);
        munmap(base, kSize);
        return nullptr;
    }

    // Guest null page; the kernel rounds this up to one 16 KiB host page, which
    // is still below the guest's first virtual allocation at 0x100000.
    if (mprotect(base, 4096, PROT_NONE) != 0)
    {
        RecordFailure(FailureOperation::ProtectNull, uint32_t(errno), -1, base, 4096);
        munmap(base, kSize);
        return nullptr;
    }

    InstallProbe(base);
    return base;
}

void Release(uint8_t* base)
{
    if (!base)
        return;
    if (s_probeBase == base)
        s_probeBase = nullptr;
    munmap(base, kSize);
}
}
#endif
