// uam_api.h on top of uam's DekoCompiler (one thread only, see the header)
#include "uam_api.h"

#include "compiler_iface.h"
#include "uam_log.h"

namespace uam {
namespace {

void append_log(const char* msg, void* user) {
    static_cast<std::string*>(user)->append(msg);
}

pipeline_stage to_pipeline_stage(Stage s) {
    switch (s) {
        case Stage::Vertex: return pipeline_stage_vertex;
        case Stage::TessCtrl: return pipeline_stage_tess_ctrl;
        case Stage::TessEval: return pipeline_stage_tess_eval;
        case Stage::Geometry: return pipeline_stage_geometry;
        case Stage::Fragment: return pipeline_stage_fragment;
        case Stage::Compute: return pipeline_stage_compute;
    }
    return pipeline_stage_fragment;
}

bool g_resident = false;

}  // namespace

void init(bool resident_frontend) {
    glsl_frontend_init();
    g_resident = resident_frontend;
    DekoCompiler::SetFrontendResident(resident_frontend);
}

void shutdown() {
    if (g_resident) glsl_frontend_exit();
    g_resident = false;
    DekoCompiler::SetFrontendResident(false);
}

extern "C" void nv50_ir_set_banjo_flags(unsigned flags);

void setBanjoFlags(unsigned flags) {
    nv50_ir_set_banjo_flags(flags);
}

Result compile(Stage stage, const char* glsl) {
    Result r;
    uam_set_log_callback(append_log, &r.log);
    {
        // banjo-nx (2026-10-18): CompileBest, the same choice as the PC tool's (the DKSH cache)
        std::unique_ptr<DekoCompiler> c = glsl ? DekoCompiler::CompileBest(to_pipeline_stage(stage), glsl, 3) : nullptr;
        r.ok = c != nullptr;
        if (r.ok) c->WriteDksh(r.dksh);
    }
    uam_set_log_callback(nullptr, nullptr);
    return r;
}

}  // namespace uam
