#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include "gpu/frame_plan.h"

// Ordered Xenos/native command consumer. PM4 remains the fallback for guest
// operations not yet replaced by the optional native SDK hooks. Both routes
// share register state, the renderer, ring/IB order and GPU resource ownership.
// Register semantics follow Xenia's gpu/command_processor.cc (BSD-3).

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
        // Thread-safe request only; GPU destruction stays on the worker.
        void RequestStopForExit();

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
        // Byte identity of the owned IM_LOAD snapshot; resolved once per change.
        uint64_t GetActiveShaderByteHash(bool pixel) const;
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
            bool ReadNativeWords(uint32_t count, uint32_t* destination);
            void Advance(uint32_t dwords);
        };

        void WorkerMain();
        void VsyncMain();
        void InterruptMain();
        void DispatchInterrupt(uint32_t source, uint32_t cpu);

        bool ExecuteNativeCommand(Reader& reader, uint32_t tag);
        bool ExecuteDraw(uint32_t initiator, uint32_t dmaBase, uint32_t dmaSize);
        uint32_t ExecutePrimaryBuffer(uint32_t readIndex, uint32_t writeIndex);
        void ExecuteIndirectBuffer(uint32_t physicalAddress, uint32_t dwordCount);
        bool ExecutePacket(Reader& reader);
        bool ExecutePacketType0(Reader& reader, uint32_t packet);
        bool ExecutePacketType1(Reader& reader, uint32_t packet);
        bool ExecutePacketType3(Reader& reader, uint32_t packet);

        uint8_t* TranslatePhysical(uint32_t physicalAddress);

        std::vector<uint32_t> m_registers;
        struct MovieClearStage
        {
            bool active = false;
            uint32_t surfaceInfo = 0, colorInfo = 0;
            uint32_t x = 0, y = 0, width = 0, height = 0, safeLeft = 0, safeRight = 0;
        } m_movieClear;
        gpu::frame_plan::wire::PlanStage m_framePlan;
        gpu::frame_plan::wire::CatalogStage m_catalog;
        uint32_t m_primaryBufferPhysical = 0;
        uint32_t m_primaryBufferSize = 0;
        uint32_t m_readPtrIndex = 0;
        uint32_t m_readPtrWritebackPhysical = 0;
        std::atomic<uint32_t> m_writePtrIndex{ 0xBAADF00D };
        std::mutex m_writePtrMutex;
        std::condition_variable m_writePtrChanged;
        std::atomic<uint32_t> m_counter{ 0 };
        std::atomic<bool> m_running{ false };

        uint32_t m_interruptCallback = 0;
        uint32_t m_interruptUserData = 0;
        std::mutex m_interruptMutex;
        std::condition_variable m_interruptCv;
        std::vector<std::pair<uint32_t, uint32_t>> m_pendingInterrupts; // (source, cpu)
        uint64_t m_binMask = 0xFFFFFFFFFFFFFFFFull;
        uint64_t m_binSelect = 0xFFFFFFFFFFFFFFFFull;

        std::thread m_worker;
        std::thread m_vsync;
        std::thread m_interruptThread;

        struct NativeCounters
        {
            uint64_t registerBlocks = 0, registerWords = 0, nativeWords = 0;
            uint64_t pm4Packets = 0, pm4Words = 0;
            uint64_t autoFans = 0;
            uint64_t indexedQuads = 0, predicatedSkips = 0, titleCloudDraws = 0;
        } m_native;
    };

    extern CommandProcessor g_commandProcessor;
}
