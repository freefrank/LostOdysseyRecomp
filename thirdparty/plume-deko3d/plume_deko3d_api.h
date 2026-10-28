//
// LostOdysseyRecomp (2026-10-27): what the application calls of plume's deko3d back end, without <deko3d.h> (it
// brings libnx's <switch.h>, whose Mutex/Event/Thread typedefs collide with the runtime's). plume_deko3d.h includes
// this, so the declarations cannot drift.
//
#pragma once

#include "plume_render_interface.h"
#include "plume_deko3d_spirv.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace plume {
    // What a DKSH cache gives the device: the DKSH for a variant's full GLSL text (its hash), or null.
    using Deko3DShaderCacheLookup = std::function<bool(uint64_t sourceHash, std::vector<uint8_t> &dksh)>;
    // Called with every DKSH the device compiled (to keep in a cache).
    using Deko3DShaderCacheStore = std::function<void(uint64_t sourceHash, const std::vector<uint8_t> &dksh)>;
    void SetDeko3DShaderCache(Deko3DShaderCacheLookup lookup, Deko3DShaderCacheStore store);
    // Where the back end's messages go (stderr until set).
    void SetDeko3DLogCallback(void (*callback)(const char *message));
    // What the SPIR-V translation assumes (the address window), before shaders are made.
    void Deko3DSetSpirvOptions(const Deko3DSpirvOptions &options);
    std::unique_ptr<RenderInterface> CreateDeko3DInterface();

    // LostOdysseyRecomp (2026-10-27): Deko3DOptions' GPU switches (plume_deko3d.h has what each does), before
    // CreateDeko3DInterface. All off by default, as banjo-nx ships them.
    struct Deko3DGpuOptions {
        bool depthCompression = false;
        bool zcull = false;
        bool mergeBarriers = false;
        bool leanBarriers = false;
        // Vertex shaders store only the outputs the pipeline's pixel shader reads (the translator's constant 905).
        bool vsOutputs = false;
        // Colour render targets with hardware compression (on by default).
        bool colorCompression = true;
    };
    void Deko3DSetGpuOptions(const Deko3DGpuOptions &options);

    // The back end's running totals (Deko3DStats), for the renderer's per-frame report.
    struct Deko3DCounters {
        uint64_t draws = 0, pipelineBinds = 0, stateBinds = 0, descriptorWrites = 0;
        uint64_t barriersPrimitives = 0, barriersFragments = 0, barriersFull = 0, barriersImageInvalidate = 0;
        uint64_t copyBuffer = 0, copyUpload = 0, copyReadback = 0, copyImage = 0, copyResolve = 0, copyBlit = 0;
    };
    Deko3DCounters Deko3DReadCounters();
}
