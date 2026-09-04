#include <stdafx.h>
#include "xma.h"
#include <kernel/memory.h>
#include <os/logger.h>

namespace apu::xma
{
    namespace
    {
        // Register indices (dword units from 0x7FEA0000), see Xenia's
        // xma_register_table.inc.
        constexpr uint32_t kRegContextArrayAddress = 0x600;
        constexpr uint32_t kRegCurrentContextIndex = 0x606;
        constexpr uint32_t kRegNextContextIndex = 0x607;
        constexpr uint32_t kRegKick0 = 0x650;   // 10 dwords, one bit per context
        constexpr uint32_t kRegLock0 = 0x690;
        constexpr uint32_t kRegClear0 = 0x6A0;
        constexpr uint32_t kRegisterGroups = kContextCount / 32;

        constexpr uint32_t kBytesPerPacket = 2048;
        constexpr uint32_t kBitsPerPacket = kBytesPerPacket * 8;
        constexpr uint32_t kBitsPerPacketHeader = 32;
        constexpr uint32_t kOutputBytesPerBlock = 256;
        constexpr uint32_t kOutputMaxSizeBytes = 31 * kOutputBytesPerBlock;

        // Host-side view of XMA_CONTEXT_DATA: 16 big-endian dwords, decoded
        // into the fields Xenia names. Only the ones the stand-in touches.
        struct ContextData
        {
            uint32_t d[16];

            static uint32_t Bits(uint32_t v, uint32_t shift, uint32_t width) { return (v >> shift) & ((1u << width) - 1); }
            static void SetBits(uint32_t& v, uint32_t shift, uint32_t width, uint32_t value)
            {
                const uint32_t mask = ((1u << width) - 1) << shift;
                v = (v & ~mask) | ((value << shift) & mask);
            }

            void Load(const void* guest)
            {
                for (int i = 0; i < 16; i++)
                    d[i] = ByteSwap(reinterpret_cast<const uint32_t*>(guest)[i]);
            }
            void Store(void* guest) const
            {
                for (int i = 0; i < 16; i++)
                    reinterpret_cast<uint32_t*>(guest)[i] = ByteSwap(d[i]);
            }

            uint32_t input0PacketCount() const { return Bits(d[0], 0, 12); }
            uint32_t loopCount() const { return Bits(d[0], 12, 8); }
            void setLoopCount(uint32_t v) { SetBits(d[0], 12, 8, v); }
            bool input0Valid() const { return Bits(d[0], 20, 1); }
            void setInput0Valid(bool v) { SetBits(d[0], 20, 1, v); }
            bool input1Valid() const { return Bits(d[0], 21, 1); }
            void setInput1Valid(bool v) { SetBits(d[0], 21, 1, v); }
            uint32_t outputBlockCount() const { return Bits(d[0], 22, 5); }
            uint32_t outputWriteOffset() const { return Bits(d[0], 27, 5); }
            void setOutputWriteOffset(uint32_t v) { SetBits(d[0], 27, 5, v); }

            uint32_t input1PacketCount() const { return Bits(d[1], 0, 12); }
            uint32_t subframeDecodeCount() const { return Bits(d[1], 20, 4); }
            bool isStereo() const { return Bits(d[1], 29, 1); }
            bool outputValid() const { return Bits(d[1], 31, 1); }
            void setOutputValid(bool v) { SetBits(d[1], 31, 1, v); }

            uint32_t inputReadOffset() const { return Bits(d[2], 0, 26); }
            void setInputReadOffset(uint32_t v) { SetBits(d[2], 0, 26, v); }
            uint32_t errorStatus() const { return Bits(d[2], 26, 5); }

            uint32_t loopStart() const { return Bits(d[3], 0, 26); }
            uint32_t loopEnd() const { return Bits(d[4], 0, 26); }
            bool currentBuffer() const { return Bits(d[4], 31, 1); }
            void setCurrentBuffer(bool v) { SetBits(d[4], 31, 1, v); }

            uint32_t input0Ptr() const { return d[5]; }
            uint32_t input1Ptr() const { return d[6]; }
            uint32_t outputPtr() const { return d[7]; }

            uint32_t outputReadOffset() const { return Bits(d[9], 0, 5); }
            void setOutputReadOffset(uint32_t v) { SetBits(d[9], 0, 5, v); }

            bool inputValid(bool which) const { return which ? input1Valid() : input0Valid(); }
            bool anyInputValid() const { return input0Valid() || input1Valid(); }
            uint32_t currentPacketCount() const { return currentBuffer() ? input1PacketCount() : input0PacketCount(); }
        };

        struct Context
        {
            std::atomic<bool> allocated{ false };
            std::atomic<bool> enabled{ false };
            uint32_t remainingSubframes = 0;       // of the "decoded" frame
            int32_t freeBlocks = 0;                 // in the output ring
        };

        uint32_t g_arrayGuest = 0;                  // virtual address of the context array
        Context g_contexts[kContextCount];
        std::mutex g_mutex;
        std::thread g_worker;
        std::atomic<bool> g_running{ false };
        std::atomic<uint32_t> g_allocatedCount{ 0 };
        bool g_trace = false;

        uint32_t* Register(uint32_t index)
        {
            return static_cast<uint32_t*>(g_memory.Translate(kRegisterBase + index * 4));
        }

        // Registers are little-endian to the guest (stwbrx/lwbrx), so the host
        // reads and writes them natively.
        uint32_t ExchangeRegister(uint32_t index, uint32_t value)
        {
            return std::atomic_ref<uint32_t>(*Register(index)).exchange(value);
        }

        uint8_t* ContextHost(uint32_t id)
        {
            return static_cast<uint8_t*>(g_memory.Translate(g_arrayGuest + id * kContextSize));
        }

        uint8_t* Physical(uint32_t physicalAddress)
        {
            return static_cast<uint8_t*>(g_memory.Translate(0xA0000000u + (physicalAddress & 0x1FFFFFFF)));
        }

        // Output ring bookkeeping with Xenia's RingBuffer conventions:
        // read == write means empty (the whole capacity is writable).
        struct Ring
        {
            uint8_t* data; uint32_t capacity, read, write;
            uint32_t WriteCount() const
            {
                if (read == write) return capacity;
                if (write < read) return read - write;
                return (capacity - write) + read;
            }
            void WriteZero(uint32_t count)
            {
                while (count)
                {
                    uint32_t chunk = std::min(count, capacity - write);
                    memset(data + write, 0, chunk);
                    write = (write + chunk) % capacity;
                    count -= chunk;
                }
            }
        };

        void ClearContext(uint32_t id)
        {
            ContextData data;
            uint8_t* guest = ContextHost(id);
            data.Load(guest);
            data.setInput0Valid(false);
            data.setInput1Valid(false);
            data.setOutputValid(false);
            data.setOutputReadOffset(0);
            data.setOutputWriteOffset(0);
            data.setInputReadOffset(kBitsPerPacketHeader);
            data.Store(guest);
            g_contexts[id].remainingSubframes = 0;
            if (g_trace) LOG_INFO("xma: clear context {}", id);
        }

        void SwapInputBuffer(ContextData& data)
        {
            if (data.currentBuffer()) data.setInput1Valid(false);
            else data.setInput0Valid(false);
            data.setCurrentBuffer(!data.currentBuffer());
            data.setInputReadOffset(kBitsPerPacketHeader);
        }

        void UpdateLoopStatus(ContextData& data)
        {
            if (data.loopCount() == 0)
                return;
            const uint32_t loopStart = std::max(kBitsPerPacketHeader, data.loopStart());
            const uint32_t loopEnd = std::max(kBitsPerPacketHeader, data.loopEnd());
            if (data.inputReadOffset() != loopEnd)
                return;
            data.setInputReadOffset(loopStart);
            if (data.loopCount() != 255)
                data.setLoopCount(data.loopCount() - 1);
        }

        // Advance over one input packet; a real decoder would parse it here.
        void ProcessPacket(Context& ctx, ContextData& data)
        {
            if (!data.anyInputValid() || ctx.remainingSubframes > 0)
                return;
            UpdateLoopStatus(data);
            const uint32_t packets = data.currentPacketCount();
            const uint32_t current = data.inputReadOffset() / kBitsPerPacket;
            if (current >= packets)
            {
                SwapInputBuffer(data);
                return;
            }
            const uint32_t next = current + 1;
            if (next >= packets)
                SwapInputBuffer(data);
            else
                data.setInputReadOffset(next * kBitsPerPacket + kBitsPerPacketHeader);
            ctx.remainingSubframes = 4u << (data.isStereo() ? 1 : 0);
        }

        void Consume(Context& ctx, ContextData& data, Ring& ring)
        {
            if (!ctx.remainingSubframes)
                return;
            const uint32_t blocks = std::min<uint32_t>(ctx.remainingSubframes, std::max<uint32_t>(1, data.subframeDecodeCount()));
            ring.WriteZero(blocks * kOutputBytesPerBlock);
            ctx.freeBlocks -= int32_t(blocks);
            ctx.remainingSubframes -= blocks;
        }

        void Work(uint32_t id)
        {
            Context& ctx = g_contexts[id];
            if (!ctx.enabled.exchange(false) || !ctx.allocated)
                return;
            uint8_t* guest = ContextHost(id);
            ContextData data;
            data.Load(guest);
            if (!data.outputValid())
                return;

            Ring ring{ Physical(data.outputPtr()), data.outputBlockCount() * kOutputBytesPerBlock,
                       data.outputReadOffset() * kOutputBytesPerBlock, data.outputWriteOffset() * kOutputBytesPerBlock };
            if (ring.capacity == 0 || ring.capacity > kOutputMaxSizeBytes)
            {
                LOG_WARNING("xma: context {} has an output ring of {} bytes", id, ring.capacity);
                return;
            }
            ctx.freeBlocks = int32_t(ring.WriteCount() / kOutputBytesPerBlock);
            const int32_t minimumBlocks = int32_t(std::max<uint32_t>(1, data.subframeDecodeCount())) * 2 - 1;
            if (minimumBlocks > ctx.freeBlocks)
            {
                data.Store(guest);
                return;
            }
            for (int guard = 0; ctx.freeBlocks >= minimumBlocks && guard < 4096; guard++)
            {
                ProcessPacket(ctx, data);
                Consume(ctx, data, ring);
                if (!data.anyInputValid() || data.errorStatus() == 4)
                    break;
            }
            data.setOutputWriteOffset(ring.write / kOutputBytesPerBlock);
            if (ring.read == ring.write)
                data.setOutputValid(false);
            data.Store(guest);
            if (g_trace)
                LOG_INFO("xma: context {} worked: in={}/{} read {} write {} valid {}", id, data.input0Valid(), data.input1Valid(),
                    data.outputReadOffset(), data.outputWriteOffset(), data.outputValid());
        }

        void WorkerMain()
        {
            while (g_running)
            {
                bool didWork = false;
                for (uint32_t group = 0; group < kRegisterGroups; group++)
                {
                    if (uint32_t bits = ExchangeRegister(kRegLock0 + group, 0))
                    {
                        for (; bits; bits &= bits - 1)
                            g_contexts[group * 32 + std::countr_zero(bits)].enabled = false;
                    }
                    if (uint32_t bits = ExchangeRegister(kRegClear0 + group, 0))
                    {
                        std::lock_guard lock(g_mutex);
                        for (; bits; bits &= bits - 1)
                            ClearContext(group * 32 + std::countr_zero(bits));
                    }
                    if (uint32_t bits = ExchangeRegister(kRegKick0 + group, 0))
                    {
                        std::lock_guard lock(g_mutex);
                        for (; bits; bits &= bits - 1)
                        {
                            const uint32_t id = group * 32 + std::countr_zero(bits);
                            g_contexts[id].enabled = true;
                            Work(id);
                            didWork = true;
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::microseconds(didWork ? 250 : 1000));
            }
        }
    }

    void Init()
    {
        if (g_arrayGuest)
            return;
        g_trace = getenv("LO_TRACE_XMA") != nullptr;
        g_arrayGuest = g_pageAllocator.Alloc(g_pageAllocator.physicalRegion, kContextCount * kContextSize, 0x1000);
        if (!g_arrayGuest)
        {
            LOG_WARNING("xma: context array allocation failed");
            return;
        }
        memset(g_memory.Translate(g_arrayGuest), 0, kContextCount * kContextSize);
        // The kernel publishes the physical address of the array; the driver
        // reads it once with lwbrx (little-endian) to convert context pointers
        // into indices. 0x606 stays 0: "not busy with any context".
        *Register(kRegContextArrayAddress) = g_arrayGuest & 0x1FFFFFFF;
        *Register(kRegCurrentContextIndex) = 0;
        *Register(kRegNextContextIndex) = 1;
        for (uint32_t group = 0; group < kRegisterGroups; group++)
        {
            *Register(kRegKick0 + group) = 0;
            *Register(kRegLock0 + group) = 0;
            *Register(kRegClear0 + group) = 0;
        }
        g_running = true;
        g_worker = std::thread(WorkerMain);
        g_worker.detach();
        LOG_INFO("xma: {} contexts at {:#x} (physical {:#x}), silent stand-in decoder", kContextCount, g_arrayGuest, g_arrayGuest & 0x1FFFFFFF);
    }

    void Shutdown()
    {
        g_running = false;
    }

    uint32_t AllocateContext()
    {
        if (!g_arrayGuest)
            return 0;
        std::lock_guard lock(g_mutex);
        for (uint32_t id = 0; id < kContextCount; id++)
        {
            if (!g_contexts[id].allocated.exchange(true))
            {
                memset(ContextHost(id), 0, kContextSize);
                g_contexts[id].enabled = false;
                g_contexts[id].remainingSubframes = 0;
                const uint32_t n = ++g_allocatedCount;
                if (g_trace || n <= 4)
                    LOG_INFO("xma: allocated context {} ({} live)", id, n);
                return g_arrayGuest + id * kContextSize;
            }
        }
        LOG_WARNING("xma: out of contexts");
        return 0;
    }

    void ReleaseContext(uint32_t guestAddress)
    {
        if (!g_arrayGuest || guestAddress < g_arrayGuest || guestAddress >= g_arrayGuest + kContextCount * kContextSize)
            return;
        const uint32_t id = (guestAddress - g_arrayGuest) / kContextSize;
        std::lock_guard lock(g_mutex);
        if (g_contexts[id].allocated.exchange(false))
        {
            g_contexts[id].enabled = false;
            memset(ContextHost(id), 0, kContextSize);
            --g_allocatedCount;
            if (g_trace) LOG_INFO("xma: released context {}", id);
        }
    }
}
