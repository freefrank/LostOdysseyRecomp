from pathlib import Path
import hashlib
import re
root = Path.cwd()
EXPECTED = {
    "LostOdysseyRecomp/gpu/renderer.cpp": "b3850e989fcf17b61139cd2469ddd2ed3934b11523516a07e16b8d4c60838e5c",
    "LostOdysseyRecomp/gpu/renderer.h": "06556a249a90cadb6867f2a0d80697f98a0004345893a96c0ce590b22bb8ae84",
    "LostOdysseyRecomp/gpu/command_processor.cpp": "1be2e9acb5146b07e01e2c1a38a948f54455356b95c71b4cfe9929a960e12d19",
    "tools/tests/native_dlss_renderer_gpu_test.cpp": "f32a81ccabba3cfe9e859af4e0806d18c3a14ff3840d8d014af1477d076387fb",
}
for name, expected in EXPECTED.items():
    actual = hashlib.sha256((root/name).read_bytes()).hexdigest()
    if actual != expected:
        raise SystemExit(f"Refuse to replace changed source: {name} {actual}")
p=root/'LostOdysseyRecomp/gpu/renderer.h';s=p.read_text(encoding="utf-8");start=s.index('    struct DrawInfo');end=s.index('    // Initialises',start);s=s[:start]+s[end:];s=s.replace('#include "present_capture.h"','#include "present_capture.h"\n#include "draw_state.h"');s=s.replace("// Xenos draw backend on plume: turns the command processor's register state\n// plus a DRAW_INDX packet into host draws, emulates EDRAM render targets as", "// Shared Xenos draw backend on plume: consumes explicit draw state, emulates\n// EDRAM render targets as");s=s.replace('    // Called for every DRAW_INDX / DRAW_INDX_2 after the registers were updated.\n    void Draw(const DrawInfo& info);','    // Synchronous, CP-worker-only. Borrowed state must remain valid until return.\n    // Both native and legacy adapters call this same preparation/recording path.\n    void Draw(const DrawState& state);');p.write_text(s, encoding="utf-8", newline="\n")
p=root/'LostOdysseyRecomp/gpu/renderer.cpp';s=p.read_text(encoding="utf-8");s=s.replace('#include "command_processor.h"\n','')
start=s.index('        // ---- register indices');end=s.index('        constexpr uint32_t kUploadRingSize',start);s=s[:start]+s[end:]
s=s.replace('        uint32_t Reg(uint32_t index) { return g_commandProcessor.ReadRegister(index); }\n        float RegF(uint32_t index) { uint32_t v = Reg(index); float f; memcpy(&f, &v, 4); return f; }\n','')
s=s.replace('            void Draw(const DrawInfo& info)\n            {','            void Draw(const DrawState& state)\n            {\n                const auto& info = state.draw;')
s=s.replace('debugRegisters.resize(REGISTER_COUNT)','debugRegisters.resize(state.legacyTraceRegisters.Size())')
s=s.replace('                    const bool first = debugRegisters.empty();\n                    if (first) debugRegisters.resize(state.legacyTraceRegisters.Size());','                    const bool first = debugRegisters.size() != state.legacyTraceRegisters.Size() || debugRegisters.empty();\n                    if (first) debugRegisters.resize(state.legacyTraceRegisters.Size());')
s=s.replace('                DrawImpl(info);','                DrawImpl(state);')
s=s.replace('            void DrawImpl(const DrawInfo& info)\n            {','            void DrawImpl(const DrawState& state)\n            {\n                const auto& info = state.draw;')
s=s.replace('                    Resolve();','                    Resolve(state);')
s=s.replace("                // Shaders come from the command processor's last IM_LOAD.","                // Immutable shader bindings are supplied by either front-end adapter.")
s=s.replace('                    vsWords = g_commandProcessor.GetActiveShader(false, vsCount, vsCommandHash);\n                    psWords = g_commandProcessor.GetActiveShader(true, psCount, psCommandHash);', '''                    vsWords = state.vertexShader.words.data();
                    psWords = state.pixelShader.words.data();
                    vsCount = uint32_t(state.vertexShader.words.size());
                    psCount = uint32_t(state.pixelShader.words.size());
                    vsCommandHash = state.vertexShader.commandHash;
                    psCommandHash = state.pixelShader.commandHash;''')
s=s.replace('g_commandProcessor.GetActiveShaderByteHash(false)', 'state.vertexShader.byteHash').replace('g_commandProcessor.GetActiveShaderByteHash(true)', 'state.pixelShader.byteHash')
s=s.replace('g_commandProcessor.ReadRegisters(REG_ALU_CONSTANTS, 256 * 4, vsConstants);','state.vertexConstants.Copy(vsConstants);').replace('g_commandProcessor.ReadRegisters(REG_ALU_CONSTANTS + 256 * 4, 256 * 4, psConstants);','state.pixelConstants.Copy(psConstants);')
s=s.replace('            void Resolve()','            void Resolve(const DrawState& state)').replace('                ResolveImpl();','                ResolveImpl(state);').replace('            void ResolveImpl()','            void ResolveImpl(const DrawState& state)')
s=s.replace('ClearDepthTarget(pitch, rtHeight)', 'ClearDepthTarget(state, pitch, rtHeight)').replace('void ClearDepthTarget(uint32_t pitch, uint32_t rtHeight)','void ClearDepthTarget(const DrawState& state, uint32_t pitch, uint32_t rtHeight)')
s=s.replace('                              uint32_t x0, uint32_t y0, uint32_t w, uint32_t h)\n            {\n                if (!Begin()) return false;\n                const uint32_t guestW', '                              uint32_t x0, uint32_t y0, uint32_t w, uint32_t h, bool swapRedBlue)\n            {\n                if (!Begin()) return false;\n                const uint32_t guestW')
s=s.replace('rs.swapRedBlue = ((Reg(REG_RB_COPY_DEST_INFO) >> 24) & 1) != 0;', 'rs.swapRedBlue = swapRedBlue;')
s=s.replace('ResolveOnGpu(*color, destBase, destFormat, destPitch, destHeight, x0, y0, copyWidth, copyHeight)', 'ResolveOnGpu(*color, destBase, destFormat, destPitch, destHeight, x0, y0, copyWidth, copyHeight, ((destInfo >> 24) & 1) != 0)')
s=s.replace('    void Draw(const DrawInfo& info)\n    {\n        if (g_renderer)\n            g_renderer->Draw(info);\n    }','    void Draw(const DrawState& state)\n    {\n        if (g_renderer)\n            g_renderer->Draw(state);\n    }').replace('    void Draw(const DrawInfo&) {}','    void Draw(const DrawState&) {}')
fields={
'REG_RB_SURFACE_INFO':'targets.surfaceInfo','REG_RB_COLOR_INFO':'targets.colorInfo[0]',
'REG_RB_DEPTH_INFO':'targets.depthInfo','REG_RB_DEPTH_CLEAR':'resolve.depthClear',
'REG_RB_COLOR_CLEAR':'resolve.colorClear','REG_PA_SC_WINDOW_OFFSET':'viewport.windowOffset',
'REG_PA_SC_WINDOW_SCISSOR_TL':'viewport.scissorTL','REG_PA_SC_WINDOW_SCISSOR_BR':'viewport.scissorBR',
'REG_VGT_INDX_OFFSET':'baseVertex','REG_RB_COLOR_MASK':'pipeline.colorMask',
'REG_RB_ALPHA_REF':'pipeline.alphaRef','REG_RB_STENCILREFMASK_BF':'pipeline.stencilRefMaskBack',
'REG_RB_STENCILREFMASK':'pipeline.stencilRefMask','REG_RB_DEPTHCONTROL':'pipeline.depthControl',
'REG_RB_BLENDCONTROL0':'pipeline.blendControl','REG_RB_COLORCONTROL':'pipeline.colorControl',
'REG_PA_SU_SC_MODE_CNTL':'pipeline.modeCull','REG_PA_SU_POINT_SIZE':'pipeline.pointSize',
'REG_PA_SU_POINT_MINMAX':'pipeline.pointMinMax','REG_PA_CL_VTE_CNTL':'viewport.transformControl',
'REG_RB_MODECONTROL':'pipeline.modeControl','REG_PA_SU_VTX_CNTL':'viewport.vertexControl',
'REG_RB_COPY_CONTROL':'resolve.control','REG_RB_COPY_DEST_BASE':'resolve.destinationBase',
'REG_RB_COPY_DEST_PITCH':'resolve.destinationPitch','REG_RB_COPY_DEST_INFO':'resolve.destinationInfo',
'0x2204':'pipeline.clipControl','i':'legacyTraceRegisters.Read(i)',
}
def convert(kind,expr):
    if expr in fields: return 'state.'+fields[expr]
    if expr.startswith('REG_RB_COLOR_INFO +'): return 'state.targets.colorInfo[srcSelect < 4 ? srcSelect : 0]'
    for base,name in [('REG_ALU_CONSTANTS','AluConstantFloat' if kind=='RegF' else 'AluConstant'),('REG_FETCH_CONSTANTS','fetchConstants.Read'),('REG_BOOL_CONSTANTS','boolConstants.Read'),('REG_LOOP_CONSTANTS','loopConstants.Read')]:
        if expr.startswith(base):
            offset=expr[len(base):].strip()
            assert not offset or offset.startswith('+'), expr
            offset=offset[1:].strip() if offset else '0'
            return 'state.'+name+'('+offset+')'
    if expr.startswith('REG_PA_CL_VPORT_XSCALE'):
        offset=expr[len('REG_PA_CL_VPORT_XSCALE'):].strip()
        offset=offset[1:].strip() if offset else '0'
        return 'state.viewport.scaleOffset['+offset+']'
    if kind=='RegF' and expr in ['0x2380','0x2381','0x2382','0x2383']:
        return f'state.pipeline.polygonOffset[{int(expr,16)-0x2380}]'
    raise ValueError((kind,expr))
changes=[]
for m in re.finditer(r'\b(RegF|Reg)\(',s):
    start=m.end();depth=1;i=start
    while depth:
        if s[i]=='(': depth+=1
        elif s[i]==')': depth-=1
        i+=1
    changes.append((m.start(),i,convert(m[1],s[start:i-1])))
for a,b,value in reversed(changes): s=s[:a]+value+s[b:]
assert not re.search(r'\bRegF?\(|g_commandProcessor|REG_[A-Z]',s)
p.write_text(s, encoding="utf-8", newline="\n")
p=root/'LostOdysseyRecomp/gpu/command_processor.cpp';s=p.read_text(encoding="utf-8");s=s.replace('#include "renderer.h"','#include "renderer.h"\n#include "legacy_draw_state.h"');s=s.replace('            renderer::Draw(di);', '''            // Capture at execution time on this worker. Borrowed constant banks
            // remain live only until Draw returns; no delayed CP/global reads.
            renderer::ShaderBinding shaders[2];
            for (uint32_t stage = 0; stage < 2; ++stage) {
                uint32_t count = 0;
                const auto* words = GetActiveShader(stage != 0, count, shaders[stage].commandHash);
                shaders[stage].words = std::span<const uint32_t>(words, count);
                shaders[stage].byteHash = count ? GetActiveShaderByteHash(stage != 0) : 0;
            }
            auto registers = renderer::DrawWords::Legacy(m_registers,
                static_cast<const uint8_t*>(g_memory.Translate(MMIO_BASE)));
            renderer::Draw(renderer::CaptureLegacyDrawState(di, registers, shaders[0], shaders[1]));''');p.write_text(s, encoding="utf-8", newline="\n")
p=root/'tools/tests/native_dlss_renderer_gpu_test.cpp';s=p.read_text(encoding="utf-8");s=s.replace("// Only the resolve's swap-red/blue MMIO bit is used by the new asset-free case.\n// Register setup is synthetic; actual resolve/copy and handoff code is unchanged.\nnamespace gpu {\nCommandProcessor g_commandProcessor;\nuint32_t CommandProcessor::ReadRegister(uint32_t) { return 0; }\n}\n", "// Resolve copy flags are explicit inputs; this fixture no longer needs a\n// synthetic command processor or MMIO register reader.\n");p.write_text(s, encoding="utf-8", newline="\n")
print('Converted',len(changes),'register accesses and shader/constant entry points')
RESULT = {
    "LostOdysseyRecomp/gpu/renderer.cpp": "4a6ee9d03fdfb67012e51401f519df1de6dea2a106fccc8b20d0f3e41d06a096",
    "LostOdysseyRecomp/gpu/renderer.h": "28e90d8a9799ecf91161a7aea0c7e05fbf19f5e404c7de0be01e6a5e4d87ae6a",
    "LostOdysseyRecomp/gpu/command_processor.cpp": "0bc4a954547cd05ee37d74f5c1737c85166753b2057d0dc58abf0b1d26bdc257",
    "tools/tests/native_dlss_renderer_gpu_test.cpp": "35abe840b0562eaecdc0b99ebd32711fe78ecee87cef625708294cd8514c98ea",
}
for name, expected in RESULT.items():
    actual = hashlib.sha256((root/name).read_bytes()).hexdigest()
    if actual != expected: raise SystemExit(f'Result differs from reviewed source: {name} {actual}')
