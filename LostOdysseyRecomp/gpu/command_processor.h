#pragma once

#include <atomic>
#include <thread>
#include <vector>

// Minimal Xenos command processor: consumes the primary ring buffer, executes
// the PM4 packets the CPU synchronises against (memory writes, fences, waits,
// indirect buffers, swaps, interrupts) and skips everything that would draw.
// Register semantics follow Xenia's gpu/command_processor.cc (BSD-3).
// Draw/state packets are the hook point for the future plume renderer.

namespace gpu
{
    // Thread-safe target snapshot; false rejects unsupported settings values.
    bool SetFrameRateTarget(uint32_t fps);
    uint32_t GetFrameRateTarget();
    constexpr uint32_t MMIO_BASE = 0x7FC80000;
    constexpr uint32_t REGISTER_COUNT = 0x5003;

    // Register indices (Xenia register_table.inc)
    constexpr uint32_t REG_CP_RB_WPTR = 0x01C5;
    constexpr uint32_t REG_SCRATCH_REG0 = 0x0578;
    constexpr uint32_t REG_SCRATCH_REG7 = 0x057F;
    constexpr uint32_t REG_SCRATCH_UMSK = 0x01DC;
    constexpr uint32_t REG_SCRATCH_ADDR = 0x01DD;
    constexpr uint32_t REG_COHER_STATUS_HOST = 0x0A31;
    constexpr uint32_t REG_VGT_EVENT_INITIATOR = 0x21F9;
    constexpr uint32_t REG_RB_EDRAM_TIMING = 0x0F00;
    constexpr uint32_t REG_RB_BC_CONTROL = 0x0F01;
    constexpr uint32_t REG_D1MODE_V_COUNTER = 0x194C;
    constexpr uint32_t REG_INTERRUPT_STATUS = 0x1951;
    constexpr uint32_t REG_D1MODE_VIEWPORT_SIZE = 0x1961;

    struct CommandProcessor
    {
        bool Init();
        void Shutdown();

        // Kernel entry points
        void InitializeRingBuffer(uint32_t physicalAddress, uint32_t sizeLog2);
        void EnableReadPointerWriteBack(uint32_t physicalAddress, uint32_t blockSizeLog2);
        void SetInterruptCallback(uint32_t callback, uint32_t userData);
        void UpdateWritePointer(uint32_t dwordIndex);

        // MMIO
        void WriteRegister(uint32_t index, uint32_t value);
        uint32_t ReadRegister(uint32_t index);
        // Same values as ordered ReadRegister calls, without a call per word.
        void ReadRegisters(uint32_t first, uint32_t count, uint32_t* destination);
        // Microcode of the last IM_LOAD for the vertex (false) / pixel (true) stage.
        const uint32_t* GetActiveShader(bool pixel, uint32_t& dwordCount, uint64_t& commandHash) const;
        void MmioWrite32(uint32_t address, uint32_t value);
        uint32_t MmioRead32(uint32_t address);

        uint32_t counter() const { return m_counter.load(); }

    private:
        struct Reader
        {
            uint8_t* base;      // host pointer to buffer start
            uint32_t size;      // bytes
            uint32_t readOffset;
            uint32_t writeOffset;
            bool ring;          // primary buffer wraps; indirect buffers are linear

            uint32_t ReadCount() const
            {
                if (!ring)
                    return writeOffset > readOffset ? writeOffset - readOffset : 0;
                return writeOffset >= readOffset ? writeOffset - readOffset : size - readOffset + writeOffset;
            }
            uint32_t ReadAndSwap();
            void Advance(uint32_t dwords);
        };

        void WorkerMain();
        void VsyncMain();
        void InterruptMain();
        void DispatchInterrupt(uint32_t source, uint32_t cpu);

        uint32_t ExecutePrimaryBuffer(uint32_t readIndex, uint32_t writeIndex);
        void ExecuteIndirectBuffer(uint32_t physicalAddress, uint32_t dwordCount);
        bool ExecutePacket(Reader& reader);
        bool ExecutePacketType0(Reader& reader, uint32_t packet);
        bool ExecutePacketType1(Reader& reader, uint32_t packet);
        bool ExecutePacketType3(Reader& reader, uint32_t packet);

        uint8_t* TranslatePhysical(uint32_t physicalAddress);

        std::vector<uint32_t> m_registers;
        uint32_t m_primaryBufferPhysical = 0;
        uint32_t m_primaryBufferSize = 0;
        uint32_t m_readPtrIndex = 0;
        uint32_t m_readPtrWritebackPhysical = 0;
        std::atomic<uint32_t> m_writePtrIndex{ 0xBAADF00D };
        std::atomic<uint32_t> m_counter{ 0 };
        std::atomic<bool> m_running{ false };

        uint32_t m_interruptCallback = 0;
        uint32_t m_interruptUserData = 0;
        Mutex m_interruptMutex;
        std::vector<std::pair<uint32_t, uint32_t>> m_pendingInterrupts; // (source, cpu)
        std::atomic<uint32_t> m_interruptSignal{ 0 };
        uint64_t m_binMask = 0xFFFFFFFFFFFFFFFFull;
        uint64_t m_binSelect = 0xFFFFFFFFFFFFFFFFull;

        std::thread m_worker;
        std::thread m_vsync;
        std::thread m_interruptThread;
    };

    extern CommandProcessor g_commandProcessor;
}
