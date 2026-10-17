// Exercise production publication guards without a game image or kernel runtime.
#include <stdafx.h>
#include <kernel/memory.h>
#include <cctype>
#include <cstdlib>
#include "../../LostOdysseyRecomp/patches/save_worker.cpp"

Memory::Memory() = default;
Memory g_memory;

namespace
{
void Check(bool ok, const char* message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

struct RequestSite { uint32_t begin; uint32_t end; };
constexpr RequestSite requestSites[] = {
    {0x82DCC0A0, 0x82DCC0DC}, // Content open.
    {0x82DCC138, 0x82DCC178}, // Save-file write.
    {0x82DCC1E0, 0x82DCC21C}, // Save-file read.
    {0x82DCC51C, 0x82DCC55C}, // Content close, after synchronous flush/close.
};

struct RequestFixture
{
    std::mutex criticalSection;
    std::binary_semaphore preparing{0}, prepared{0}, published{0}, attempted{0};
    std::atomic<bool> requestHoldsLock{false};
    std::atomic<uint32_t> status{0};
    bool completedBeforeBusy = false;
    bool preparedOutsideLock = false;
    unsigned enterCalls = 0, leaveCalls = 0;
    uint32_t manager;
    alignas(64) uint8_t memory[64]{};
    PPCRegister managerRegister{};

    explicit RequestFixture(unsigned entry) : manager(0x400000 + entry * 0x10000)
    {
        // Nonzero upper bits catch accidental modification of the source r31.
        managerRegister.u64 = 0x5A5A5A5A00000000ull | manager;
    }

    void Complete()
    {
        preparing.acquire();
        {
            // Simulated preparation can depend on worker progress. It finishes
            // before the publication guard takes the manager lock.
            std::lock_guard lock(criticalSection);
            preparedOutsideLock = !requestHoldsLock.load();
        }
        prepared.release();
        published.acquire();
        // Force completion between the flag store and final busy store, with
        // deterministic handshakes instead of sleeps or try_lock results.
        if (!requestHoldsLock.load())
        {
            std::lock_guard lock(criticalSection);
            completedBeforeBusy = status.load() != 2;
            status.store(3);
            attempted.release();
        }
        else
        {
            attempted.release();
            std::lock_guard lock(criticalSection);
            completedBeforeBusy = status.load() != 2;
            status.store(3);
        }
    }
};
RequestFixture* fixture = nullptr;

void CheckRequest(unsigned entry, bool protectedRequest)
{
    RequestFixture run(entry);
    fixture = &run;
    g_memory.base = run.memory;
    const PPCRegister originalManager = run.managerRegister;
    std::thread worker([&] { run.Complete(); });

    // API work precedes the configured Begin boundary, including the close
    // request's synchronous XamContentFlush/XamContentClose calls.
    Check(run.enterCalls == 0, "preparation precedes manager-lock acquisition");
    run.preparing.release();
    run.prepared.acquire();
    Check(run.preparedOutsideLock, "preparation permits worker progress without the manager lock");
    if (protectedRequest)
    {
        SaveWorkerRequestBegin(run.managerRegister);
        Check(std::memcmp(&run.managerRegister, &originalManager, sizeof(originalManager)) == 0,
            "Begin preserves the incoming r31 register");
    }
    run.published.release();
    run.attempted.acquire();
    run.status.store(2);
    if (protectedRequest)
    {
        SaveWorkerRequestEnd(run.managerRegister);
        Check(std::memcmp(&run.managerRegister, &originalManager, sizeof(originalManager)) == 0,
            "End preserves the incoming r31 register");
    }
    worker.join();
    if (protectedRequest)
    {
        Check(run.enterCalls == 1 && run.leaveCalls == 1,
            "publication acquires and releases exactly one manager lock");
        Check(!run.completedBeforeBusy, "worker cannot complete a partially published request");
        Check(run.status.load() == 3, "completed status survives the request's final busy store");
    }
    else
    {
        Check(run.enterCalls == 0 && run.leaveCalls == 0, "unprotected control does not use guards");
        Check(run.completedBeforeBusy && run.status.load() == 2,
            "unprotected instruction order loses an early worker completion");
    }
    fixture = nullptr;
    g_memory.base = nullptr;
}

std::string Trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

void CheckConfiguredBoundaries()
{
    const auto configPath = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
        / "LostOdysseyRecompLib/config/LostOdysseyRecomp.toml";
    std::ifstream config(configPath);
    Check(config.is_open(), "production recompiler configuration is available");
    std::array<unsigned, std::size(requestSites)> begins{}, ends{};
    std::string name, registers, afterInstruction, line;
    uint32_t address = 0;
    const auto finishHook = [&]
    {
        const bool begin = name == "\"SaveWorkerRequestBegin\"";
        const bool end = name == "\"SaveWorkerRequestEnd\"";
        if (!begin && !end) return;
        Check(registers == "[\"r31\"]", "publication hooks receive only the manager register r31");
        Check(afterInstruction == "false", "publication boundaries run before their instruction");
        bool found = false;
        for (size_t i = 0; i < std::size(requestSites); ++i)
        {
            if (address == (begin ? requestSites[i].begin : requestSites[i].end))
            {
                ++(begin ? begins[i] : ends[i]);
                found = true;
            }
        }
        Check(found, "configured hook surrounds publication and excludes preparation I/O");
    };
    while (std::getline(config, line))
    {
        line = Trim(line.substr(0, line.find('#')));
        if (line.starts_with('['))
        {
            finishHook();
            name.clear(); registers.clear(); afterInstruction.clear(); address = 0;
        }
        const auto equal = line.find('=');
        if (equal == std::string::npos) continue;
        const auto key = Trim(line.substr(0, equal));
        auto value = Trim(line.substr(equal + 1));
        if (key == "name") name = value;
        else if (key == "address") address = uint32_t(std::stoul(value, nullptr, 0));
        else if (key == "registers")
        {
            std::erase_if(value, [](unsigned char c) { return std::isspace(c); });
            registers = value;
        }
        else if (key == "after_instruction") afterInstruction = value;
    }
    finishHook();
    for (size_t i = 0; i < std::size(requestSites); ++i)
        Check(begins[i] == 1 && ends[i] == 1, "each request has exactly one Begin/End publication pair");
}
}

PPC_FUNC(__imp__RtlEnterCriticalSection)
{
    Check(base == fixture->memory, "Begin passes the current guest memory base");
    Check(ctx.r3.u32 == fixture->manager + 2908, "Begin locks this manager's worker critical section");
    fixture->criticalSection.lock();
    fixture->requestHoldsLock.store(true);
    ++fixture->enterCalls;
    // Imports may clobber their isolated call context, never the source r31.
    std::memset(&ctx, 0xCC, sizeof(ctx));
}
PPC_FUNC(__imp__RtlLeaveCriticalSection)
{
    Check(base == fixture->memory, "End passes the current guest memory base");
    Check(ctx.r3.u32 == fixture->manager + 2908, "End unlocks the same manager critical section");
    fixture->requestHoldsLock.store(false);
    ++fixture->leaveCalls;
    fixture->criticalSection.unlock();
    std::memset(&ctx, 0xDD, sizeof(ctx));
}

int main()
{
    CheckConfiguredBoundaries();
    for (unsigned entry = 0; entry < std::size(requestSites); ++entry)
    {
        CheckRequest(entry, false);
        CheckRequest(entry, true);
    }
    std::puts("PASS: 4 unprotected publications lose completion; 4 production guards retain completion, preserve r31, and exclude preparation I/O");
}
