#include <stdafx.h>
#include "command_processor.h"
#include "video.h"
#include <cpu/guest_thread.h>
#include <kernel/memory.h>
#include <kernel/function.h>
#include <os/logger.h>
#include <set>
#include <mutex>

void DumpGuestThreadStates();

namespace gpu
{
    CommandProcessor g_commandProcessor;
    static std::atomic<uint32_t> g_swapCount{ 0 };
    static std::atomic<uint32_t> g_traceBudget{ 0 };

    // Ring of recently executed packets, dumped when the parser derails.
    struct PacketRecord { uint32_t header; uint32_t offset; uint32_t d0, d1, d2; bool ring; };
    static PacketRecord g_history[64];
    static uint32_t g_historyPos = 0;

    static void DumpHistory(const char* why)
    {
        LOG_WARNING("packet history ({}):", why);
        for (uint32_t i = 0; i < 64; i++)
        {
            auto& r = g_history[(g_historyPos + i) % 64];
            if (r.header == 0 && r.d0 == 0 && r.d1 == 0)
                continue;
            uint32_t type = r.header >> 30;
            uint32_t op = (r.header >> 8) & 0x7F;
            uint32_t count = ((r.header >> 16) & 0x3FFF) + 1;
            LOG_WARNING("  {} @{:#x} hdr={:#010x} type={} op={:#x} count={} [{:#x} {:#x} {:#x}]",
                r.ring ? "ring" : "ib  ", r.offset, r.header, type, op, count, r.d0, r.d1, r.d2);
        }
    }

    // -----------------------------------------------------------------------
    // Draw statistics and shader capture (LO_GPU_STATS=1, LO_SHADER_DUMP_DIR).
    // Purely diagnostic: tells us what the title screen actually draws before
    // a renderer exists.
    // -----------------------------------------------------------------------
    struct FrameStats
    {
        uint32_t draws = 0, indexed = 0, autoIndex = 0, copies = 0, shaderLoads = 0;
        uint32_t prim[64] = {};
        uint32_t constantWrites = 0;
    };
    static FrameStats g_frame;
    static uint64_t g_activeShader[2] = {};      // [0]=vertex [1]=pixel
    static uint32_t g_activeShaderSize[2] = {};
    static std::mutex g_shaderMutex;
    static std::set<uint64_t> g_seenShaders;
    static const bool g_gpuStats = getenv("LO_GPU_STATS") != nullptr;
    static uint32_t g_detailBudget = 0;

    static uint64_t HashWords(const uint32_t* words, uint32_t count)
    {
        uint64_t h = 0xcbf29ce484222325ull;
        for (uint32_t i = 0; i < count; i++)
        {
            h ^= words[i];
            h *= 0x100000001b3ull;
        }
        return h;
    }

    // words point at big-endian microcode in guest memory.
    static void CaptureShader(uint32_t type, const uint32_t* words, uint32_t count)
    {
        if (type > 1 || count == 0 || count > 0x10000)
            return;
        uint64_t hash = HashWords(words, count);
        g_activeShader[type] = hash;
        g_activeShaderSize[type] = count;
        g_frame.shaderLoads++;

        std::lock_guard lock(g_shaderMutex);
        if (!g_seenShaders.insert(hash).second)
            return;
        if (g_gpuStats)
            LOG_INFO("new {} shader {:016x} ({} dwords), {} distinct so far", type ? "pixel" : "vertex", hash, count, g_seenShaders.size());
        if (const char* dir = getenv("LO_SHADER_DUMP_DIR"))
        {
            std::string path = fmt::format("{}/{}_{:016x}.bin", dir, type ? "ps" : "vs", hash);
            if (FILE* f = fopen(path.c_str(), "wb"))
            {
                fwrite(words, 4, count, f);
                fclose(f);
            }
        }
    }

    namespace
    {
        enum Type3Opcode : uint32_t
        {
            PM4_ME_INIT = 0x48,
            PM4_NOP = 0x10,
            PM4_INDIRECT_BUFFER = 0x3f,
            PM4_INDIRECT_BUFFER_PFD = 0x37,
            PM4_WAIT_FOR_IDLE = 0x26,
            PM4_WAIT_REG_MEM = 0x3c,
            PM4_REG_RMW = 0x21,
            PM4_REG_TO_MEM = 0x3e,
            PM4_MEM_WRITE = 0x3d,
            PM4_COND_WRITE = 0x45,
            PM4_EVENT_WRITE = 0x46,
            PM4_EVENT_WRITE_SHD = 0x58,
            PM4_EVENT_WRITE_EXT = 0x5a,
            PM4_EVENT_WRITE_ZPD = 0x5b,
            PM4_INTERRUPT = 0x54,
            PM4_XE_SWAP = 0x64,
            PM4_SET_BIN_MASK_LO = 0x60,
            PM4_SET_BIN_MASK_HI = 0x61,
            PM4_SET_BIN_SELECT_LO = 0x62,
            PM4_SET_BIN_SELECT_HI = 0x63,
            PM4_SET_BIN_MASK = 0x50,
            PM4_SET_BIN_SELECT = 0x51,
            PM4_CONTEXT_UPDATE = 0x5e,
            PM4_DRAW_INDX = 0x22,
            PM4_DRAW_INDX_2 = 0x36,
            PM4_IM_LOAD = 0x27,
            PM4_IM_LOAD_IMMEDIATE = 0x2B,
            PM4_SET_CONSTANT = 0x2D,
            PM4_SET_CONSTANT2 = 0x55,
            PM4_LOAD_ALU_CONSTANT = 0x2F,
            PM4_SET_SHADER_CONSTANTS = 0x56,
            PM4_INVALIDATE_STATE = 0x3B,
        };

        constexpr uint32_t kSwapSignature = 0x53574150; // 'SWAP'

        uint32_t GpuSwap(uint32_t value, uint32_t endian)
        {
            switch (endian & 3)
            {
            case 1: return ((value & 0xFF00FF00u) >> 8) | ((value & 0x00FF00FFu) << 8);   // 8in16
            case 2: return ByteSwap(value);                                                // 8in32
            case 3: return (value >> 16) | (value << 16);                                  // 16in32
            default: return value;
            }
        }
    }

    uint32_t CommandProcessor::Reader::ReadAndSwap()
    {
        if (!ring && readOffset >= size)
            return 0;
        uint32_t v = ByteSwap(*reinterpret_cast<uint32_t*>(base + readOffset));
        readOffset += 4;
        if (ring && readOffset >= size)
            readOffset -= size;
        return v;
    }

    void CommandProcessor::Reader::Advance(uint32_t dwords)
    {
        readOffset += dwords * 4;
        if (ring)
            while (readOffset >= size)
                readOffset -= size;
    }

    uint8_t* CommandProcessor::TranslatePhysical(uint32_t physicalAddress)
    {
        // Physical allocations live at 0xA0000000 + physical (see MmGetPhysicalAddress).
        return static_cast<uint8_t*>(g_memory.Translate(0xA0000000u + (physicalAddress & 0x1FFFFFFF)));
    }

    void CommandProcessor::Init()
    {
        m_registers.assign(REGISTER_COUNT, 0);

        // Registers the guest reads back through plain loads: keep the MMIO
        // window populated with big-endian values (Xenia ReadRegister defaults).
        auto seed = [&](uint32_t index, uint32_t value)
        {
            m_registers[index] = value;
            *reinterpret_cast<be<uint32_t>*>(g_memory.Translate(MMIO_BASE + index * 4)) = value;
        };
        seed(REG_RB_EDRAM_TIMING, 0x08100748);
        seed(REG_RB_BC_CONTROL, 0x0000200E);
        seed(REG_D1MODE_V_COUNTER, 0x000002D0);
        seed(REG_INTERRUPT_STATUS, 1);
        seed(REG_D1MODE_VIEWPORT_SIZE, 0x050002D0);

        m_running = true;
        m_worker = std::thread([this] { WorkerMain(); });
        m_vsync = std::thread([this] { VsyncMain(); });
        m_interruptThread = std::thread([this] { InterruptMain(); });
    }

    void CommandProcessor::Shutdown()
    {
        m_running = false;
        m_writePtrIndex.notify_all();
        m_interruptSignal++;
        m_interruptSignal.notify_all();
        for (auto* t : { &m_worker, &m_vsync, &m_interruptThread })
            if (t->joinable())
                t->join();
    }

    void CommandProcessor::InitializeRingBuffer(uint32_t physicalAddress, uint32_t sizeLog2)
    {
        m_primaryBufferPhysical = physicalAddress;
        // size_log2 counts 8-byte units (Xenia: 1 << (size_log2 + 3)); D3D's
        // 4 KiB "log2 = 12" ring is really 32 KiB and WPTR runs to 0x2000.
        m_primaryBufferSize = 1u << (sizeLog2 + 3);
        m_readPtrIndex = 0;
        LOG_INFO("ring buffer at physical {:#x} size {:#x}", physicalAddress, m_primaryBufferSize);
    }

    void CommandProcessor::EnableReadPointerWriteBack(uint32_t physicalAddress, uint32_t blockSizeLog2)
    {
        m_readPtrWritebackPhysical = physicalAddress;
    }

    void CommandProcessor::SetInterruptCallback(uint32_t callback, uint32_t userData)
    {
        m_interruptCallback = callback;
        m_interruptUserData = userData;
    }

    void CommandProcessor::UpdateWritePointer(uint32_t dwordIndex)
    {
        static const bool traceIb = getenv("LO_TRACE_IB") != nullptr;
        if (traceIb)
            LOG_INFO("swap#{} WPTR <- {:#x} (rd {:#x})", g_swapCount.load(), dwordIndex, m_readPtrIndex);
        m_writePtrIndex = dwordIndex;
        m_writePtrIndex.notify_all();
    }

    void CommandProcessor::WriteRegister(uint32_t index, uint32_t value)
    {
        if (index >= REGISTER_COUNT)
            return;

        // Read-only status registers: the interrupt handler acknowledges the
        // vblank by writing here, but Xenia's ReadRegister always reports the
        // constant, so keep ours constant too.
        switch (index)
        {
        case REG_RB_EDRAM_TIMING:
        case REG_RB_BC_CONTROL:
        case REG_D1MODE_V_COUNTER:
        case REG_INTERRUPT_STATUS:
        case REG_D1MODE_VIEWPORT_SIZE:
            *reinterpret_cast<be<uint32_t>*>(g_memory.Translate(MMIO_BASE + index * 4)) = m_registers[index];
            return;
        default:
            break;
        }

        m_registers[index] = value;

        // Guest code reads registers back with plain loads from the MMIO
        // window (the D3D interrupt handler inspects the scratch registers),
        // so keep the big-endian memory image in sync.
        *reinterpret_cast<be<uint32_t>*>(g_memory.Translate(MMIO_BASE + index * 4)) = value;

        if (index >= REG_SCRATCH_REG0 && index <= REG_SCRATCH_REG7)
        {
            // SCRATCH_UMSK / SCRATCH_ADDR are programmed by D3D through plain
            // (non-eieio) MMIO stores that never reach WriteRegister, so read
            // them from the memory window rather than the register file.
            uint32_t scratchReg = index - REG_SCRATCH_REG0;
            uint32_t umsk = *reinterpret_cast<be<uint32_t>*>(g_memory.Translate(MMIO_BASE + REG_SCRATCH_UMSK * 4));
            uint32_t scratchAddr = *reinterpret_cast<be<uint32_t>*>(g_memory.Translate(MMIO_BASE + REG_SCRATCH_ADDR * 4));
            if ((1u << scratchReg) & umsk)
            {
                *reinterpret_cast<be<uint32_t>*>(TranslatePhysical(scratchAddr + scratchReg * 4)) = value;
                static uint32_t logged = 0;
                if (logged++ < 4 || (g_swapCount >= 110 && scratchReg <= 1))
                    LOG_INFO("scratch writeback reg{} = {:#x} -> physical {:#x} (umsk {:#x}) swap #{}", scratchReg, value, scratchAddr + scratchReg * 4, umsk, g_swapCount.load());
            }
        }
        else if (index == REG_COHER_STATUS_HOST)
        {
            m_registers[index] |= 0x80000000u;
        }
    }

    uint32_t CommandProcessor::ReadRegister(uint32_t index)
    {
        if (index >= REGISTER_COUNT)
            return 0;
        if (m_registers[index] == 0)
            return *reinterpret_cast<be<uint32_t>*>(g_memory.Translate(MMIO_BASE + index * 4));
        return m_registers[index];
    }

    void CommandProcessor::MmioWrite32(uint32_t address, uint32_t value)
    {
        uint32_t index = (address & 0xFFFF) / 4;
        if (index == REG_CP_RB_WPTR)
            UpdateWritePointer(value);
        else if (g_traceBudget > 0)
            LOG_INFO("mmio write reg {:#x} = {:#x}", index, value);
        WriteRegister(index, value);
    }

    uint32_t CommandProcessor::MmioRead32(uint32_t address)
    {
        return ReadRegister((address & 0xFFFF) / 4);
    }

    // -----------------------------------------------------------------------

    void CommandProcessor::WorkerMain()
    {
        video::Init();
        uint32_t idle = 0;
        while (m_running)
        {
            uint32_t writePtr = m_writePtrIndex.load();

            // The write pointer is also visible in the MMIO window when the
            // recompiled store did not go through the MMIO hook.
            if (m_primaryBufferSize)
            {
                uint32_t mirrored = *reinterpret_cast<be<uint32_t>*>(g_memory.Translate(MMIO_BASE + REG_CP_RB_WPTR * 4));
                if (mirrored != writePtr && mirrored < m_primaryBufferSize / 4)
                {
                    static const bool traceIb = getenv("LO_TRACE_IB") != nullptr;
                    if (traceIb)
                        LOG_INFO("swap#{} WPTR mirror {:#x} (was {:#x}, rd {:#x})", g_swapCount.load(), mirrored, writePtr, m_readPtrIndex);
                    writePtr = mirrored;
                    m_writePtrIndex = mirrored;
                }
            }

            if (writePtr == 0xBAADF00D || m_readPtrIndex == writePtr || m_primaryBufferSize == 0)
            {
                if (++idle > 200)
                {
                    video::PumpEvents();
                    std::this_thread::sleep_for(std::chrono::microseconds(500));
                }
                else
                    std::this_thread::yield();
                continue;
            }
            idle = 0;

            m_readPtrIndex = ExecutePrimaryBuffer(m_readPtrIndex, writePtr);

            if (m_readPtrWritebackPhysical)
                *reinterpret_cast<be<uint32_t>*>(TranslatePhysical(m_readPtrWritebackPhysical)) = m_readPtrIndex;
        }
    }

    // The vblank ISR runs on its own guest thread so a CP-triggered interrupt
    // handler that spins waiting for a vblank cannot deadlock against it (on
    // hardware the two arrive on different CPUs).
    void CommandProcessor::VsyncMain()
    {
        GuestThreadContext ctx(2); // Xenia dispatches vblanks on CPU 2
        auto next = std::chrono::steady_clock::now();
        while (m_running)
        {
            next += std::chrono::microseconds(16667);
            std::this_thread::sleep_until(next);
            ++m_counter;

            // Watchdog: no swap for 5 seconds -> dump what every guest thread waits on.
            {
                static uint32_t lastSwaps = 0, stillFrames = 0, dumps = 0;
                uint32_t swaps = g_swapCount.load();
                if (swaps == lastSwaps)
                {
                    if (++stillFrames == 300 && swaps > 0 && dumps++ < 2)
                    {
                        ::DumpGuestThreadStates();
                    }
                }
                else
                {
                    lastSwaps = swaps;
                    stillFrames = 0;
                }
            }
            if (!m_interruptCallback)
                continue;
            auto* blk = reinterpret_cast<be<uint32_t>*>(TranslatePhysical(0xB000));
            uint32_t b0 = blk[0], b1 = blk[1];
            ctx.ppcContext.r3.u64 = 0;
            ctx.ppcContext.r4.u64 = m_interruptUserData;
            g_memory.FindFunction(m_interruptCallback)(ctx.ppcContext, g_memory.base);
            if (g_swapCount >= 110 && (uint32_t(blk[0]) != b0 || uint32_t(blk[1]) != b1))
                LOG_INFO("vblank isr changed block [{:#x} {:#x}] -> [{:#x} {:#x}] (swap #{})", b0, b1, uint32_t(blk[0]), uint32_t(blk[1]), g_swapCount.load());
        }
    }

    // Interrupt callbacks run on their own guest thread (they need a PCR/TEB
    // and a guest stack like any other guest code).
    void CommandProcessor::InterruptMain()
    {
        GuestThreadContext ctx(2);
        while (m_running)
        {
            std::pair<uint32_t, uint32_t> item;
            {
                std::lock_guard lock(m_interruptMutex);
                if (m_pendingInterrupts.empty())
                {
                    uint32_t signal = m_interruptSignal.load();
                    m_interruptMutex.unlock();
                    m_interruptSignal.wait(signal);
                    m_interruptMutex.lock();
                    continue;
                }
                item = m_pendingInterrupts.front();
                m_pendingInterrupts.erase(m_pendingInterrupts.begin());
            }

            if (!m_interruptCallback)
                continue;

            // The handler clears "its" CPU bit in the D3D interrupt block, so
            // the PCR must report the CPU the interrupt was aimed at.
            ctx.SetCpuNumber(item.second);
            ctx.ppcContext.r3.u64 = item.first;
            ctx.ppcContext.r4.u64 = m_interruptUserData;
            auto* blk = reinterpret_cast<be<uint32_t>*>(TranslatePhysical(0xB000));
            bool trace = g_swapCount >= 110;
            if (trace)
                LOG_INFO("isr source={} cpu={} block=[{:#x} {:#x} {:#x} {:#x} {:#x} {:#x}]", item.first, item.second,
                    uint32_t(blk[0]), uint32_t(blk[1]), uint32_t(blk[2]), uint32_t(blk[3]), uint32_t(blk[4]), uint32_t(blk[5]));
            g_memory.FindFunction(m_interruptCallback)(ctx.ppcContext, g_memory.base);
            if (trace)
                LOG_INFO("isr done block=[{:#x} {:#x} {:#x} {:#x} {:#x} {:#x}]",
                    uint32_t(blk[0]), uint32_t(blk[1]), uint32_t(blk[2]), uint32_t(blk[3]), uint32_t(blk[4]), uint32_t(blk[5]));
        }
    }

    void CommandProcessor::DispatchInterrupt(uint32_t source, uint32_t cpu)
    {
        {
            std::lock_guard lock(m_interruptMutex);
            m_pendingInterrupts.emplace_back(source, cpu);
        }
        m_interruptSignal++;
        m_interruptSignal.notify_all();
    }

    uint32_t CommandProcessor::ExecutePrimaryBuffer(uint32_t readIndex, uint32_t writeIndex)
    {
        Reader reader{ TranslatePhysical(m_primaryBufferPhysical), m_primaryBufferSize,
            (readIndex * 4) % m_primaryBufferSize, (writeIndex * 4) % m_primaryBufferSize, true };
        while (reader.ReadCount())
        {
            if (!ExecutePacket(reader))
            {
                LOG_ERROR("primary ring buffer: bad packet at dword {}", reader.readOffset / 4);
                break;
            }
        }
        return writeIndex;
    }

    void CommandProcessor::ExecuteIndirectBuffer(uint32_t physicalAddress, uint32_t dwordCount)
    {
        Reader reader{ TranslatePhysical(physicalAddress), dwordCount * 4, 0, dwordCount * 4, false };
        while (reader.ReadCount())
        {
            if (!ExecutePacket(reader))
            {
                LOG_ERROR("indirect buffer {:#x}: bad packet at dword {}", physicalAddress, reader.readOffset / 4);
                break;
            }
        }
    }

    bool CommandProcessor::ExecutePacket(Reader& reader)
    {
        const uint32_t offset = reader.readOffset;
        const uint32_t packet = reader.ReadAndSwap();
        if (packet == 0)
            return true;

        {
            auto& r = g_history[g_historyPos++ % 64];
            r.header = packet; r.offset = offset; r.ring = reader.ring;
            r.d0 = ByteSwap(*reinterpret_cast<uint32_t*>(reader.base + reader.readOffset % reader.size));
            r.d1 = ByteSwap(*reinterpret_cast<uint32_t*>(reader.base + (reader.readOffset + 4) % reader.size));
            r.d2 = ByteSwap(*reinterpret_cast<uint32_t*>(reader.base + (reader.readOffset + 8) % reader.size));
        }

        switch (packet >> 30)
        {
        case 0: return ExecutePacketType0(reader, packet);
        case 1: return ExecutePacketType1(reader, packet);
        case 2: return true;
        case 3: return ExecutePacketType3(reader, packet);
        }
        return false;
    }

    bool CommandProcessor::ExecutePacketType0(Reader& reader, uint32_t packet)
    {
        uint32_t count = ((packet >> 16) & 0x3FFF) + 1;
        uint32_t baseIndex = packet & 0x7FFF;
        bool writeOneReg = (packet >> 15) & 1;
        for (uint32_t m = 0; m < count; m++)
        {
            uint32_t data = reader.ReadAndSwap();
            WriteRegister(writeOneReg ? baseIndex : baseIndex + m, data);
        }
        return true;
    }

    bool CommandProcessor::ExecutePacketType1(Reader& reader, uint32_t packet)
    {
        uint32_t i1 = packet & 0x7FF, i2 = (packet >> 11) & 0x7FF;
        uint32_t d1 = reader.ReadAndSwap();
        uint32_t d2 = reader.ReadAndSwap();
        WriteRegister(i1, d1);
        WriteRegister(i2, d2);
        return true;
    }

    bool CommandProcessor::ExecutePacketType3(Reader& reader, uint32_t packet)
    {
        const uint32_t opcode = (packet >> 8) & 0x7F;
        const uint32_t count = ((packet >> 16) & 0x3FFF) + 1;

        if (g_traceBudget > 0)
        {
            --g_traceBudget;
            uint32_t peek0 = ByteSwap(*reinterpret_cast<uint32_t*>(reader.base + reader.readOffset % reader.size));
            uint32_t peek1 = ByteSwap(*reinterpret_cast<uint32_t*>(reader.base + (reader.readOffset + 4) % reader.size));
            uint32_t peek2 = ByteSwap(*reinterpret_cast<uint32_t*>(reader.base + (reader.readOffset + 8) % reader.size));
            LOG_INFO("pm4 {} op={:#x} count={} [{:#x} {:#x} {:#x}]", reader.ring ? "ring" : "ib", opcode, count, peek0, peek1, peek2);
        }

        if (packet & 1)
        {
            bool anyPass = (m_binSelect & m_binMask) != 0;
            if (!anyPass || opcode == PM4_XE_SWAP)
            {
                reader.Advance(count);
                return true;
            }
        }

        switch (opcode)
        {
        case PM4_ME_INIT:
        case PM4_NOP:
        case PM4_WAIT_FOR_IDLE:
            reader.Advance(count);
            return true;

        case PM4_INTERRUPT:
        {
            uint32_t cpuMask = reader.ReadAndSwap();
            for (uint32_t n = 0; n < 6; n++)
                if (cpuMask & (1u << n))
                    DispatchInterrupt(1, n);
            reader.Advance(count - 1);
            return true;
        }

        case PM4_XE_SWAP:
        {
            uint32_t magic = reader.ReadAndSwap();
            uint32_t frontbuffer = reader.ReadAndSwap();
            uint32_t width = reader.ReadAndSwap();
            uint32_t height = reader.ReadAndSwap();
            reader.Advance(count - 4);
            ++m_counter;
            uint32_t swaps = ++g_swapCount;
            if ((swaps % 60) == 1)
                LOG_INFO("swap #{} frontbuffer {:#x} {}x{} (magic {:#x})", swaps, frontbuffer, width, height, magic);
            video::PresentFrontbuffer(frontbuffer, width, height, ReadRegister(0x231B));
            video::PumpEvents();
            {
                static const uint32_t shotSwap = getenv("LO_SCREENSHOT_SWAP") ? strtoul(getenv("LO_SCREENSHOT_SWAP"), nullptr, 10) : 0;
                if (shotSwap && swaps == shotSwap)
                {
                    const char* path = getenv("LO_SCREENSHOT_PATH") ? getenv("LO_SCREENSHOT_PATH") : "screenshot.ppm";
                    LOG_INFO("screenshot at swap {} -> {} ({})", swaps, path, video::SaveScreenshot(path) ? "ok" : "failed");
                }
            }
            if (g_gpuStats)
            {
                if ((swaps % 60) == 0 || swaps < 5)
                {
                    std::string prims;
                    for (uint32_t i = 0; i < 64; i++)
                        if (g_frame.prim[i])
                            prims += fmt::format(" p{}={}", i, g_frame.prim[i]);
                    LOG_INFO("frame {} stats: draws={} indexed={} auto={} copies={} shaderLoads={} constWrites={}{}",
                        swaps, g_frame.draws, g_frame.indexed, g_frame.autoIndex, g_frame.copies, g_frame.shaderLoads, g_frame.constantWrites, prims);
                }
                if (swaps == 120 || swaps == 600 || swaps == 1500 || swaps == 3000)
                    g_detailBudget = 60;
            }
            g_frame = FrameStats{};
            if (swaps == 118 && getenv("LO_GPU_TRACE"))
                g_traceBudget = 400;
            return true;
        }

        case PM4_INDIRECT_BUFFER:
        case PM4_INDIRECT_BUFFER_PFD:
        {
            uint32_t listPtr = reader.ReadAndSwap();
            uint32_t listLength = reader.ReadAndSwap() & 0xFFFFF;
            static const bool traceIb = getenv("LO_TRACE_IB") != nullptr;
            if (traceIb && reader.ring)
                LOG_INFO("swap#{} ring rd={:#x} wr={:#x} IB [{:#x} {:#x}]", g_swapCount.load(), reader.readOffset, reader.writeOffset, listPtr, listLength);
            ExecuteIndirectBuffer(listPtr, listLength);
            return true;
        }

        case PM4_WAIT_REG_MEM:
        {
            // The CPU may still be filling this buffer when we get here (on
            // hardware the GPU lags behind); re-read until the operands parse.
            const uint32_t operandOffset = reader.readOffset;
            uint32_t waitInfo = 0, pollRegAddr = 0, ref = 0, mask = 0, wait = 0;
            for (int attempt = 0; attempt < 200; attempt++)
            {
                reader.readOffset = operandOffset;
                waitInfo = reader.ReadAndSwap();
                pollRegAddr = reader.ReadAndSwap();
                ref = reader.ReadAndSwap();
                mask = reader.ReadAndSwap();
                wait = reader.ReadAndSwap();
                bool memWait = (waitInfo & 0x10) != 0;
                if ((waitInfo & ~0x1FFu) == 0 && (memWait || pollRegAddr < REGISTER_COUNT))
                {
                    if (attempt > 0)
                        LOG_INFO("WAIT_REG_MEM operands became valid after {} ms", attempt);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            bool isMemory = (waitInfo & 0x10) != 0;
            if ((!isMemory && pollRegAddr >= REGISTER_COUNT) || (waitInfo & ~0x1FFu))
            {
                // Operands are not a WAIT_REG_MEM: the buffer was recycled by
                // the CPU before we got here. Skip instead of stalling forever.
                static uint32_t dumped = 0;
                if (dumped++ < 3)
                {
                    DumpHistory("WAIT_REG_MEM with corrupt operands, skipping");
                    // Does another physical alias hold the real packet?
                    uint32_t ibPhys = uint32_t(reader.base - static_cast<uint8_t*>(g_memory.Translate(0xA0000000u)));
                    for (uint32_t alias : { 0xA0000000u, 0xC0000000u, 0xE0000000u })
                    {
                        auto* p = reinterpret_cast<be<uint32_t>*>(g_memory.Translate(alias + ibPhys));
                        std::string words;
                        for (int i = 0; i < 11; i++) words += fmt::format(" {:#x}", uint32_t(p[i]));
                        LOG_WARNING("  alias {:#x}+{:#x}:{}", alias, ibPhys, words);
                    }
                }
                return true;
            }
            if (g_traceBudget > 0 || (g_swapCount >= 110 && isMemory && (pollRegAddr & ~3u) >= 0xB000 && (pollRegAddr & ~3u) < 0xB020))
                LOG_INFO("WAIT_REG_MEM {} {:#x} op={} ref={:#x} mask={:#x} value now {:#x} swap #{}", isMemory ? "mem" : "reg", pollRegAddr, waitInfo & 7, ref, mask,
                    isMemory ? GpuSwap(*reinterpret_cast<uint32_t*>(TranslatePhysical(pollRegAddr & ~3u)), pollRegAddr & 3) : 0, g_swapCount.load());
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (m_running)
            {
                uint32_t value;
                if (isMemory)
                    value = GpuSwap(*reinterpret_cast<volatile uint32_t*>(TranslatePhysical(pollRegAddr & ~3u)), pollRegAddr & 3);
                else
                {
                    if (pollRegAddr == REG_COHER_STATUS_HOST)
                        m_registers[REG_COHER_STATUS_HOST] &= ~0x80000000u; // "make coherent"
                    value = ReadRegister(pollRegAddr);
                }
                bool matched = false;
                switch (waitInfo & 7)
                {
                case 0: matched = false; break;
                case 1: matched = (value & mask) < ref; break;
                case 2: matched = (value & mask) <= ref; break;
                case 3: matched = (value & mask) == ref; break;
                case 4: matched = (value & mask) != ref; break;
                case 5: matched = (value & mask) >= ref; break;
                case 6: matched = (value & mask) > ref; break;
                case 7: matched = true; break;
                }
                if (matched)
                    break;
                if (std::chrono::steady_clock::now() > deadline)
                {
                    LOG_WARNING("WAIT_REG_MEM stalled 5s ({} {:#x} ref {:#x} mask {:#x} value {:#x}), still waiting", isMemory ? "mem" : "reg", pollRegAddr, ref, mask, value);
                    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
                }
                if (wait >= 0x100)
                    std::this_thread::sleep_for(std::chrono::milliseconds(wait / 0x100));
                else
                    std::this_thread::yield();
            }
            return true;
        }

        case PM4_REG_RMW:
        {
            uint32_t rmwInfo = reader.ReadAndSwap();
            uint32_t andMask = reader.ReadAndSwap();
            uint32_t orMask = reader.ReadAndSwap();
            uint32_t value = ReadRegister(rmwInfo & 0x1FFF);
            value &= ((rmwInfo >> 31) & 1) ? ReadRegister(andMask & 0x1FFF) : andMask;
            value |= ((rmwInfo >> 30) & 1) ? ReadRegister(orMask & 0x1FFF) : orMask;
            WriteRegister(rmwInfo & 0x1FFF, value);
            return true;
        }

        case PM4_REG_TO_MEM:
        {
            uint32_t regAddr = reader.ReadAndSwap();
            uint32_t memAddr = reader.ReadAndSwap();
            uint32_t value = GpuSwap(ReadRegister(regAddr), memAddr & 3);
            *reinterpret_cast<uint32_t*>(TranslatePhysical(memAddr & ~3u)) = value;
            return true;
        }

        case PM4_MEM_WRITE:
        {
            uint32_t writeAddr = reader.ReadAndSwap();
            for (uint32_t i = 0; i < count - 1; i++)
            {
                uint32_t data = GpuSwap(reader.ReadAndSwap(), writeAddr & 3);
                *reinterpret_cast<uint32_t*>(TranslatePhysical(writeAddr & ~3u)) = data;
                writeAddr += 4;
            }
            return true;
        }

        case PM4_COND_WRITE:
        {
            uint32_t waitInfo = reader.ReadAndSwap();
            uint32_t pollRegAddr = reader.ReadAndSwap();
            uint32_t ref = reader.ReadAndSwap();
            uint32_t mask = reader.ReadAndSwap();
            uint32_t writeRegAddr = reader.ReadAndSwap();
            uint32_t writeData = reader.ReadAndSwap();
            uint32_t value = (waitInfo & 0x10)
                ? GpuSwap(*reinterpret_cast<uint32_t*>(TranslatePhysical(pollRegAddr & ~3u)), pollRegAddr & 3)
                : ReadRegister(pollRegAddr);
            bool matched = false;
            switch (waitInfo & 7)
            {
            case 1: matched = (value & mask) < ref; break;
            case 2: matched = (value & mask) <= ref; break;
            case 3: matched = (value & mask) == ref; break;
            case 4: matched = (value & mask) != ref; break;
            case 5: matched = (value & mask) >= ref; break;
            case 6: matched = (value & mask) > ref; break;
            case 7: matched = true; break;
            }
            if (matched)
            {
                if (waitInfo & 0x100)
                    *reinterpret_cast<uint32_t*>(TranslatePhysical(writeRegAddr & ~3u)) = GpuSwap(writeData, writeRegAddr & 3);
                else
                    WriteRegister(writeRegAddr, writeData);
            }
            return true;
        }

        case PM4_EVENT_WRITE:
        {
            uint32_t initiator = reader.ReadAndSwap();
            WriteRegister(REG_VGT_EVENT_INITIATOR, initiator & 0x3F);
            reader.Advance(count - 1);
            return true;
        }

        case PM4_EVENT_WRITE_SHD:
        {
            uint32_t initiator = reader.ReadAndSwap();
            uint32_t address = reader.ReadAndSwap();
            uint32_t value = reader.ReadAndSwap();
            WriteRegister(REG_VGT_EVENT_INITIATOR, initiator & 0x3F);
            uint32_t data = ((initiator >> 31) & 1) ? m_counter.load() : value;
            *reinterpret_cast<uint32_t*>(TranslatePhysical(address & ~3u)) = GpuSwap(data, address & 3);
            return true;
        }

        case PM4_EVENT_WRITE_EXT:
        {
            uint32_t initiator = reader.ReadAndSwap();
            uint32_t address = reader.ReadAndSwap();
            WriteRegister(REG_VGT_EVENT_INITIATOR, initiator & 0x3F);
            // Screen extents: whole 8192x8192 surface, z 0..1 (8in16 swapped).
            uint16_t extents[] = { 0, 8192 >> 3, 0, 8192 >> 3, 0, 1 };
            auto* dst = reinterpret_cast<uint16_t*>(TranslatePhysical(address & ~3u));
            for (size_t i = 0; i < 6; i++)
                dst[i] = ByteSwap(extents[i]);
            return true;
        }

        case PM4_EVENT_WRITE_ZPD:
        {
            uint32_t initiator = reader.ReadAndSwap();
            WriteRegister(REG_VGT_EVENT_INITIATOR, initiator & 0x3F);
            reader.Advance(count - 1);
            return true;
        }

        case PM4_SET_BIN_MASK_LO: m_binMask = (m_binMask & 0xFFFFFFFF00000000ull) | reader.ReadAndSwap(); return true;
        case PM4_SET_BIN_MASK_HI: m_binMask = (m_binMask & 0xFFFFFFFFull) | (uint64_t(reader.ReadAndSwap()) << 32); return true;
        case PM4_SET_BIN_SELECT_LO: m_binSelect = (m_binSelect & 0xFFFFFFFF00000000ull) | reader.ReadAndSwap(); return true;
        case PM4_SET_BIN_SELECT_HI: m_binSelect = (m_binSelect & 0xFFFFFFFFull) | (uint64_t(reader.ReadAndSwap()) << 32); return true;
        case PM4_SET_BIN_MASK:
        {
            uint32_t lo = reader.ReadAndSwap(), hi = reader.ReadAndSwap();
            m_binMask = (uint64_t(hi) << 32) | lo;
            return true;
        }
        case PM4_SET_BIN_SELECT:
        {
            uint32_t lo = reader.ReadAndSwap(), hi = reader.ReadAndSwap();
            m_binSelect = (uint64_t(hi) << 32) | lo;
            return true;
        }
        case PM4_CONTEXT_UPDATE:
            reader.Advance(count);
            return true;

        case PM4_SET_CONSTANT:
        case PM4_LOAD_ALU_CONSTANT:
        {
            uint32_t address = 0;
            if (opcode == PM4_LOAD_ALU_CONSTANT)
                address = reader.ReadAndSwap() & 0x3FFFFFFF;
            uint32_t offsetType = reader.ReadAndSwap();
            uint32_t index = offsetType & 0x7FF;
            uint32_t type = (offsetType >> 16) & 0xFF;
            uint32_t n = count - 1;
            if (opcode == PM4_LOAD_ALU_CONSTANT)
            {
                n = reader.ReadAndSwap() & 0xFFF;
            }
            static const uint32_t bases[] = { 0x4000, 0x4800, 0x4900, 0x4908, 0x2000 };
            if (type < 5)
            {
                index += bases[type];
                g_frame.constantWrites += n;
                if (opcode == PM4_LOAD_ALU_CONSTANT)
                {
                    auto* src = reinterpret_cast<be<uint32_t>*>(TranslatePhysical(address));
                    for (uint32_t i = 0; i < n; i++)
                        WriteRegister(index + i, src[i]);
                }
                else
                {
                    for (uint32_t i = 0; i < n; i++)
                        WriteRegister(index + i, reader.ReadAndSwap());
                }
            }
            else if (opcode == PM4_SET_CONSTANT)
                reader.Advance(count - 1);
            return true;
        }

        case PM4_SET_CONSTANT2:
        case PM4_SET_SHADER_CONSTANTS:
        {
            uint32_t index = reader.ReadAndSwap() & 0xFFFF;
            g_frame.constantWrites += count - 1;
            for (uint32_t i = 0; i < count - 1; i++)
                WriteRegister(index + i, reader.ReadAndSwap());
            return true;
        }

        case PM4_IM_LOAD:
        {
            uint32_t addrType = reader.ReadAndSwap();
            uint32_t startSize = reader.ReadAndSwap();
            uint32_t sizeDwords = startSize & 0xFFFF;
            CaptureShader(addrType & 3, reinterpret_cast<uint32_t*>(TranslatePhysical(addrType & ~3u)), sizeDwords);
            reader.Advance(count - 2);
            return true;
        }

        case PM4_IM_LOAD_IMMEDIATE:
        {
            uint32_t type = reader.ReadAndSwap();
            uint32_t startSize = reader.ReadAndSwap();
            uint32_t sizeDwords = startSize & 0xFFFF;
            CaptureShader(type & 3, reinterpret_cast<uint32_t*>(reader.base + reader.readOffset % reader.size), sizeDwords);
            reader.Advance(count - 2);
            return true;
        }

        case PM4_INVALIDATE_STATE:
            reader.Advance(count);
            return true;

        case PM4_DRAW_INDX:
        case PM4_DRAW_INDX_2:
        {
            uint32_t consumed = 0;
            if (opcode == PM4_DRAW_INDX)
            {
                reader.ReadAndSwap(); // viz query condition
                consumed++;
            }
            uint32_t initiator = reader.ReadAndSwap();
            consumed++;
            WriteRegister(0x21FC, initiator);
            uint32_t primType = initiator & 0x3F;
            uint32_t sourceSelect = (initiator >> 6) & 3;
            uint32_t numIndices = initiator >> 16;
            uint32_t dmaBase = 0, dmaSize = 0;
            if (sourceSelect == 0 && consumed + 2 <= count)
            {
                dmaBase = reader.ReadAndSwap();
                dmaSize = reader.ReadAndSwap();
                consumed += 2;
                WriteRegister(0x21FA, dmaBase);
                WriteRegister(0x21FB, dmaSize);
            }
            reader.Advance(count - consumed);

            g_frame.draws++;
            g_frame.prim[primType & 63]++;
            if (sourceSelect == 0) g_frame.indexed++;
            if (sourceSelect == 2) g_frame.autoIndex++;
            uint32_t modeControl = ReadRegister(0x2208);
            bool isCopy = (modeControl & 7) == 6; // xenos::ModeControl::kCopy
            if (isCopy) g_frame.copies++;

            if (g_gpuStats && g_detailBudget == 60)
            {
                // Once per detailed frame: every non-zero fetch constant slot
                // (0x4800 + 6 dwords each) and the first vertex ALU constants.
                for (uint32_t slot = 0; slot < 96; slot++)
                {
                    uint32_t w[6];
                    bool any = false;
                    for (int k = 0; k < 6; k++) { w[k] = ReadRegister(0x4800 + slot * 6 + k); any |= w[k] != 0; }
                    if (any)
                        LOG_INFO("  fetch[{}] = {:#x} {:#x} {:#x} {:#x} {:#x} {:#x}", slot, w[0], w[1], w[2], w[3], w[4], w[5]);
                }
                for (uint32_t c = 0; c < 8; c++)
                    LOG_INFO("  vsconst c{} = {:#x} {:#x} {:#x} {:#x}", c, ReadRegister(0x4000 + c * 4), ReadRegister(0x4001 + c * 4), ReadRegister(0x4002 + c * 4), ReadRegister(0x4003 + c * 4));
                LOG_INFO("  viewport xs={:#x} xo={:#x} ys={:#x} yo={:#x} zs={:#x} zo={:#x} vte={:#x} su_sc={:#x} colorctl={:#x} copyDestInfo={:#x}",
                    ReadRegister(0x210F), ReadRegister(0x2110), ReadRegister(0x2111), ReadRegister(0x2112), ReadRegister(0x2113), ReadRegister(0x2114),
                    ReadRegister(0x2206), ReadRegister(0x2205), ReadRegister(0x2202), ReadRegister(0x231B));
            }
            if (g_gpuStats && g_detailBudget > 0)
            {
                g_detailBudget--;
                if (isCopy)
                    LOG_INFO("  resolve: copyCtl={:#x} dest={:#x} pitch={:#x} destInfo={:#x} surf={:#x} color={:#x} depth={:#x}",
                        ReadRegister(0x2318), ReadRegister(0x2319), ReadRegister(0x231A), ReadRegister(0x231B),
                        ReadRegister(0x2000), ReadRegister(0x2001), ReadRegister(0x2002));
                else
                    LOG_INFO("  draw prim={} n={} src={} vs={:016x}/{} ps={:016x}/{} color={:#x} depth={:#x} surf={:#x} mode={:#x} blend={:#x} depthCtl={:#x} scissor={:#x}-{:#x} pgm={:#x} idx={:#x}/{:#x}",
                        primType, numIndices, sourceSelect, g_activeShader[0], g_activeShaderSize[0], g_activeShader[1], g_activeShaderSize[1],
                        ReadRegister(0x2001), ReadRegister(0x2002), ReadRegister(0x2000), modeControl, ReadRegister(0x2201), ReadRegister(0x2200),
                        ReadRegister(0x2081), ReadRegister(0x2082), ReadRegister(0x2180), dmaBase, dmaSize);
            }
            return true;
        }

        default:
            // Remaining state packets: no renderer yet, skip.
            reader.Advance(count);
            return true;
        }
    }
}
