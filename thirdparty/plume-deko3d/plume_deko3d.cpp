//
// plume deko3d back end (banjo-nx, 2026-10-17): device, memory, resources, shaders and pipelines. The command lists,
// the queue and the swapchain are in plume_deko3d_commands.cpp. docs/deko3d-renderer.md is the design; the facts about
// deko3d 0.5.0 used here (and its bugs) are listed there with where they were found.
//
#include "plume_deko3d.h"

#include <switch.h>
#include <pthread.h>

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <unistd.h>

#include <uam_api.h>

namespace plume {
    // ---- helpers

    static void (*g_logCallback)(const char *message) = nullptr;
    void SetDeko3DLogCallback(void (*callback)(const char *message)) { g_logCallback = callback; }

    void deko3dLog(const char *fmt, ...) {
        char line[1024];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(line, sizeof(line), fmt, ap);
        va_end(ap);
        if (g_logCallback)
            g_logCallback(line);
        else
            fprintf(stderr, "[deko3d] %s\n", line);
    }
#define logf deko3dLog

    // Free memory, for the log lines that may be about running out of it: the heap's free bytes (newlib) and the
    // process's unused memory (the system's figures).
    extern "C" char *fake_heap_end; // libnx: the heap's end (sbrk grows towards it)
    static void memoryFigures(uint64_t &heapFree, uint64_t &processFree) {
        // Free chunks inside what malloc has taken, and what sbrk can still give.
        struct mallinfo mi = mallinfo();
        char *brk = static_cast<char *>(sbrk(0));
        heapFree = uint64_t(mi.fordblks) + (fake_heap_end > brk ? uint64_t(fake_heap_end - brk) : 0);
        uint64_t total = 0, used = 0;
        svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
        svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
        processFree = total > used ? total - used : 0;
    }

    static constexpr uint32_t alignUp(uint32_t v, uint32_t a) { return (v + a - 1) / a * a; }
    static constexpr uint64_t alignUp64(uint64_t v, uint64_t a) { return (v + a - 1) / a * a; }

    // FNV-1a 64: the variant cache key (tools/deko3d_shaders.py computes the same over the same text).
    static uint64_t fnv1a64(const void *data, size_t size) {
        const uint8_t *p = static_cast<const uint8_t *>(data);
        uint64_t h = 0xcbf29ce484222325ull;
        for (size_t i = 0; i < size; i++) {
            h ^= p[i];
            h *= 0x100000001b3ull;
        }
        return h;
    }

    static uint64_t mixHash(uint64_t h, const void *data, size_t size) {
        const uint8_t *p = static_cast<const uint8_t *>(data);
        for (size_t i = 0; i < size; i++) {
            h ^= p[i];
            h *= 0x100000001b3ull;
        }
        return h;
    }

    Deko3DStats g_deko3dStats;
    // PLUME_DEKO3D_GUARDS: bytes after each block's storage, and their value.
    static constexpr uint32_t GuardBytes = 64u << 10;
    static constexpr uint8_t GuardByte = 0xA5;


    Deko3DOptions g_deko3dOptions;
    void Deko3DSetOptions(const Deko3DOptions &options) { g_deko3dOptions = options; }
    void Deko3DSetGpuOptions(const Deko3DGpuOptions &options) {
        g_deko3dOptions.depthCompression = options.depthCompression;
        g_deko3dOptions.zcull = options.zcull;
        g_deko3dOptions.mergeBarriers = options.mergeBarriers;
        g_deko3dOptions.leanBarriers = options.leanBarriers;
        g_deko3dOptions.vsOutputs = options.vsOutputs;
        g_deko3dOptions.colorCompression = options.colorCompression;
    }
    Deko3DCounters Deko3DReadCounters() {
        const Deko3DStats &s = g_deko3dStats;
        const auto r = [](const std::atomic<uint64_t> &v) { return v.load(std::memory_order_relaxed); };
        Deko3DCounters c;
        c.draws = r(s.draws); c.pipelineBinds = r(s.pipelineBinds); c.stateBinds = r(s.stateBinds);
        c.descriptorWrites = r(s.descriptorWrites);
        c.barriersPrimitives = r(s.barriersPrimitives); c.barriersFragments = r(s.barriersFragments);
        c.barriersFull = r(s.barriersFull); c.barriersImageInvalidate = r(s.barriersImageInvalidate);
        c.copyBuffer = r(s.copyBuffer); c.copyUpload = r(s.copyUpload); c.copyReadback = r(s.copyReadback);
        c.copyImage = r(s.copyImage); c.copyResolve = r(s.copyResolve); c.copyBlit = r(s.copyBlit);
        return c;
    }

    static Deko3DShaderCacheLookup g_cacheLookup;
    static Deko3DShaderCacheStore g_cacheStore;
    void SetDeko3DShaderCache(Deko3DShaderCacheLookup lookup, Deko3DShaderCacheStore store) {
        g_cacheLookup = std::move(lookup);
        g_cacheStore = std::move(store);
    }

    // ---- formats

    DkImageFormat toDkFormat(RenderFormat format) {
        switch (format) {
        case RenderFormat::R32G32B32A32_TYPELESS:
        case RenderFormat::R32G32B32A32_FLOAT: return DkImageFormat_RGBA32_Float;
        case RenderFormat::R32G32B32A32_UINT: return DkImageFormat_RGBA32_Uint;
        case RenderFormat::R32G32B32A32_SINT: return DkImageFormat_RGBA32_Sint;
        case RenderFormat::R32G32B32_TYPELESS:
        case RenderFormat::R32G32B32_FLOAT: return DkImageFormat_RGB32_Float;
        case RenderFormat::R32G32B32_UINT: return DkImageFormat_RGB32_Uint;
        case RenderFormat::R32G32B32_SINT: return DkImageFormat_RGB32_Sint;
        case RenderFormat::R16G16B16A16_TYPELESS:
        case RenderFormat::R16G16B16A16_FLOAT: return DkImageFormat_RGBA16_Float;
        case RenderFormat::R16G16B16A16_UNORM: return DkImageFormat_RGBA16_Unorm;
        case RenderFormat::R16G16B16A16_UINT: return DkImageFormat_RGBA16_Uint;
        case RenderFormat::R16G16B16A16_SNORM: return DkImageFormat_RGBA16_Snorm;
        case RenderFormat::R16G16B16A16_SINT: return DkImageFormat_RGBA16_Sint;
        case RenderFormat::R32G32_TYPELESS:
        case RenderFormat::R32G32_FLOAT: return DkImageFormat_RG32_Float;
        case RenderFormat::R32G32_UINT: return DkImageFormat_RG32_Uint;
        case RenderFormat::R32G32_SINT: return DkImageFormat_RG32_Sint;
        case RenderFormat::R8G8B8A8_TYPELESS:
        case RenderFormat::R8G8B8A8_UNORM: return DkImageFormat_RGBA8_Unorm;
        case RenderFormat::R8G8B8A8_UINT: return DkImageFormat_RGBA8_Uint;
        case RenderFormat::R8G8B8A8_SNORM: return DkImageFormat_RGBA8_Snorm;
        case RenderFormat::R8G8B8A8_SINT: return DkImageFormat_RGBA8_Sint;
        case RenderFormat::B8G8R8A8_UNORM: return DkImageFormat_BGRA8_Unorm;
        case RenderFormat::R16G16_TYPELESS:
        case RenderFormat::R16G16_FLOAT: return DkImageFormat_RG16_Float;
        case RenderFormat::R16G16_UNORM: return DkImageFormat_RG16_Unorm;
        case RenderFormat::R16G16_UINT: return DkImageFormat_RG16_Uint;
        case RenderFormat::R16G16_SNORM: return DkImageFormat_RG16_Snorm;
        case RenderFormat::R16G16_SINT: return DkImageFormat_RG16_Sint;
        case RenderFormat::R32_TYPELESS:
        case RenderFormat::R32_FLOAT: return DkImageFormat_R32_Float;
        case RenderFormat::D32_FLOAT: return DkImageFormat_ZF32;
        case RenderFormat::D32_FLOAT_S8_UINT: return DkImageFormat_ZF32_X24S8;
        case RenderFormat::R32_UINT: return DkImageFormat_R32_Uint;
        case RenderFormat::R32_SINT: return DkImageFormat_R32_Sint;
        case RenderFormat::R8G8_TYPELESS:
        case RenderFormat::R8G8_UNORM: return DkImageFormat_RG8_Unorm;
        case RenderFormat::R8G8_UINT: return DkImageFormat_RG8_Uint;
        case RenderFormat::R8G8_SNORM: return DkImageFormat_RG8_Snorm;
        case RenderFormat::R8G8_SINT: return DkImageFormat_RG8_Sint;
        case RenderFormat::R16_TYPELESS:
        case RenderFormat::R16_FLOAT: return DkImageFormat_R16_Float;
        case RenderFormat::D16_UNORM: return DkImageFormat_Z16;
        case RenderFormat::R16_UNORM: return DkImageFormat_R16_Unorm;
        case RenderFormat::R16_UINT: return DkImageFormat_R16_Uint;
        case RenderFormat::R16_SNORM: return DkImageFormat_R16_Snorm;
        case RenderFormat::R16_SINT: return DkImageFormat_R16_Sint;
        case RenderFormat::R8_TYPELESS:
        case RenderFormat::R8_UNORM: return DkImageFormat_R8_Unorm;
        case RenderFormat::R8_UINT: return DkImageFormat_R8_Uint;
        case RenderFormat::R8_SNORM: return DkImageFormat_R8_Snorm;
        case RenderFormat::R8_SINT: return DkImageFormat_R8_Sint;
        case RenderFormat::BC1_TYPELESS:
        case RenderFormat::BC1_UNORM: return DkImageFormat_RGBA_BC1;
        case RenderFormat::BC1_UNORM_SRGB: return DkImageFormat_RGBA_BC1_sRGB;
        case RenderFormat::BC2_TYPELESS:
        case RenderFormat::BC2_UNORM: return DkImageFormat_RGBA_BC2;
        case RenderFormat::BC2_UNORM_SRGB: return DkImageFormat_RGBA_BC2_sRGB;
        case RenderFormat::BC3_TYPELESS:
        case RenderFormat::BC3_UNORM: return DkImageFormat_RGBA_BC3;
        case RenderFormat::BC3_UNORM_SRGB: return DkImageFormat_RGBA_BC3_sRGB;
        case RenderFormat::BC4_TYPELESS:
        case RenderFormat::BC4_UNORM: return DkImageFormat_R_BC4_Unorm;
        case RenderFormat::BC4_SNORM: return DkImageFormat_R_BC4_Snorm;
        case RenderFormat::BC5_TYPELESS:
        case RenderFormat::BC5_UNORM: return DkImageFormat_RG_BC5_Unorm;
        case RenderFormat::BC5_SNORM: return DkImageFormat_RG_BC5_Snorm;
        case RenderFormat::BC6H_TYPELESS:
        case RenderFormat::BC6H_UF16: return DkImageFormat_RGBA_BC6H_UF16_Float;
        case RenderFormat::BC6H_SF16: return DkImageFormat_RGBA_BC6H_SF16_Float;
        case RenderFormat::BC7_TYPELESS:
        case RenderFormat::BC7_UNORM: return DkImageFormat_RGBA_BC7_Unorm;
        case RenderFormat::BC7_UNORM_SRGB: return DkImageFormat_RGBA_BC7_Unorm_sRGB;
        // LostOdysseyRecomp's packed colour targets (B10G10R10A2 has no deko3d format: the RGB10A2 one, R and B swapped).
        case RenderFormat::R10G10B10A2_UNORM:
        case RenderFormat::B10G10R10A2_UNORM: return DkImageFormat_RGB10A2_Unorm;
        case RenderFormat::R11G11B10_FLOAT: return DkImageFormat_RG11B10_Float;
        default: return DkImageFormat_None;
        }
    }

    static bool isDepthFormat(DkImageFormat f) {
        return f == DkImageFormat_Z16 || f == DkImageFormat_Z24X8 || f == DkImageFormat_ZF32 ||
               f == DkImageFormat_Z24S8 || f == DkImageFormat_ZF32_X24S8 || f == DkImageFormat_S8;
    }

    static bool isCompressedFormat(DkImageFormat f) {
        return f >= DkImageFormat_RGB_BC1 && f <= DkImageFormat_RGBA_ASTC_12x12_sRGB;
    }

    static bool isSrgbFormat(DkImageFormat f) {
        return f == DkImageFormat_RGBA8_Unorm_sRGB || f == DkImageFormat_RGBX8_Unorm_sRGB ||
               f == DkImageFormat_BGRA8_Unorm_sRGB || f == DkImageFormat_BGRX8_Unorm_sRGB;
    }

    // The 2D engine (blits, MSAA resolves) takes uncompressed colour formats.
    static bool canUse2DEngine(DkImageFormat f) {
        return !isDepthFormat(f) && !isCompressedFormat(f) && f != DkImageFormat_RGB32_Float &&
               f != DkImageFormat_RGB32_Uint && f != DkImageFormat_RGB32_Sint;
    }

    // What a texture of this plume format is made of when it is a depth target (R32_TYPELESS and friends).
    static DkImageFormat depthTargetFormat(RenderFormat format) {
        switch (format) {
        case RenderFormat::R32_TYPELESS:
        case RenderFormat::R32_FLOAT:
        case RenderFormat::D32_FLOAT: return DkImageFormat_ZF32;
        case RenderFormat::R16_TYPELESS:
        case RenderFormat::R16_UNORM:
        case RenderFormat::D16_UNORM: return DkImageFormat_Z16;
        case RenderFormat::D32_FLOAT_S8_UINT: return DkImageFormat_ZF32_X24S8;
        default: return toDkFormat(format);
        }
    }

    static bool vertexFormat(RenderFormat format, DkVtxAttribSize &size, DkVtxAttribType &type, bool &bgra) {
        bgra = false;
        switch (format) {
        case RenderFormat::R32G32B32A32_FLOAT: size = DkVtxAttribSize_4x32; type = DkVtxAttribType_Float; return true;
        case RenderFormat::R32G32B32A32_UINT: size = DkVtxAttribSize_4x32; type = DkVtxAttribType_Uint; return true;
        case RenderFormat::R32G32B32A32_SINT: size = DkVtxAttribSize_4x32; type = DkVtxAttribType_Sint; return true;
        case RenderFormat::R32G32B32_FLOAT: size = DkVtxAttribSize_3x32; type = DkVtxAttribType_Float; return true;
        case RenderFormat::R32G32B32_UINT: size = DkVtxAttribSize_3x32; type = DkVtxAttribType_Uint; return true;
        case RenderFormat::R32G32B32_SINT: size = DkVtxAttribSize_3x32; type = DkVtxAttribType_Sint; return true;
        case RenderFormat::R32G32_FLOAT: size = DkVtxAttribSize_2x32; type = DkVtxAttribType_Float; return true;
        case RenderFormat::R32G32_UINT: size = DkVtxAttribSize_2x32; type = DkVtxAttribType_Uint; return true;
        case RenderFormat::R32G32_SINT: size = DkVtxAttribSize_2x32; type = DkVtxAttribType_Sint; return true;
        case RenderFormat::R32_FLOAT: size = DkVtxAttribSize_1x32; type = DkVtxAttribType_Float; return true;
        case RenderFormat::R32_UINT: size = DkVtxAttribSize_1x32; type = DkVtxAttribType_Uint; return true;
        case RenderFormat::R32_SINT: size = DkVtxAttribSize_1x32; type = DkVtxAttribType_Sint; return true;
        case RenderFormat::R16G16B16A16_FLOAT: size = DkVtxAttribSize_4x16; type = DkVtxAttribType_Float; return true;
        case RenderFormat::R16G16B16A16_UNORM: size = DkVtxAttribSize_4x16; type = DkVtxAttribType_Unorm; return true;
        case RenderFormat::R16G16B16A16_SNORM: size = DkVtxAttribSize_4x16; type = DkVtxAttribType_Snorm; return true;
        case RenderFormat::R16G16B16A16_UINT: size = DkVtxAttribSize_4x16; type = DkVtxAttribType_Uint; return true;
        case RenderFormat::R16G16B16A16_SINT: size = DkVtxAttribSize_4x16; type = DkVtxAttribType_Sint; return true;
        case RenderFormat::R16G16_FLOAT: size = DkVtxAttribSize_2x16; type = DkVtxAttribType_Float; return true;
        case RenderFormat::R16G16_UNORM: size = DkVtxAttribSize_2x16; type = DkVtxAttribType_Unorm; return true;
        case RenderFormat::R16G16_SNORM: size = DkVtxAttribSize_2x16; type = DkVtxAttribType_Snorm; return true;
        case RenderFormat::R16G16_UINT: size = DkVtxAttribSize_2x16; type = DkVtxAttribType_Uint; return true;
        case RenderFormat::R16G16_SINT: size = DkVtxAttribSize_2x16; type = DkVtxAttribType_Sint; return true;
        case RenderFormat::R16_FLOAT: size = DkVtxAttribSize_1x16; type = DkVtxAttribType_Float; return true;
        case RenderFormat::R16_UNORM: size = DkVtxAttribSize_1x16; type = DkVtxAttribType_Unorm; return true;
        case RenderFormat::R16_SNORM: size = DkVtxAttribSize_1x16; type = DkVtxAttribType_Snorm; return true;
        case RenderFormat::R16_UINT: size = DkVtxAttribSize_1x16; type = DkVtxAttribType_Uint; return true;
        case RenderFormat::R16_SINT: size = DkVtxAttribSize_1x16; type = DkVtxAttribType_Sint; return true;
        case RenderFormat::R8G8B8A8_UNORM: size = DkVtxAttribSize_4x8; type = DkVtxAttribType_Unorm; return true;
        case RenderFormat::R8G8B8A8_SNORM: size = DkVtxAttribSize_4x8; type = DkVtxAttribType_Snorm; return true;
        case RenderFormat::R8G8B8A8_UINT: size = DkVtxAttribSize_4x8; type = DkVtxAttribType_Uint; return true;
        case RenderFormat::R8G8B8A8_SINT: size = DkVtxAttribSize_4x8; type = DkVtxAttribType_Sint; return true;
        case RenderFormat::B8G8R8A8_UNORM: size = DkVtxAttribSize_4x8; type = DkVtxAttribType_Unorm; bgra = true; return true;
        case RenderFormat::R8G8_UNORM: size = DkVtxAttribSize_2x8; type = DkVtxAttribType_Unorm; return true;
        case RenderFormat::R8G8_SNORM: size = DkVtxAttribSize_2x8; type = DkVtxAttribType_Snorm; return true;
        case RenderFormat::R8G8_UINT: size = DkVtxAttribSize_2x8; type = DkVtxAttribType_Uint; return true;
        case RenderFormat::R8G8_SINT: size = DkVtxAttribSize_2x8; type = DkVtxAttribType_Sint; return true;
        case RenderFormat::R8_UNORM: size = DkVtxAttribSize_1x8; type = DkVtxAttribType_Unorm; return true;
        case RenderFormat::R8_SNORM: size = DkVtxAttribSize_1x8; type = DkVtxAttribType_Snorm; return true;
        case RenderFormat::R8_UINT: size = DkVtxAttribSize_1x8; type = DkVtxAttribType_Uint; return true;
        case RenderFormat::R8_SINT: size = DkVtxAttribSize_1x8; type = DkVtxAttribType_Sint; return true;
        default: return false;
        }
    }

    static DkCompareOp toDk(RenderComparisonFunction f) {
        switch (f) {
        case RenderComparisonFunction::NEVER: return DkCompareOp_Never;
        case RenderComparisonFunction::LESS: return DkCompareOp_Less;
        case RenderComparisonFunction::EQUAL: return DkCompareOp_Equal;
        case RenderComparisonFunction::LESS_EQUAL: return DkCompareOp_Lequal;
        case RenderComparisonFunction::GREATER: return DkCompareOp_Greater;
        case RenderComparisonFunction::NOT_EQUAL: return DkCompareOp_NotEqual;
        case RenderComparisonFunction::GREATER_EQUAL: return DkCompareOp_Gequal;
        case RenderComparisonFunction::ALWAYS:
        default: return DkCompareOp_Always;
        }
    }

    static DkStencilOp toDk(RenderStencilOp op) {
        switch (op) {
        case RenderStencilOp::ZERO: return DkStencilOp_Zero;
        case RenderStencilOp::REPLACE: return DkStencilOp_Replace;
        case RenderStencilOp::INCREMENT_AND_CLAMP: return DkStencilOp_Incr;
        case RenderStencilOp::DECREMENT_AND_CLAMP: return DkStencilOp_Decr;
        case RenderStencilOp::INVERT: return DkStencilOp_Invert;
        case RenderStencilOp::INCREMENT_AND_WRAP: return DkStencilOp_IncrWrap;
        case RenderStencilOp::DECREMENT_AND_WRAP: return DkStencilOp_DecrWrap;
        case RenderStencilOp::KEEP:
        default: return DkStencilOp_Keep;
        }
    }

    static DkBlendFactor toDk(RenderBlend b) {
        switch (b) {
        case RenderBlend::ZERO: return DkBlendFactor_Zero;
        case RenderBlend::ONE: return DkBlendFactor_One;
        case RenderBlend::SRC_COLOR: return DkBlendFactor_SrcColor;
        case RenderBlend::INV_SRC_COLOR: return DkBlendFactor_InvSrcColor;
        case RenderBlend::SRC_ALPHA: return DkBlendFactor_SrcAlpha;
        case RenderBlend::INV_SRC_ALPHA: return DkBlendFactor_InvSrcAlpha;
        case RenderBlend::DEST_ALPHA: return DkBlendFactor_DstAlpha;
        case RenderBlend::INV_DEST_ALPHA: return DkBlendFactor_InvDstAlpha;
        case RenderBlend::DEST_COLOR: return DkBlendFactor_DstColor;
        case RenderBlend::INV_DEST_COLOR: return DkBlendFactor_InvDstColor;
        case RenderBlend::SRC_ALPHA_SAT: return DkBlendFactor_SrcAlphaSaturate;
        case RenderBlend::BLEND_FACTOR: return DkBlendFactor_ConstColor;
        case RenderBlend::INV_BLEND_FACTOR: return DkBlendFactor_InvConstColor;
        case RenderBlend::SRC1_COLOR: return DkBlendFactor_Src1Color;
        case RenderBlend::INV_SRC1_COLOR: return DkBlendFactor_InvSrc1Color;
        case RenderBlend::SRC1_ALPHA: return DkBlendFactor_Src1Alpha;
        case RenderBlend::INV_SRC1_ALPHA: return DkBlendFactor_InvSrc1Alpha;
        default: return DkBlendFactor_One;
        }
    }

    // In the alpha factors, the constant's colour factors mean its alpha (D3D's BLEND_FACTOR on alpha).
    static DkBlendFactor toDkAlpha(RenderBlend b) {
        switch (b) {
        case RenderBlend::BLEND_FACTOR: return DkBlendFactor_ConstAlpha;
        case RenderBlend::INV_BLEND_FACTOR: return DkBlendFactor_InvConstAlpha;
        case RenderBlend::SRC_COLOR: return DkBlendFactor_SrcAlpha;
        case RenderBlend::INV_SRC_COLOR: return DkBlendFactor_InvSrcAlpha;
        case RenderBlend::DEST_COLOR: return DkBlendFactor_DstAlpha;
        case RenderBlend::INV_DEST_COLOR: return DkBlendFactor_InvDstAlpha;
        case RenderBlend::SRC1_COLOR: return DkBlendFactor_Src1Alpha;
        case RenderBlend::INV_SRC1_COLOR: return DkBlendFactor_InvSrc1Alpha;
        default: return toDk(b);
        }
    }

    static DkBlendOp toDk(RenderBlendOperation op) {
        switch (op) {
        case RenderBlendOperation::SUBTRACT: return DkBlendOp_Sub;
        case RenderBlendOperation::REV_SUBTRACT: return DkBlendOp_RevSub;
        case RenderBlendOperation::MIN: return DkBlendOp_Min;
        case RenderBlendOperation::MAX: return DkBlendOp_Max;
        case RenderBlendOperation::ADD:
        default: return DkBlendOp_Add;
        }
    }

    static DkLogicOp toDk(RenderLogicOperation op) {
        switch (op) {
        case RenderLogicOperation::CLEAR: return DkLogicOp_Clear;
        case RenderLogicOperation::SET: return DkLogicOp_Set;
        case RenderLogicOperation::COPY: return DkLogicOp_Copy;
        case RenderLogicOperation::COPY_INVERTED: return DkLogicOp_CopyInverted;
        case RenderLogicOperation::NOOP: return DkLogicOp_NoOp;
        case RenderLogicOperation::INVERT: return DkLogicOp_Invert;
        case RenderLogicOperation::AND: return DkLogicOp_And;
        case RenderLogicOperation::NAND: return DkLogicOp_Nand;
        case RenderLogicOperation::OR: return DkLogicOp_Or;
        case RenderLogicOperation::NOR: return DkLogicOp_Nor;
        case RenderLogicOperation::XOR: return DkLogicOp_Xor;
        case RenderLogicOperation::EQUIV: return DkLogicOp_Equivalent;
        case RenderLogicOperation::AND_REVERSE: return DkLogicOp_AndReverse;
        case RenderLogicOperation::AND_INVERTED: return DkLogicOp_AndInverted;
        case RenderLogicOperation::OR_REVERSE: return DkLogicOp_OrReverse;
        case RenderLogicOperation::OR_INVERTED: return DkLogicOp_OrInverted;
        default: return DkLogicOp_Copy;
        }
    }

    static DkWrapMode toDk(RenderTextureAddressMode m) {
        switch (m) {
        case RenderTextureAddressMode::MIRROR: return DkWrapMode_MirroredRepeat;
        case RenderTextureAddressMode::CLAMP: return DkWrapMode_ClampToEdge;
        case RenderTextureAddressMode::BORDER: return DkWrapMode_ClampToBorder;
        case RenderTextureAddressMode::MIRROR_ONCE: return DkWrapMode_MirrorClampToEdge;
        case RenderTextureAddressMode::WRAP:
        default: return DkWrapMode_Repeat;
        }
    }

    static DkImageSwizzle toDk(RenderSwizzle s, DkImageSwizzle identity) {
        switch (s) {
        case RenderSwizzle::ZERO: return DkImageSwizzle_Zero;
        case RenderSwizzle::ONE: return DkImageSwizzle_One;
        case RenderSwizzle::R: return DkImageSwizzle_Red;
        case RenderSwizzle::G: return DkImageSwizzle_Green;
        case RenderSwizzle::B: return DkImageSwizzle_Blue;
        case RenderSwizzle::A: return DkImageSwizzle_Alpha;
        case RenderSwizzle::IDENTITY:
        default: return identity;
        }
    }

    static DkMsMode toDkMsMode(RenderSampleCounts count) {
        switch (count) {
        case RenderSampleCount::COUNT_2: return DkMsMode_2x;
        case RenderSampleCount::COUNT_4: return DkMsMode_4x;
        case RenderSampleCount::COUNT_8: return DkMsMode_8x;
        default: return DkMsMode_1x;
        }
    }

    // deko3d 0.5.0's tile height bug (SwitchWakerHD, memory.cpp image_tile_size_fix): dkImageLayoutInitialize picks the
    // tile from 1.5 x the height and its layout shrinks it for level 0, but the engines and the descriptor use the
    // unshrunk one. When the two differ, the shrunk tile is given as a custom tile size.
    static void tileSizeFix(DkImageLayoutMaker &m, uint32_t rows) {
        if (m.flags & (DkImageFlags_PitchLinear | DkImageFlags_CustomTileSize | DkImageFlags_UsageVideo))
            return;
        if (m.type == DkImageType_1D || m.type == DkImageType_1DArray || m.type == DkImageType_3D || m.type == DkImageType_Buffer)
            return;
        if ((m.type == DkImageType_2DMS || m.type == DkImageType_2DMSArray) && (m.msMode == DkMsMode_4x || m.msMode == DkMsMode_8x))
            rows *= 2;
        const uint32_t gobs = (rows + rows / 2 + 7) / 8;
        uint32_t tile = gobs >= 16 ? 4 : gobs >= 8 ? 3 : gobs >= 4 ? 2 : gobs >= 2 ? 1 : 0;
        uint32_t shrunk = tile;
        while (shrunk && (8u << (shrunk - 1)) >= rows)
            shrunk--;
        if (shrunk == tile)
            return;
        m.flags |= DkImageFlags_CustomTileSize;
        m.tileSize = DkTileSize(shrunk);
    }

    // ---- Deko3DBuffer

    Deko3DBuffer::Deko3DBuffer(Deko3DDevice *device, const RenderBufferDesc &desc) : device(device), desc(desc) {
        const uint64_t size = std::max<uint64_t>(alignUp64(desc.size, DK_MEMBLOCK_ALIGNMENT), DK_MEMBLOCK_ALIGNMENT);
        if (size > 0xFFFFFFFFull) {
            logf("buffer of %" PRIu64 " bytes is too large", desc.size);
            return;
        }
        uint32_t flags = DkMemBlockFlags_GpuCached;
        // Uncached for the CPU, so the GPU sees the CPU's writes and the CPU the GPU's after a fence (LostOdysseyRecomp's
        // plume has no HOST_CACHED upload buffers).
        cpuCached = false;
        flags |= cpuCached ? DkMemBlockFlags_CpuCached : DkMemBlockFlags_CpuUncached;
        block = device->makeBlock(uint32_t(size), flags, "buffer");
        if (!block)
            return;
        gpu = dkMemBlockGetGpuAddr(block);
        cpu = static_cast<uint8_t *>(dkMemBlockGetCpuAddr(block));
        g_deko3dStats.bufferBytes += size;
        {
            std::lock_guard<std::mutex> lock(device->bufferMutex);
            device->bufferRanges[gpu] = {uint32_t(size), cpu};
        }
    }

    Deko3DBuffer::~Deko3DBuffer() {
        if (block) {
            {
                std::lock_guard<std::mutex> lock(device->bufferMutex);
                device->bufferRanges.erase(gpu);
            }
            g_deko3dStats.bufferBytes -= dkMemBlockGetSize(block);
            device->retire({}, block);
        }
    }

    void *Deko3DBuffer::map(uint32_t, const RenderRange *) { return cpu; }

    void Deko3DBuffer::unmap(uint32_t, const RenderRange *writtenRange) {
        if (!cpuCached || !block)
            return;
        uint64_t begin = 0, end = desc.size;
        if (writtenRange != nullptr) {
            begin = writtenRange->begin;
            end = writtenRange->end;
        }
        // banjo-nx (2026-10-19, CPU proposal 14): a clean (dc cvac) of the written lines, not deko3d's clean and
        // invalidate (dc civac): only upload buffers are CPU-cached (written by the CPU, read by the GPU, never read
        // back), so the lines may stay valid for the ring's next pass.
        if (end > begin)
            armDCacheClean(static_cast<uint8_t *>(cpu) + begin, size_t(end - begin));
    }

    std::unique_ptr<RenderBufferFormattedView> Deko3DBuffer::createBufferFormattedView(RenderFormat format) {
        auto view = std::make_unique<Deko3DBufferFormattedView>();
        view->format = format;
        view->buffer = this;
        return view;
    }

    void Deko3DBuffer::setName(const std::string &) {}

    uint64_t Deko3DBuffer::getDeviceAddress() const { return gpu; }

    // ---- Deko3DTexture

    Deko3DTexture::Deko3DTexture(Deko3DDevice *device, const RenderTextureDesc &desc) : device(device), desc(desc) {
        const bool depth = (desc.flags & RenderTextureFlag::DEPTH_TARGET) != 0;
        format = depth ? depthTargetFormat(desc.format) : toDkFormat(desc.format);
        if (format == DkImageFormat_None) {
            logf("texture format %d has no deko3d format", int(desc.format));
            return;
        }
        const uint32_t layers = std::max<uint32_t>(desc.arraySize, 1);
        const bool multisampled = desc.multisampling.sampleCount > RenderSampleCount::COUNT_1;
        switch (desc.dimension) {
        case RenderTextureDimension::TEXTURE_1D:
            type = layers > 1 ? DkImageType_1DArray : DkImageType_1D;
            break;
        case RenderTextureDimension::TEXTURE_3D:
            type = DkImageType_3D;
            break;
        case RenderTextureDimension::TEXTURE_2D:
        default:
            if (multisampled)
                type = layers > 1 ? DkImageType_2DMSArray : DkImageType_2DMS;
            else if ((desc.flags & RenderTextureFlag::CUBE) && layers % 6 == 0)
                type = layers > 6 ? DkImageType_CubemapArray : DkImageType_Cubemap;
            else
                type = layers > 1 ? DkImageType_2DArray : DkImageType_2D;
            break;
        }

        DkImageLayoutMaker m;
        dkImageLayoutMakerDefaults(&m, device->dk);
        m.type = type;
        m.format = format;
        m.msMode = toDkMsMode(desc.multisampling.sampleCount);
        m.dimensions[0] = std::max<uint32_t>(desc.width, 1);
        m.dimensions[1] = std::max<uint32_t>(desc.height, 1);
        // deko3d's layer counts (dk_image.cpp): a cube map's six faces are implied, a cube map array gives cubes (it
        // multiplies by six), a 1D array gives its layers as the height.
        m.dimensions[2] = type == DkImageType_3D ? std::max<uint32_t>(desc.depth, 1)
                          : type == DkImageType_Cubemap ? 6
                          : type == DkImageType_CubemapArray ? layers / 6 : layers;
        if (type == DkImageType_1D)
            m.dimensions[1] = m.dimensions[2] = 0;
        else if (type == DkImageType_1DArray) {
            m.dimensions[1] = layers;
            m.dimensions[2] = 0;
        }
        else if (type == DkImageType_2D || type == DkImageType_2DMS)
            m.dimensions[2] = 0;
        m.mipLevels = std::max<uint32_t>(desc.mipLevels, 1);
        uint32_t flags = 0;
        const bool render = (desc.flags & (RenderTextureFlag::RENDER_TARGET | RenderTextureFlag::DEPTH_TARGET)) != 0;
        if (render && type != DkImageType_3D) {
            flags |= DkImageFlags_UsageRender;
            // Colour targets compressed; depth too with Deko3DOptions::depthCompression (SwitchWakerHD's defaults left it
            // off as not verified).
            if ((!depth && g_deko3dOptions.colorCompression) || (depth && g_deko3dOptions.depthCompression))
                flags |= DkImageFlags_HwCompression;
        }
        if (desc.flags & (RenderTextureFlag::UNORDERED_ACCESS | RenderTextureFlag::STORAGE))
            flags |= DkImageFlags_UsageLoadStore;
        if (type != DkImageType_3D && canUse2DEngine(format))
            flags |= DkImageFlags_Usage2DEngine;
        m.flags = flags;
        uint32_t rows = m.dimensions[1] ? m.dimensions[1] : 1;
        if (isCompressedFormat(format))
            rows = (rows + 3) / 4;
        tileSizeFix(m, rows);
        dkImageLayoutInitialize(&layout, &m);
        const uint64_t size = dkImageLayoutGetSize(&layout);
        const uint32_t alignment = dkImageLayoutGetAlignment(&layout);
        // LostOdysseyRecomp (2026-10-28): a compressed render target gets a memory block of its own. The compression
        // state of image heap memory outlives the image that used it: a target made in memory a target of another
        // size used (dynamic resolution resizes the scene targets mid-scene) read back as stripes 4 pixels apart,
        // which NVK (no compression on the Switch) never showed. A new block starts with clear compression state.
        if ((flags & DkImageFlags_HwCompression) && size <= 0xFFFFFFFFull) {
            DkMemBlock own = device->makeBlock(uint32_t(size), DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image,
                                               "compressed render target");
            if (own) {
                alloc.block = own;
                alloc.offset = 0;
                alloc.size = uint32_t(alignUp64(size, DK_MEMBLOCK_ALIGNMENT));
                alloc.chunk = -1;
                ownsBlock = true;
            }
        }
        if (!ownsBlock && (size > 0xFFFFFFFFull || !device->imageAlloc(uint32_t(size), alignment, alloc))) {
            logf("texture %ux%ux%u format %d: no memory for %" PRIu64 " bytes", desc.width, desc.height, desc.depth,
                 int(desc.format), size);
            return;
        }
        dkImageInitialize(&image, &layout, alloc.block, alloc.offset);
        g_deko3dStats.imageBytes += alloc.size;
        valid = true;
    }

    Deko3DTexture::~Deko3DTexture() {
        if (valid && ownsMemory) {
            g_deko3dStats.imageBytes -= alloc.size;
            if (ownsBlock)
                device->retire({}, alloc.block);
            else
                device->retire(alloc, nullptr);
        }
    }

    DkImageView Deko3DTexture::defaultView() const {
        DkImageView v;
        dkImageViewDefaults(&v, &image);
        return v;
    }

    std::unique_ptr<RenderTextureView> Deko3DTexture::createTextureView(const RenderTextureViewDesc &viewDesc) const {
        auto view = std::make_unique<Deko3DTextureView>();
        view->texture = this;
        view->desc = viewDesc;
        dkImageViewDefaults(&view->view, &image);
        // A different format of the same size (sRGB, typeless, integer views). A depth image keeps its own: deko3d
        // samples depth formats as depth (the red channel), which is what Renut's R32 views of depth read.
        if (viewDesc.format != RenderFormat::UNKNOWN && !isDepthFormat(format)) {
            const DkImageFormat f = toDkFormat(viewDesc.format);
            if (f != DkImageFormat_None && f != format)
                view->view.format = f;
        }
        // banjo-nx (2026-10-18): an 8-bit unsigned view (R8_UINT here) of a combined depth/stencil image samples its
        // stencil (an unsigned integer in the red channel).
        if (viewDesc.format == RenderFormat::R8_UINT && (format == DkImageFormat_ZF32_X24S8 || format == DkImageFormat_Z24S8))
            view->view.dsSource = DkDsSource_Stencil;
        switch (viewDesc.dimension) {
        case RenderTextureViewDimension::TEXTURE_1D:
            view->view.type = DkImageType_1D;
            break;
        case RenderTextureViewDimension::TEXTURE_2D:
            if (type == DkImageType_2DArray || type == DkImageType_Cubemap || type == DkImageType_CubemapArray)
                view->view.type = DkImageType_2D;
            break;
        case RenderTextureViewDimension::TEXTURE_3D:
            view->view.type = DkImageType_3D;
            break;
        case RenderTextureViewDimension::TEXTURE_CUBE:
            view->view.type = DkImageType_Cubemap;
            break;
        default:
            break;
        }
        if (viewDesc.arrayIndex != 0 || (viewDesc.arraySize != UINT32_MAX && viewDesc.arraySize != 0)) {
            view->view.layerOffset = uint16_t(viewDesc.arrayIndex);
            if (viewDesc.arraySize != UINT32_MAX)
                view->view.layerCount = uint16_t(viewDesc.arraySize);
        }
        if (viewDesc.mipSlice != 0 || viewDesc.mipLevels != UINT32_MAX) {
            view->view.mipLevelOffset = uint8_t(viewDesc.mipSlice);
            if (viewDesc.mipLevels != UINT32_MAX)
                view->view.mipLevelCount = uint8_t(viewDesc.mipLevels);
        }
        const RenderComponentMapping &c = viewDesc.componentMapping;
        view->view.swizzle[0] = toDk(c.r, DkImageSwizzle_Red);
        view->view.swizzle[1] = toDk(c.g, DkImageSwizzle_Green);
        view->view.swizzle[2] = toDk(c.b, DkImageSwizzle_Blue);
        view->view.swizzle[3] = toDk(c.a, DkImageSwizzle_Alpha);
        return view;
    }

    void Deko3DTexture::setName(const std::string &) {}

    // ---- Deko3DDescriptorSet

    Deko3DDescriptorSet::Deko3DDescriptorSet(Deko3DDevice *device, const RenderDescriptorSetDesc &desc) : device(device) {
        uint32_t index = 0;
        uint32_t images = 0, samplers = 0, buffers = 0;
        bool imageStarted = false, samplerStarted = false;
        for (uint32_t i = 0; i < desc.descriptorRangesCount; i++) {
            const RenderDescriptorRange &r = desc.descriptorRanges[i];
            uint32_t count = r.count;
            if (desc.lastRangeIsBoundless && i + 1 == desc.descriptorRangesCount)
                count = std::max<uint32_t>(count, desc.boundlessRangeSize);
            Range range;
            range.type = r.type;
            range.binding = r.binding;
            range.first = index;
            range.count = count;
            ranges.push_back(range);
            switch (r.type) {
            case RenderDescriptorRangeType::TEXTURE:
            case RenderDescriptorRangeType::READ_WRITE_TEXTURE:
                if (!imageStarted) {
                    imageFirst = index;
                    imageStarted = true;
                }
                images = index + count - imageFirst;
                break;
            case RenderDescriptorRangeType::SAMPLER:
                if (!samplerStarted) {
                    samplerFirst = index;
                    samplerStarted = true;
                }
                samplers = index + count - samplerFirst;
                break;
            default:
                buffers = index + count;
                break;
            }
            index += count;
        }
        // LostOdysseyRecomp (2026-10-27): from the device's heaps (a set had a 4 KB memory block of its own: Lost
        // Odyssey makes thousands of texture sets, and several are bound at once).
        if (images) {
            imageCount = images;
            imageHeapBase = device->heapAlloc(false, images);
            if (imageHeapBase != UINT32_MAX)
                memset(static_cast<DkImageDescriptor *>(dkMemBlockGetCpuAddr(device->imageHeap)) + imageHeapBase, 0, images * 32);
        }
        if (samplers) {
            samplerCount = samplers;
            samplerHeapBase = device->heapAlloc(true, samplers);
            if (samplerHeapBase != UINT32_MAX)
                memset(static_cast<DkSamplerDescriptor *>(dkMemBlockGetCpuAddr(device->samplerHeap)) + samplerHeapBase, 0, samplers * 32);
        }
        this->buffers.resize(buffers ? index : 0);
    }

    Deko3DDescriptorSet::~Deko3DDescriptorSet() {
        if (imageHeapBase != UINT32_MAX)
            device->heapRetire(false, imageHeapBase, imageCount);
        if (samplerHeapBase != UINT32_MAX)
            device->heapRetire(true, samplerHeapBase, samplerCount);
    }

    const Deko3DDescriptorSet::Range *Deko3DDescriptorSet::rangeOf(uint32_t descriptorIndex) const {
        for (const Range &r : ranges) {
            if (descriptorIndex >= r.first && descriptorIndex < r.first + r.count)
                return &r;
        }
        return nullptr;
    }

    void Deko3DDescriptorSet::setBuffer(uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize, const RenderBufferStructuredView *structuredView, const RenderBufferFormattedView *) {
        if (descriptorIndex >= buffers.size())
            return;
        const Deko3DBuffer *b = static_cast<const Deko3DBuffer *>(buffer);
        DkBufExtents &e = buffers[descriptorIndex];
        if (b == nullptr || b->block == nullptr) {
            e = {};
            return;
        }
        uint64_t offset = 0;
        if (structuredView != nullptr)
            offset = uint64_t(structuredView->firstElement) * structuredView->structureByteStride;
        e.addr = b->gpu + offset;
        uint64_t size = bufferSize ? bufferSize : (b->desc.size - offset);
        const Range *r = rangeOf(descriptorIndex);
        if (r && (r->type == RenderDescriptorRangeType::CONSTANT_BUFFER || r->type == RenderDescriptorRangeType::DYNAMIC_CONSTANT_BUFFER))
            size = std::min<uint64_t>(size, DK_UNIFORM_BUF_MAX_SIZE);
        e.size = uint32_t(std::min<uint64_t>(size, 0xFFFFFFFFull));
    }

    void Deko3DDescriptorSet::setTexture(uint32_t descriptorIndex, const RenderTexture *texture, RenderTextureLayout, const RenderTextureView *textureView) {
        if (imageHeapBase == UINT32_MAX || descriptorIndex < imageFirst || descriptorIndex - imageFirst >= imageCount)
            return;
        DkImageDescriptor *d = static_cast<DkImageDescriptor *>(dkMemBlockGetCpuAddr(device->imageHeap)) + imageHeapBase + (descriptorIndex - imageFirst);
        const Deko3DTexture *t = static_cast<const Deko3DTexture *>(texture);
        if (t == nullptr || !t->valid) {
            memset(d, 0, sizeof(*d));
        }
        else {
            const Range *r = rangeOf(descriptorIndex);
            const bool loadStore = r && r->type == RenderDescriptorRangeType::READ_WRITE_TEXTURE;
            DkImageView view = textureView ? static_cast<const Deko3DTextureView *>(textureView)->view : t->defaultView();
            // Written into a local copy first: the GPU may read the descriptor block (other slots) while the CPU
            // writes it, so a slot is written whole, with one copy.
            DkImageDescriptor local;
            dkImageDescriptorInitialize(&local, &view, loadStore, false);
            memcpy(d, &local, sizeof(local));
            // banjo-nx (2026-10-19, GPU, txq): the size a texture query of this view returns at level 0: the image's
            // size at the view's first level; the depth of a 3D image there, else the view's layers (all from its first
            // when it names no count). tools/deko3d_glsl_opt.py's txq verify (constant 908 = 2) checks it on the GPU.
            const uint32_t id = descriptorIndex - imageFirst;
            if (imageSizes.size() <= id)
                imageSizes.resize(id + 1);
            const uint32_t m = view.mipLevelOffset;
            const uint32_t w = std::max(1u, t->desc.width >> m), h = std::max(1u, t->desc.height >> m);
            const DkImageType type = view.type != DkImageType_None ? view.type : t->type;
            uint32_t z;
            if (type == DkImageType_3D)
                z = std::max(1u, t->desc.depth >> m);
            else
                z = view.layerCount ? view.layerCount
                                    : (t->desc.arraySize > view.layerOffset ? t->desc.arraySize - view.layerOffset : 1u);
            imageSizes[id] = {w, h, z};
        }
        g_deko3dStats.descriptorWrites++;
        device->descriptorEpoch.fetch_add(1, std::memory_order_release);
    }

    void Deko3DDescriptorSet::setSampler(uint32_t descriptorIndex, const RenderSampler *sampler) {
        if (samplerHeapBase == UINT32_MAX || descriptorIndex < samplerFirst || descriptorIndex - samplerFirst >= samplerCount)
            return;
        DkSamplerDescriptor *d = static_cast<DkSamplerDescriptor *>(dkMemBlockGetCpuAddr(device->samplerHeap)) + samplerHeapBase + (descriptorIndex - samplerFirst);
        if (sampler == nullptr)
            memset(d, 0, sizeof(*d));
        else
            memcpy(d, &static_cast<const Deko3DSampler *>(sampler)->descriptor, sizeof(*d));
        g_deko3dStats.descriptorWrites++;
        device->descriptorEpoch.fetch_add(1, std::memory_order_release);
    }

    void Deko3DDescriptorSet::setAccelerationStructure(uint32_t, const RenderAccelerationStructure *) {}

    // ---- Deko3DQueryPool

    Deko3DQueryPool::Deko3DQueryPool(Deko3DDevice *device, uint32_t count) : device(device) {
        results.resize(count);
        // A counter report is 16 bytes: the counter, then the GPU timestamp.
        block = device->makeBlock(alignUp(std::max<uint32_t>(count, 1) * 16, DK_MEMBLOCK_ALIGNMENT), DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, "query pool");
        if (block)
            memset(dkMemBlockGetCpuAddr(block), 0, count * 16);
    }

    Deko3DQueryPool::~Deko3DQueryPool() {
        if (block)
            device->retire({}, block);
    }

    void Deko3DQueryPool::queryResults() { queryResults(uint32_t(results.size())); }

    bool Deko3DQueryPool::queryResults(uint32_t count) {
        count = std::min<uint32_t>(count, uint32_t(results.size()));
        if (!block)
            return true;
        const volatile uint64_t *reports = static_cast<const volatile uint64_t *>(dkMemBlockGetCpuAddr(block));
        for (uint32_t i = 0; i < count; i++)
            results[i] = dkTimestampToNs(reports[i * 2 + 1]);
        return true;
    }

    // ---- Deko3DDevice

    static void debugCallback(void *, const char *context, DkResult result, const char *message) {
        logf("deko3d says (%s, result %d): %s", context ? context : "", int(result), message ? message : "");
    }

    Deko3DDevice::Deko3DDevice() {}

    static void *compileThreadEntry(void *arg) {
        static_cast<Deko3DDevice *>(arg)->compileWorker();
        return nullptr;
    }

    bool Deko3DDevice::initialize() {
        {
            uint64_t heapFree = 0, processFree = 0;
            memoryFigures(heapFree, processFree);
            logf("device: creating (heap %" PRIu64 " MB free, process %" PRIu64 " MB unused)", heapFree >> 20, processFree >> 20);
        }
        DkDeviceMaker dm;
        dkDeviceMakerDefaults(&dm);
        dm.cbDebug = debugCallback;
        // Upper-left origin and depth from 0 to 1 (Vulkan's, D3D's); with the upper-left origin deko3d 0.5.0 maps clip
        // space y = +1 to the top row, D3D's convention: the deko3d shaders are compiled without -fvk-invert-y.
        dm.flags = DkDeviceFlags_OriginUpperLeft | DkDeviceFlags_DepthZeroToOne;
        dk = dkDeviceCreate(&dm);
        if (!dk) {
            logf("dkDeviceCreate failed");
            return false;
        }
        DkQueueMaker qm;
        dkQueueMakerDefaults(&qm, dk);
        // Zcull off unless Deko3DOptions::zcull (SwitchWakerHD: with it deko3d dropped depth-biased decals in whole 4x8
        // tiles).
        qm.flags = DkQueueFlags_Graphics | DkQueueFlags_Compute | DkQueueFlags_MediumPrio |
                   (g_deko3dOptions.zcull ? 0u : uint32_t(DkQueueFlags_DisableZcull));
        qm.commandMemorySize = 1u << 20;
        qm.flushThreshold = qm.commandMemorySize / 8;
        // Scratch (local) memory per warp: Renut's biggest vertex shaders need 32 KB (uam's DKSH headers; 81 of the 4,881
        // need some), variants maybe more: 64 KB a warp (about 8 MB for the GPU's warps).
        qm.perWarpScratchMemorySize = 64u << 10;
        perWarpScratch = qm.perWarpScratchMemorySize;
        queue = dkQueueCreate(&qm);
        if (!queue) {
            logf("dkQueueCreate failed");
            return false;
        }
        pushBlock = makeBlock(DK_MEMBLOCK_ALIGNMENT, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, "push constants");
        if (!pushBlock)
            return false;
        memset(dkMemBlockGetCpuAddr(pushBlock), 0, DK_MEMBLOCK_ALIGNMENT);
        pushAddr = dkMemBlockGetGpuAddr(pushBlock);
        texSizeBlock = makeBlock(DK_MEMBLOCK_ALIGNMENT, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, "texture sizes");
        if (!texSizeBlock)
            return false;
        memset(dkMemBlockGetCpuAddr(texSizeBlock), 0, DK_MEMBLOCK_ALIGNMENT);
        texSizeAddr = dkMemBlockGetGpuAddr(texSizeBlock);
        zeroBlock = makeBlock(ZeroSize, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, "zero vertex buffer");
        if (!zeroBlock)
            return false;
        memset(dkMemBlockGetCpuAddr(zeroBlock), 0, ZeroSize);
        zeroAddr = dkMemBlockGetGpuAddr(zeroBlock);
        // LostOdysseyRecomp (2026-10-27): the descriptor heaps (2 MB of images, 128 KB of samplers).
        imageHeap = makeBlock(ImageHeapCount * 32, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, "image descriptor heap");
        samplerHeap = makeBlock(SamplerHeapCount * 32, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, "sampler descriptor heap");
        if (!imageHeap || !samplerHeap)
            return false;
        memset(dkMemBlockGetCpuAddr(imageHeap), 0, ImageHeapCount * 32);
        memset(dkMemBlockGetCpuAddr(samplerHeap), 0, SamplerHeapCount * 32);
        imageHeapAddr = dkMemBlockGetGpuAddr(imageHeap);
        samplerHeapAddr = dkMemBlockGetGpuAddr(samplerHeap);
        imageHeapFree.push_back({0, ImageHeapCount});
        samplerHeapFree.push_back({0, SamplerHeapCount});

        // LostOdysseyRecomp (2026-10-27): the application compiles its own shaders to SPIR-V for this device
        // (gpu/shader/target_format.h reads the device's format; unset, it compiled DXIL).
        capabilities.shaderFormat = RenderShaderFormat::SPIRV;
        capabilities.geometryShader = true;
        capabilities.descriptorIndexing = true;
        capabilities.scalarBlockLayout = false;
        capabilities.bufferDeviceAddress = true;
        capabilities.samplerMirrorClampToEdge = true;
        capabilities.maxTextureSize = 16384;
        capabilities.triangleFan = true;
        capabilities.dynamicDepthBias = true;
        capabilities.uma = true;
        capabilities.queryPools = true;
        capabilities.resolveRegion = false;
        description.name = "deko3d (Tegra X1, Maxwell GM20B)";
        description.type = RenderDeviceType::INTEGRATED;
        description.vendor = RenderDeviceVendor::NVIDIA;
        description.driverVersion = 0x000500;

        // The uam worker: one thread (uam is not reentrant), 8 MB of stack (Mesa's parser and nv50_ir recurse deeply).
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 8u << 20);
        pthread_t thread;
        if (pthread_create(&thread, &attr, compileThreadEntry, this) != 0) {
            logf("the shader compile thread did not start");
            pthread_attr_destroy(&attr);
            return false;
        }
        pthread_attr_destroy(&attr);
        pthread_detach(thread);
        logf("device ready: queue with zcull %s, depth targets %scompressed, vertex shader outputs %s, 1 MB of queue "
             "command memory; uam compile thread started", g_deko3dOptions.zcull ? "on" : "off",
             g_deko3dOptions.depthCompression ? "" : "not ", g_deko3dOptions.vsOutputs ? "pruned per pipeline" : "all written");
        return true;
    }

    Deko3DDevice::~Deko3DDevice() {
        if (queue)
            dkQueueWaitIdle(queue);
        {
            std::lock_guard<std::mutex> lock(compileMutex);
            compileStop = true;
        }
        compileWake.notify_all();
        {
            std::unique_lock<std::mutex> lock(compileMutex);
            compileDone.wait_for(lock, std::chrono::seconds(2), [this] { return !compilerReady; });
        }
        collectRetired();
        for (Retired &r : retired) {
            if (r.block)
                destroyBlock(r.block);
        }
        retired.clear();
        for (auto &c : chunks)
            destroyBlock(c.block);
        for (auto &c : codeBlocks)
            destroyBlock(c.first);
        if (pushBlock)
            destroyBlock(pushBlock);
        if (zeroBlock)
            destroyBlock(zeroBlock);
        if (imageHeap)
            destroyBlock(imageHeap);
        if (samplerHeap)
            destroyBlock(samplerHeap);
        if (queue)
            dkQueueDestroy(queue);
        if (dk)
            dkDeviceDestroy(dk);
    }

    DkMemBlock Deko3DDevice::makeBlock(uint32_t size, uint32_t flags, const char *what) {
        DkMemBlockMaker m;
        dkMemBlockMakerDefaults(&m, dk, alignUp(size, DK_MEMBLOCK_ALIGNMENT));
        m.flags = flags;
        // deko3d takes the block's memory from the heap (aligned_alloc) and aborts the process when a creation fails:
        // big ones, and every 64 MB of them, are logged with the heap's free bytes.
        static std::atomic<uint64_t> total{0};
        static std::atomic<uint32_t> logged{0};
        const uint64_t before = total.fetch_add(m.size);
        // banjo-nx (2026-10-19, render-thread proposal 18): 24 lines, not 96 (each walks the heap and writes the SD card,
        // on the thread making the block: the render thread for the upload ring's chunks).
        if ((size >= (8u << 20) || (before >> 26) != ((before + m.size) >> 26)) && logged.fetch_add(1) < 24) {
            uint64_t heapFree = 0, processFree = 0;
            memoryFigures(heapFree, processFree);
            logf("memory block: %s, %u KiB (deko3d blocks %" PRIu64 " MB in all; heap %" PRIu64 " MB free, process %" PRIu64 " MB unused)",
                 what, size >> 10, (before + m.size) >> 20, heapFree >> 20, processFree >> 20);
        }
#if defined(PLUME_DEKO3D_GUARDS)
        // The block's storage with guard bytes after it (deko3d takes storage given to it as it is).
        uint8_t *storage = static_cast<uint8_t *>(memalign(DK_MEMBLOCK_ALIGNMENT, m.size + GuardBytes));
        if (storage) {
            memset(storage + m.size, GuardByte, GuardBytes);
            m.storage = storage;
        }
        DkMemBlock block = dkMemBlockCreate(&m);
        if (storage) {
            std::lock_guard<std::mutex> lock(guardMutex);
            guarded.push_back({block, storage, m.size, what});
        }
        return block;
#else
        return dkMemBlockCreate(&m);
#endif
    }

    void Deko3DDevice::destroyBlock(DkMemBlock block) {
        if (!block)
            return;
        dkMemBlockDestroy(block);
#if defined(PLUME_DEKO3D_GUARDS)
        std::lock_guard<std::mutex> lock(guardMutex);
        for (size_t i = 0; i < guarded.size(); i++) {
            if (guarded[i].block == block) {
                free(guarded[i].storage);
                guarded[i] = guarded.back();
                guarded.pop_back();
                break;
            }
        }
#endif
    }

    void Deko3DDevice::checkGuards() {
#if defined(PLUME_DEKO3D_GUARDS)
        std::lock_guard<std::mutex> lock(guardMutex);
        for (GuardedBlock &g : guarded) {
            const uint8_t *guard = g.storage + g.size;
            for (uint32_t i = 0; i < GuardBytes; i++) {
                if (guard[i] != GuardByte) {
                    uint32_t last = i;
                    for (uint32_t j = i; j < GuardBytes; j++)
                        if (guard[j] != GuardByte)
                            last = j;
                    uint32_t word = 0;
                    memcpy(&word, guard + (i & ~3u), 4);
                    logf("GUARD: written past the end of %s (%u bytes at GPU 0x%" PRIx64 "): guard bytes %u..%u changed "
                         "(first word 0x%08X)", g.what, g.size, dkMemBlockGetGpuAddr(g.block), i, last, word);
                    memset(const_cast<uint8_t *>(guard), GuardByte, GuardBytes);
                    break;
                }
            }
        }
#endif
    }

    bool Deko3DDevice::findBuffer(DkGpuAddr address, DkGpuAddr &base, uint32_t &size, const uint8_t *&cpu) {
        std::lock_guard<std::mutex> lock(bufferMutex);
        auto it = bufferRanges.upper_bound(address);
        if (it == bufferRanges.begin())
            return false;
        --it;
        if (address >= it->first + it->second.first)
            return false;
        base = it->first;
        size = it->second.first;
        cpu = it->second.second;
        return true;
    }

    // Image heap: 64 MB chunks (bigger for a bigger image), first fit.
    static constexpr uint32_t ImageChunkSize = 64u << 20;

    static void chunkFree(Deko3DDevice::Chunk &c, uint32_t offset, uint32_t size) {
        auto it = std::lower_bound(c.freeList.begin(), c.freeList.end(), std::make_pair(offset, 0u));
        it = c.freeList.insert(it, {offset, size});
        if (it + 1 != c.freeList.end() && it->first + it->second == (it + 1)->first) {
            it->second += (it + 1)->second;
            c.freeList.erase(it + 1);
        }
        if (it != c.freeList.begin() && (it - 1)->first + (it - 1)->second == it->first) {
            (it - 1)->second += it->second;
            c.freeList.erase(it);
        }
    }

    bool Deko3DDevice::imageAlloc(uint32_t size, uint32_t alignment, Deko3DImageAlloc &out) {
        size = alignUp(size, 256);
        alignment = std::max<uint32_t>(alignment, 256);
        collectRetired();
        std::lock_guard<std::mutex> lock(heapMutex);
        for (int pass = 0; pass < 2; pass++) {
            for (size_t ci = 0; ci < chunks.size(); ci++) {
                Chunk &c = chunks[ci];
                for (size_t i = 0; i < c.freeList.size(); i++) {
                    const auto [off, len] = c.freeList[i];
                    const uint32_t at = alignUp(off, alignment);
                    if (at - off + uint64_t(size) > len)
                        continue;
                    c.freeList.erase(c.freeList.begin() + long(i));
                    if (at > off)
                        chunkFree(c, off, at - off);
                    if (at + size < off + len)
                        chunkFree(c, at + size, off + len - at - size);
                    out.block = c.block;
                    out.offset = at;
                    out.size = size;
                    out.chunk = int32_t(ci);
                    return true;
                }
            }
            if (pass == 0) {
                const uint32_t chunkSize = std::max(ImageChunkSize, alignUp(size + alignment, DK_MEMBLOCK_ALIGNMENT));
                Chunk c;
                c.block = makeBlock(chunkSize, DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image, "image heap chunk");
                if (!c.block)
                    return false;
                c.size = chunkSize;
                c.freeList.push_back({0, chunkSize});
                chunks.push_back(std::move(c));
                logf("image heap: chunk %zu of %u MiB", chunks.size(), chunkSize >> 20);
            }
        }
        return false;
    }

    // LostOdysseyRecomp (2026-10-27): descriptor heap ranges, first fit.
    uint32_t Deko3DDevice::heapAlloc(bool sampler, uint32_t count) {
        std::lock_guard<std::mutex> lock(descriptorHeapMutex);
        auto &list = sampler ? samplerHeapFree : imageHeapFree;
        for (size_t i = 0; i < list.size(); i++) {
            if (list[i].second < count)
                continue;
            const uint32_t first = list[i].first;
            list[i].first += count;
            list[i].second -= count;
            if (list[i].second == 0)
                list.erase(list.begin() + ptrdiff_t(i));
            return first;
        }
        static std::atomic<uint32_t> logged{0};
        if (logged.fetch_add(1) < 8)
            logf("%s descriptor heap full (%u wanted)", sampler ? "sampler" : "image", count);
        return UINT32_MAX;
    }

    void Deko3DDevice::heapRetire(bool sampler, uint32_t first, uint32_t count) {
        std::lock_guard<std::mutex> lock(retireMutex);
        retiredDescriptors.push_back({submitSerial.load(std::memory_order_acquire), sampler, first, count});
    }

    static void heapFree(std::vector<std::pair<uint32_t, uint32_t>> &list, uint32_t first, uint32_t count) {
        auto it = std::lower_bound(list.begin(), list.end(), std::make_pair(first, 0u));
        it = list.insert(it, {first, count});
        // merge with the next and the previous
        if (it + 1 != list.end() && it->first + it->second == (it + 1)->first) {
            it->second += (it + 1)->second;
            list.erase(it + 1);
        }
        if (it != list.begin() && (it - 1)->first + (it - 1)->second == it->first) {
            (it - 1)->second += it->second;
            list.erase(it);
        }
    }

    void Deko3DDevice::retire(const Deko3DImageAlloc &image, DkMemBlock block) {
        std::lock_guard<std::mutex> lock(retireMutex);
        retired.push_back({submitSerial.load(std::memory_order_acquire), image, block});
    }

    void Deko3DDevice::noteSubmission() {
        // A fence per submission serial (16 in a ring): what was retired before a submission is freed once its fence
        // signalled.
        std::lock_guard<std::mutex> lock(retireMutex);
        const uint64_t serial = submitSerial.load(std::memory_order_relaxed);
        DkFence &fence = serialFences[serial % SerialFences];
        if (serialFenceValue[serial % SerialFences] != 0) {
            // The ring wrapped: that older submission must be finished before its fence is reused.
            dkFenceWait(&fence, -1);
            completedSerial.store(std::max(completedSerial.load(), serialFenceValue[serial % SerialFences]));
        }
        dkQueueSignalFence(queue, &fence, false);
        serialFenceValue[serial % SerialFences] = serial;
        submitSerial.store(serial + 1, std::memory_order_release);
    }

    void Deko3DDevice::collectRetired() {
        std::lock_guard<std::mutex> lock(retireMutex);
        if (retired.empty() && retiredDescriptors.empty())
            return;
        uint64_t done = completedSerial.load();
        for (uint32_t i = 0; i < SerialFences; i++) {
            if (serialFenceValue[i] > done && dkFenceWait(&serialFences[i], 0) == DkResult_Success)
                done = std::max(done, serialFenceValue[i]);
        }
        completedSerial.store(done);
        size_t kept = 0;
        for (size_t i = 0; i < retired.size(); i++) {
            Retired &r = retired[i];
            // Retired while submission r.serial was being recorded: free once that one has completed.
            if (r.serial <= done) {
                if (r.block)
                    destroyBlock(r.block);
                if (r.image.chunk >= 0) {
                    std::lock_guard<std::mutex> heap(heapMutex);
                    chunkFree(chunks[size_t(r.image.chunk)], r.image.offset, r.image.size);
                }
            }
            else {
                retired[kept++] = r;
            }
        }
        retired.resize(kept);
        // LostOdysseyRecomp (2026-10-27): descriptor heap ranges.
        kept = 0;
        for (size_t i = 0; i < retiredDescriptors.size(); i++) {
            const RetiredDescriptors &r = retiredDescriptors[i];
            if (r.serial <= done) {
                std::lock_guard<std::mutex> heap(descriptorHeapMutex);
                heapFree(r.sampler ? samplerHeapFree : imageHeapFree, r.first, r.count);
            }
            else {
                retiredDescriptors[kept++] = r;
            }
        }
        retiredDescriptors.resize(kept);
    }

    // ---- shaders

    static Deko3DShaderUsage parseUsage(const std::string &glsl) {
        Deko3DShaderUsage usage;
        const size_t at = glsl.find("// banjo-dk ");
        if (at == std::string::npos)
            return usage;
        const size_t end = glsl.find('\n', at);
        const std::string line = glsl.substr(at, end == std::string::npos ? std::string::npos : end - at);
        auto field = [&](const char *key) -> std::string {
            const size_t k = line.find(key);
            if (k == std::string::npos)
                return {};
            const size_t v = k + strlen(key);
            const size_t e = line.find(' ', v);
            return line.substr(v, e == std::string::npos ? std::string::npos : e - v);
        };
        usage.uboMask = uint32_t(strtoul(field("ubo=").c_str(), nullptr, 16));
        usage.ssboMask = uint32_t(strtoul(field("ssbo=").c_str(), nullptr, 16));
        usage.textures = field("tex=") == "1";
        usage.txqMask = uint32_t(strtoul(field("txq=").c_str(), nullptr, 16));  // banjo-nx (2026-10-19)
        // texb=k:textureWord:samplerWord,... in binding order
        const std::string texb = field("texb=");
        size_t q = 0;
        while (q < texb.size()) {
            const size_t comma = texb.find(',', q);
            const std::string item = texb.substr(q, comma == std::string::npos ? std::string::npos : comma - q);
            unsigned k = 0, t = 0, s = 0;
            if (sscanf(item.c_str(), "%u:%u:%u", &k, &t, &s) == 3 && k == usage.texBindings.size())
                usage.texBindings.push_back({uint16_t(t), uint16_t(s)});
            if (comma == std::string::npos)
                break;
            q = comma + 1;
        }
        const std::string vin = field("vin=");
        size_t p = 0;
        while (p < vin.size()) {
            const size_t comma = vin.find(',', p);
            usage.vertexInputs.push_back(uint32_t(strtoul(vin.substr(p, comma - p).c_str(), nullptr, 10)));
            if (comma == std::string::npos)
                break;
            p = comma + 1;
        }
        return usage;
    }

    struct SpecDefault {
        uint32_t id;
        uint32_t value;
    };

    // The specialization constants the GLSL defines: "#define SPIRV_CROSS_CONSTANT_ID_<id> <value>u".
    static std::vector<SpecDefault> parseSpecDefaults(const std::string &glsl) {
        std::vector<SpecDefault> out;
        const char *key = "#define SPIRV_CROSS_CONSTANT_ID_";
        size_t p = 0;
        while ((p = glsl.find(key, p)) != std::string::npos) {
            p += strlen(key);
            char *end = nullptr;
            const uint32_t id = uint32_t(strtoul(glsl.c_str() + p, &end, 10));
            const uint32_t value = uint32_t(strtoul(end, nullptr, 0));
            out.push_back({id, value});
        }
        std::sort(out.begin(), out.end(), [](const SpecDefault &a, const SpecDefault &b) { return a.id < b.id; });
        return out;
    }

    struct Deko3DShaderImpl : Deko3DShader {
        std::vector<SpecDefault> specDefaults;
        // LostOdysseyRecomp (2026-10-27): a translation read from the cache has no GLSL until a variant needs it
        // (Deko3DDevice::translationText): its default variant's cache key, and what to translate it from.
        uint64_t glslKey = 0;
        const Deko3DShader *spirvSource = nullptr;
        Deko3DSpirvLayout spirvLayout;
        std::mutex textMutex;
        // Variants already looked up (spec values -> variant), so a pipeline does not hash the text again.
        std::mutex mutex;
        std::unordered_map<uint64_t, const DkShader *> known;
    };

    // A shader from GLSL (with its banjo-dk line, if any) and the DKSH of its default variant, if any.
    static std::unique_ptr<Deko3DShaderImpl> makeGlslShader(Deko3DDevice *device, DkStage stage, std::string glsl, std::vector<uint8_t> dksh) {
        auto shader = std::make_unique<Deko3DShaderImpl>();
        shader->device = device;
        shader->stage = stage;
        shader->glsl = std::move(glsl);
        shader->defaultDksh = std::move(dksh);
        shader->usage = parseUsage(shader->glsl);
        // banjo-nx (2026-10-18): the varying locations it declares ("layout(location = N) in / out" lines).
        {
            const char *key = "\nlayout(location = ";
            size_t p = 0;
            while ((p = shader->glsl.find(key, p)) != std::string::npos) {
                p += strlen(key);
                char *end = nullptr;
                const unsigned long loc = strtoul(shader->glsl.c_str() + p, &end, 10);
                if (loc < 32 && end && strncmp(end, ") in ", 5) == 0)
                    shader->usage.inputMask |= 1u << loc;
                else if (loc < 32 && end && strncmp(end, ") out ", 6) == 0)
                    shader->usage.outputMask |= 1u << loc;
            }
        }
        {
            const size_t z = shader->glsl.find("\n// banjo-vszero=0x");
            if (z != std::string::npos)
                shader->usage.zeroOutputMask = uint32_t(strtoul(shader->glsl.c_str() + z + 19, nullptr, 16));  // after "0x"
        }
        shader->specDefaults = parseSpecDefaults(shader->glsl);
        for (const SpecDefault &d : shader->specDefaults)
            shader->specIds.push_back(d.id);
        return shader;
    }

    // LostOdysseyRecomp (2026-10-27): the execution model of a SPIR-V module's first entry point.
    static bool spirvStage(const uint32_t *words, size_t count, Deko3DSpirvStage &stage, DkStage &dkStage) {
        if (count < 5 || words[0] != 0x07230203u)
            return false;
        for (size_t i = 5; i < count;) {
            const uint32_t op = words[i] & 0xFFFFu, length = words[i] >> 16;
            if (length == 0 || i + length > count)
                return false;
            if (op == 15 && length >= 2) {  // OpEntryPoint
                switch (words[i + 1]) {
                case 0: stage = Deko3DSpirvStage::Vertex; dkStage = DkStage_Vertex; return true;
                case 3: stage = Deko3DSpirvStage::Geometry; dkStage = DkStage_Geometry; return true;
                case 4: stage = Deko3DSpirvStage::Fragment; dkStage = DkStage_Fragment; return true;
                case 5: stage = Deko3DSpirvStage::Compute; dkStage = DkStage_Compute; return true;
                default: return false;
                }
            }
            i += length;
        }
        return false;
    }

    static Deko3DSpirvOptions g_spirvOptions;

    void Deko3DSetSpirvOptions(const Deko3DSpirvOptions &options) {
        g_spirvOptions = options;
    }

    std::unique_ptr<RenderShader> Deko3DDevice::createShader(const void *data, uint64_t size, const char *, RenderShaderFormat format) {
        if (format == RenderShaderFormat::SPIRV && data != nullptr && size >= 20 && size % 4 == 0) {
            auto shader = std::make_unique<Deko3DShaderImpl>();
            shader->device = this;
            const uint32_t *words = static_cast<const uint32_t *>(data);
            DkStage dkStage = DkStage_Vertex;
            if (!spirvStage(words, size_t(size / 4), shader->spirvStage, dkStage)) {
                logf("createShader: SPIR-V without a vertex, geometry, fragment or compute entry point");
                return nullptr;
            }
            shader->stage = dkStage;
            shader->spirv.assign(words, words + size / 4);
            return shader;
        }
        logf("createShader: not a SPIR-V shader (format %d, %" PRIu64 " bytes)", int(format), size);
        return nullptr;
    }

    const Deko3DShader *Deko3DDevice::translated(const Deko3DShader *shader, const Deko3DPipelineLayout *layout) {
        if (shader == nullptr || shader->spirv.empty())
            return shader;
        std::lock_guard<std::mutex> lock(shader->translateMutex);
        auto it = shader->translations.find(layout);
        if (it != shader->translations.end())
            return static_cast<const Deko3DShader *>(it->second.get());
        // plume_deko3d's numbering of the layout (createPipelineLayout) for the translation.
        Deko3DSpirvLayout map;
        if (layout) {
            for (uint32_t s = 0; s < layout->sets.size(); s++) {
                for (const Deko3DPipelineLayout::Range &r : layout->sets[s].ranges) {
                    Deko3DSpirvLayout::Range m;
                    m.set = s;
                    m.binding = r.binding;
                    m.count = r.count;
                    m.descriptorIndex = r.descriptorIndex;
                    m.slot = r.slot;
                    switch (r.type) {
                    case RenderDescriptorRangeType::CONSTANT_BUFFER:
                    case RenderDescriptorRangeType::DYNAMIC_CONSTANT_BUFFER: m.kind = Deko3DSpirvLayout::Kind::Uniform; break;
                    case RenderDescriptorRangeType::STRUCTURED_BUFFER:
                    case RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER:
                    case RenderDescriptorRangeType::BYTE_ADDRESS_BUFFER:
                    case RenderDescriptorRangeType::READ_WRITE_BYTE_ADDRESS_BUFFER:
                    case RenderDescriptorRangeType::FORMATTED_BUFFER:
                    case RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER: m.kind = Deko3DSpirvLayout::Kind::Storage; break;
                    case RenderDescriptorRangeType::TEXTURE:
                    case RenderDescriptorRangeType::READ_WRITE_TEXTURE: m.kind = Deko3DSpirvLayout::Kind::Texture; break;
                    case RenderDescriptorRangeType::SAMPLER: m.kind = Deko3DSpirvLayout::Kind::Sampler; break;
                    default: m.kind = Deko3DSpirvLayout::Kind::Other; break;
                    }
                    map.ranges.push_back(m);
                }
            }
        }
        // A translation the cache holds (the PC tool's, or one made on an earlier run): no SPIRV-Cross now.
        const uint64_t recordKey = Deko3DTranslationKey(shader->spirv.data(), shader->spirv.size(), map, g_spirvOptions);
        {
            std::vector<uint8_t> bytes;
            Deko3DTranslationRecord record;
            if (g_cacheLookup && g_cacheLookup(recordKey, bytes) && Deko3DReadTranslation(bytes, record)) {
                auto cached = std::make_unique<Deko3DShaderImpl>();
                cached->device = this;
                cached->stage = shader->stage;
                cached->name = shader->name;
                cached->glslKey = record.glslKey;
                cached->spirvSource = shader;
                cached->spirvLayout = map;
                cached->usage.uboMask = record.uniformMask;
                cached->usage.ssboMask = record.storageMask;
                cached->usage.textureSources = std::move(record.textures);
                cached->usage.pushWindows = std::move(record.pushWindows);
                cached->usage.vertexInputs = std::move(record.vertexInputs);
                cached->usage.inputMask = record.inputMask;    // LostOdysseyRecomp (2026-10-27): for constant 905
                cached->usage.outputMask = record.outputMask;
                for (const auto &[id, value] : record.specDefaults)
                    cached->specDefaults.push_back({id, value});
                // By id, as parseSpecDefaults orders them: a variant's defines (and its key) do not depend on where
                // the shader came from.
                std::sort(cached->specDefaults.begin(), cached->specDefaults.end(),
                          [](const SpecDefault &a, const SpecDefault &b) { return a.id < b.id; });
                for (const SpecDefault &d : cached->specDefaults)
                    cached->specIds.push_back(d.id);
                g_deko3dStats.translationHits++;
                const Deko3DShader *result = cached.get();
                shader->translations[layout] = std::move(cached);
                return result;
            }
        }
        const uint64_t t0 = armTicksToNs(armGetSystemTick());
        Deko3DSpirvResult r = Deko3DTranslateSpirv(shader->spirv.data(), shader->spirv.size(), shader->spirvStage, map, g_spirvOptions);
        g_deko3dStats.translations++;
        if (r.ok && g_cacheStore)
            g_cacheStore(recordKey, Deko3DWriteTranslation(r));
        g_deko3dStats.shaderTranslateNs += armTicksToNs(armGetSystemTick()) - t0;
        std::unique_ptr<RenderShader> out;
        if (!r.ok) {
            static std::atomic<uint32_t> logged{0};
            if (logged.fetch_add(1) < 32)
                logf("SPIR-V translation failed for %s: %s", shader->name.empty() ? "a shader" : shader->name.c_str(), r.error.c_str());
        }
        else {
            for (const std::string &n : r.notes) {
                static std::atomic<uint32_t> noted{0};
                if (noted.fetch_add(1) < 16)
                    logf("SPIR-V translation of %s: %s", shader->name.empty() ? "a shader" : shader->name.c_str(), n.c_str());
            }
            auto glsl = makeGlslShader(this, shader->stage, std::move(r.glsl), {});
            glsl->name = shader->name;
            glsl->usage.uboMask = r.uniformMask;
            glsl->usage.ssboMask = r.storageMask;
            glsl->usage.textureSources = std::move(r.textures);
            glsl->usage.pushWindows = std::move(r.pushWindows);
            glsl->usage.vertexInputs = std::move(r.vertexInputs);
            out = std::move(glsl);
        }
        const Deko3DShader *result = static_cast<const Deko3DShader *>(out.get());
        shader->translations[layout] = std::move(out);
        return result;
    }

    void Deko3DDevice::compileWorker() {
        uint64_t heapFree = 0, processFree = 0;
        memoryFigures(heapFree, processFree);
        logf("uam thread: starting (heap %" PRIu64 " MB free, process %" PRIu64 " MB unused)", heapFree >> 20, processFree >> 20);
        const uint64_t i0 = armTicksToNs(armGetSystemTick());
        uam::setBanjoFlags(g_deko3dOptions.uamFlags);  // banjo-nx (2026-10-18)
        // banjo-nx (2026-10-18): off the render thread's core and below the game's threads (package 33's first run
        // compiled 231 variants during play: a core busy for minutes, every thread slower).
        {
            std::lock_guard<std::mutex> lock(compileMutex);  // compile() reads it under the lock
            compileThreadHandle = threadGetCurHandle();
        }
        if (g_deko3dOptions.compileCore >= 0)
            svcSetThreadCoreMask(CUR_THREAD_HANDLE, g_deko3dOptions.compileCore, 1u << g_deko3dOptions.compileCore);
        if (g_deko3dOptions.compilePriority > 0)
            svcSetThreadPriority(CUR_THREAD_HANDLE, u32(g_deko3dOptions.compilePriority));
        logf("uam thread: core %d, priority 0x%X (a waiting thread lends its own)", g_deko3dOptions.compileCore,
             g_deko3dOptions.compilePriority);
        uam::init(true);
        // Self-test (SwitchWakerHD's): a small shader now, which also builds the resident frontend's tables before the
        // game's first variant.
        static const char test[] = "#version 460\nlayout(location = 0) in vec4 pos;\nlayout(std140, binding = 0) uniform B { vec4 s; };\n"
                                   "void main() { gl_Position = pos * s; }\n";
        const uint64_t i1 = armTicksToNs(armGetSystemTick());
        uam::Result selfTest = uam::compile(uam::Stage::Vertex, test);
        const uint64_t i2 = armTicksToNs(armGetSystemTick());
        memoryFigures(heapFree, processFree);
        logf("uam thread: %s (init %.0f ms, self-test %zu bytes of DKSH in %.0f ms; heap %" PRIu64 " MB free, process %" PRIu64 " MB unused)%s%s",
             selfTest.ok ? "ready" : "SELF-TEST FAILED", double(i1 - i0) / 1e6, selfTest.dksh.size(), double(i2 - i1) / 1e6,
             heapFree >> 20, processFree >> 20, selfTest.ok ? "" : ": ", selfTest.ok ? "" : selfTest.log.c_str());
        {
            std::lock_guard<std::mutex> lock(compileMutex);
            compilerReady = true;
        }
        for (;;) {
            CompileJob *job = nullptr;
            {
                std::unique_lock<std::mutex> lock(compileMutex);
                compileWake.wait(lock, [this] { return compileStop || !compileQueue.empty(); });
                if (compileStop)
                    break;
                job = compileQueue.front();
                compileQueue.erase(compileQueue.begin());
            }
            static const uam::Stage stages[] = {uam::Stage::Vertex, uam::Stage::TessCtrl, uam::Stage::TessEval,
                                                uam::Stage::Geometry, uam::Stage::Fragment, uam::Stage::Compute};
            const uint64_t t0 = armTicksToNs(armGetSystemTick());
            {
                static uint32_t logged = 0;
                if (logged < 4) {
                    logged++;
                    uint64_t hf = 0, pf = 0;
                    memoryFigures(hf, pf);
                    logf("uam: compile %u (stage %u, %zu bytes of GLSL; heap %" PRIu64 " MB free, process %" PRIu64 " MB unused)",
                         logged, uint32_t(job->stage), job->source->size(), hf >> 20, pf >> 20);
                }
                // banjo-nx (2026-10-20): every run-time compile's specialization (its constants), so the variants
                // a cache lacks can be found (tools/deko3d_variant_cache.py rebuilds their keys from these).
                static uint32_t named = 0;
                if (named < 64) {
                    named++;
                    std::string defines;
                    const std::string &src = *job->source;
                    const char *key = "#define SPIRV_CROSS_CONSTANT_ID_";
                    for (size_t p = src.find(key); p != std::string::npos; p = src.find(key, p + 1)) {
                        const size_t end = src.find('\n', p);
                        defines += src.substr(p + strlen(key), end == std::string::npos ? std::string::npos : end - p - strlen(key));
                        defines += ';';
                        if (defines.size() > 900)
                            break;
                    }
                    logf("uam: run-time variant (stage %u, %zu bytes): %s", uint32_t(job->stage), src.size(),
                         defines.c_str());
                }
            }
            uam::Result r = uam::compile(stages[std::min<uint32_t>(job->stage, 5)], job->source->c_str());
            g_deko3dStats.shaderCompileNs += armTicksToNs(armGetSystemTick()) - t0;
            g_deko3dStats.shaderCompiles++;
            {
                std::lock_guard<std::mutex> lock(compileMutex);
                job->ok = r.ok;
                *job->dksh = std::move(r.dksh);
                *job->log = std::move(r.log);
                job->done = true;
            }
            compileDone.notify_all();
        }
        uam::shutdown();
        std::lock_guard<std::mutex> lock(compileMutex);
        compilerReady = false;
        compileDone.notify_all();
    }

    bool Deko3DDevice::compile(DkStage stage, const std::string &source, std::vector<uint8_t> &dksh, std::string &log) {
        CompileJob job;
        job.stage = stage;
        job.source = &source;
        job.dksh = &dksh;
        job.log = &log;
        // banjo-nx (2026-10-18): the waiting thread's priority, lent to the uam thread while it waits (the render
        // thread must not wait behind the game's threads for a low-priority compile).
        s32 waiterPriority = 0;
        bool lend = g_deko3dOptions.compilePriority > 0 && R_SUCCEEDED(svcGetThreadPriority(&waiterPriority, CUR_THREAD_HANDLE)) &&
                    waiterPriority < g_deko3dOptions.compilePriority;
        {
            std::lock_guard<std::mutex> lock(compileMutex);
            compileQueue.push_back(&job);
            lend = lend && compileThreadHandle != 0;
            if (lend) {
                compileWaiters.push_back(waiterPriority);
                applyCompilePriority();
            }
        }
        compileWake.notify_one();
        std::unique_lock<std::mutex> lock(compileMutex);
        compileDone.wait(lock, [&] { return job.done; });
        if (lend) {
            compileWaiters.erase(std::find(compileWaiters.begin(), compileWaiters.end(), waiterPriority));
            applyCompilePriority();
        }
        return job.ok;
    }

    // compileMutex held: the uam thread at its own priority, or the highest a waiting thread has.
    void Deko3DDevice::applyCompilePriority() {
        s32 priority = g_deko3dOptions.compilePriority;
        for (s32 p : compileWaiters)
            priority = std::min(priority, p);
        if (priority != compileAppliedPriority && compileThreadHandle != 0 &&
            R_SUCCEEDED(svcSetThreadPriority(compileThreadHandle, u32(priority))))
            compileAppliedPriority = priority;
    }

    // Shader code: 16 MB blocks, bump allocated (deko3d maps every code block into its one code segment).
    static constexpr uint32_t CodeBlockSize = 16u << 20;

    bool Deko3DDevice::loadCode(const std::vector<uint8_t> &dksh, DkShader &shader, const char *what) {
        if (dksh.size() < 64)
            return false;
        {
            // The DKSH header (uam's dksh.h): programs_off at byte 16; the program's per_warp_scratch_sz at byte 20.
            uint32_t programsOff = 0, scratch = 0;
            memcpy(&programsOff, dksh.data() + 16, 4);
            if (uint64_t(programsOff) + 24 <= dksh.size())
                memcpy(&scratch, dksh.data() + programsOff + 20, 4);
            if (scratch > perWarpScratch) {
                logf("shader %s needs %u bytes of scratch a warp, the queue has %u: not loaded", what, scratch, perWarpScratch);
                return false;
            }
        }
        std::lock_guard<std::mutex> lock(codeMutex);
        const uint32_t size = uint32_t(dksh.size());
        if (codeBlocks.empty() || alignUp(codeBlocks.back().second, DK_SHADER_CODE_ALIGNMENT) + size > CodeBlockSize - DK_SHADER_CODE_UNUSABLE_SIZE) {
            if (size > CodeBlockSize - DK_SHADER_CODE_UNUSABLE_SIZE) {
                logf("shader %s: %u bytes of DKSH do not fit a code block", what, size);
                return false;
            }
            DkMemBlock block = makeBlock(CodeBlockSize, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code, "shader code");
            if (!block)
                return false;
            codeBlocks.push_back({block, 0});
            logf("shader code: block %zu (16 MiB)", codeBlocks.size());
        }
        auto &cb = codeBlocks.back();
        const uint32_t at = alignUp(cb.second, DK_SHADER_CODE_ALIGNMENT);
        memcpy(static_cast<uint8_t *>(dkMemBlockGetCpuAddr(cb.first)) + at, dksh.data(), size);
        DkShaderMaker m;
        dkShaderMakerDefaults(&m, cb.first, at);
        dkShaderInitialize(&shader, &m);
        if (!dkShaderIsValid(&shader)) {
            logf("shader %s: %u bytes of DKSH are not valid", what, size);
            return false;
        }
        cb.second = at + size;
        g_deko3dStats.codeBytes += size;
        // Command lists invalidate the GPU's shader cache before their next draw (deko3d does it only at a queue
        // flush).
        codeEpoch.fetch_add(1, std::memory_order_release);
        return true;
    }

    // LostOdysseyRecomp (2026-10-27): a variant already loaded, or in the DKSH cache, by key (false: neither).
    bool Deko3DDevice::loadByKey(const Deko3DShader *base, uint64_t key, const DkShader *&out) {
        {
            std::lock_guard<std::mutex> lock(variantMutex);
            auto it = variants.find(key);
            if (it != variants.end()) {
                out = it->second->valid ? &it->second->shader : nullptr;
                return true;
            }
        }
        std::vector<uint8_t> dksh;
        if (!g_cacheLookup || !g_cacheLookup(key, dksh))
            return false;
        g_deko3dStats.shaderCacheHits++;
        auto fresh = std::make_unique<Deko3DShaderVariant>();
        fresh->valid = loadCode(dksh, fresh->shader, base->name.c_str());
        std::lock_guard<std::mutex> lock(variantMutex);
        auto &slot = variants[key];
        if (!slot)
            slot = std::move(fresh);
        out = slot->valid ? &slot->shader : nullptr;
        return true;
    }

    // The GLSL of a translation read from the cache, made when a variant needs the text.
    bool Deko3DDevice::translationText(const Deko3DShader *base) {
        Deko3DShaderImpl *shader = const_cast<Deko3DShaderImpl *>(static_cast<const Deko3DShaderImpl *>(base));
        std::lock_guard<std::mutex> lock(shader->textMutex);
        if (!shader->glsl.empty())
            return true;
        const Deko3DShader *source = shader->spirvSource;
        Deko3DSpirvResult r = Deko3DTranslateSpirv(source->spirv.data(), source->spirv.size(), source->spirvStage,
                                                   shader->spirvLayout, g_spirvOptions);
        g_deko3dStats.translations++;
        if (!r.ok) {
            logf("SPIR-V translation failed for %s: %s", shader->name.empty() ? "a shader" : shader->name.c_str(), r.error.c_str());
            return false;
        }
        shader->glsl = std::move(r.glsl);
        return true;
    }

    const DkShader *Deko3DDevice::loadVariant(const Deko3DShader *base, const RenderSpecConstant *specConstants, uint32_t specConstantsCount) {
        Deko3DShaderImpl *shader = const_cast<Deko3DShaderImpl *>(static_cast<const Deko3DShaderImpl *>(base));
        // The constants this shader has whose value differs from its default, by id.
        std::string defines;
        uint64_t specKey = 0xcbf29ce484222325ull;
        for (const SpecDefault &d : shader->specDefaults) {
            for (uint32_t i = 0; i < specConstantsCount; i++) {
                if (specConstants[i].index == d.id && specConstants[i].value != d.value) {
                    char line[64];
                    snprintf(line, sizeof(line), "#define SPIRV_CROSS_CONSTANT_ID_%u %uu\n", d.id, specConstants[i].value);
                    defines += line;
                    specKey = mixHash(specKey, &d.id, sizeof(d.id));
                    specKey = mixHash(specKey, &specConstants[i].value, sizeof(uint32_t));
                    break;
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(shader->mutex);
            auto it = shader->known.find(specKey);
            if (it != shader->known.end())
                return it->second;
        }
        // LostOdysseyRecomp (2026-10-27): a specialized variant is also kept under the key of its default text and its
        // defines (Deko3DVariantKey), so a cached translation finds it without the text (SPIRV-Cross on the console).
        uint64_t variantKey = 0;
        if (!defines.empty()) {
            const uint64_t baseKey = shader->glslKey ? shader->glslKey
                                     : !shader->glsl.empty() ? fnv1a64(shader->glsl.data(), shader->glsl.size()) : 0;
            if (baseKey) {
                variantKey = Deko3DVariantKey(baseKey, defines);
                const DkShader *found = nullptr;
                if (loadByKey(shader, variantKey, found)) {
                    std::lock_guard<std::mutex> lock(shader->mutex);
                    shader->known[specKey] = found;
                    return found;
                }
            }
        }
        // LostOdysseyRecomp (2026-10-27): a cached translation's default variant is looked up by its key alone; any
        // other use needs the text, translated now.
        if (shader->glsl.empty() && shader->spirvSource) {
            if (defines.empty() && shader->glslKey) {
                const DkShader *found = nullptr;
                if (loadByKey(shader, shader->glslKey, found)) {
                    std::lock_guard<std::mutex> lock(shader->mutex);
                    shader->known[specKey] = found;
                    return found;
                }
            }
            if (!translationText(shader))
                return nullptr;
        }
        // The variant's text: the GLSL with the constants defined after its first line (#version).
        std::string source;
        const size_t nl = shader->glsl.find('\n');
        if (defines.empty() || nl == std::string::npos) {
            source = shader->glsl;
        }
        else {
            source.reserve(shader->glsl.size() + defines.size());
            source.append(shader->glsl, 0, nl + 1);
            source += defines;
            source.append(shader->glsl, nl + 1, std::string::npos);
        }
        const uint64_t key = fnv1a64(source.data(), source.size());
        Deko3DShaderVariant *variant = nullptr;
        {
            std::lock_guard<std::mutex> lock(variantMutex);
            auto it = variants.find(key);
            if (it != variants.end())
                variant = it->second.get();
        }
        if (variant == nullptr) {
            std::vector<uint8_t> dksh;
            bool have = false;
            if (defines.empty() && !shader->defaultDksh.empty()) {
                dksh = shader->defaultDksh;
                have = true;
            }
            if (!have && g_cacheLookup && g_cacheLookup(key, dksh)) {
                g_deko3dStats.shaderCacheHits++;
                have = true;
            }
            if (!have) {
                std::string log;
                if (!compile(shader->stage, source, dksh, log)) {
                    g_deko3dStats.shaderCompileFailures++;
                    static std::atomic<uint32_t> logged{0};
                    if (logged.fetch_add(1) < 32)
                        logf("uam failed for %s (variant %016" PRIx64 "): %s", shader->name.empty() ? "a shader" : shader->name.c_str(), key, log.c_str());
                }
                else {
                    have = true;
                    if (g_cacheStore)
                        g_cacheStore(key, dksh);
                }
            }
            if (have && variantKey && g_cacheStore)
                g_cacheStore(variantKey, dksh);  // LostOdysseyRecomp: found by its variant key next time
            auto fresh = std::make_unique<Deko3DShaderVariant>();
            if (have)
                fresh->valid = loadCode(dksh, fresh->shader, shader->name.c_str());
            std::lock_guard<std::mutex> lock(variantMutex);
            auto &slot = variants[key];
            if (!slot)
                slot = std::move(fresh);
            variant = slot.get();
        }
        const DkShader *out = variant->valid ? &variant->shader : nullptr;
        std::lock_guard<std::mutex> lock(shader->mutex);
        shader->known[specKey] = out;
        return out;
    }

    // ---- samplers

    std::unique_ptr<RenderSampler> Deko3DDevice::createSampler(const RenderSamplerDesc &desc) {
        auto sampler = std::make_unique<Deko3DSampler>();
        sampler->desc = desc;
        DkSampler s;
        dkSamplerDefaults(&s);
        s.minFilter = desc.minFilter == RenderFilter::NEAREST ? DkFilter_Nearest : DkFilter_Linear;
        s.magFilter = desc.magFilter == RenderFilter::NEAREST ? DkFilter_Nearest : DkFilter_Linear;
        s.mipFilter = desc.mipmapMode == RenderMipmapMode::NEAREST ? DkMipFilter_Nearest : DkMipFilter_Linear;
        s.wrapMode[0] = toDk(desc.addressU);
        s.wrapMode[1] = toDk(desc.addressV);
        s.wrapMode[2] = toDk(desc.addressW);
        s.lodClampMin = desc.minLOD;
        s.lodClampMax = std::min(desc.maxLOD, 1000.0f);
        s.lodBias = desc.mipLODBias;
        s.compareEnable = desc.comparisonEnabled;
        s.compareOp = toDk(desc.comparisonFunc);
        const float border = desc.borderColor == RenderBorderColor::OPAQUE_WHITE ? 1.0f : 0.0f;
        const float borderAlpha = desc.borderColor == RenderBorderColor::TRANSPARENT_BLACK ? 0.0f : 1.0f;
        s.borderColor[0].value_f = border;
        s.borderColor[1].value_f = border;
        s.borderColor[2].value_f = border;
        s.borderColor[3].value_f = borderAlpha;
        s.maxAnisotropy = desc.anisotropyEnabled ? float(std::max<uint32_t>(desc.maxAnisotropy, 1)) : 1.0f;
        dkSamplerDescriptorInitialize(&sampler->descriptor, &s);
        return sampler;
    }

    // ---- pipeline layouts

    std::unique_ptr<RenderPipelineLayout> Deko3DDevice::createPipelineLayout(const RenderPipelineLayoutDesc &desc) {
        auto layout = std::make_unique<Deko3DPipelineLayout>();
        // Constant buffers: the k-th one of the layout (set order, binding order) is UBO binding k; storage buffers
        // likewise (an array takes one binding per element). tools/deko3d_glsl.py numbers the shaders' the same way.
        uint32_t ubo = 0, ssbo = 0;
        for (uint32_t s = 0; s < desc.descriptorSetDescsCount; s++) {
            const RenderDescriptorSetDesc &sd = desc.descriptorSetDescs[s];
            Deko3DPipelineLayout::Set set;
            uint32_t index = 0;
            for (uint32_t r = 0; r < sd.descriptorRangesCount; r++) {
                const RenderDescriptorRange &dr = sd.descriptorRanges[r];
                Deko3DPipelineLayout::Range range;
                range.type = dr.type;
                range.binding = dr.binding;
                range.count = dr.count;
                if (sd.lastRangeIsBoundless && r + 1 == sd.descriptorRangesCount)
                    range.count = std::max<uint32_t>(range.count, sd.boundlessRangeSize);
                range.descriptorIndex = index;
                switch (dr.type) {
                case RenderDescriptorRangeType::CONSTANT_BUFFER:
                case RenderDescriptorRangeType::DYNAMIC_CONSTANT_BUFFER:
                    range.slot = ubo;
                    ubo += range.count;
                    break;
                case RenderDescriptorRangeType::STRUCTURED_BUFFER:
                case RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER:
                case RenderDescriptorRangeType::BYTE_ADDRESS_BUFFER:
                case RenderDescriptorRangeType::READ_WRITE_BYTE_ADDRESS_BUFFER:
                case RenderDescriptorRangeType::FORMATTED_BUFFER:
                case RenderDescriptorRangeType::READ_WRITE_FORMATTED_BUFFER:
                    range.slot = ssbo;
                    ssbo += range.count;
                    break;
                default:
                    break;
                }
                index += range.count;
                set.ranges.push_back(range);
            }
            layout->sets.push_back(std::move(set));
        }
        if (ubo > 5)
            logf("pipeline layout with %u constant buffers: the push constants' UBO binding 5 overlaps them", ubo);
        for (uint32_t i = 0; i < desc.pushConstantRangesCount; i++) {
            layout->pushRanges.push_back(desc.pushConstantRanges[i]);
            layout->pushSize = std::max(layout->pushSize, desc.pushConstantRanges[i].offset + desc.pushConstantRanges[i].size);
        }
        if (layout->pushSize > PushSize)
            logf("push constants of %u bytes (more than %u)", layout->pushSize, PushSize);
        return layout;
    }

    // ---- pipelines

    static DkPrimitive toDk(RenderPrimitiveTopology t) {
        switch (t) {
        case RenderPrimitiveTopology::POINT_LIST: return DkPrimitive_Points;
        case RenderPrimitiveTopology::LINE_LIST: return DkPrimitive_Lines;
        case RenderPrimitiveTopology::LINE_STRIP: return DkPrimitive_LineStrip;
        case RenderPrimitiveTopology::TRIANGLE_STRIP: return DkPrimitive_TriangleStrip;
        case RenderPrimitiveTopology::TRIANGLE_FAN: return DkPrimitive_TriangleFan;
        case RenderPrimitiveTopology::TRIANGLE_LIST:
        default: return DkPrimitive_Triangles;
        }
    }

    // deko3d 0.5.0 writes the colour destination factor into FuncAlphaDst (SwitchWakerHD's deko3d patch 1, Tenkaichi-3-NX's
    // deko_blend_state.h): the target's IndependentBlend registers (method 0x781 + 8 * target, six of them) are
    // written again with the right alpha factor after dkCmdBufBindBlendStates.
    static uint32_t blendFactorWord(DkBlendFactor f) { return (uint32_t(f) & 0x1Fu) | ((uint32_t(f) & 0x20u) ? 0xC000u : 0u); }

    static void blendWords(uint32_t words[7], uint32_t target, const DkBlendState &s) {
        words[0] = (1u << 29) | (6u << 16) | (0u << 13) | (0x781u + 8u * target); // increasing, 6 words, 3D subchannel
        words[1] = s.colorBlendOp;
        words[2] = blendFactorWord(s.srcColorBlendFactor);
        words[3] = blendFactorWord(s.dstColorBlendFactor);
        words[4] = s.alphaBlendOp;
        words[5] = blendFactorWord(s.srcAlphaBlendFactor);
        words[6] = blendFactorWord(s.dstAlphaBlendFactor);
    }

    std::unique_ptr<RenderPipeline> Deko3DDevice::createGraphicsPipeline(const RenderGraphicsPipelineDesc &desc) {
        auto p = std::make_unique<Deko3DGraphicsPipeline>();
        p->type = Deko3DPipeline::Type::Graphics;
        p->device = this;
        p->layout = static_cast<const Deko3DPipelineLayout *>(desc.pipelineLayout);
        const Deko3DShader *stages[3] = {static_cast<const Deko3DShader *>(desc.vertexShader),
                                         static_cast<const Deko3DShader *>(desc.geometryShader),
                                         static_cast<const Deko3DShader *>(desc.pixelShader)};
        static const DkStage dkStages[3] = {DkStage_Vertex, DkStage_Geometry, DkStage_Fragment};
        // LostOdysseyRecomp (2026-10-27): SPIR-V shaders as GLSL for this layout.
        bool translatedOk = true;
        for (int i = 0; i < 3; i++) {
            if (!stages[i])
                continue;
            stages[i] = translated(stages[i], p->layout);
            translatedOk = translatedOk && stages[i] != nullptr;
        }
        bool ok = stages[0] != nullptr && translatedOk;
        // banjo-nx (2026-10-18, Deko3DOptions::vsOutputs): the vertex shader's constant 905, the locations the pixel
        // shader reads of the ones it writes, when that is not all of them (tools/deko3d_variant_cache.py's stage_spec
        // gives the DKSH cache the same).
        std::vector<RenderSpecConstant> vsSpec;
        if (g_deko3dOptions.vsOutputs && stages[0] && !stages[1] &&
            std::find(stages[0]->specIds.begin(), stages[0]->specIds.end(), 905u) != stages[0]->specIds.end()) {
            const uint32_t outs = stages[0]->usage.outputMask;
            const uint32_t mask = (stages[2] ? stages[2]->usage.inputMask : 0u) & outs;
            if (mask != outs) {
                vsSpec.assign(desc.specConstants, desc.specConstants + desc.specConstantsCount);
                vsSpec.push_back(RenderSpecConstant(905, mask));
            }
        }
        // banjo-nx (2026-10-18): the pixel shader's constant 906, its inputs the vertex shader writes as zero (not with a
        // geometry shader, which writes them).
        std::vector<RenderSpecConstant> psSpec;
        if (g_deko3dOptions.vsOutputs && stages[0] && stages[2] && !stages[1] &&
            std::find(stages[2]->specIds.begin(), stages[2]->specIds.end(), 906u) != stages[2]->specIds.end()) {
            const uint32_t zero = stages[0]->usage.zeroOutputMask & stages[2]->usage.inputMask;
            if (zero) {
                psSpec.assign(desc.specConstants, desc.specConstants + desc.specConstantsCount);
                psSpec.push_back(RenderSpecConstant(906, zero));
            }
        }
        for (int i = 0; i < 3; i++) {
            if (!stages[i])
                continue;
            const bool own = i == 0 && !vsSpec.empty();
            const bool ownPs = i == 2 && !psSpec.empty();
            const DkShader *s = loadVariant(stages[i], own ? vsSpec.data() : ownPs ? psSpec.data() : desc.specConstants,
                                            own ? uint32_t(vsSpec.size()) : ownPs ? uint32_t(psSpec.size())
                                                                                  : desc.specConstantsCount);
            if (!s) {
                ok = false;
                continue;
            }
            p->shaders[p->shaderCount++] = s;
            p->stageShaders[i] = s;  // banjo-nx (2026-10-19)
            p->stageMask |= 1u << dkStages[i];
            p->uboMask[i] = stages[i]->usage.uboMask;
            p->ssboMask[i] = stages[i]->usage.ssboMask;
            p->texBindings[i] = stages[i]->usage.texBindings;
            p->textureSources[i] = stages[i]->usage.textureSources;  // LostOdysseyRecomp (2026-10-27)
            p->pushWindows[i] = stages[i]->usage.pushWindows;
            p->hasPushWindows = p->hasPushWindows || !p->pushWindows[i].empty();
            p->boundTextures = p->boundTextures || !p->textureSources[i].empty();
            p->txqMask[i] = stages[i]->usage.txqMask;
            p->boundTextures = p->boundTextures || !p->texBindings[i].empty();
            p->textures = p->textures || stages[i]->usage.textures;
        }
        p->valid = ok;
        p->primitive = toDk(desc.primitiveTopology);

        dkRasterizerStateDefaults(&p->rasterizer);
        p->rasterizer.cullMode = desc.cullMode == RenderCullMode::FRONT ? DkFace_Front : desc.cullMode == RenderCullMode::BACK ? DkFace_Back : DkFace_None;
        p->rasterizer.frontFace = desc.frontFace == RenderFrontFace::COUNTER_CLOCKWISE ? DkFrontFace_CCW : DkFrontFace_CW;
        // D3D's (and Vulkan's) provoking vertex is the first one.
        p->rasterizer.provokingVertex = DkProvokingVertex_First;
        p->rasterizer.depthClampEnable = desc.depthClipEnabled ? 0 : 1;
        p->depthBias = float(desc.depthBias);
        p->depthBiasClamp = desc.depthBiasClamp;
        p->slopeScaledDepthBias = desc.slopeScaledDepthBias;
        p->dynamicDepthBias = desc.dynamicDepthBiasEnabled;
        if (desc.dynamicDepthBiasEnabled || desc.depthBias != 0 || desc.slopeScaledDepthBias != 0.0f)
            p->rasterizer.depthBiasEnableMask = DkPolygonFlag_All;

        dkMultisampleStateDefaults(&p->multisample);
        p->multisample.mode = toDkMsMode(desc.multisampling.sampleCount);
        p->multisample.rasterizerMode = p->multisample.mode;
        p->multisample.alphaToCoverageEnable = desc.alphaToCoverageEnabled;

        dkColorStateDefaults(&p->color);
        if (desc.logicOpEnabled)
            p->color.logicOp = toDk(desc.logicOp);
        dkColorWriteStateDefaults(&p->colorWrite);
        p->blendCount = std::min<uint32_t>(desc.renderTargetCount, 8);
        for (uint32_t i = 0; i < p->blendCount; i++) {
            const RenderBlendDesc &b = desc.renderTargetBlend[i];
            dkColorStateSetBlendEnable(&p->color, i, b.blendEnabled);
            dkColorWriteStateSetMask(&p->colorWrite, i, b.renderTargetWriteMask & 0xF);
            DkBlendState &s = p->blend[i];
            dkBlendStateDefaults(&s);
            if (b.blendEnabled) {
                s.colorBlendOp = toDk(b.blendOp);
                s.srcColorBlendFactor = toDk(b.srcBlend);
                s.dstColorBlendFactor = toDk(b.dstBlend);
                s.alphaBlendOp = toDk(b.blendOpAlpha);
                s.srcAlphaBlendFactor = toDkAlpha(b.srcBlendAlpha);
                s.dstAlphaBlendFactor = toDkAlpha(b.dstBlendAlpha);
                for (RenderBlend f : {b.srcBlend, b.dstBlend, b.srcBlendAlpha, b.dstBlendAlpha})
                    p->usesBlendConstant = p->usesBlendConstant || f == RenderBlend::BLEND_FACTOR || f == RenderBlend::INV_BLEND_FACTOR;
            }
            blendWords(p->blendWords[i], i, s);
        }
        for (uint32_t i = p->blendCount; i < 8; i++)
            dkColorWriteStateSetMask(&p->colorWrite, i, 0);

        dkDepthStencilStateDefaults(&p->depthStencil);
        p->depthStencil.depthTestEnable = desc.depthEnabled;
        p->depthStencil.depthWriteEnable = desc.depthWriteEnabled;
        p->depthStencil.depthCompareOp = toDk(desc.depthFunction);
        p->depthStencil.stencilTestEnable = desc.stencilEnabled;
        p->depthStencil.stencilFrontFailOp = toDk(desc.stencilFrontFace.failOp);
        p->depthStencil.stencilFrontPassOp = toDk(desc.stencilFrontFace.passOp);
        p->depthStencil.stencilFrontDepthFailOp = toDk(desc.stencilFrontFace.depthFailOp);
        p->depthStencil.stencilFrontCompareOp = toDk(desc.stencilFrontFace.compareFunction);
        p->depthStencil.stencilBackFailOp = toDk(desc.stencilBackFace.failOp);
        p->depthStencil.stencilBackPassOp = toDk(desc.stencilBackFace.passOp);
        p->depthStencil.stencilBackDepthFailOp = toDk(desc.stencilBackFace.depthFailOp);
        p->depthStencil.stencilBackCompareOp = toDk(desc.stencilBackFace.compareFunction);
        p->stencilEnabled = desc.stencilEnabled;
        p->stencilReadMask = uint8_t(desc.stencilReadMask);
        p->stencilWriteMask = uint8_t(desc.stencilWriteMask);
        p->stencilReference = uint8_t(desc.stencilReference);
        p->dynamicStencilReference = false; // LostOdysseyRecomp's plume: the reference is pipeline state

        // Vertex input: the vertex shader's attribute i reads the element at its original location vin[i]
        // (tools/deko3d_glsl.py renumbered them to fit uam's 16).
        const std::vector<uint32_t> &vin = stages[0] ? stages[0]->usage.vertexInputs : std::vector<uint32_t>();
        p->attribCount = uint32_t(vin.size());
        for (uint32_t a = 0; a < p->attribCount; a++) {
            DkVtxAttribState &st = p->attribs[a];
            memset(&st, 0, sizeof(st));
            st.isFixed = 1; // a constant zero unless an element feeds it
            st.size = DkVtxAttribSize_1x32;
            st.type = DkVtxAttribType_Float;
            for (uint32_t e = 0; e < desc.inputElementsCount; e++) {
                const RenderInputElement &el = desc.inputElements[e];
                if (el.location != vin[a])
                    continue;
                DkVtxAttribSize size;
                DkVtxAttribType type;
                bool bgra;
                if (!vertexFormat(el.format, size, type, bgra)) {
                    logf("vertex element format %d has no deko3d form", int(el.format));
                    break;
                }
                st.isFixed = 0;
                st.bufferId = el.slotIndex;
                st.offset = el.alignedByteOffset;
                st.size = size;
                st.type = type;
                st.isBgra = bgra ? 1 : 0;
                break;
            }
        }
        for (uint32_t s = 0; s < desc.inputSlotsCount; s++) {
            const RenderInputSlot &slot = desc.inputSlots[s];
            if (slot.index >= 16)
                continue;
            p->vtxBuffers[slot.index].stride = slot.stride;
            p->vtxBuffers[slot.index].divisor = slot.classification == RenderInputSlotClassification::PER_INSTANCE_DATA ? 1 : 0;
            p->vtxBufferCount = std::max(p->vtxBufferCount, slot.index + 1);
        }

        uint64_t h = 0xcbf29ce484222325ull;
        h = mixHash(h, &p->primitive, sizeof(p->primitive));
        h = mixHash(h, &p->rasterizer, sizeof(p->rasterizer));
        h = mixHash(h, &p->multisample, sizeof(p->multisample));
        h = mixHash(h, &p->color, sizeof(p->color));
        h = mixHash(h, &p->colorWrite, sizeof(p->colorWrite));
        h = mixHash(h, p->blendWords, sizeof(p->blendWords));
        h = mixHash(h, &p->depthStencil, sizeof(p->depthStencil));
        h = mixHash(h, &p->stencilReadMask, 3);
        h = mixHash(h, &p->depthBias, sizeof(float) * 3);
        h = mixHash(h, p->attribs, sizeof(DkVtxAttribState) * p->attribCount);
        h = mixHash(h, &p->attribCount, sizeof(p->attribCount));
        h = mixHash(h, p->vtxBuffers, sizeof(DkVtxBufferState) * p->vtxBufferCount);
        h = mixHash(h, &p->vtxBufferCount, sizeof(p->vtxBufferCount));
        p->stateHash = h | 1;
        // banjo-nx (2026-10-19, CPU proposal 13): each group of setPipeline's state binds, hashed apart (never 0).
        auto group = [](std::initializer_list<std::pair<const void *, size_t>> parts) {
            uint64_t g = 0xcbf29ce484222325ull;
            for (const auto &[data, size] : parts)
                g = mixHash(g, data, size);
            return g | 1;
        };
        p->groupHash[0] = group({{&p->rasterizer, sizeof(p->rasterizer)}});
        p->groupHash[1] = group({{&p->multisample, sizeof(p->multisample)}});
        p->groupHash[2] = group({{&p->color, sizeof(p->color)}});
        p->groupHash[3] = group({{&p->colorWrite, sizeof(p->colorWrite)}});
        p->groupHash[4] = group({{&p->blendCount, sizeof(p->blendCount)},
                                 {p->blend, sizeof(DkBlendState) * p->blendCount},
                                 {p->blendWords, sizeof(p->blendWords[0]) * p->blendCount}});
        p->groupHash[5] = group({{&p->depthStencil, sizeof(p->depthStencil)}});
        p->groupHash[6] = group({{&p->stencilReadMask, 1}, {&p->stencilWriteMask, 1}, {&p->stencilReference, 1}});
        p->groupHash[7] = group({{&p->depthBias, sizeof(float)}, {&p->depthBiasClamp, sizeof(float)},
                                 {&p->slopeScaledDepthBias, sizeof(float)}});
        p->groupHash[8] = group({{&p->attribCount, sizeof(p->attribCount)},
                                 {p->attribs, sizeof(DkVtxAttribState) * p->attribCount},
                                 {&p->vtxBufferCount, sizeof(p->vtxBufferCount)},
                                 {p->vtxBuffers, sizeof(DkVtxBufferState) * p->vtxBufferCount}});
        return p;
    }

    std::unique_ptr<RenderPipeline> Deko3DDevice::createComputePipeline(const RenderComputePipelineDesc &desc) {
        auto p = std::make_unique<Deko3DComputePipeline>();
        p->type = Deko3DPipeline::Type::Compute;
        p->device = this;
        p->layout = static_cast<const Deko3DPipelineLayout *>(desc.pipelineLayout);
        const Deko3DShader *shader = translated(static_cast<const Deko3DShader *>(desc.computeShader), p->layout);
        if (shader) {
            p->shader = loadVariant(shader, desc.specConstants, desc.specConstantsCount);
            p->uboMask = shader->usage.uboMask;
            p->ssboMask = shader->usage.ssboMask;
        }
        p->valid = p->shader != nullptr;
        return p;
    }

    std::unique_ptr<RenderPipeline> Deko3DDevice::createRaytracingPipeline(const RenderRaytracingPipelineDesc &, const RenderPipeline *) { return nullptr; }

    // ---- the rest of the device

    std::unique_ptr<RenderDescriptorSet> Deko3DDevice::createDescriptorSet(const RenderDescriptorSetDesc &desc) {
        return std::make_unique<Deko3DDescriptorSet>(this, desc);
    }

    std::unique_ptr<RenderCommandQueue> Deko3DDevice::createCommandQueue(RenderCommandListType type) {
        return std::make_unique<Deko3DCommandQueue>(this, type);
    }

    std::unique_ptr<RenderBuffer> Deko3DDevice::createBuffer(const RenderBufferDesc &desc) {
        auto buffer = std::make_unique<Deko3DBuffer>(this, desc);
        if (!buffer->block)
            return nullptr;
        return buffer;
    }

    std::unique_ptr<RenderTexture> Deko3DDevice::createTexture(const RenderTextureDesc &desc) {
        return std::make_unique<Deko3DTexture>(this, desc);
    }

    std::unique_ptr<RenderAccelerationStructure> Deko3DDevice::createAccelerationStructure(const RenderAccelerationStructureDesc &) { return nullptr; }

    std::unique_ptr<RenderPool> Deko3DDevice::createPool(const RenderPoolDesc &) { return nullptr; }

    std::unique_ptr<RenderCommandFence> Deko3DDevice::createCommandFence() { return std::make_unique<Deko3DCommandFence>(); }

    std::unique_ptr<RenderCommandSemaphore> Deko3DDevice::createCommandSemaphore() { return std::make_unique<Deko3DCommandSemaphore>(); }

    std::unique_ptr<RenderFramebuffer> Deko3DDevice::createFramebuffer(const RenderFramebufferDesc &desc) {
        auto fb = std::make_unique<Deko3DFramebuffer>();
        fb->colorCount = std::min<uint32_t>(desc.colorAttachmentsCount, 8);
        for (uint32_t i = 0; i < fb->colorCount; i++) {
            const Deko3DTexture *t = static_cast<const Deko3DTexture *>(desc.colorAttachments[i]);
            const Deko3DTextureView *v = desc.colorAttachmentViews ? static_cast<const Deko3DTextureView *>(desc.colorAttachmentViews[i]) : nullptr;
            fb->colors[i] = v ? v->view : t->defaultView();
            fb->srgb[i] = isSrgbFormat(fb->colors[i].format != DkImageFormat_None ? fb->colors[i].format : t->format);
            if (i == 0) {
                // The view's level (mip chains rendered level by level).
                fb->width = std::max<uint32_t>(t->desc.width >> fb->colors[i].mipLevelOffset, 1);
                fb->height = std::max<uint32_t>(t->desc.height >> fb->colors[i].mipLevelOffset, 1);
            }
        }
        if (desc.depthAttachment) {
            const Deko3DTexture *t = static_cast<const Deko3DTexture *>(desc.depthAttachment);
            const Deko3DTextureView *v = static_cast<const Deko3DTextureView *>(desc.depthAttachmentView);
            fb->depth = v ? v->view : t->defaultView();
            fb->hasDepth = true;
            if (fb->colorCount == 0) {
                fb->width = t->desc.width;
                fb->height = t->desc.height;
            }
        }
        return fb;
    }

    std::unique_ptr<RenderQueryPool> Deko3DDevice::createQueryPool(uint32_t queryCount) {
        return std::make_unique<Deko3DQueryPool>(this, queryCount);
    }

    void Deko3DDevice::setBottomLevelASBuildInfo(RenderBottomLevelASBuildInfo &, const RenderBottomLevelASMesh *, uint32_t, bool, bool) {}
    void Deko3DDevice::setTopLevelASBuildInfo(RenderTopLevelASBuildInfo &, const RenderTopLevelASInstance *, uint32_t, bool, bool) {}
    void Deko3DDevice::setShaderBindingTableInfo(RenderShaderBindingTableInfo &, const RenderShaderBindingGroups &, const RenderPipeline *, RenderDescriptorSet **, uint32_t) {}

    RenderSampleCounts Deko3DDevice::getSampleCountsSupported(RenderFormat) const {
        return RenderSampleCount::COUNT_1 | RenderSampleCount::COUNT_2 | RenderSampleCount::COUNT_4 | RenderSampleCount::COUNT_8;
    }

    // ---- Deko3DInterface

    Deko3DInterface::Deko3DInterface() {
        capabilities.shaderFormat = RenderShaderFormat::SPIRV;  // LostOdysseyRecomp: translated per pipeline layout
        deviceNames.push_back("deko3d");
    }

    std::unique_ptr<RenderDevice> Deko3DInterface::createDevice(const std::string &) {
        auto device = std::make_unique<Deko3DDevice>();
        if (!device->initialize())
            return nullptr;
        return device;
    }

    std::unique_ptr<RenderInterface> CreateDeko3DInterface() {
        return std::make_unique<Deko3DInterface>();
    }

    Deko3DFenceCopy Deko3DCopyFence(const RenderCommandFence *fence) {
        Deko3DFenceCopy copy;
        if (const auto *f = static_cast<const Deko3DCommandFence *>(fence)) {
            if (f->pending) {
                copy.fence = f->fence;
                copy.valid = true;
            } else {
                copy.done = true;
            }
        }
        return copy;
    }

    bool Deko3DFenceCopyDone(Deko3DFenceCopy &copy) {
        if (!copy.done && copy.valid && dkFenceWait(&copy.fence, 0) == DkResult_Success)
            copy.done = true;  // signalled once is signalled for good: no second wait
        return copy.done;
    }

    bool Deko3DFenceCopyWait(Deko3DFenceCopy &copy, RenderDevice *device) {
        if (copy.done || !copy.valid)
            return copy.done;
        Deko3DDevice *d = static_cast<Deko3DDevice *>(device);
        for (;;) {
            if (dkFenceWait(&copy.fence, 2'000'000'000ll) == DkResult_Success) {
                copy.done = true;
                return true;
            }
            if (!d || !d->queue || dkQueueIsInErrorState(d->queue))
                return false;
        }
    }

    bool Deko3DDeviceLost(RenderDevice *device) {
        Deko3DDevice *d = static_cast<Deko3DDevice *>(device);
        return d && d->queue && dkQueueIsInErrorState(d->queue);
    }
};
