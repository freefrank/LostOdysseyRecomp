#include <stdafx.h>
#include "command_processor.h"
#include <cpu/guest_thread.h>
#include <kernel/memory.h>
#include <kernel/function.h>
#include <os/logger.h>

namespace gpu
{
    CommandProcessor g_commandProcessor;

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
        uint32_t v = ByteSwap(*reinterpret_cast<uint32_t*>(base + readOffset));
        readOffset += 4;
        if (readOffset >= size)
            readOffset -= size;
        return v;
    }

    void CommandProcessor::Reader::Advance(uint32_t dwords)
    {
        readOffset += dwords * 4;
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
        m_pendingInterrupts.notify_all();
        for (auto* t : { &m_worker, &m_vsync, &m_interruptThread })
            if (t->joinable())
                t->join();
    }

    void CommandProcessor::InitializeRingBuffer(uint32_t physicalAddress, uint32_t sizeLog2)
    {
        m_primaryBufferPhysical = physicalAddress;
        m_primaryBufferSize = 1u << sizeLog2;
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
        m_writePtrIndex = dwordIndex;
        m_writePtrIndex.notify_all();
    }

    void CommandProcessor::WriteRegister(uint32_t index, uint32_t value)
    {
        if (index >= REGISTER_COUNT)
            return;

        m_registers[index] = value;

        if (index >= REG_SCRATCH_REG0 && index <= REG_SCRATCH_REG7)
        {
            uint32_t scratchReg = index - REG_SCRATCH_REG0;
            if ((1u << scratchReg) & m_registers[REG_SCRATCH_UMSK])
            {
                uint32_t scratchAddr = m_registers[REG_SCRATCH_ADDR];
                *reinterpret_cast<be<uint32_t>*>(TranslatePhysical(scratchAddr + scratchReg * 4)) = value;
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
        return m_registers[index];
    }

    void CommandProcessor::MmioWrite32(uint32_t address, uint32_t value)
    {
        uint32_t index = (address & 0xFFFF) / 4;
        if (index == REG_CP_RB_WPTR)
            UpdateWritePointer(value);
        WriteRegister(index, value);
    }

    uint32_t CommandProcessor::MmioRead32(uint32_t address)
    {
        return ReadRegister((address & 0xFFFF) / 4);
    }

    // -----------------------------------------------------------------------

    void CommandProcessor::WorkerMain()
    {
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
                    writePtr = mirrored;
                    m_writePtrIndex = mirrored;
                }
            }

            if (writePtr == 0xBAADF00D || m_readPtrIndex == writePtr || m_primaryBufferSize == 0)
            {
                if (++idle > 200)
                    std::this_thread::sleep_for(std::chrono::microseconds(500));
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

    void CommandProcessor::VsyncMain()
    {
        while (m_running)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(16667));
            ++m_counter;
            if (m_interruptCallback)
            {
                m_pendingInterrupts |= 1u << 0;
                m_pendingInterrupts.notify_all();
            }
        }
    }

    // Interrupt callbacks run on their own guest thread (they need a PCR/TEB
    // and a guest stack like any other guest code).
    void CommandProcessor::InterruptMain()
    {
        GuestThreadContext ctx(2);
        while (m_running)
        {
            uint32_t pending = m_pendingInterrupts.exchange(0);
            if (pending == 0)
            {
                m_pendingInterrupts.wait(0);
                continue;
            }
            for (uint32_t source = 0; source < 2; source++)
            {
                if (!(pending & (1u << source)) || !m_interruptCallback)
                    continue;
                ctx.ppcContext.r3.u64 = source;
                ctx.ppcContext.r4.u64 = m_interruptUserData;
                g_memory.FindFunction(m_interruptCallback)(ctx.ppcContext, g_memory.base);
            }
        }
    }

    void CommandProcessor::DispatchInterrupt(uint32_t source)
    {
        m_pendingInterrupts |= 1u << source;
        m_pendingInterrupts.notify_all();
    }

    uint32_t CommandProcessor::ExecutePrimaryBuffer(uint32_t readIndex, uint32_t writeIndex)
    {
        Reader reader{ TranslatePhysical(m_primaryBufferPhysical), m_primaryBufferSize, readIndex * 4, writeIndex * 4 };
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
        Reader reader{ TranslatePhysical(physicalAddress), dwordCount * 4, 0, dwordCount * 4 };
        // Linear buffer: writeOffset == size means "everything readable".
        reader.writeOffset = dwordCount * 4;
        uint32_t consumed = 0;
        while (consumed < dwordCount * 4)
        {
            uint32_t before = reader.readOffset;
            if (!ExecutePacket(reader))
            {
                LOG_ERROR("indirect buffer {:#x}: bad packet at dword {}", physicalAddress, reader.readOffset / 4);
                break;
            }
            uint32_t after = reader.readOffset;
            consumed += after >= before ? after - before : reader.size - before + after;
            if (after == 0 && before != 0)
                break; // wrapped == end of linear buffer
        }
    }

    bool CommandProcessor::ExecutePacket(Reader& reader)
    {
        const uint32_t packet = reader.ReadAndSwap();
        if (packet == 0)
            return true;

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
            for (int n = 0; n < 6; n++)
                if (cpuMask & (1u << n))
                    DispatchInterrupt(1);
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
            static uint32_t swaps = 0;
            if ((swaps++ % 60) == 0)
                LOG_INFO("swap #{} frontbuffer {:#x} {}x{} (magic {:#x})", swaps, frontbuffer, width, height, magic);
            return true;
        }

        case PM4_INDIRECT_BUFFER:
        case PM4_INDIRECT_BUFFER_PFD:
        {
            uint32_t listPtr = reader.ReadAndSwap();
            uint32_t listLength = reader.ReadAndSwap() & 0xFFFFF;
            ExecuteIndirectBuffer(listPtr, listLength);
            return true;
        }

        case PM4_WAIT_REG_MEM:
        {
            uint32_t waitInfo = reader.ReadAndSwap();
            uint32_t pollRegAddr = reader.ReadAndSwap();
            uint32_t ref = reader.ReadAndSwap();
            uint32_t mask = reader.ReadAndSwap();
            uint32_t wait = reader.ReadAndSwap();
            bool isMemory = (waitInfo & 0x10) != 0;
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
                    LOG_WARNING("WAIT_REG_MEM timeout ({} {:#x} ref {:#x} mask {:#x} value {:#x})", isMemory ? "mem" : "reg", pollRegAddr, ref, mask, value);
                    break;
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

        default:
            // Draw calls, constants, shader loads: no renderer yet, skip.
            reader.Advance(count);
            return true;
        }
    }
}
