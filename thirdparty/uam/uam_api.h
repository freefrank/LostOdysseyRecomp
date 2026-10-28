#pragma once
// The renderer's view of uam (devkitPro's deko3d shader compiler, vendored here with the patches in
// PATCHES.md): GLSL source in, DKSH bytes out (the same bytes the uam command line tool writes).
//
// NOT thread-safe and NOT reentrant: Mesa's GLSL frontend inside uam keeps one static gl_context and
// global tables, so init() and every compile() must run on ONE thread (the renderer's compile worker).
// That thread needs a large stack (Mesa's parser and nv50_ir recurse deeply: 8 MB on the Switch).
#include <cstdint>
#include <string>
#include <vector>

namespace uam {

enum class Stage { Vertex, TessCtrl, TessEval, Geometry, Fragment, Compute };

struct Result {
    bool ok = false;
    std::vector<uint8_t> dksh;  // a whole .dksh file (header, program header, code, constants); empty on failure
    std::string log;            // everything uam reported for this shader (errors, warnings); may be set when ok
};

// once, before the first compile(). resident_frontend keeps Mesa's built-in GLSL function library and
// type tables alive between compiles (built on the first compile, a few MB); false rebuilds and frees
// them around every compile, as the uam tool does.
void init(bool resident_frontend = true);
// frees what init() and the compiles kept; compile() works again after another init()
void shutdown();

// compiles one stage of a separable program (uam has no linking; every binding must be explicit).
// A compile error, and any fatal error inside Mesa or nv50_ir, returns ok = false with the reason in
// log; it never exits the process.
Result compile(Stage stage, const char* glsl);

// banjo-nx (2026-10-18): banjo-nx's codegen passes (nv50_ir_peephole.cpp): 1 PRMT byte moves, 2 the storage buffer
// offset fold, 4 the shift distribution, 8 the fragment input interpolation cache, 16 choose per fragment shader
// whether 8 pays (default all). Before compiling; the DKSH cache must be one made with the same.
void setBanjoFlags(unsigned flags);

}  // namespace uam
