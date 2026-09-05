#include <stdafx.h>
#include "xma.h"
#include "xma_loop.h"
#include <kernel/memory.h>
#include <os/logger.h>
#include <cmath>
extern "C" {
#include <libavcodec/avcodec.h>
}


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

            void StoreDecoded(void* guest) const
            {
                // The consumer advances DWORD 9 while decoding. Writing the
                // snapshot back rolls its read cursor backwards. Publish only
                // words updated by the decoder, leaving consumer fields alone.
                for (unsigned i : {0u, 1u, 2u, 4u})
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
            uint32_t sampleRate() const { constexpr uint32_t rates[] = {24000,32000,44100,48000}; return rates[Bits(d[1],27,2)]; }
            uint32_t skipCount() const { return Bits(d[1],24,3); }
            void setSkipCount(uint32_t n) { SetBits(d[1],24,3,n); }
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
            uint32_t remainingSubframes = 0;
            uint32_t pcmOffset = 0;
            std::array<uint8_t, 2048> pcm{};
            std::array<uint8_t, 4096 + AV_INPUT_BUFFER_PADDING_SIZE> compressed{};
            uint32_t frameBits = 0, copiedBits = 0, packetsSkip = 0;
            struct WorkSnapshot { ContextData data; uint32_t skip, copied; };
            std::array<WorkSnapshot, 8> workHistory{};
            uint32_t workHistoryCount = 0;
            uint32_t publishedInputOffset = 0;
            bool hasPublishedInputOffset = false;
            AVCodecContext* decoder = nullptr;
            AVFrame* frame = nullptr;
            AVPacket* packet = nullptr;
            void Reset()
            {
                remainingSubframes = pcmOffset = frameBits = copiedBits = packetsSkip = 0;
                workHistoryCount = 0;
                hasPublishedInputOffset = false;
                avcodec_free_context(&decoder);
                av_frame_free(&frame);
                av_packet_free(&packet);
            }
            int32_t freeBlocks = 0;                 // in the output ring
        };

        uint32_t g_arrayGuest = 0;                  // virtual address of the context array
        Context g_contexts[kContextCount];
        std::mutex g_mutex;
        std::thread g_worker;
        std::atomic<bool> g_running{ false };
        std::atomic<uint32_t> g_allocatedCount{ 0 };
        bool g_trace = false;
        bool g_traceContextWrites = false;

        uint32_t* Register(uint32_t index)
        {
            return static_cast<uint32_t*>(g_memory.Translate(kRegisterBase + index * 4));
        }

        // Registers are little-endian to the guest (stwbrx/lwbrx), so the host
        // reads and writes them natively.
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
            void Write(const uint8_t* source, uint32_t count)
            {
                while (count)
                {
                    uint32_t chunk = std::min(count, capacity - write);
                    memcpy(data + write, source, chunk);
                    source += chunk;
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
            g_contexts[id].Reset();
            if (g_trace) LOG_INFO("xma: clear context {}", id);
        }

        void SwapInputBuffer(ContextData& data)
        {
            if (data.currentBuffer()) data.setInput1Valid(false);
            else data.setInput0Valid(false);
            data.setCurrentBuffer(!data.currentBuffer());
            data.setInputReadOffset(kBitsPerPacketHeader);
        }

        bool UpdateLoopStatus(ContextData& data, bool bufferEnded)
        {
            auto offset = data.inputReadOffset();
            auto count = data.loopCount();
            if (!RestartLoop(data.loopStart(), data.loopEnd(), bufferEnded, offset, count)) return false;
            data.setInputReadOffset(offset);
            data.setLoopCount(count);
            return true;
        }

        uint32_t ReadBits(const uint8_t* source, uint32_t offset, uint32_t count)
        {
            uint32_t result = 0;
            for (uint32_t i = 0; i < count; ++i)
                result = (result << 1) | ((source[(offset+i)/8] >> (7-(offset+i)%8)) & 1);
            return result;
        }

        bool PrepareDecoder(Context& ctx, const ContextData& data)
        {
            const int channels = data.isStereo() ? 2 : 1;
            if (ctx.decoder && ctx.decoder->channels == channels && ctx.decoder->sample_rate == int(data.sampleRate())) return true;
            avcodec_free_context(&ctx.decoder);
            const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_XMAFRAMES);
            if (!codec) return false;
            ctx.decoder = avcodec_alloc_context3(codec);
            if (!ctx.decoder) return false;
            ctx.decoder->channels = channels;
            ctx.decoder->sample_rate = data.sampleRate();
            ctx.decoder->thread_count = 1;
            if (!ctx.frame) ctx.frame = av_frame_alloc();
            if (!ctx.packet) ctx.packet = av_packet_alloc();
            if (!ctx.frame || !ctx.packet || avcodec_open2(ctx.decoder, codec, nullptr) < 0)
            {
                avcodec_free_context(&ctx.decoder);
                return false;
            }
            return true;
        }

        // Assemble one XMA frame across 2 KB packets and alternating input
        // buffers. Packet skip counts select this stream in multichannel data.
        // Bit layout and the XMAFRAMES padding byte follow Xenia's xma_context.
        void ProcessPacket(Context& ctx, ContextData& data)
        {
            if (ctx.remainingSubframes || !data.anyInputValid()) return;
            for (unsigned guard = 0; guard < 8192; ++guard)
            {
                if (!data.inputValid(data.currentBuffer())) return;
                // Check before retiring the last packet: advancing past an
                // end-of-packet loop point must not invalidate a looping voice.
                if (!ctx.copiedBits && UpdateLoopStatus(data,
                    data.inputReadOffset() / kBitsPerPacket >= data.currentPacketCount()))
                    ctx.packetsSkip = 0;
                uint32_t packetIndex = data.inputReadOffset() / kBitsPerPacket;
                if (packetIndex >= data.currentPacketCount())
                {
                    ctx.packetsSkip += packetIndex - data.currentPacketCount();
                    SwapInputBuffer(data);
                    data.setInputReadOffset(0);
                    continue;
                }
                if (ctx.packetsSkip)
                {
                    const auto skip = std::min(ctx.packetsSkip, data.currentPacketCount() - packetIndex);
                    ctx.packetsSkip -= skip;
                    data.setInputReadOffset((packetIndex + skip) * kBitsPerPacket);
                    continue;
                }
                const auto* packet = Physical(data.currentBuffer() ? data.input1Ptr() : data.input0Ptr()) + packetIndex * kBytesPerPacket;
                uint32_t offset = data.inputReadOffset() % kBitsPerPacket;
                if (offset < kBitsPerPacketHeader)
                {
                    offset = ctx.copiedBits ? kBitsPerPacketHeader : ReadBits(packet, 6, 15) + kBitsPerPacketHeader;
                    data.setInputReadOffset(packetIndex * kBitsPerPacket + offset);
                }
                auto nextPacket = [&] {
                    data.setInputReadOffset((packetIndex + 1) * kBitsPerPacket);
                    ctx.packetsSkip = packet[3];
                };
                if (offset >= kBitsPerPacket) { nextPacket(); continue; }
                if (!ctx.copiedBits)
                {
                    ctx.compressed.fill(0);
                    ctx.frameBits = 15;
                }
                uint32_t count = std::min(ctx.frameBits - ctx.copiedBits, kBitsPerPacket - offset);
                for (uint32_t i = 0; i < count; ++i)
                {
                    const uint32_t bit = ReadBits(packet, offset+i, 1);
                    const uint32_t target = ctx.copiedBits+i;
                    ctx.compressed[1+target/8] |= uint8_t(bit << (7-target%8));
                }
                ctx.copiedBits += count;
                offset += count;
                data.setInputReadOffset(packetIndex * kBitsPerPacket + offset);
                if (ctx.frameBits == 15 && ctx.copiedBits == 15)
                {
                    ctx.frameBits = ReadBits(ctx.compressed.data()+1, 0, 15);
                    if (ctx.frameBits < 16 || ctx.frameBits >= 0x7fff)
                    {
                        ctx.copiedBits = ctx.frameBits = 0;
                        nextPacket();
                        continue;
                    }
                }
                if (ctx.copiedBits < ctx.frameBits)
                {
                    if (offset == kBitsPerPacket) nextPacket();
                    continue;
                }
                const bool more = ReadBits(ctx.compressed.data()+1, ctx.frameBits-1, 1) != 0;
                const uint32_t bytes = (ctx.frameBits + 7) / 8;
                ctx.compressed[0] = uint8_t((bytes*8 - ctx.frameBits) << 2);
                ctx.copiedBits = ctx.frameBits = 0;
                if (!more || offset == kBitsPerPacket) nextPacket();
                if (!PrepareDecoder(ctx, data)) return;
                ctx.packet->data = ctx.compressed.data();
                ctx.packet->size = int(bytes + 1);
                const int sent = avcodec_send_packet(ctx.decoder, ctx.packet);
                const int decoded = sent >= 0 ? avcodec_receive_frame(ctx.decoder, ctx.frame) : sent;
                if (decoded < 0 || ctx.frame->nb_samples != 512 || ctx.frame->format != AV_SAMPLE_FMT_FLTP)
                {
                    static unsigned errors = 0;
                    if (errors++ < 16)
                    {
                        const uint32_t id = uint32_t(&ctx - g_contexts);
                        LOG_WARNING("xma frame decode failed: result={} context={} stereo={} input={} offset={} bytes={}",
                            decoded, id, data.isStereo(), data.currentBuffer(), data.inputReadOffset(), bytes + 1);
                        // Exact decoder input, not a later guest-buffer snapshot.
                        // Opt-in and bounded; these private samples stay local.
                        if (const char* directory = getenv("LO_XMA_ERROR_CAPTURE_DIR"))
                        {
                            std::error_code error;
                            std::filesystem::create_directories(directory, error);
                            const auto prefix = std::filesystem::path(directory) / fmt::format("error-{}-context-{}", errors, id);
                            const auto packetPath = prefix.string() + ".frame";
                            if (!error && !std::filesystem::exists(packetPath))
                            {
                                std::ofstream packetFile(packetPath, std::ios::binary);
                                packetFile.write(reinterpret_cast<const char*>(ctx.compressed.data()), bytes + 1);
                                std::ofstream stateFile(prefix.string() + ".txt");
                                stateFile << "result " << decoded << " channels " << (data.isStereo() ? 2 : 1)
                                          << " rate " << data.sampleRate() << "\n";
                                for (uint32_t word : data.d) stateFile << fmt::format("{:08X} ", word);
                                stateFile << "\npacket " << packetIndex << " offset " << offset
                                          << " pending_skip " << ctx.packetsSkip << "\n";
                                const uint32_t historyCount = std::min(ctx.workHistoryCount, 8u);
                                for (uint32_t h = 0; h < historyCount; ++h)
                                {
                                    const auto& snapshot = ctx.workHistory[(ctx.workHistoryCount - historyCount + h) % 8];
                                    stateFile << "work skip " << snapshot.skip << " copied " << snapshot.copied << " words ";
                                    for (uint32_t word : snapshot.data.d) stateFile << fmt::format("{:08X} ", word);
                                    stateFile << "\n";
                                }
                                // Preserve the interleaved packet headers along with
                                // the failed frame. Later streaming refills can replace
                                // this data before an external sampler observes it.
                                if (errors == 1)
                                {
                                    for (unsigned buffer = 0; buffer < 2; ++buffer)
                                    {
                                        const uint32_t pointer = buffer ? data.input1Ptr() : data.input0Ptr();
                                        const uint32_t packets = buffer ? data.input1PacketCount() : data.input0PacketCount();
                                        if (!pointer || !packets || !data.inputValid(buffer != 0)) continue;
                                        const size_t size = size_t(std::min(packets, 32u)) * kBytesPerPacket;
                                        std::ofstream inputFile(prefix.string() + fmt::format("-input{}.bin", buffer), std::ios::binary);
                                        inputFile.write(reinterpret_cast<const char*>(Physical(pointer)), size);
                                    }
                                }
                            }
                        }
                    }
                    return;
                }
                const uint32_t channels = data.isStereo() ? 2 : 1;
                for (uint32_t i = 0; i < 512; ++i)
                    for (uint32_t c = 0; c < channels; ++c)
                    {
                        const float value = reinterpret_cast<float*>(ctx.frame->data[c])[i];
                        const int16_t sample = int16_t(std::lrint(std::clamp(std::isfinite(value) ? value : 0.0f, -1.0f, 1.0f) * 32767.0f));
                        const uint16_t big = ByteSwap(uint16_t(sample));
                        memcpy(ctx.pcm.data() + (i*channels+c)*2, &big, 2);
                    }
                const uint32_t skip = std::min(4u, data.skipCount());
                data.setSkipCount(data.skipCount()-skip);
                ctx.pcmOffset = skip * 256 * channels;
                ctx.remainingSubframes = 4 - skip;
                return;
            }
        }

        void Consume(Context& ctx, ContextData& data, Ring& ring)
        {
            const uint32_t channels = data.isStereo() ? 2 : 1;
            const uint32_t subframes = std::min({ctx.remainingSubframes,
                std::max(1u, data.subframeDecodeCount()), uint32_t(ctx.freeBlocks) / channels});
            const uint32_t bytes = subframes * kOutputBytesPerBlock * channels;
            if (!bytes) return;
            ring.Write(ctx.pcm.data()+ctx.pcmOffset, bytes);
            ctx.pcmOffset += bytes;
            ctx.freeBlocks -= int32_t(subframes * channels);
            ctx.remainingSubframes -= subframes;
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
            const ContextData original = data;
            if (getenv("LO_XMA_ERROR_CAPTURE_DIR"))
                ctx.workHistory[ctx.workHistoryCount++ % 8] = { data, ctx.packetsSkip, ctx.copiedBits };
            // The guest may select the stream's first packet when submitting
            // a new block. A pending skip is relative to our published cursor;
            // applying it again to that explicit cursor selects another stream.
            if (ctx.hasPublishedInputOffset && data.inputReadOffset() != ctx.publishedInputOffset)
                ctx.packetsSkip = 0;

            Ring ring{ Physical(data.outputPtr()), data.outputBlockCount() * kOutputBytesPerBlock,
                       data.outputReadOffset() * kOutputBytesPerBlock, data.outputWriteOffset() * kOutputBytesPerBlock };
            if (ring.capacity == 0 || ring.capacity > kOutputMaxSizeBytes)
            {
                LOG_WARNING("xma: context {} has an output ring of {} bytes", id, ring.capacity);
                return;
            }
            ctx.freeBlocks = int32_t(ring.WriteCount() / kOutputBytesPerBlock);
            const int32_t minimumBlocks = data.isStereo() ? 2 : 1;
            if (minimumBlocks > ctx.freeBlocks)
            {
                return;
            }
            for (int guard = 0; ctx.freeBlocks >= minimumBlocks && guard < 4096; guard++)
            {
                ProcessPacket(ctx, data);
                Consume(ctx, data, ring);
                if ((!data.anyInputValid() && !ctx.remainingSubframes) || data.errorStatus() == 4)
                    break;
            }
            data.setOutputWriteOffset(ring.write / kOutputBytesPerBlock);
            if (ring.read == ring.write)
                data.setOutputValid(false);
            if (g_traceContextWrites)
            {
                // Observe guest changes during decoding before the full context
                // writeback. Do not merge fields or alter synchronization here.
                ContextData current;
                current.Load(guest);
                static unsigned reports = 0;
                for (unsigned word = 0; word < 16; ++word)
                    if (current.d[word] != original.d[word] && reports < 64)
                    {
                        ++reports;
                        LOG_WARNING("xma: concurrent context write id={} word={} before={:08X} guest={:08X} decoded={:08X} published={}",
                            id, word, original.d[word], current.d[word], data.d[word],
                            word == 0 || word == 1 || word == 2 || word == 4);
                    }
            }
            data.StoreDecoded(guest);
            ctx.publishedInputOffset = data.inputReadOffset();
            ctx.hasPublishedInputOffset = true;
            if (g_trace)
                LOG_INFO("xma: context {} worked: in={}/{} read {} write {} valid {}", id, data.input0Valid(), data.input1Valid(),
                    data.outputReadOffset(), data.outputWriteOffset(), data.outputValid());
        }

        void WorkerMain()
        {
            while (g_running)
            {
                bool didWork = false;
                for (uint32_t id = 0; id < kContextCount; ++id)
                {
                    if (!g_contexts[id].enabled.load()) continue;
                    std::lock_guard lock(g_mutex);
                    Work(id);
                    didWork = true;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(didWork ? 250 : 1000));
            }
        }
    }

    bool WriteCommand(uint32_t address, uint32_t value)
    {
        if ((address & 3) || address < kRegisterBase) return false;
        const uint32_t reg = (address - kRegisterBase) / 4;
        uint32_t first;
        if (reg >= kRegKick0 && reg < kRegKick0 + kRegisterGroups) first = kRegKick0;
        else if (reg >= kRegLock0 && reg < kRegLock0 + kRegisterGroups) first = kRegLock0;
        else if (reg >= kRegClear0 && reg < kRegClear0 + kRegisterGroups) first = kRegClear0;
        else return false;

        // These are commands, not mailboxes containing only the last write.
        // Serialize with decoding so lock/clear completes before the guest
        // updates or reuses the context, matching Xenia's context mutex.
        std::lock_guard lock(g_mutex);
        if (!g_arrayGuest) return false;
        *Register(reg) = value;
        for (uint32_t bits = value; bits; bits &= bits - 1)
        {
            const uint32_t id = (reg - first) * 32 + std::countr_zero(bits);
            if (first == kRegKick0) g_contexts[id].enabled = true;
            else if (first == kRegLock0) g_contexts[id].enabled = false;
            else ClearContext(id);
            if (g_trace) LOG_INFO("xma command: register={:#x} context={}", reg, id);
        }
        return true;
    }

    void Init()
    {
        if (g_arrayGuest)
            return;
        g_trace = getenv("LO_TRACE_XMA") != nullptr;
        g_traceContextWrites = getenv("LO_TRACE_XMA_CONTEXT_WRITES") != nullptr;
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
        LOG_INFO("xma: {} contexts at {:#x} (physical {:#x}), FFmpeg XMA frame decoder", kContextCount, g_arrayGuest, g_arrayGuest & 0x1FFFFFFF);
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
                g_contexts[id].Reset();
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
            g_contexts[id].Reset();
            memset(ContextHost(id), 0, kContextSize);
            --g_allocatedCount;
            if (g_trace) LOG_INFO("xma: released context {}", id);
        }
    }
}
