#include <gpu/frame_generation_settings.h>
#include <cstdio>
#include <cstdlib>

void Check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

int main() {
    settings::Config saved;
    Check(!gpu::frame_generation::ResolveD3D12Selection(saved,nullptr,nullptr,nullptr,nullptr,nullptr).Enabled(),
        "missing setting defaults to off");
    saved.frameGenerationProvider = framegen::Provider::Dlss;
    saved.frameGenerationMode = framegen::Mode::Dynamic;
    saved.frameGenerationMultiplier = 4;
    saved.frameGenerationTargetFps = 144;
    auto selected = gpu::frame_generation::ResolveD3D12Selection(saved,nullptr,nullptr,nullptr,nullptr,nullptr);
    Check(selected.config.provider == framegen::Provider::Dlss && selected.config.mode == framegen::Mode::Dynamic &&
          selected.config.generatedFrames == 3 && selected.config.targetFrameRate == 144,
        "persisted DLSS request maps to SDK values");
    selected = gpu::frame_generation::ResolveD3D12Selection(saved,"off",nullptr,nullptr,nullptr,"1");
    Check(!selected.Enabled(), "explicit provider off wins over legacy enable");
    selected = gpu::frame_generation::ResolveD3D12Selection(saved,nullptr,nullptr,nullptr,nullptr,"0");
    Check(!selected.Enabled(), "legacy disable overrides saved enable");
    selected = gpu::frame_generation::ResolveD3D12Selection(saved,nullptr,nullptr,nullptr,nullptr,"1");
    Check(selected.config.provider == framegen::Provider::Dlss && selected.config.mode == framegen::Mode::Fixed &&
          selected.config.generatedFrames == 1, "legacy enable retains fixed 2x defaults");
    selected = gpu::frame_generation::ResolveD3D12Selection(saved,"fsr",nullptr,nullptr,nullptr,nullptr);
    Check(selected.config.provider == framegen::Provider::Fsr && selected.config.mode == framegen::Mode::Fixed &&
          selected.config.generatedFrames == 1, "explicit provider keeps environment defaults");
    selected = gpu::frame_generation::ResolveD3D12Selection(saved,nullptr,"fixed","2","0",nullptr);
    Check(selected.config.provider == framegen::Provider::Dlss && selected.config.mode == framegen::Mode::Fixed &&
          selected.config.generatedFrames == 1 && selected.config.targetFrameRate == 0,
        "individual switches override saved values");
    selected = gpu::frame_generation::ResolveD3D12Selection(saved,"fsr","dynamic",nullptr,nullptr,nullptr);
    Check(!selected.Enabled() && selected.error, "unsupported FSR dynamic request fails closed");
#ifdef _WIN32
    Check(gpu::frame_generation::D3D12CompiledProvider(framegen::Provider::Dlss) &&
          gpu::frame_generation::D3D12CompiledProvider(framegen::Provider::Fsr), "test build exposes both compiled providers");
#else
    Check(!gpu::frame_generation::D3D12CompiledProvider(framegen::Provider::Dlss), "non-Windows has no D3D12 FG");
#endif
    saved.variableRefreshRate = true;
    saved.frameGenerationTargetFps = 0;
    selected = gpu::frame_generation::ResolveD3D12Selection(saved,nullptr,nullptr,nullptr,nullptr,nullptr,144);
#ifdef _WIN32
    Check(selected.Enabled() && selected.config.targetFrameRate == 141,
        "VRR bounds automatic dynamic SDK target below display refresh");
    selected = gpu::frame_generation::ResolveD3D12Selection(saved,nullptr,nullptr,nullptr,"100",nullptr,144);
    Check(selected.config.targetFrameRate == 100, "lower explicit dynamic target is preserved");
    selected = gpu::frame_generation::ResolveD3D12Selection(saved,nullptr,nullptr,nullptr,"240",nullptr,144);
    Check(selected.config.targetFrameRate == 141, "higher explicit dynamic target gets VRR headroom");
    selected = gpu::frame_generation::ResolveD3D12Selection(saved,nullptr,nullptr,nullptr,nullptr,nullptr,0);
    Check(selected.config.targetFrameRate == 0, "unknown display leaves SDK automatic target intact");
#else
    Check(!selected.Enabled(), "VRR never enables an uncompiled provider");
#endif
    selected = gpu::frame_generation::ResolveD3D12Selection(saved,nullptr,nullptr,nullptr,"-1",nullptr,144);
    Check(!selected.Enabled() && selected.error, "VRR cannot sanitize invalid SDK input into success");
    Check(saved.frameGenerationTargetFps == 0 && saved.frameGenerationMultiplier == 4,
        "effective VRR target never overwrites saved FG settings");
    saved.variableRefreshRate = false;
    selected = gpu::frame_generation::ResolveD3D12Selection(saved,nullptr,nullptr,nullptr,nullptr,nullptr,144);
    Check(selected.config.targetFrameRate == 0, "VRR off restores SDK automatic target");
    std::puts("PASS frame-generation settings selection including VRR");
}
