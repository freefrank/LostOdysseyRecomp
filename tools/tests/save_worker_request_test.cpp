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
    {0x82DCC0A0, 0x82DCC0DC},
    {0x82DCC138, 0x82DCC178},
    {0x82DCC1E0, 0x82DCC21C},
    {0x82DCC51C, 0x82DCC55C},
};

struct RequestFixture
{
    std::binary_semaphore preparing{0}, prepared{0}, published{0}, attempted{0};
    std::atomic<uint32_t> status{0};
    bool completedBeforeBusy = false, preparedOutsideLock = false;
    uint32_t manager, mainPcr, workerPcr;
    alignas(64) uint8_t memory[4096]{};

    explicit RequestFixture(unsigned entry)
        : manager(64 + entry * 16), mainPcr(0x302000 + entry * 0x1000), workerPcr(0xCF0000 + entry * 0x1000) {}

    XRTL_CRITICAL_SECTION& Section()
    {
        return *reinterpret_cast<XRTL_CRITICAL_SECTION*>(memory + manager + 2908);
    }
    uint32_t Owner() { return GuestCriticalSection::Encode(Section().OwningThread); }
    void Enter() { GuestCriticalSection::Enter(Section().RecursionCount, Section().OwningThread, workerPcr); }
    void Leave() { GuestCriticalSection::Leave(Section().RecursionCount, Section().OwningThread); }

    void Complete(bool protectedRequest)
    {
        preparing.acquire();
        preparedOutsideLock = Owner() == 0;
        Enter();
        Leave();
        prepared.release();
        published.acquire();
        if (protectedRequest)
        {
            Check(Owner() == mainPcr, "publication uses the current guest PCR as lock owner");
            attempted.release();
        }
        Enter();
        completedBeforeBusy = status.load() != 2;
        status.store(3);
        Leave();
        if (!protectedRequest) attempted.release();
    }
};

void CheckRequest(unsigned entry, bool protectedRequest)
{
    RequestFixture run(entry);
    g_memory.base = run.memory;
    PPCContext ctx;
    std::memset(&ctx, 0x5A, sizeof(ctx));
    ctx.r13.u64 = run.mainPcr;
    ctx.r31.u64 = 0x5A5A5A5A00000000ull | run.manager;
    PPCContext original;
    std::memcpy(&original, &ctx, sizeof(ctx));
    SetPPCContext(ctx);
    std::thread worker([&] { run.Complete(protectedRequest); });

    // The configured boundaries leave all preparation I/O outside the lock.
    run.preparing.release();
    run.prepared.acquire();
    Check(run.preparedOutsideLock, "preparation allows worker progress without the manager lock");
    if (protectedRequest) SaveWorkerRequestBegin(ctx.r31);
    run.published.release();
    run.attempted.acquire();
    run.status.store(2);
    if (protectedRequest) SaveWorkerRequestEnd(ctx.r31);
    worker.join();

    Check(std::memcmp(&ctx, &original, sizeof(ctx)) == 0, "publication guards preserve the complete guest context");
    Check(run.Owner() == 0 && run.Section().RecursionCount == 0, "publication releases the shared critical section completely");
    Check(protectedRequest ? (!run.completedBeforeBusy && run.status.load() == 3)
                           : (run.completedBeforeBusy && run.status.load() == 2),
        "protected publication retains completion; original order loses it");
    g_ppcContext = nullptr;
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

int main()
{
    CheckConfiguredBoundaries();
    for (unsigned entry = 0; entry < std::size(requestSites); ++entry)
    {
        CheckRequest(entry, false);
        CheckRequest(entry, true);
    }
    std::puts("PASS: 4 unsafe publications lose completion; 4 direct production guards retain completion and context, exclude preparation I/O, and release the shared lock");
}
