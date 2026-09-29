#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <span>

namespace gpu::renderer
{
    struct DrawInfo
    {
        uint32_t primitiveType = 0;
        uint32_t indexCount = 0;
        bool indexed = false;
        uint32_t indexBase = 0;
        uint32_t indexBufferWords = 0;
        uint32_t indexEndian = 0;
        bool index32 = false;
    };

    // Borrowed only for the synchronous renderer::Draw call. Native producers
    // must retain the immutable owner until consumption, not just a revision
    // number. A GPU upload has its own, longer, slot/fence lifetime.
    class DrawWords
    {
    public:
        DrawWords() = default;
        explicit DrawWords(std::span<const uint32_t> words) : words_(words) {}
        static DrawWords Legacy(std::span<const uint32_t> words, const uint8_t* zeroFallbackBE)
        {
            DrawWords result(words);
            result.zeroFallbackBE_ = zeroFallbackBE;
            return result;
        }
        size_t Size() const { return words_.size(); }
        bool HasLegacyFallback() const { return zeroFallbackBE_ != nullptr; }
        DrawWords NativeValues() const { return DrawWords(words_); }
        uint32_t Read(size_t index) const
        {
            if (index >= words_.size()) return 0;
            const auto value = words_[index];
            if (value || !zeroFallbackBE_) return value;
            uint32_t fallback;
            std::memcpy(&fallback, zeroFallbackBE_ + index * 4, 4);
            if constexpr (std::endian::native == std::endian::little)
                fallback = (fallback >> 24) | ((fallback >> 8) & 0xff00u) |
                    ((fallback << 8) & 0xff0000u) | (fallback << 24);
            return fallback;
        }
        float ReadFloat(size_t index) const { return std::bit_cast<float>(Read(index)); }
        DrawWords Subspan(size_t first, size_t count) const
        {
            if (first >= words_.size()) return {};
            auto result = DrawWords(words_.subspan(first, std::min(count, words_.size() - first)));
            if (zeroFallbackBE_) result.zeroFallbackBE_ = zeroFallbackBE_ + first * 4;
            return result;
        }
        void Copy(std::span<uint32_t> destination) const
        {
            const size_t count = std::min(destination.size(), words_.size());
            if (count && !zeroFallbackBE_)
                std::memcpy(destination.data(), words_.data(), count * sizeof(uint32_t));
            else if (count)
            {
                // Bounds and fallback policy are invariant across this bank.
                // Do not route every ALU lane through the general Read view:
                // the extra per-lane tests inhibit bulk-loop optimization.
                const auto* source = words_.data();
                const auto* fallbackBE = zeroFallbackBE_;
                for (size_t i = 0; i < count; ++i)
                {
                    uint32_t value = source[i];
                    if (value == 0)
                    {
                        std::memcpy(&value, fallbackBE + i * 4, 4);
                        if constexpr (std::endian::native == std::endian::little)
                            value = (value >> 24) | ((value >> 8) & 0xff00u) |
                                ((value << 8) & 0xff0000u) | (value << 24);
                    }
                    destination[i] = value;
                }
            }
            std::fill(destination.begin() + count, destination.end(), 0);
        }
    private:
        std::span<const uint32_t> words_;
        const uint8_t* zeroFallbackBE_ = nullptr;
    };

    struct ShaderBinding
    {
        // Xenos microcode retains its existing byte order and identity rules.
        std::span<const uint32_t> words;
        uint64_t commandHash = 0;
        uint64_t byteHash = 0;
    };
    struct TargetState
    {
        uint32_t surfaceInfo = 0;
        std::array<uint32_t, 4> colorInfo{};
        uint32_t depthInfo = 0;
    };
    struct PipelineState
    {
        uint32_t modeControl = 0, depthControl = 0, blendControl = 0;
        uint32_t colorControl = 0, colorMask = 0, modeCull = 0;
        uint32_t stencilRefMask = 0, stencilRefMaskBack = 0;
        uint32_t clipControl = 0, pointSize = 0, pointMinMax = 0;
        float alphaRef = 0;
        std::array<float, 4> polygonOffset{};
    };
    struct ViewportState
    {
        uint32_t windowOffset = 0, scissorTL = 0, scissorBR = 0;
        uint32_t transformControl = 0, vertexControl = 0;
        std::array<float, 6> scaleOffset{}; // xs, xo, ys, yo, zs, zo
    };
    struct ResolveState
    {
        uint32_t control = 0, destinationBase = 0, destinationPitch = 0;
        uint32_t destinationInfo = 0, colorClear = 0, depthClear = 0;
    };
    struct StateRevision
    {
        uint64_t generation = 0, value = 0;
        // Zero is explicitly untracked. Legacy/direct MMIO writes may not use
        // revision-only reuse. Resource identity also needs its own generation.
        bool Tracked() const { return generation != 0 && value != 0; }
        bool operator==(const StateRevision&) const = default;
    };
    struct DrawState
    {
        DrawInfo draw;
        TargetState targets;
        PipelineState pipeline;
        ViewportState viewport;
        ResolveState resolve;
        int32_t baseVertex = 0;
        ShaderBinding vertexShader, pixelShader;
        DrawWords vertexConstants, pixelConstants;
        DrawWords fetchConstants, boolConstants, loopConstants;
        StateRevision shaderRevision, vertexConstantRevision, pixelConstantRevision;
        StateRevision bindingRevision, pipelineRevision, targetRevision;
        uint32_t AluConstant(size_t index) const
        {
            return index < 1024 ? vertexConstants.Read(index) : pixelConstants.Read(index - 1024);
        }
        float AluConstantFloat(size_t index) const { return std::bit_cast<float>(AluConstant(index)); }
        // Optional legacy diagnostic view, never used for rendering. Native
        // state is not expanded into a full register bank just for this trace.
        DrawWords legacyTraceRegisters;
    };
}
