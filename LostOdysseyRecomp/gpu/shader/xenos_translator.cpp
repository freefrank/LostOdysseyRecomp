// Xenos microcode -> HLSL translator.
//
// The ALU/fetch instruction printing is adapted from XenosRecomp
// (hedge-dev, MIT License, Copyright (c) 2025 hedge-dev and contributors);
// the front end works on raw microcode instead of the D3D9 shader container:
// constants are plain register arrays, vertex inputs are decoded from vfetch
// instructions against ByteAddressBuffers, interpolators are fixed TEXCOORDs.
// Instruction semantics were checked against Xenia's ucode.h.

#include "xenos_translator.h"
#include "xenos_shader_code.h"

#include <fmt/format.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <map>
#include <set>

namespace xenos
{
    namespace
    {
        constexpr char kSwizzles[] = { 'x', 'y', 'z', 'w' };

        // Pixel shaders may use implicit derivatives; vertex shaders must not.
        const char* const kPixelSampleMacro = "#define XE_SAMPLE(t, s, uv) t.Sample(s, uv)\n";
        const char* const kVertexSampleMacro = "#define XE_SAMPLE(t, s, uv) t.SampleLevel(s, uv, 0.0)\n";

        const char* const kCommonHlsl = R"HLSL(
// ---- Xenos shader prelude (LostOdysseyRecomp) ----
#define FLT_MIN asfloat(0xff7fffff)
#define FLT_MAX asfloat(0x7f7fffff)

cbuffer XeConstants : register(b0, space0)
{
    float4 c[256];
};

cbuffer XeShared : register(b1, space0)
{
    uint4 xeBools[2];
    uint4 xeLoops[8];
    float4 xeNdcScale;
    float4 xeNdcOffset;
    float2 xeHalfPixelOffset;
    uint xeVtxFmt;          // PA_CL_VTE_CNTL bits 8..10: xy already /w, z already /w, w is 1/w
    uint xeFlags;           // bit0: alpha test enable
    float4 xeAlphaTest;     // x = reference, y = compare function
    float4 xeColorMax;      // per-channel range of the bound EDRAM format
    uint4 xeVfetchOffset[24]; // byte offset of each vertex fetch slot inside its buffer
    uint4 xeSamplerIndex[8];  // sampler palette index per texture fetch slot
};

uint XeVfetchOffset(uint slot)
{
    return xeVfetchOffset[slot >> 2][slot & 3];
}

SamplerState xeSamplers[64] : register(s0, space0);

SamplerState XeSampler(uint slot)
{
    return xeSamplers[xeSamplerIndex[slot >> 2][slot & 3]];
}

float4 XeConst(int index)
{
    return c[clamp(index, 0, 255)];
}

bool XeBool(uint index)
{
    return ((xeBools[(index >> 7) & 1][(index >> 5) & 3] >> (index & 31)) & 1u) != 0u;
}

uint XeLoopConst(uint id)
{
    return xeLoops[(id >> 2) & 7][id & 3];
}

// ---- vertex fetch ----
// Data is little-endian after the CPU applied the fetch constant's endian swap.
float XeNorm(uint v, uint bits, bool sgn, bool nrm)
{
    if (sgn)
    {
        int sv = int(v << (32u - bits)) >> (32u - bits);
        return nrm ? max(float(sv) / float((1u << (bits - 1u)) - 1u), -1.0) : float(sv);
    }
    return nrm ? float(v) / float((1u << bits) - 1u) : float(v);
}

float4 XeVF_8_8_8_8(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(XeNorm(v & 0xFF, 8, sgn, nrm), XeNorm((v >> 8) & 0xFF, 8, sgn, nrm),
                  XeNorm((v >> 16) & 0xFF, 8, sgn, nrm), XeNorm(v >> 24, 8, sgn, nrm));
}

float4 XeVF_2_10_10_10(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(XeNorm(v & 0x3FF, 10, sgn, nrm), XeNorm((v >> 10) & 0x3FF, 10, sgn, nrm),
                  XeNorm((v >> 20) & 0x3FF, 10, sgn, nrm), XeNorm(v >> 30, 2, sgn, nrm));
}

float4 XeVF_10_11_11(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(XeNorm(v & 0x7FF, 11, sgn, nrm), XeNorm((v >> 11) & 0x7FF, 11, sgn, nrm),
                  XeNorm(v >> 22, 10, sgn, nrm), 0.0);
}

float4 XeVF_11_11_10(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(XeNorm(v & 0x3FF, 10, sgn, nrm), XeNorm((v >> 10) & 0x7FF, 11, sgn, nrm),
                  XeNorm(v >> 21, 11, sgn, nrm), 0.0);
}

float4 XeVF_16_16(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(XeNorm(v & 0xFFFF, 16, sgn, nrm), XeNorm(v >> 16, 16, sgn, nrm), 0.0, 0.0);
}

float4 XeVF_16_16_16_16(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint2 v = b.Load2(a);
    return float4(XeNorm(v.x & 0xFFFF, 16, sgn, nrm), XeNorm(v.x >> 16, 16, sgn, nrm),
                  XeNorm(v.y & 0xFFFF, 16, sgn, nrm), XeNorm(v.y >> 16, 16, sgn, nrm));
}

float4 XeVF_16_16_FLOAT(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(f16tof32(v & 0xFFFF), f16tof32(v >> 16), 0.0, 0.0);
}

float4 XeVF_16_16_16_16_FLOAT(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint2 v = b.Load2(a);
    return float4(f16tof32(v.x & 0xFFFF), f16tof32(v.x >> 16), f16tof32(v.y & 0xFFFF), f16tof32(v.y >> 16));
}

float4 XeVF_32(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint v = b.Load(a);
    return float4(sgn ? float(int(v)) : float(v), 0.0, 0.0, 0.0);
}

float4 XeVF_32_32(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint2 v = b.Load2(a);
    return float4(sgn ? float2(int2(v)) : float2(v), 0.0, 0.0);
}

float4 XeVF_32_32_32_32(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    uint4 v = b.Load4(a);
    return sgn ? float4(int4(v)) : float4(v);
}

float4 XeVF_32_FLOAT(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    return float4(asfloat(b.Load(a)), 0.0, 0.0, 0.0);
}

float4 XeVF_32_32_FLOAT(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    return float4(asfloat(b.Load2(a)), 0.0, 0.0);
}

float4 XeVF_32_32_32_FLOAT(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    return float4(asfloat(b.Load3(a)), 0.0);
}

float4 XeVF_32_32_32_32_FLOAT(ByteAddressBuffer b, uint a, bool sgn, bool nrm)
{
    return asfloat(b.Load4(a));
}

// ---- texture fetch ----
float4 XeTex2D(Texture2D<float4> t, SamplerState s, float2 uv, float2 offset)
{
    uint2 dims;
    t.GetDimensions(dims.x, dims.y);
    return XE_SAMPLE(t, s, uv + offset / float2(dims));
}

float4 XeTex3D(Texture3D<float4> t, SamplerState s, float3 uvw)
{
    return XE_SAMPLE(t, s, uvw);
}

struct CubeMapData
{
    float3 cubeMapDirections[2];
    uint cubeMapIndex;
};

float4 XeTexCube(TextureCube<float4> t, SamplerState s, float3 coord, inout CubeMapData cubeMapData)
{
    return XE_SAMPLE(t, s, cubeMapData.cubeMapDirections[uint(coord.z) & 1]);
}

float2 XeWeights2D(Texture2D<float4> t, float2 uv, float2 offset)
{
    uint2 dims;
    t.GetDimensions(dims.x, dims.y);
    return select(isnan(uv), 0.0, frac(uv * float2(dims) + offset - 0.5));
}

float4 cube(float4 value, inout CubeMapData cubeMapData)
{
    uint index = cubeMapData.cubeMapIndex;
    cubeMapData.cubeMapDirections[index & 1] = value.xyz;
    ++cubeMapData.cubeMapIndex;
    return float4(0.0, 0.0, 0.0, index);
}

float4 dst(float4 src0, float4 src1)
{
    return float4(1.0, src0.y * src1.y, src0.z, src1.w);
}

float4 max4(float4 src0)
{
    return max(max(src0.x, src0.y), max(src0.z, src0.w));
}
// ---- end prelude ----
)HLSL";

        struct Translator
        {
            const uint32_t* code = nullptr;
            uint32_t dwordCount = 0;
            bool isPixelShader = false;
            TranslatedShader result;

            std::string out;
            uint32_t indentation = 1;

            // Pass-1 results.
            uint32_t maxTemp = 0;
            std::set<uint32_t> vfetchSlots;
            std::map<uint32_t, uint32_t> texSlots;   // slot -> TextureDimension
            std::map<uint32_t, uint32_t> ifEndLabels;
            bool simpleControlFlow = true;
            uint32_t exportedInterpolators = 0;
            bool exportsPosition = false;
            uint32_t cfByteSize = 0;

            template<typename... Args>
            void print(fmt::format_string<Args...> f, Args&&... args) { out += fmt::format(f, std::forward<Args>(args)...); }
            template<typename... Args>
            void println(fmt::format_string<Args...> f, Args&&... args) { out += fmt::format(f, std::forward<Args>(args)...); out += '\n'; }
            void indent() { for (uint32_t i = 0; i < indentation; i++) out += '\t'; }
            void note(const std::string& s) { result.errors += s; result.errors += '\n'; }

            // --- microcode access -----------------------------------------
            struct CfPair
            {
                ControlFlowInstruction cf[2];
            };

            CfPair ReadCfPair(uint32_t dwordIndex) const
            {
                uint32_t w0 = dwordIndex + 2 < dwordCount ? code[dwordIndex] : 0;
                uint32_t w1 = dwordIndex + 2 < dwordCount ? code[dwordIndex + 1] : 0;
                uint32_t w2 = dwordIndex + 2 < dwordCount ? code[dwordIndex + 2] : 0;
                uint32_t words[4] = { w0, w1 & 0xFFFF, (w1 >> 16) | (w2 << 16), w2 >> 16 };
                CfPair pair;
                memcpy(&pair, words, sizeof(words));
                return pair;
            }

            void ReadInstruction(uint32_t address, uint32_t w[3]) const
            {
                uint32_t idx = address * 3;
                for (int i = 0; i < 3; i++)
                    w[i] = (idx + i) < dwordCount ? code[idx + i] : 0;
            }

            static bool CfHasExecBody(ControlFlowOpcode op)
            {
                switch (op)
                {
                case ControlFlowOpcode::Exec: case ControlFlowOpcode::ExecEnd:
                case ControlFlowOpcode::CondExec: case ControlFlowOpcode::CondExecEnd:
                case ControlFlowOpcode::CondExecPred: case ControlFlowOpcode::CondExecPredEnd:
                case ControlFlowOpcode::CondExecPredClean: case ControlFlowOpcode::CondExecPredCleanEnd:
                    return true;
                default:
                    return false;
                }
            }

            static uint32_t CfExecAddress(const ControlFlowInstruction& cf)
            {
                switch (cf.opcode)
                {
                case ControlFlowOpcode::Exec: case ControlFlowOpcode::ExecEnd:
                    return cf.exec.address;
                case ControlFlowOpcode::CondExecPred: case ControlFlowOpcode::CondExecPredEnd:
                    return cf.condExecPred.address;
                default:
                    return cf.condExec.address;
                }
            }

            static uint32_t CfExecCount(const ControlFlowInstruction& cf)
            {
                switch (cf.opcode)
                {
                case ControlFlowOpcode::Exec: case ControlFlowOpcode::ExecEnd:
                    return cf.exec.count;
                case ControlFlowOpcode::CondExecPred: case ControlFlowOpcode::CondExecPredEnd:
                    return cf.condExecPred.count;
                default:
                    return cf.condExec.count;
                }
            }

            static uint32_t CfExecSequence(const ControlFlowInstruction& cf)
            {
                switch (cf.opcode)
                {
                case ControlFlowOpcode::Exec: case ControlFlowOpcode::ExecEnd:
                    return cf.exec.sequence;
                case ControlFlowOpcode::CondExecPred: case ControlFlowOpcode::CondExecPredEnd:
                    return cf.condExecPred.sequence;
                default:
                    return cf.condExec.sequence;
                }
            }

            // --- pass 1 -----------------------------------------------------
            void Scan()
            {
                // The control-flow section ends where the first executed
                // instruction begins.
                uint32_t instrSize = dwordCount * 4;
                for (uint32_t byteOffset = 0; byteOffset < instrSize; byteOffset += 12)
                {
                    CfPair pair = ReadCfPair(byteOffset / 4);
                    for (auto& cf : pair.cf)
                    {
                        if (CfHasExecBody(cf.opcode))
                        {
                            uint32_t address = CfExecAddress(cf);
                            if (address != 0)
                                instrSize = std::min<uint32_t>(instrSize, address * 12);
                        }
                        else if (cf.opcode == ControlFlowOpcode::CondJmp)
                        {
                            if (cf.condJmp.isUnconditional || cf.condJmp.direction)
                                simpleControlFlow = false;
                            else
                                ++ifEndLabels[cf.condJmp.address];
                        }
                        else if (cf.opcode == ControlFlowOpcode::CondCall || cf.opcode == ControlFlowOpcode::Return)
                        {
                            note("unsupported control flow: call/return");
                        }
                    }
                }
                cfByteSize = instrSize;

                for (uint32_t byteOffset = 0; byteOffset < cfByteSize; byteOffset += 12)
                {
                    CfPair pair = ReadCfPair(byteOffset / 4);
                    for (auto& cf : pair.cf)
                    {
                        if (!CfHasExecBody(cf.opcode))
                            continue;
                        uint32_t address = CfExecAddress(cf);
                        uint32_t count = CfExecCount(cf);
                        uint32_t sequence = CfExecSequence(cf);
                        for (uint32_t i = 0; i < count; i++, sequence >>= 2)
                        {
                            uint32_t w[3];
                            ReadInstruction(address + i, w);
                            if (sequence & 1)
                            {
                                FetchInstruction fetch;
                                memcpy(&fetch, w, 8);
                                if (fetch.opcode == FetchOpcode::VertexFetch)
                                {
                                    VertexFetchInstruction vf;
                                    memcpy(&vf, w, 12);
                                    vfetchSlots.insert(vf.constIndex * 3 + vf.constIndexSelect);
                                    maxTemp = std::max<uint32_t>(maxTemp, std::max<uint32_t>(vf.dstRegister, vf.srcRegister));
                                }
                                else
                                {
                                    TextureFetchInstruction tf;
                                    memcpy(&tf, w, 12);
                                    if (tf.opcode == FetchOpcode::TextureFetch || tf.opcode == FetchOpcode::GetTextureWeights)
                                        texSlots[tf.constIndex] = uint32_t(tf.dimension);
                                    maxTemp = std::max<uint32_t>(maxTemp, std::max<uint32_t>(tf.dstRegister, tf.srcRegister));
                                }
                            }
                            else
                            {
                                AluInstruction alu;
                                memcpy(&alu, w, 12);
                                if (alu.src1Select) maxTemp = std::max<uint32_t>(maxTemp, alu.src1Register & 0x3F);
                                if (alu.src2Select) maxTemp = std::max<uint32_t>(maxTemp, alu.src2Register & 0x3F);
                                if (alu.src3Select) maxTemp = std::max<uint32_t>(maxTemp, alu.src3Register & 0x3F);
                                if (alu.exportData)
                                {
                                    if (isPixelShader)
                                    {
                                        if (alu.vectorDest < 4) result.colorTargetsWritten |= 1u << alu.vectorDest;
                                        if (alu.vectorDest == uint32_t(ExportRegister::PSDepth)) result.writesDepth = true;
                                    }
                                    else
                                    {
                                        if (alu.vectorDest < 16) exportedInterpolators |= 1u << alu.vectorDest;
                                        if (alu.vectorDest == uint32_t(ExportRegister::VSPosition)) exportsPosition = true;
                                        if (alu.vectorDest == uint32_t(ExportRegister::VSPointSizeEdgeFlagKillVertex)) result.usesPointSize = true;
                                    }
                                }
                                else
                                {
                                    if (alu.vectorWriteMask) maxTemp = std::max<uint32_t>(maxTemp, alu.vectorDest);
                                    if (alu.scalarWriteMask) maxTemp = std::max<uint32_t>(maxTemp, alu.scalarDest);
                                }
                            }
                        }
                    }
                }
                if (isPixelShader)
                    maxTemp = std::max<uint32_t>(maxTemp, 15); // interpolators land in r0..r15
                maxTemp = std::min<uint32_t>(maxTemp, 63);

                for (uint32_t slot : vfetchSlots)
                    result.vertexFetchSlotMask[slot >> 6] |= 1ull << (slot & 63);
                for (auto& [slot, dim] : texSlots)
                {
                    result.textureSlotMask |= 1u << slot;
                    result.textureDimension[slot] = uint8_t(dim);
                }
            }

            // --- helpers shared by fetch/alu printing --------------------
            void printDstSwizzle(uint32_t dstSwizzle, bool operand)
            {
                for (uint32_t i = 0; i < 4; i++)
                {
                    uint32_t s = (dstSwizzle >> (i * 3)) & 7;
                    if (s == uint32_t(FetchDestinationSwizzle::Keep))
                        continue;
                    if (operand)
                        out += kSwizzles[s < 4 ? s : 0];
                    else
                        out += kSwizzles[i];
                }
            }

            void printDstSwizzle01(uint32_t dstRegister, uint32_t dstSwizzle)
            {
                for (uint32_t i = 0; i < 4; i++)
                {
                    uint32_t s = (dstSwizzle >> (i * 3)) & 7;
                    if (s == uint32_t(FetchDestinationSwizzle::Zero))
                    {
                        indent();
                        println("r{}.{} = 0.0;", dstRegister, kSwizzles[i]);
                    }
                    else if (s == uint32_t(FetchDestinationSwizzle::One))
                    {
                        indent();
                        println("r{}.{} = 1.0;", dstRegister, kSwizzles[i]);
                    }
                }
            }

            static const char* VertexFormatFunction(uint32_t format)
            {
                switch (VertexFormat(format))
                {
                case VertexFormat::k_8_8_8_8: return "XeVF_8_8_8_8";
                case VertexFormat::k_2_10_10_10: return "XeVF_2_10_10_10";
                case VertexFormat::k_10_11_11: return "XeVF_10_11_11";
                case VertexFormat::k_11_11_10: return "XeVF_11_11_10";
                case VertexFormat::k_16_16: return "XeVF_16_16";
                case VertexFormat::k_16_16_16_16: return "XeVF_16_16_16_16";
                case VertexFormat::k_16_16_FLOAT: return "XeVF_16_16_FLOAT";
                case VertexFormat::k_16_16_16_16_FLOAT: return "XeVF_16_16_16_16_FLOAT";
                case VertexFormat::k_32: return "XeVF_32";
                case VertexFormat::k_32_32: return "XeVF_32_32";
                case VertexFormat::k_32_32_32_32: return "XeVF_32_32_32_32";
                case VertexFormat::k_32_FLOAT: return "XeVF_32_FLOAT";
                case VertexFormat::k_32_32_FLOAT: return "XeVF_32_32_FLOAT";
                case VertexFormat::k_32_32_32_FLOAT: return "XeVF_32_32_32_FLOAT";
                case VertexFormat::k_32_32_32_32_FLOAT: return "XeVF_32_32_32_32_FLOAT";
                default: return nullptr;
                }
            }

            // --- vertex fetch -----------------------------------------------
            void recompile(const VertexFetchInstruction& instr)
            {
                if (instr.isPredicated)
                {
                    indent();
                    println("if ({}p0)", instr.predicateCondition ? "" : "!");
                    indent();
                    out += "{\n";
                    ++indentation;
                }

                uint32_t slot = instr.constIndex * 3 + instr.constIndexSelect;
                if (!instr.isMiniFetch)
                {
                    // Full fetch: recompute the vertex base address from the
                    // (floating-point) index register; the stride is in dwords.
                    indent();
                    if (instr.isIndexRounded)
                        println("xeVfetchBase = uint(int(floor(r{}.{} + 0.5)) * {}) * 4u;",
                            instr.srcRegister, kSwizzles[instr.srcSwizzle & 3], uint32_t(instr.stride));
                    else
                        println("xeVfetchBase = uint(int(floor(r{}.{})) * {}) * 4u;",
                            instr.srcRegister, kSwizzles[instr.srcSwizzle & 3], uint32_t(instr.stride));
                }

                const char* fn = VertexFormatFunction(instr.format);
                indent();
                print("r{}.", instr.dstRegister);
                printDstSwizzle(instr.dstSwizzle, false);
                out += " = ";
                if (!fn)
                {
                    note(fmt::format("unsupported vertex format {}", uint32_t(instr.format)));
                    out += "float4(0.0, 0.0, 0.0, 1.0).";
                }
                else
                {
                    std::string expr = fmt::format("{0}(vfetch{1}, XeVfetchOffset({1}u) + xeVfetchBase + {2}u, {3}, {4})", fn, slot,
                        uint32_t(int32_t(instr.offset) * 4), instr.formatCompAll ? "true" : "false", instr.numFormatAll ? "false" : "true");
                    if (instr.expAdjust != 0)
                        expr = fmt::format("({} * {})", expr, std::ldexp(1.0f, instr.expAdjust));
                    out += expr;
                    out += '.';
                }
                printDstSwizzle(instr.dstSwizzle, true);
                out += ";\n";
                printDstSwizzle01(instr.dstRegister, instr.dstSwizzle);

                if (instr.isPredicated)
                {
                    --indentation;
                    indent();
                    out += "}\n";
                }
            }

            // --- texture fetch ----------------------------------------------
            void recompile(const TextureFetchInstruction& instr)
            {
                if (instr.opcode != FetchOpcode::TextureFetch && instr.opcode != FetchOpcode::GetTextureWeights)
                {
                    indent();
                    println("// tfetch opcode {} ignored", uint32_t(instr.opcode));
                    return;
                }

                if (instr.isPredicated)
                {
                    indent();
                    println("if ({}p0)", instr.predCondition ? "" : "!");
                    indent();
                    out += "{\n";
                    ++indentation;
                }

                auto printSrcRegister = [&](uint32_t componentCount)
                {
                    print("r{}.", instr.srcRegister);
                    for (uint32_t i = 0; i < componentCount; i++)
                        out += kSwizzles[(instr.srcSwizzle >> (i * 2)) & 3];
                };

                indent();
                print("r{}.", instr.dstRegister);
                printDstSwizzle(instr.dstSwizzle, false);
                out += " = ";

                uint32_t slot = instr.constIndex;
                if (instr.opcode == FetchOpcode::GetTextureWeights)
                {
                    print("float4(XeWeights2D(tex2D_{}, ", slot);
                    printSrcRegister(2);
                    println(", float2({}, {})), 0.0, 0.0).", instr.offsetX * 0.5f, instr.offsetY * 0.5f);
                    out.pop_back(); // keep the trailing '.' for the swizzle below
                }
                else
                {
                    switch (instr.dimension)
                    {
                    case TextureDimension::Texture1D:
                    case TextureDimension::Texture2D:
                        print("XeTex2D(tex2D_{0}, XeSampler({0}u), ", slot);
                        if (instr.dimension == TextureDimension::Texture1D)
                        {
                            out += "float2(";
                            printSrcRegister(1);
                            out += ", 0.5)";
                        }
                        else
                            printSrcRegister(2);
                        print(", float2({}, {}))", instr.offsetX * 0.5f, instr.offsetY * 0.5f);
                        break;
                    case TextureDimension::Texture3D:
                        print("XeTex3D(tex3D_{0}, XeSampler({0}u), ", slot);
                        printSrcRegister(3);
                        out += ")";
                        break;
                    case TextureDimension::TextureCube:
                        print("XeTexCube(texCube_{0}, XeSampler({0}u), ", slot);
                        printSrcRegister(3);
                        out += ", cubeMapData)";
                        break;
                    }
                    out += '.';
                }
                printDstSwizzle(instr.dstSwizzle, true);
                out += ";\n";
                printDstSwizzle01(instr.dstRegister, instr.dstSwizzle);
                // LO_PS_TEXDEBUG: remember the last sampled value.
                if (isPixelShader && instr.opcode != FetchOpcode::GetTextureWeights)
                {
                    indent();
                    println("xeDbgTex = r{};", instr.dstRegister);
                }

                if (instr.isPredicated)
                {
                    --indentation;
                    indent();
                    out += "}\n";
                }
            }

            // --- ALU ----------------------------------------------------------
            void recompile(const AluInstruction& instr)
            {
                if (instr.isPredicated)
                {
                    indent();
                    println("if ({}p0)", instr.predicateCondition ? "" : "!");
                    indent();
                    out += "{\n";
                    ++indentation;
                }

                enum { VECTOR_0, VECTOR_1, VECTOR_2, SCALAR_0, SCALAR_1, SCALAR_CONSTANT_0, SCALAR_CONSTANT_1 };

                auto op = [&](size_t operand)
                {
                    uint32_t reg = 0, swizzle = 0;
                    bool select = true, negate = false, abs = false;
                    switch (operand)
                    {
                    case SCALAR_CONSTANT_0:
                        reg = instr.src3Register; swizzle = instr.src3Swizzle; select = false;
                        negate = instr.src3Negate; abs = instr.absConstants;
                        break;
                    case SCALAR_CONSTANT_1:
                        reg = (uint32_t(instr.scalarOpcode) & 1) | (instr.src3Select << 1) | (instr.src3Swizzle & 0x3C);
                        swizzle = instr.src3Swizzle; select = true; negate = instr.src3Negate; abs = instr.absConstants;
                        break;
                    default:
                        switch (operand)
                        {
                        case VECTOR_0: reg = instr.src1Register; swizzle = instr.src1Swizzle; select = instr.src1Select; negate = instr.src1Negate; break;
                        case VECTOR_1: reg = instr.src2Register; swizzle = instr.src2Swizzle; select = instr.src2Select; negate = instr.src2Negate; break;
                        default:       reg = instr.src3Register; swizzle = instr.src3Swizzle; select = instr.src3Select; negate = instr.src3Negate; break;
                        }
                        if (select)
                        {
                            abs = (reg & 0x80) != 0;
                            reg &= 0x3F;
                        }
                        else
                            abs = instr.absConstants;
                        break;
                    }

                    std::string regFormatted;
                    if (select)
                        regFormatted = fmt::format("r{}", reg);
                    else
                    {
                        // The two relative-addressing flags belong to the first and second
                        // *constant* operands in src1..src3 order, not to operand positions
                        // (Xenia ParseAluInstructionOperand): src2 is slot 1 only when src1
                        // is a constant, src3 is slot 1 when src1 or src2 is one.
                        const uint32_t srcIndex = (operand == VECTOR_0) ? 1 : (operand == VECTOR_1) ? 2 : 3;
                        uint32_t constSlot = 0;
                        if (srcIndex >= 2 && !instr.src1Select) constSlot = 1;
                        if (srcIndex >= 3 && !instr.src2Select) constSlot = 1;
                        bool relative = constSlot == 0 ? instr.const0Relative : instr.const1Relative;
                        if (relative)
                            regFormatted = fmt::format("XeConst({} + {})", reg, instr.constAddressRegisterRelative ? "a0" : "aL");
                        else
                            regFormatted = fmt::format("c[{}]", reg);
                    }

                    std::string result;
                    if (negate) result += '-';
                    if (abs) result += "abs(";
                    result += regFormatted;
                    result += '.';
                    switch (operand)
                    {
                    case VECTOR_0: case VECTOR_1: case VECTOR_2:
                    {
                        uint32_t mask;
                        switch (instr.vectorOpcode)
                        {
                        case AluVectorOpcode::Dp2Add: mask = (operand == VECTOR_2) ? 0b1 : 0b11; break;
                        case AluVectorOpcode::Dp3: mask = 0b111; break;
                        case AluVectorOpcode::Dp4: case AluVectorOpcode::Max4: mask = 0b1111; break;
                        default: mask = instr.vectorWriteMask != 0 ? instr.vectorWriteMask : 0b1; break;
                        }
                        for (uint32_t i = 0; i < 4; i++)
                            if ((mask >> i) & 1)
                                result += kSwizzles[((swizzle >> (i * 2)) + i) & 3];
                        break;
                    }
                    case SCALAR_0: case SCALAR_CONSTANT_0:
                        result += kSwizzles[((swizzle >> 6) + 3) & 3];
                        break;
                    case SCALAR_1: case SCALAR_CONSTANT_1:
                        result += kSwizzles[swizzle & 3];
                        break;
                    }
                    if (abs) result += ')';
                    return result;
                };

                const char* kill = isPixelShader ? "clip" : "XeNoKill";
                switch (instr.vectorOpcode)
                {
                case AluVectorOpcode::KillEq: indent(); println("{}(any({} == {}) ? -1 : 1);", kill, op(VECTOR_0), op(VECTOR_1)); break;
                case AluVectorOpcode::KillGt: indent(); println("{}(any({} > {}) ? -1 : 1);", kill, op(VECTOR_0), op(VECTOR_1)); break;
                case AluVectorOpcode::KillGe: indent(); println("{}(any({} >= {}) ? -1 : 1);", kill, op(VECTOR_0), op(VECTOR_1)); break;
                case AluVectorOpcode::KillNe: indent(); println("{}(any({} != {}) ? -1 : 1);", kill, op(VECTOR_0), op(VECTOR_1)); break;
                default: break;
                }

                std::string exportRegister;
                if (instr.exportData)
                {
                    if (isPixelShader)
                    {
                        switch (ExportRegister(instr.vectorDest))
                        {
                        case ExportRegister::PSColor0: exportRegister = "oC0"; break;
                        case ExportRegister::PSColor1: exportRegister = "oC1"; break;
                        case ExportRegister::PSColor2: exportRegister = "oC2"; break;
                        case ExportRegister::PSColor3: exportRegister = "oC3"; break;
                        case ExportRegister::PSDepth: exportRegister = "oDepthVec"; break;
                        default: exportRegister = "xeDiscard"; note(fmt::format("unknown ps export {}", instr.vectorDest)); break;
                        }
                    }
                    else
                    {
                        if (instr.vectorDest < 16)
                            exportRegister = fmt::format("o{}", instr.vectorDest);
                        else if (instr.vectorDest == uint32_t(ExportRegister::VSPosition))
                            exportRegister = "oPos";
                        else if (instr.vectorDest == uint32_t(ExportRegister::VSPointSizeEdgeFlagKillVertex))
                            exportRegister = "oPointSize";
                        else
                        {
                            exportRegister = "xeDiscard";
                            note(fmt::format("unknown vs export {}", instr.vectorDest));
                        }
                    }
                }

                if (instr.vectorOpcode >= AluVectorOpcode::SetpEqPush && instr.vectorOpcode <= AluVectorOpcode::SetpGePush)
                {
                    indent();
                    print("p0 = {} == 0.0 && {} ", op(VECTOR_0), op(VECTOR_1));
                    switch (instr.vectorOpcode)
                    {
                    case AluVectorOpcode::SetpEqPush: out += "=="; break;
                    case AluVectorOpcode::SetpNePush: out += "!="; break;
                    case AluVectorOpcode::SetpGtPush: out += ">"; break;
                    default: out += ">="; break;
                    }
                    out += " 0.0;\n";
                }
                else if (instr.vectorOpcode == AluVectorOpcode::MaxA)
                {
                    indent();
                    println("a0 = (int)clamp(floor(({}).w + 0.5), -256.0, 255.0);", op(VECTOR_0));
                }

                uint32_t vectorWriteMask = instr.vectorWriteMask;
                if (instr.exportData)
                    vectorWriteMask &= ~instr.scalarWriteMask;
                if (vectorWriteMask != 0)
                {
                    indent();
                    if (!exportRegister.empty())
                        print("{}.", exportRegister);
                    else
                        print("r{}.", instr.vectorDest);
                    for (uint32_t i = 0; i < 4; i++)
                        if ((vectorWriteMask >> i) & 1)
                            out += kSwizzles[i];
                    out += " = ";
                    if (instr.vectorSaturate)
                        out += "saturate(";
                    switch (instr.vectorOpcode)
                    {
                    case AluVectorOpcode::Add: print("{} + {}", op(VECTOR_0), op(VECTOR_1)); break;
                    case AluVectorOpcode::Mul: print("{} * {}", op(VECTOR_0), op(VECTOR_1)); break;
                    case AluVectorOpcode::Max: case AluVectorOpcode::MaxA: print("max({}, {})", op(VECTOR_0), op(VECTOR_1)); break;
                    case AluVectorOpcode::Min: print("min({}, {})", op(VECTOR_0), op(VECTOR_1)); break;
                    case AluVectorOpcode::Seq: print("select({} == {}, 1.0, 0.0)", op(VECTOR_0), op(VECTOR_1)); break;
                    case AluVectorOpcode::Sgt: print("select({} > {}, 1.0, 0.0)", op(VECTOR_0), op(VECTOR_1)); break;
                    case AluVectorOpcode::Sge: print("select({} >= {}, 1.0, 0.0)", op(VECTOR_0), op(VECTOR_1)); break;
                    case AluVectorOpcode::Sne: print("select({} != {}, 1.0, 0.0)", op(VECTOR_0), op(VECTOR_1)); break;
                    case AluVectorOpcode::Frc: print("frac({})", op(VECTOR_0)); break;
                    case AluVectorOpcode::Trunc: print("trunc({})", op(VECTOR_0)); break;
                    case AluVectorOpcode::Floor: print("floor({})", op(VECTOR_0)); break;
                    case AluVectorOpcode::Mad: print("{} * {} + {}", op(VECTOR_0), op(VECTOR_1), op(VECTOR_2)); break;
                    case AluVectorOpcode::CndEq: print("select({} == 0.0, {}, {})", op(VECTOR_0), op(VECTOR_1), op(VECTOR_2)); break;
                    case AluVectorOpcode::CndGe: print("select({} >= 0.0, {}, {})", op(VECTOR_0), op(VECTOR_1), op(VECTOR_2)); break;
                    case AluVectorOpcode::CndGt: print("select({} > 0.0, {}, {})", op(VECTOR_0), op(VECTOR_1), op(VECTOR_2)); break;
                    case AluVectorOpcode::Dp4: case AluVectorOpcode::Dp3: print("dot({}, {})", op(VECTOR_0), op(VECTOR_1)); break;
                    case AluVectorOpcode::Dp2Add: print("dot({}, {}) + {}", op(VECTOR_0), op(VECTOR_1), op(VECTOR_2)); break;
                    case AluVectorOpcode::Cube: print("cube(r{}, cubeMapData)", instr.src1Register & 0x3F); break;
                    case AluVectorOpcode::Max4: print("max4({})", op(VECTOR_0)); break;
                    case AluVectorOpcode::SetpEqPush: case AluVectorOpcode::SetpNePush:
                    case AluVectorOpcode::SetpGtPush: case AluVectorOpcode::SetpGePush:
                        print("p0 ? 0.0 : {} + 1.0", op(VECTOR_0)); break;
                    case AluVectorOpcode::KillEq: print("select(any({} == {}), 1.0, 0.0)", op(VECTOR_0), op(VECTOR_1)); break;
                    case AluVectorOpcode::KillGt: print("select(any({} > {}), 1.0, 0.0)", op(VECTOR_0), op(VECTOR_1)); break;
                    case AluVectorOpcode::KillGe: print("select(any({} >= {}), 1.0, 0.0)", op(VECTOR_0), op(VECTOR_1)); break;
                    case AluVectorOpcode::KillNe: print("select(any({} != {}), 1.0, 0.0)", op(VECTOR_0), op(VECTOR_1)); break;
                    case AluVectorOpcode::Dst: print("dst({}, {})", op(VECTOR_0), op(VECTOR_1)); break;
                    }
                    if (instr.vectorSaturate)
                        out += ')';
                    out += ";\n";
                }

                if (instr.scalarOpcode != AluScalarOpcode::RetainPrev)
                {
                    if (instr.scalarOpcode >= AluScalarOpcode::SetpEq && instr.scalarOpcode <= AluScalarOpcode::SetpRstr)
                    {
                        indent();
                        out += "p0 = ";
                        switch (instr.scalarOpcode)
                        {
                        case AluScalarOpcode::SetpEq: print("{} == 0.0", op(SCALAR_0)); break;
                        case AluScalarOpcode::SetpNe: print("{} != 0.0", op(SCALAR_0)); break;
                        case AluScalarOpcode::SetpGt: print("{} > 0.0", op(SCALAR_0)); break;
                        case AluScalarOpcode::SetpGe: print("{} >= 0.0", op(SCALAR_0)); break;
                        case AluScalarOpcode::SetpInv: print("{} == 1.0", op(SCALAR_0)); break;
                        case AluScalarOpcode::SetpPop: print("{} - 1.0 <= 0.0", op(SCALAR_0)); break;
                        case AluScalarOpcode::SetpClr: out += "false"; break;
                        default: print("{} == 0.0", op(SCALAR_0)); break;
                        }
                        out += ";\n";
                    }

                    indent();
                    out += "ps = ";
                    if (instr.scalarSaturate)
                        out += "saturate(";
                    switch (instr.scalarOpcode)
                    {
                    case AluScalarOpcode::Adds: print("{} + {}", op(SCALAR_0), op(SCALAR_1)); break;
                    case AluScalarOpcode::AddsPrev: print("{} + ps", op(SCALAR_0)); break;
                    case AluScalarOpcode::Muls: print("{} * {}", op(SCALAR_0), op(SCALAR_1)); break;
                    case AluScalarOpcode::MulsPrev: case AluScalarOpcode::MulsPrev2: print("{} * ps", op(SCALAR_0)); break;
                    case AluScalarOpcode::Maxs: case AluScalarOpcode::MaxAs: case AluScalarOpcode::MaxAsf: print("max({}, {})", op(SCALAR_0), op(SCALAR_1)); break;
                    case AluScalarOpcode::Mins: print("min({}, {})", op(SCALAR_0), op(SCALAR_1)); break;
                    case AluScalarOpcode::Seqs: print("select({} == 0.0, 1.0, 0.0)", op(SCALAR_0)); break;
                    case AluScalarOpcode::Sgts: print("select({} > 0.0, 1.0, 0.0)", op(SCALAR_0)); break;
                    case AluScalarOpcode::Sges: print("select({} >= 0.0, 1.0, 0.0)", op(SCALAR_0)); break;
                    case AluScalarOpcode::Snes: print("select({} != 0.0, 1.0, 0.0)", op(SCALAR_0)); break;
                    case AluScalarOpcode::Frcs: print("frac({})", op(SCALAR_0)); break;
                    case AluScalarOpcode::Truncs: print("trunc({})", op(SCALAR_0)); break;
                    case AluScalarOpcode::Floors: print("floor({})", op(SCALAR_0)); break;
                    case AluScalarOpcode::Exp: print("exp2({})", op(SCALAR_0)); break;
                    case AluScalarOpcode::Logc: case AluScalarOpcode::Log: print("clamp(log2({}), FLT_MIN, FLT_MAX)", op(SCALAR_0)); break;
                    case AluScalarOpcode::Rcpc: case AluScalarOpcode::Rcpf: case AluScalarOpcode::Rcp: print("clamp(rcp({}), FLT_MIN, FLT_MAX)", op(SCALAR_0)); break;
                    case AluScalarOpcode::Rsqc: case AluScalarOpcode::Rsqf: case AluScalarOpcode::Rsq: print("clamp(rsqrt({}), FLT_MIN, FLT_MAX)", op(SCALAR_0)); break;
                    case AluScalarOpcode::Subs: print("{} - {}", op(SCALAR_0), op(SCALAR_1)); break;
                    case AluScalarOpcode::SubsPrev: print("{} - ps", op(SCALAR_0)); break;
                    case AluScalarOpcode::SetpEq: case AluScalarOpcode::SetpNe: case AluScalarOpcode::SetpGt: case AluScalarOpcode::SetpGe:
                        out += "p0 ? 0.0 : 1.0"; break;
                    case AluScalarOpcode::SetpInv: print("{0} == 0.0 ? 1.0 : {0}", op(SCALAR_0)); break;
                    case AluScalarOpcode::SetpPop: print("p0 ? 0.0 : ({} - 1.0)", op(SCALAR_0)); break;
                    case AluScalarOpcode::SetpClr: out += "FLT_MAX"; break;
                    case AluScalarOpcode::SetpRstr: print("p0 ? 0.0 : {}", op(SCALAR_0)); break;
                    case AluScalarOpcode::KillsEq: print("select({} == 0.0, 1.0, 0.0)", op(SCALAR_0)); break;
                    case AluScalarOpcode::KillsGt: print("select({} > 0.0, 1.0, 0.0)", op(SCALAR_0)); break;
                    case AluScalarOpcode::KillsGe: print("select({} >= 0.0, 1.0, 0.0)", op(SCALAR_0)); break;
                    case AluScalarOpcode::KillsNe: print("select({} != 0.0, 1.0, 0.0)", op(SCALAR_0)); break;
                    case AluScalarOpcode::KillsOne: print("select({} == 1.0, 1.0, 0.0)", op(SCALAR_0)); break;
                    case AluScalarOpcode::Sqrt: print("sqrt({})", op(SCALAR_0)); break;
                    case AluScalarOpcode::Mulsc0: case AluScalarOpcode::Mulsc1: print("{} * {}", op(SCALAR_CONSTANT_0), op(SCALAR_CONSTANT_1)); break;
                    case AluScalarOpcode::Addsc0: case AluScalarOpcode::Addsc1: print("{} + {}", op(SCALAR_CONSTANT_0), op(SCALAR_CONSTANT_1)); break;
                    case AluScalarOpcode::Subsc0: case AluScalarOpcode::Subsc1: print("{} - {}", op(SCALAR_CONSTANT_0), op(SCALAR_CONSTANT_1)); break;
                    case AluScalarOpcode::Sin: print("sin({})", op(SCALAR_0)); break;
                    case AluScalarOpcode::Cos: print("cos({})", op(SCALAR_0)); break;
                    default: out += "ps"; note(fmt::format("unknown scalar opcode {}", uint32_t(instr.scalarOpcode))); break;
                    }
                    if (instr.scalarSaturate)
                        out += ')';
                    out += ";\n";

                    switch (instr.scalarOpcode)
                    {
                    case AluScalarOpcode::MaxAs:
                        indent(); println("a0 = (int)clamp(floor({} + 0.5), -256.0, 255.0);", op(SCALAR_0)); break;
                    case AluScalarOpcode::MaxAsf:
                        indent(); println("a0 = (int)clamp(floor({}), -256.0, 255.0);", op(SCALAR_0)); break;
                    default: break;
                    }
                }

                uint32_t scalarWriteMask = instr.scalarWriteMask;
                if (instr.exportData)
                    scalarWriteMask &= ~instr.vectorWriteMask;
                if (scalarWriteMask != 0)
                {
                    indent();
                    if (!exportRegister.empty())
                        print("{}.", exportRegister);
                    else
                        print("r{}.", instr.scalarDest);
                    for (uint32_t i = 0; i < 4; i++)
                        if ((scalarWriteMask >> i) & 1)
                            out += kSwizzles[i];
                    out += " = ps;\n";
                }

                if (instr.exportData && !exportRegister.empty())
                {
                    uint32_t zeroMask = instr.scalarDestRelative ? (0b1111 & ~(instr.vectorWriteMask | instr.scalarWriteMask)) : 0;
                    uint32_t oneMask = instr.vectorWriteMask & instr.scalarWriteMask;
                    for (uint32_t i = 0; i < 4; i++)
                    {
                        uint32_t mask = 1u << i;
                        if (zeroMask & mask) { indent(); println("{}.{} = 0.0;", exportRegister, kSwizzles[i]); }
                        else if (oneMask & mask) { indent(); println("{}.{} = 1.0;", exportRegister, kSwizzles[i]); }
                    }
                }

                if (isPixelShader && instr.scalarOpcode >= AluScalarOpcode::KillsEq && instr.scalarOpcode <= AluScalarOpcode::KillsOne)
                {
                    indent();
                    out += "clip(ps != 0.0 ? -1 : 1);\n";
                }

                if (instr.isPredicated)
                {
                    --indentation;
                    indent();
                    out += "}\n";
                }
            }

            // --- pass 2 -----------------------------------------------------
            void EmitDeclarations()
            {
                out += isPixelShader ? kPixelSampleMacro : kVertexSampleMacro;
                out += kCommonHlsl;
                out += '\n';
                for (uint32_t slot : vfetchSlots)
                    println("ByteAddressBuffer vfetch{0} : register(t{0}, space0);", slot);
                for (auto& [slot, dim] : texSlots)
                {
                    switch (TextureDimension(dim))
                    {
                    case TextureDimension::Texture3D: println("Texture3D<float4> tex3D_{0} : register(t{0}, space2);", slot); break;
                    case TextureDimension::TextureCube: println("TextureCube<float4> texCube_{0} : register(t{0}, space3);", slot); break;
                    default: println("Texture2D<float4> tex2D_{0} : register(t{0}, space1);", slot); break;
                    }
                }
                out += "void XeNoKill(float x) {}\n\n";

                out += "void main(\n";
                if (isPixelShader)
                {
                    out += "\tin float4 iPos : SV_Position,\n";
                    for (uint32_t i = 0; i < 16; i++)
                        println("\tin float4 i{0} : TEXCOORD{0},", i);
                    out += "\tin bool iFace : SV_IsFrontFace";
                    uint32_t targets = result.colorTargetsWritten ? result.colorTargetsWritten : 1;
                    for (uint32_t i = 0; i < 4; i++)
                        if (targets & (1u << i))
                            print(",\n\tout float4 oC{0} : SV_Target{0}", i);
                    if (result.writesDepth)
                        out += ",\n\tout float oDepth : SV_Depth";
                }
                else
                {
                    out += "\tin uint xeVertexId : SV_VertexID,\n";
                    out += "\tout precise float4 oPos : SV_Position";
                    for (uint32_t i = 0; i < 16; i++)
                        print(",\n\tout float4 o{0} : TEXCOORD{0}", i);
                }
                out += ")\n{\n";

                for (uint32_t i = 0; i <= maxTemp; i++)
                    println("\tfloat4 r{} = 0.0;", i);
                out += "\tfloat4 xeDiscard = 0.0;\n";
                out += "\tfloat4 xeDbgTex = float4(0.0, 0.0, 0.0, 1.0);\n";
                out += "\tint a0 = 0;\n\tint aL = 0;\n\tbool p0 = false;\n\tfloat ps = 0.0;\n";
                out += "\tuint xeVfetchBase = 0u;\n";
                out += "\tCubeMapData cubeMapData = (CubeMapData)0;\n";
                if (isPixelShader)
                {
                    for (uint32_t i = 0; i < 16; i++)
                        println("\tr{0} = i{0};", i);
                    uint32_t targets = result.colorTargetsWritten ? result.colorTargetsWritten : 1;
                    for (uint32_t i = 0; i < 4; i++)
                        if (targets & (1u << i))
                            println("\toC{} = 0.0;", i);
                        else
                            println("\tfloat4 oC{} = 0.0;", i);
                    out += "\tfloat4 oDepthVec = 0.0;\n";
                }
                else
                {
                    out += "\tr0.x = float(xeVertexId);\n";
                    out += "\toPos = float4(0.0, 0.0, 0.0, 1.0);\n";
                    for (uint32_t i = 0; i < 16; i++)
                        println("\to{} = 0.0;", i);
                    out += "\tfloat4 oPointSize = 0.0;\n";
                }
            }

            void EmitEpilogue()
            {
                if (isPixelShader)
                {
                    out += "\tif (xeFlags & 1u)\n\t{\n";
                    out += "\t\tbool pass = true;\n";
                    out += "\t\tuint func = uint(xeAlphaTest.y);\n";
                    out += "\t\tfloat a = oC0.w, ref = xeAlphaTest.x;\n";
                    out += "\t\tif (func == 0) pass = false; else if (func == 1) pass = a < ref; else if (func == 2) pass = a == ref;\n";
                    out += "\t\telse if (func == 3) pass = a <= ref; else if (func == 4) pass = a > ref; else if (func == 5) pass = a != ref;\n";
                    out += "\t\telse if (func == 6) pass = a >= ref;\n";
                    out += "\t\tif (!pass) discard;\n\t}\n";
                    // EDRAM formats 8_8_8_8 and 2_10_10_10 are fixed point and 2_10_10_10_FLOAT
                    // is 7e3: the hardware clamps the pixel output to their range. Our render
                    // targets are FP16 for all of them, so the clamp has to be explicit.
                    out += "\toC0 = clamp(oC0, -xeColorMax, xeColorMax);\n";
                    // Debug aid (LO_PS_TEXDEBUG): show the last texture fetch result.
                    out += "\tif (xeFlags & 16u) oC0 = float4(xeDbgTex.rgb, 1.0);\n";
                    // Debug aid (LO_PS_DEBUG): paint every surviving fragment magenta.
                    out += "\tif (xeFlags & 2u) oC0 = (xeFlags & 4u) ? float4(i15.w > 0.0 ? 1.0 : 0.0, saturate(log2(abs(i15.w) + 1.0) / 16.0), saturate(log2(abs(i15.z) + 1.0) / 16.0), 1.0) : float4(1.0, 0.0, 1.0, 1.0);\n";
                    if (result.writesDepth)
                        out += "\toDepth = oDepthVec.x;\n";
                }
                else
                {
                    out += "\tif ((xeFlags & 8u) == 0u) {\n";
                    out += "\tif (xeVtxFmt & 1u) oPos.xy *= oPos.w;\n";
                    out += "\tif (xeVtxFmt & 2u) oPos.z *= oPos.w;\n";
                    // PA_CL_VTE_CNTL.VTX_W0_FMT=1 means the shader already outputs w
                    // rather than 1/w (Xenia kSysFlag_WNotReciprocal); only without
                    // it is the reciprocal taken.
                    out += "\tif ((xeVtxFmt & 4u) == 0u) oPos.w = rcp(oPos.w);\n";
                    out += "\toPos.xyz = oPos.xyz * xeNdcScale.xyz + xeNdcOffset.xyz * oPos.w;\n";
                    out += "\toPos.xy += xeHalfPixelOffset * oPos.w;\n";
                    out += "\t}\n";
                    // Debug aid (LO_VS_DEBUG): replace every triangle by one fixed on-screen triangle.
                    out += "\tif (xeFlags & 4u) { o15 = oPos; uint k = xeVertexId % 3u; oPos = float4(k == 0u ? -0.6 : (k == 1u ? 0.6 : 0.0), k == 2u ? 0.6 : -0.6, 0.5, 1.0); }\n";
                }
            }

            void EmitBoolCondition(uint32_t boolAddress, bool condition)
            {
                println("if (XeBool({}u) == {})", boolAddress, condition ? "true" : "false");
            }

            void Emit()
            {
                EmitDeclarations();

                // Main body. "shouldReturn" blocks end the shader: since all
                // outputs are out-parameters we run the epilogue and return.
                if (!simpleControlFlow)
                {
                    out += "\n\tuint pc = 0;\n\tuint xeLoopIter = 0;\n\twhile (true)\n\t{\n\t\tswitch (pc)\n\t\t{\n";
                }
                else
                {
                    out += '\n';
                }
                indentation = simpleControlFlow ? 1 : 3;

                uint32_t pc = 0;
                bool ended = false;
                for (uint32_t byteOffset = 0; byteOffset < cfByteSize && !ended; byteOffset += 12)
                {
                    CfPair pair = ReadCfPair(byteOffset / 4);
                    for (auto& cf : pair.cf)
                    {
                        if (ended)
                            break;
                        if (!simpleControlFlow)
                        {
                            indentation = 2;
                            indent();
                            println("case {}:", pc);
                            indentation = 3;
                        }
                        else
                        {
                            auto it = ifEndLabels.find(pc);
                            if (it != ifEndLabels.end())
                            {
                                for (uint32_t i = 0; i < it->second; i++)
                                {
                                    --indentation;
                                    indent();
                                    out += "}\n";
                                }
                            }
                        }
                        ++pc;

                        bool shouldReturn = false;
                        bool closeBlock = false;
                        switch (cf.opcode)
                        {
                        case ControlFlowOpcode::Nop:
                        case ControlFlowOpcode::Alloc:
                        case ControlFlowOpcode::MarkVsFetchDone:
                            break;

                        case ControlFlowOpcode::Exec:
                        case ControlFlowOpcode::ExecEnd:
                            shouldReturn = cf.opcode == ControlFlowOpcode::ExecEnd;
                            break;

                        case ControlFlowOpcode::CondExec:
                        case ControlFlowOpcode::CondExecEnd:
                            indent();
                            EmitBoolCondition(cf.condExec.boolAddress, cf.condExec.condition);
                            indent(); out += "{\n"; ++indentation; closeBlock = true;
                            shouldReturn = cf.opcode == ControlFlowOpcode::CondExecEnd;
                            break;

                        case ControlFlowOpcode::CondExecPred:
                        case ControlFlowOpcode::CondExecPredEnd:
                        case ControlFlowOpcode::CondExecPredClean:
                        case ControlFlowOpcode::CondExecPredCleanEnd:
                            indent();
                            println("if ({}p0)", cf.condExecPred.condition ? "" : "!");
                            indent(); out += "{\n"; ++indentation; closeBlock = true;
                            shouldReturn = cf.opcode == ControlFlowOpcode::CondExecPredEnd || cf.opcode == ControlFlowOpcode::CondExecPredCleanEnd;
                            break;

                        case ControlFlowOpcode::LoopStart:
                            if (simpleControlFlow)
                            {
                                indent();
                                println("for (uint xeLoop{0} = 0, xeLoopCount{0} = XeLoopConst({0}u) & 0xFFu; xeLoop{0} < xeLoopCount{0}; xeLoop{0}++)",
                                    uint32_t(cf.loopStart.loopId));
                                indent(); out += "{\n"; ++indentation;
                                indent();
                                println("aL = int((XeLoopConst({0}u) >> 8) & 0xFFu) + int(xeLoop{0}) * (int(XeLoopConst({0}u) << 8) >> 24);", uint32_t(cf.loopStart.loopId));
                            }
                            else
                            {
                                indent(); out += "xeLoopIter = 0;\n";
                                indent(); println("aL = int((XeLoopConst({0}u) >> 8) & 0xFFu);", uint32_t(cf.loopStart.loopId));
                            }
                            break;

                        case ControlFlowOpcode::LoopEnd:
                            if (simpleControlFlow)
                            {
                                --indentation;
                                indent(); out += "}\n";
                            }
                            else
                            {
                                indent(); out += "++xeLoopIter;\n";
                                indent(); println("aL += (int(XeLoopConst({0}u) << 8) >> 24);", uint32_t(cf.loopEnd.loopId));
                                indent(); println("if (xeLoopIter < (XeLoopConst({0}u) & 0xFFu)) {{ pc = {1}; continue; }}", uint32_t(cf.loopEnd.loopId), uint32_t(cf.loopEnd.address));
                            }
                            break;

                        case ControlFlowOpcode::CondJmp:
                            if (cf.condJmp.isUnconditional)
                            {
                                indent(); println("pc = {}; continue;", uint32_t(cf.condJmp.address));
                            }
                            else
                            {
                                indent();
                                if (cf.condJmp.isPredicated)
                                    println("if ({}p0)", (cf.condJmp.condition ^ (simpleControlFlow ? 1u : 0u)) ? "" : "!");
                                else
                                    println("if (XeBool({}u) {}= {})", uint32_t(cf.condJmp.boolAddress),
                                        simpleControlFlow ? "!" : "=", cf.condJmp.condition ? "true" : "false");
                                if (simpleControlFlow)
                                {
                                    indent(); out += "{\n"; ++indentation;
                                }
                                else
                                {
                                    indent(); println("{{ pc = {}; continue; }}", uint32_t(cf.condJmp.address));
                                }
                            }
                            break;

                        default:
                            indent();
                            println("// unsupported control flow opcode {}", uint32_t(cf.opcode));
                            break;
                        }

                        if (CfHasExecBody(cf.opcode))
                        {
                            uint32_t address = CfExecAddress(cf);
                            uint32_t count = CfExecCount(cf);
                            uint32_t sequence = CfExecSequence(cf);
                            for (uint32_t i = 0; i < count; i++, sequence >>= 2)
                            {
                                uint32_t w[3];
                                ReadInstruction(address + i, w);
                                if (sequence & 1)
                                {
                                    FetchInstruction fetch;
                                    memcpy(&fetch, w, 8);
                                    if (fetch.opcode == FetchOpcode::VertexFetch)
                                    {
                                        VertexFetchInstruction vf;
                                        memcpy(&vf, w, 12);
                                        recompile(vf);
                                    }
                                    else
                                    {
                                        TextureFetchInstruction tf;
                                        memcpy(&tf, w, 12);
                                        recompile(tf);
                                    }
                                }
                                else
                                {
                                    AluInstruction alu;
                                    memcpy(&alu, w, 12);
                                    recompile(alu);
                                }
                            }
                        }

                        if (shouldReturn)
                        {
                            if (simpleControlFlow)
                            {
                                if (!closeBlock)
                                    ended = true; // nothing after an unconditional end can execute
                                else
                                {
                                    indent(); out += "xeEnd = true;\n";
                                }
                            }
                            else
                            {
                                indent(); out += "pc = 0xFFFFu; break;\n";
                            }
                        }

                        if (closeBlock)
                        {
                            --indentation;
                            indent(); out += "}\n";
                            if (simpleControlFlow && shouldReturn)
                            {
                                indent(); out += "if (xeEnd) { xeEnd = false; } else\n";
                                // The rest of the program runs only when the block did not end the shader.
                                indent(); out += "{\n"; ++indentation;
                                result.errors += ""; // structural, no note
                                pendingEndBlocks++;
                            }
                        }
                        if (!simpleControlFlow)
                        {
                            indent(); out += "break;\n";
                        }
                    }
                }

                if (simpleControlFlow)
                {
                    while (pendingEndBlocks > 0)
                    {
                        --indentation;
                        indent(); out += "}\n";
                        pendingEndBlocks--;
                    }
                    // Close any unterminated conditional-jump blocks.
                    while (indentation > 1)
                    {
                        --indentation;
                        indent(); out += "}\n";
                    }
                }
                else
                {
                    out += "\t\tdefault:\n\t\t\tbreak;\n\t\t}\n\t\tbreak;\n\t}\n";
                }

                EmitEpilogue();
                out += "}\n";
            }

            uint32_t pendingEndBlocks = 0;

            TranslatedShader Run()
            {
                result.isPixelShader = isPixelShader;
                Scan();
                Emit();
                // Conditional end blocks use a flag declared up front.
                out.insert(out.find("\tuint xeVfetchBase = 0u;\n"), "\tbool xeEnd = false;\n");
                // Both stages share one pipeline layout: pixel shader constants live in b2.
                if (isPixelShader)
                {
                    size_t pos = out.find("cbuffer XeConstants : register(b0, space0)");
                    if (pos != std::string::npos)
                        out.replace(pos, strlen("cbuffer XeConstants : register(b0, space0)"), "cbuffer XeConstants : register(b2, space0)");
                }
                result.hlsl = std::move(out);
                return std::move(result);
            }
        };
    }

    TranslatedShader TranslateShader(const uint32_t* dwords, uint32_t dwordCount, bool isPixelShader)
    {
        Translator t;
        t.code = dwords;
        t.dwordCount = dwordCount;
        t.isPixelShader = isPixelShader;
        return t.Run();
    }

    const char* GetShaderCommonHlsl()
    {
        return kCommonHlsl;
    }
}
