//
// LostOdysseyRecomp (2026-10-27): SPIR-V to uam's GLSL for plume's deko3d back end (plume_deko3d_spirv.h).
// SPIRV-Cross writes Vulkan GLSL; the resources get deko3d bindings through decorations, and what Vulkan GLSL has
// and uam lacks (separate samplers, buffer_reference, constant_id) is rewritten in the text.
//
#include "plume_deko3d_spirv.h"

#include <spirv_glsl.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>

namespace plume {
    namespace {
        using Kind = Deko3DSpirvLayout::Kind;

        bool isIdent(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

        // s[open] is '(' or '['; the index of its match, or npos.
        size_t matchBracket(const std::string &s, size_t open) {
            const char o = s[open], c = o == '(' ? ')' : ']';
            int depth = 0;
            for (size_t i = open; i < s.size(); i++) {
                if (s[i] == o)
                    depth++;
                else if (s[i] == c && --depth == 0)
                    return i;
            }
            return std::string::npos;
        }

        // Replaces every whole-word occurrence of 'from'.
        void replaceWord(std::string &s, const std::string &from, const std::string &to) {
            size_t p = 0;
            while ((p = s.find(from, p)) != std::string::npos) {
                const bool before = p > 0 && isIdent(s[p - 1]);
                const bool after = p + from.size() < s.size() && isIdent(s[p + from.size()]);
                if (before || after) {
                    p += from.size();
                    continue;
                }
                s.replace(p, from.size(), to);
                p += to.size();
            }
        }

        void replaceAll(std::string &s, const std::string &from, const std::string &to) {
            size_t p = 0;
            while ((p = s.find(from, p)) != std::string::npos) {
                s.replace(p, from.size(), to);
                p += to.size();
            }
        }

        // Removes whole lines that start (after indentation) with the prefix.
        void removeLines(std::string &s, const std::string &prefix) {
            size_t p = 0;
            while ((p = s.find(prefix, p)) != std::string::npos) {
                const size_t lineStart = s.rfind('\n', p == 0 ? 0 : p - 1);
                const size_t begin = lineStart == std::string::npos ? 0 : lineStart + 1;
                bool atLineStart = true;
                for (size_t i = begin; i < p; i++)
                    atLineStart = atLineStart && (s[i] == ' ' || s[i] == '\t');
                if (!atLineStart) {
                    p += prefix.size();
                    continue;
                }
                size_t end = s.find('\n', p);
                end = end == std::string::npos ? s.size() : end + 1;
                s.erase(begin, end - begin);
                p = begin;
            }
        }

        struct SeparateResource {
            uint32_t set = 0, binding = 0;
            std::string name;
            std::string dim;   // texture: 2D, 3D, Cube, 2DArray, ...
            bool array = false;
        };

        struct Pair {
            std::string texture;      // name of the separate texture (with any constant element, e.g. "t[2]")
            std::string samplerExpr;  // the sampler expression as written, "" for none
            bool shadow = false;
            std::string combined;     // the combined sampler's name
            uint32_t binding = 0;
            std::string type;         // sampler2D, sampler2DShadow, ...
        };
    }

    Deko3DSpirvResult Deko3DTranslateSpirv(const uint32_t *words, size_t wordCount, Deko3DSpirvStage stage,
                                           const Deko3DSpirvLayout &layout, const Deko3DSpirvOptions &options) {
        Deko3DSpirvResult result;
        try {
            spirv_cross::CompilerGLSL compiler(words, wordCount);
            spirv_cross::CompilerGLSL::Options o = compiler.get_common_options();
            o.version = 460;
            o.es = false;
            o.vulkan_semantics = true;  // separate samplers and buffer_reference survive; rewritten below
            o.separate_shader_objects = true;
            o.emit_push_constant_as_uniform_buffer = true;
            // The vertex stage only: a geometry shader (the Switch rect lists) builds its corners from the vertex
            // shader's already flipped positions.
            o.vertex.flip_vert_y = stage == Deko3DSpirvStage::Vertex;
            o.vertex.fixup_clipspace = false;
            compiler.set_common_options(o);
            const spirv_cross::ShaderResources res = compiler.get_shader_resources();

            auto bindTo = [&](const spirv_cross::Resource &r, Kind kind, uint32_t &mask) -> bool {
                const uint32_t set = compiler.get_decoration(r.id, spv::DecorationDescriptorSet);
                const uint32_t binding = compiler.get_decoration(r.id, spv::DecorationBinding);
                const Deko3DSpirvLayout::Range *range = layout.find(set, binding);
                if (!range || range->kind != kind) {
                    char text[160];
                    snprintf(text, sizeof(text), "%s %s (set %u binding %u) is not in the pipeline layout",
                             kind == Kind::Uniform ? "uniform buffer" : "storage buffer", r.name.c_str(), set, binding);
                    result.error = text;
                    return false;
                }
                compiler.unset_decoration(r.id, spv::DecorationDescriptorSet);
                compiler.set_decoration(r.id, spv::DecorationBinding, range->slot);
                mask |= 1u << range->slot;
                return true;
            };
            for (const auto &r : res.uniform_buffers)
                if (!bindTo(r, Kind::Uniform, result.uniformMask))
                    return result;
            for (const auto &r : res.storage_buffers)
                if (!bindTo(r, Kind::Storage, result.storageMask))
                    return result;
            for (const auto &r : res.push_constant_buffers) {
                compiler.set_decoration(r.id, spv::DecorationBinding, Deko3DPushUniformSlot);
                result.uniformMask |= 1u << Deko3DPushUniformSlot;
            }
            if (!res.sampled_images.empty() || !res.storage_images.empty() || !res.atomic_counters.empty() ||
                !res.subpass_inputs.empty() || !res.acceleration_structures.empty()) {
                result.error = "combined samplers, storage images, atomic counters or subpass inputs (not translated)";
                return result;
            }

            // The separate textures and samplers, by name (Vulkan GLSL keeps the SPIR-V's names).
            std::map<std::string, SeparateResource> textures, samplers;
            for (const auto &r : res.separate_images) {
                SeparateResource t;
                t.set = compiler.get_decoration(r.id, spv::DecorationDescriptorSet);
                t.binding = compiler.get_decoration(r.id, spv::DecorationBinding);
                t.name = compiler.get_name(r.id);
                const spirv_cross::SPIRType &type = compiler.get_type(r.type_id);
                t.array = !type.array.empty();
                switch (type.image.dim) {
                case spv::Dim1D: t.dim = "1D"; break;
                case spv::Dim3D: t.dim = "3D"; break;
                case spv::DimCube: t.dim = "Cube"; break;
                case spv::DimBuffer: t.dim = "Buffer"; break;
                default: t.dim = "2D"; break;
                }
                if (type.image.arrayed)
                    t.dim += "Array";
                if (type.image.ms)
                    t.dim += "MS";
                if (t.name.empty() || t.dim == "Buffer") {
                    result.error = "an unnamed or buffer texture";
                    return result;
                }
                textures[t.name] = t;
            }
            for (const auto &r : res.separate_samplers) {
                SeparateResource s;
                s.set = compiler.get_decoration(r.id, spv::DecorationDescriptorSet);
                s.binding = compiler.get_decoration(r.id, spv::DecorationBinding);
                s.name = compiler.get_name(r.id);
                s.array = !compiler.get_type(r.type_id).array.empty();
                if (s.name.empty()) {
                    result.error = "an unnamed sampler";
                    return result;
                }
                samplers[s.name] = s;
            }
            // Uniform buffers by instance name (for sampler indices read from them).
            std::map<std::string, spirv_cross::Resource> uniformsByName;
            for (const auto &r : res.uniform_buffers) {
                std::string name = compiler.get_name(r.id);
                if (name.empty())
                    name = compiler.get_fallback_name(r.id);
                uniformsByName[name] = r;
            }

            if (stage == Deko3DSpirvStage::Vertex) {
                uint32_t maxLocation = 0;
                bool any = false;
                for (const auto &r : res.stage_inputs) {
                    if (!compiler.has_decoration(r.id, spv::DecorationLocation))
                        continue;
                    maxLocation = std::max(maxLocation, compiler.get_decoration(r.id, spv::DecorationLocation));
                    any = true;
                }
                if (any)
                    for (uint32_t l = 0; l <= maxLocation; l++)
                        result.vertexInputs.push_back(l);
            }

            std::string glsl = compiler.compile();

            // ---- specialization constants: macros plume_deko3d's variants define (#define SPIRV_CROSS_CONSTANT_ID_n v)
            {
                const std::string key = "layout(constant_id = ";
                size_t p = 0;
                while ((p = glsl.find(key, p)) != std::string::npos) {
                    const size_t end = glsl.find(';', p);
                    if (end == std::string::npos)
                        break;
                    // layout(constant_id = N) const TYPE NAME = VALUE;
                    const std::string decl = glsl.substr(p, end - p);
                    unsigned id = 0;
                    char type[64] = {}, name[256] = {};
                    int consumed = 0;
                    if (sscanf(decl.c_str(), "layout(constant_id = %u) const %63s %255s = %n", &id, type, name, &consumed) < 3 || consumed <= 0) {
                        result.error = "a specialization constant SPIRV-Cross wrote in an unknown form: " + decl;
                        return result;
                    }
                    std::string value = decl.substr(size_t(consumed));
                    char text[512];
                    snprintf(text, sizeof(text),
                             "#ifndef SPIRV_CROSS_CONSTANT_ID_%u\n#define SPIRV_CROSS_CONSTANT_ID_%u %s\n#endif\nconst %s %s = %s(SPIRV_CROSS_CONSTANT_ID_%u)",
                             id, id, value.c_str(), type, name, type, id);
                    glsl.replace(p, end - p, text);
                    p += strlen(text);
                }
            }

            // ---- textures and samplers
            // Declarations: "layout(set = S, binding = B) uniform textureXX NAME;" and "... uniform sampler NAME[N];".
            size_t firstDecl = std::string::npos;
            for (const auto &[name, t] : textures) {
                const std::string decl = " uniform texture" + t.dim + " " + name;
                const size_t at = glsl.find(decl);
                if (at == std::string::npos) {
                    result.error = "texture declaration of " + name + " not found";
                    return result;
                }
                const size_t begin = glsl.rfind('\n', at);
                firstDecl = std::min(firstDecl, begin == std::string::npos ? 0 : begin + 1);
            }
            for (const auto &[name, t] : textures)
                removeLines(glsl, "layout(set = " + std::to_string(t.set) + ", binding = " + std::to_string(t.binding) + ") uniform texture" + t.dim + " " + name);
            for (const auto &[name, s] : samplers) {
                const size_t at = glsl.find(" uniform sampler " + name);
                if (at != std::string::npos) {
                    const size_t begin = glsl.rfind('\n', at);
                    firstDecl = std::min(firstDecl, begin == std::string::npos ? 0 : begin + 1);
                }
                removeLines(glsl, "layout(set = " + std::to_string(s.set) + ", binding = " + std::to_string(s.binding) + ") uniform sampler " + name);
                removeLines(glsl, "layout(set = " + std::to_string(s.set) + ", binding = " + std::to_string(s.binding) + ") uniform samplerShadow " + name);
            }
            if (firstDecl != std::string::npos)
                firstDecl = std::min(firstDecl, glsl.size());

            std::vector<Pair> pairs;
            auto pairFor = [&](const std::string &texture, const std::string &dim, const std::string &samplerExpr, bool shadow) -> Pair * {
                for (Pair &p : pairs)
                    if (p.texture == texture && p.samplerExpr == samplerExpr && p.shadow == shadow)
                        return &p;
                if (pairs.size() >= 32)
                    return nullptr;
                Pair p;
                p.texture = texture;
                p.samplerExpr = samplerExpr;
                p.shadow = shadow;
                p.binding = uint32_t(pairs.size());
                p.combined = "dkTex" + std::to_string(p.binding);
                p.type = "sampler" + dim + (shadow ? "Shadow" : "");
                pairs.push_back(p);
                return &pairs.back();
            };
            // The base name of a texture expression ("t" or "t[3]").
            auto textureBase = [&](const std::string &expr) -> const SeparateResource * {
                const size_t b = expr.find('[');
                auto it = textures.find(expr.substr(0, b));
                return it == textures.end() ? nullptr : &it->second;
            };

            // samplerXX(TEXTURE, SAMPLER) constructions.
            {
                size_t p = 0;
                std::string out;
                out.reserve(glsl.size());
                while (true) {
                    size_t at = glsl.find("sampler", p);
                    if (at == std::string::npos) {
                        out.append(glsl, p, std::string::npos);
                        break;
                    }
                    if (at > 0 && isIdent(glsl[at - 1])) {
                        out.append(glsl, p, at + 7 - p);
                        p = at + 7;
                        continue;
                    }
                    size_t q = at + 7;
                    while (q < glsl.size() && isIdent(glsl[q]))
                        q++;
                    if (q >= glsl.size() || glsl[q] != '(') {
                        out.append(glsl, p, q - p);
                        p = q;
                        continue;
                    }
                    const std::string ctor = glsl.substr(at + 7, q - at - 7);  // 2D, 2DShadow, CubeArray, ...
                    const size_t close = matchBracket(glsl, q);
                    if (close == std::string::npos) {
                        result.error = "unbalanced sampler constructor";
                        return result;
                    }
                    const std::string args = glsl.substr(q + 1, close - q - 1);
                    // TEXTURE, SAMPLER at the top level of the arguments.
                    int depth = 0;
                    size_t comma = std::string::npos;
                    for (size_t i = 0; i < args.size(); i++) {
                        if (args[i] == '(' || args[i] == '[')
                            depth++;
                        else if (args[i] == ')' || args[i] == ']')
                            depth--;
                        else if (args[i] == ',' && depth == 0) {
                            comma = i;
                            break;
                        }
                    }
                    std::string texture = comma == std::string::npos ? args : args.substr(0, comma);
                    std::string samplerExpr = comma == std::string::npos ? std::string() : args.substr(comma + 1);
                    while (!samplerExpr.empty() && samplerExpr.front() == ' ')
                        samplerExpr.erase(samplerExpr.begin());
                    const SeparateResource *t = textureBase(texture);
                    if (!t) {
                        // Not one of ours (a sampler type name used otherwise): keep it.
                        out.append(glsl, p, close + 1 - p);
                        p = close + 1;
                        continue;
                    }
                    const bool shadow = ctor.size() > 6 && ctor.compare(ctor.size() - 6, 6, "Shadow") == 0;
                    Pair *pair = pairFor(texture, t->dim, samplerExpr, shadow);
                    if (!pair) {
                        result.error = "more than 32 texture and sampler pairs";
                        return result;
                    }
                    out.append(glsl, p, at - p);
                    out += pair->combined;
                    p = close + 1;
                }
                glsl.swap(out);
            }
            // What still names a texture alone (texelFetch, textureSize, textureQueryLevels): a pair with no sampler,
            // or the texture's first pair (the sampler does not matter to those).
            for (const auto &[name, t] : textures) {
                size_t p = 0;
                std::string out;
                while (true) {
                    size_t at = glsl.find(name, p);
                    if (at == std::string::npos) {
                        out.append(glsl, p, std::string::npos);
                        break;
                    }
                    const bool before = at > 0 && isIdent(glsl[at - 1]);
                    const size_t after = at + name.size();
                    if (before || (after < glsl.size() && isIdent(glsl[after]))) {
                        out.append(glsl, p, after - p);
                        p = after;
                        continue;
                    }
                    std::string expr = name;
                    size_t end = after;
                    if (t.array && end < glsl.size() && glsl[end] == '[') {
                        const size_t close = matchBracket(glsl, end);
                        if (close == std::string::npos) {
                            result.error = "unbalanced texture index";
                            return result;
                        }
                        expr = glsl.substr(at, close + 1 - at);
                        end = close + 1;
                    }
                    Pair *pair = nullptr;
                    for (Pair &candidate : pairs)
                        if (candidate.texture == expr && !candidate.shadow) {
                            pair = &candidate;
                            break;
                        }
                    if (!pair)
                        pair = pairFor(expr, t.dim, std::string(), false);
                    if (!pair) {
                        result.error = "more than 32 texture and sampler pairs";
                        return result;
                    }
                    out.append(glsl, p, at - p);
                    out += pair->combined;
                    p = end;
                }
                glsl.swap(out);
            }
            for (const auto &[name, s] : samplers) {
                // A sampler left over outside a constructor would not compile; uam says where.
                (void)name;
                (void)s;
            }

            // Where each pair's handle comes from.
            for (const Pair &pair : pairs) {
                Deko3DSpirvTexture tex;
                const SeparateResource *t = textureBase(pair.texture);
                const Deko3DSpirvLayout::Range *tr = t ? layout.find(t->set, t->binding) : nullptr;
                if (!tr || tr->kind != Kind::Texture) {
                    result.error = "texture " + pair.texture + " is not in the pipeline layout";
                    return result;
                }
                tex.imageSet = t->set;
                tex.imageIndex = tr->descriptorIndex;
                const size_t bracket = pair.texture.find('[');
                if (bracket != std::string::npos) {
                    char *e = nullptr;
                    const unsigned long element = strtoul(pair.texture.c_str() + bracket + 1, &e, 10);
                    if (!e || (*e != ']' && *e != 'u')) {
                        result.error = "texture " + pair.texture + " indexed by a variable";
                        return result;
                    }
                    tex.imageIndex += uint32_t(element);
                }
                if (!pair.samplerExpr.empty()) {
                    const std::string &se = pair.samplerExpr;
                    const size_t b = se.find('[');
                    const std::string base = se.substr(0, b);
                    auto it = samplers.find(base);
                    const Deko3DSpirvLayout::Range *sr = it == samplers.end() ? nullptr : layout.find(it->second.set, it->second.binding);
                    if (!sr || sr->kind != Kind::Sampler) {
                        result.error = "sampler " + se + " is not in the pipeline layout";
                        return result;
                    }
                    tex.samplerSet = it->second.set;
                    tex.samplerIndex = sr->descriptorIndex;
                    if (b == std::string::npos) {
                        tex.samplerFrom = Deko3DSpirvTexture::SamplerFrom::Fixed;
                    }
                    else {
                        const std::string index = se.substr(b + 1, se.size() - b - 2);
                        char *e = nullptr;
                        const unsigned long element = strtoul(index.c_str(), &e, 10);
                        if (e && e != index.c_str() && (*e == 0 || (*e == 'u' && e[1] == 0))) {
                            tex.samplerFrom = Deko3DSpirvTexture::SamplerFrom::Fixed;
                            tex.samplerIndex += uint32_t(element);
                        }
                        else {
                            // UBO.member[N].c, UBO.member[N] or UBO.member.c
                            char ubo[128] = {}, member[128] = {};
                            unsigned n = 0;
                            char comp = 'x';
                            bool parsed = false;
                            bool indexed = true;
                            if (sscanf(index.c_str(), "%127[A-Za-z0-9_].%127[A-Za-z0-9_][%uu].%c", ubo, member, &n, &comp) == 4 ||
                                sscanf(index.c_str(), "%127[A-Za-z0-9_].%127[A-Za-z0-9_][%u].%c", ubo, member, &n, &comp) == 4)
                                parsed = true;
                            else if (sscanf(index.c_str(), "%127[A-Za-z0-9_].%127[A-Za-z0-9_].%c", ubo, member, &comp) == 3) {
                                parsed = true;
                                indexed = false;
                            }
                            auto u = parsed ? uniformsByName.find(ubo) : uniformsByName.end();
                            if (u == uniformsByName.end()) {
                                result.error = "sampler index " + index + " is not a uniform buffer member";
                                return result;
                            }
                            const spirv_cross::SPIRType &type = compiler.get_type(u->second.base_type_id);
                            bool found = false;
                            for (uint32_t m = 0; m < uint32_t(type.member_types.size()); m++) {
                                if (compiler.get_member_name(type.self, m) != member)
                                    continue;
                                uint32_t offset = compiler.type_struct_member_offset(type, m);
                                if (indexed)
                                    offset += n * compiler.type_struct_member_array_stride(type, m);
                                offset += uint32_t(comp == 'y' ? 4 : comp == 'z' ? 8 : comp == 'w' ? 12 : 0);
                                const uint32_t set = compiler.get_decoration(u->second.id, spv::DecorationDescriptorSet);
                                (void)set;
                                tex.samplerFrom = Deko3DSpirvTexture::SamplerFrom::UniformWord;
                                tex.uniformSlot = compiler.get_decoration(u->second.id, spv::DecorationBinding);  // already the deko3d slot
                                tex.uniformOffset = offset;
                                found = true;
                                break;
                            }
                            if (!found) {
                                result.error = "sampler index member " + std::string(member) + " not found";
                                return result;
                            }
                        }
                    }
                }
                result.textures.push_back(tex);
            }
            if (!pairs.empty()) {
                std::string decls;
                for (const Pair &pair : pairs)
                    decls += "layout(binding = " + std::to_string(pair.binding) + ") uniform " + pair.type + " " + pair.combined + ";\n";
                if (firstDecl == std::string::npos) {
                    // After the #version and #extension lines.
                    size_t at = 0;
                    while (glsl.compare(at, 1, "#") == 0) {
                        const size_t nl = glsl.find('\n', at);
                        at = nl == std::string::npos ? glsl.size() : nl + 1;
                    }
                    firstDecl = at;
                }
                glsl.insert(firstDecl, decls);
            }

            // ---- buffer_reference reads: storage buffer windows
            // A load through a pointer becomes a read of a storage buffer that starts where the pointer's base points:
            // the address window (the vertex arena, a uniform buffer's uvec2 member) or a push constant member (the
            // renderer's own shaders: XePushConstants' uint64 addresses or XePushConstantWords' uvec2 words), bound
            // by plume_deko3d at each draw from the pushed address (Deko3DSpirvResult::pushWindows).
            if (glsl.find("buffer_reference") != std::string::npos) {
                struct Window {
                    uint32_t slot = 0;
                    std::string suffix;
                    std::set<std::string> loads;
                };
                std::vector<Window> windows;
                struct Base {
                    std::string text;  // what names the base in an address expression (replaced by 0)
                    size_t window = 0;
                    bool word = false; // a whole identifier
                };
                std::vector<Base> bases;
                auto containsBase = [&](const std::string &text, const Base &b) -> bool {
                    size_t p = 0;
                    while ((p = text.find(b.text, p)) != std::string::npos) {
                        const bool before = p > 0 && isIdent(text[p - 1]);
                        const bool after = p + b.text.size() < text.size() && isIdent(text[p + b.text.size()]);
                        if (!before && !after)
                            return true;
                        p += b.text.size();
                    }
                    return false;
                };
                // 64-bit addresses loaded through a pointer (the renderer's own vertex shaders read the arena address
                // from the shared constants) are the address window's: "uint64_tPointer(p).value" -> dkWindowBase.
                bool loadedWindowBase = false;
                {
                    const std::string ctor = "uint64_tPointer(";
                    size_t p = 0;
                    while ((p = glsl.find(ctor, p)) != std::string::npos) {
                        if (p > 0 && isIdent(glsl[p - 1])) {
                            p += ctor.size();
                            continue;
                        }
                        const size_t open = p + ctor.size() - 1;
                        const size_t close = matchBracket(glsl, open);
                        if (close == std::string::npos || glsl.compare(close + 1, 6, ".value") != 0) {
                            p += ctor.size();
                            continue;
                        }
                        glsl.replace(p, close + 7 - p, "dkWindowBase");
                        loadedWindowBase = true;
                        p += 12;
                    }
                }
                // The address window's base expression, in any uniform buffer.
                const std::string &m = options.addressWindowMember;
                for (const auto &[name, r] : uniformsByName) {
                    (void)r;
                    const std::string base = "uint64_t(" + name + "." + m + ".x) | (uint64_t(" + name + "." + m + ".y) << (32ul & 63ul))";
                    if (glsl.find(base) == std::string::npos)
                        continue;
                    const Deko3DSpirvLayout::Range *window = layout.find(options.addressWindowSet, options.addressWindowBinding);
                    if (!window || window->kind != Kind::Storage) {
                        result.error = "the address window is read, and no storage buffer at its binding";
                        return result;
                    }
                    Window w;
                    w.slot = window->slot;
                    w.suffix = "W" + std::to_string(windows.size());
                    windows.push_back(w);
                    bases.push_back({"(" + base + ")", windows.size() - 1, false});
                    bases.push_back({base, windows.size() - 1, false});
                }
                if (loadedWindowBase) {
                    size_t index = windows.size();
                    const Deko3DSpirvLayout::Range *window = layout.find(options.addressWindowSet, options.addressWindowBinding);
                    if (!window || window->kind != Kind::Storage) {
                        result.error = "the address window is read, and no storage buffer at its binding";
                        return result;
                    }
                    for (size_t i = 0; i < windows.size(); i++)
                        if (windows[i].slot == window->slot)
                            index = i;
                    if (index == windows.size()) {
                        Window w;
                        w.slot = window->slot;
                        w.suffix = "W" + std::to_string(windows.size());
                        windows.push_back(w);
                    }
                    bases.push_back({"dkWindowBase", index, true});
                }
                // Push constant members.
                for (const auto &r : res.push_constant_buffers) {
                    std::string instance = compiler.get_name(r.id);
                    if (instance.empty())
                        instance = compiler.get_fallback_name(r.id);
                    const spirv_cross::SPIRType &type = compiler.get_type(r.base_type_id);
                    for (uint32_t i = 0; i < uint32_t(type.member_types.size()); i++) {
                        const spirv_cross::SPIRType &mt = compiler.get_type(type.member_types[i]);
                        const std::string member = instance + "." + compiler.get_member_name(type.self, i);
                        const bool address64 = mt.basetype == spirv_cross::SPIRType::UInt64 && mt.vecsize == 1;
                        const bool words = mt.basetype == spirv_cross::SPIRType::UInt && mt.vecsize == 2;
                        if ((!address64 && !words) || glsl.find(member) == std::string::npos)
                            continue;
                        Window w;
                        w.slot = Deko3DPushWindowFirstSlot + uint32_t(result.pushWindows.size());
                        w.suffix = "W" + std::to_string(windows.size());
                        if (result.pushWindows.size() >= Deko3DPushWindowCount) {
                            result.error = "more push constant addresses than storage windows";
                            return result;
                        }
                        result.pushWindows.push_back({w.slot, compiler.type_struct_member_offset(type, i)});
                        windows.push_back(w);
                        const size_t index = windows.size() - 1;
                        if (address64)
                            bases.push_back({member, index, false});
                        else {
                            // uvec2 V = P.member; ... uint64_t(V.x + k) | (uint64_t(V.y) << s)
                            const std::string decl = " = " + member + ";";
                            size_t p = 0;
                            while ((p = glsl.find(decl, p)) != std::string::npos) {
                                size_t e = p;
                                size_t b0 = e;
                                while (b0 > 0 && isIdent(glsl[b0 - 1]))
                                    b0--;
                                const std::string var = glsl.substr(b0, e - b0);
                                if (!var.empty()) {
                                    bases.push_back({var + ".x", index, false});
                                    bases.push_back({var + ".y", index, false});
                                }
                                p += decl.size();
                            }
                            bases.push_back({member + ".x", index, false});
                            bases.push_back({member + ".y", index, false});
                        }
                    }
                }
                // 64-bit variables holding a base: "uint64_t V = <expression with a base>;" (to a fixed point).
                for (bool grown = true; grown;) {
                    grown = false;
                    size_t p = 0;
                    const std::string key = "uint64_t ";
                    while ((p = glsl.find(key, p)) != std::string::npos) {
                        if (p > 0 && isIdent(glsl[p - 1])) {
                            p += key.size();
                            continue;
                        }
                        size_t e = p + key.size();
                        while (e < glsl.size() && isIdent(glsl[e]))
                            e++;
                        const std::string var = glsl.substr(p + key.size(), e - p - key.size());
                        const size_t end = glsl.find(';', e);
                        if (var.empty() || glsl.compare(e, 3, " = ") != 0 || end == std::string::npos) {
                            p = e;
                            continue;
                        }
                        const std::string init = glsl.substr(e + 3, end - e - 3);
                        bool known = false;
                        for (const Base &b : bases)
                            known = known || (b.word && b.text == var);
                        if (!known) {
                            for (const Base &b : bases) {
                                if (containsBase(init, b)) {
                                    bases.push_back({var, b.window, true});
                                    grown = true;
                                    break;
                                }
                            }
                        }
                        p = end;
                    }
                }
                if (windows.empty()) {
                    result.error = "buffer_reference reads whose base is neither the address window nor a push constant";
                    return result;
                }
                // The window an address expression reads, and the expression with its base made 0.
                auto windowOf = [&](std::string &arg) -> int {
                    int found = -1;
                    for (const Base &b : bases) {
                        if (!containsBase(arg, b))
                            continue;
                        if (found < 0)
                            found = int(b.window);
                        else if (size_t(found) != b.window)
                            return -2;
                    }
                    if (found >= 0)
                        for (const Base &b : bases)
                            if (b.window == size_t(found))
                                replaceWord(arg, b.text, b.word ? std::string("0ul") : std::string(b.text.find(".x") != std::string::npos || b.text.find(".y") != std::string::npos ? "0u" : "0ul"));
                    return found;
                };
                static const char *const types[] = {"uvec4", "uvec3", "uvec2", "uint", "vec4", "vec3", "vec2", "float"};
                // Pointer variables: "TPointer _n = TPointer(off);" ... "_n.value".
                for (const char *t : types) {
                    const std::string decl = std::string(t) + "Pointer ";
                    size_t p = 0;
                    while ((p = glsl.find(decl, p)) != std::string::npos) {
                        if (p > 0 && isIdent(glsl[p - 1])) {
                            p += decl.size();
                            continue;
                        }
                        size_t e = p + decl.size();
                        while (e < glsl.size() && isIdent(glsl[e]))
                            e++;
                        const std::string var = glsl.substr(p + decl.size(), e - p - decl.size());
                        const std::string ctor = " = " + std::string(t) + "Pointer(";
                        if (glsl.compare(e, ctor.size(), ctor) != 0) {
                            p = e;
                            continue;
                        }
                        const size_t open = e + ctor.size() - 1;
                        const size_t close = matchBracket(glsl, open);
                        if (close == std::string::npos) {
                            result.error = "unbalanced pointer constructor";
                            return result;
                        }
                        std::string arg = glsl.substr(open + 1, close - open - 1);
                        const int w = windowOf(arg);
                        if (w < 0) {
                            result.error = "a pointer whose base is not known: " + arg.substr(0, 80);
                            return result;
                        }
                        const std::string replacement = "uint64_t " + var + " = (" + arg + ")";
                        glsl.replace(p, close + 1 - p, replacement);
                        const std::string call = "dkLoad_" + std::string(t) + windows[size_t(w)].suffix + "(" + var + ")";
                        replaceAll(glsl, var + ".value", call);
                        windows[size_t(w)].loads.insert(t);
                        p += replacement.size();
                    }
                }
                // TPointer(off).value -> dkLoad_T<window>(off)
                for (const char *t : types) {
                    const std::string ctor = std::string(t) + "Pointer(";
                    size_t p = 0;
                    while ((p = glsl.find(ctor, p)) != std::string::npos) {
                        if (p > 0 && isIdent(glsl[p - 1])) {
                            p += ctor.size();
                            continue;
                        }
                        const size_t open = p + ctor.size() - 1;
                        const size_t close = matchBracket(glsl, open);
                        if (close == std::string::npos) {
                            result.error = "unbalanced pointer constructor";
                            return result;
                        }
                        if (glsl.compare(close + 1, 6, ".value") != 0) {
                            result.error = "a pointer used other than for a load";
                            return result;
                        }
                        std::string arg = glsl.substr(open + 1, close - open - 1);
                        const int w = windowOf(arg);
                        if (w < 0) {
                            result.error = "a pointer whose base is not known: " + arg.substr(0, 80);
                            return result;
                        }
                        const std::string call = "dkLoad_" + std::string(t) + windows[size_t(w)].suffix + "(" + arg + ")";
                        glsl.replace(p, close + 7 - p, call);
                        windows[size_t(w)].loads.insert(t);
                        p += call.size();
                    }
                }
                // 64-bit integers left (offsets) are 32-bit: the windows are under 4 GB.
                replaceWord(glsl, "uint64_t", "uint");
                {
                    std::string out;
                    for (size_t i = 0; i < glsl.size(); i++) {
                        if (i + 1 < glsl.size() && glsl[i] == 'u' && glsl[i + 1] == 'l' && i > 0 && std::isdigit(static_cast<unsigned char>(glsl[i - 1])) &&
                            (i + 2 >= glsl.size() || !isIdent(glsl[i + 2]))) {
                            out += 'u';
                            i++;
                            continue;
                        }
                        out += glsl[i];
                    }
                    glsl.swap(out);
                }
                // The pointer types' blocks.
                {
                    size_t p;
                    while ((p = glsl.find("layout(buffer_reference")) != std::string::npos) {
                        // A forward declaration ("... buffer uintPointer;") or a block ("... { ... };").
                        const size_t semicolon = glsl.find(';', p), brace = glsl.find('{', p);
                        const size_t blockEnd = semicolon < brace ? semicolon - 1 : glsl.find("};", p);
                        if (blockEnd == std::string::npos || semicolon == std::string::npos)
                            break;
                        size_t end = blockEnd + 2;
                        while (end < glsl.size() && glsl[end] == '\n')
                            end++;
                        glsl.erase(p, end - p);
                    }
                }
                // Push constant members of 64 bits are uvec2 now: the block is std140 with explicit offsets.
                std::string helpers;
                for (const Window &w : windows) {
                    helpers += "layout(binding = " + std::to_string(w.slot) + ", std430) readonly buffer DkWindow" + w.suffix + " { uint w[]; } dkWindow" + w.suffix + ";\n";
                    for (const std::string &ts : w.loads) {
                        const bool isFloat = ts[0] != 'u';
                        const int n = (ts == "uint" || ts == "float") ? 1 : ts.back() - '0';
                        std::string body = n == 1 ? std::string("dkWindow" + w.suffix + ".w[o >> 2u]")
                                                  : std::string(isFloat ? "uvec" : "uvec") + std::to_string(n) + "(";
                        if (n > 1) {
                            for (int i = 0; i < n; i++)
                                body += std::string(i ? ", " : "") + "dkWindow" + w.suffix + ".w[(o >> 2u) + " + std::to_string(i) + "u]";
                            body += ")";
                        }
                        if (isFloat)
                            body = (n == 1 ? std::string("uintBitsToFloat(") : std::string("uintBitsToFloat(")) + body + ")";
                        helpers += ts + " dkLoad_" + ts + w.suffix + "(uint o) { return " + body + "; }\n";
                    }
                    result.storageMask |= 1u << w.slot;
                }
                // Before the first function: the first line ending in ')' whose next line is '{' (blocks and structs
                // end their line with a name).
                size_t at = std::string::npos;
                for (size_t ls = 0; ls < glsl.size();) {
                    const size_t nl = glsl.find('\n', ls);
                    if (nl == std::string::npos)
                        break;
                    if (nl > ls && glsl[nl - 1] == ')' && glsl.compare(nl + 1, 2, "{\n") == 0 && glsl.compare(ls, 7, "layout(") != 0) {
                        at = ls;
                        break;
                    }
                    ls = nl + 1;
                }
                glsl.insert(at == std::string::npos ? glsl.size() : at, helpers);
            }

            // ---- GLSL.std.450 instructions SPIRV-Cross declares through GL_EXT_spirv_intrinsics (NMin, NMax, NClamp:
            // the operand that is not NaN), written out in GLSL
            {
                const std::string key = "spirv_instruction(set = \"GLSL.std.450\", id = ";
                size_t p;
                while ((p = glsl.find(key)) != std::string::npos) {
                    const size_t end = glsl.find(';', p);
                    if (end == std::string::npos)
                        break;
                    unsigned id = 0;
                    char type[32] = {}, name[64] = {};
                    if (sscanf(glsl.c_str() + p + key.size(), "%u) %31s %63[A-Za-z0-9_](", &id, type, name) != 3) {
                        result.error = "an intrinsic SPIRV-Cross wrote in an unknown form";
                        return result;
                    }
                    const std::string t = type, n = name;
                    std::string def;
                    // Maxwell's FMNMX is IEEE 754 minNum/maxNum: it returns the operand that is not NaN, which is what
                    // NMin/NMax/NClamp ask for, so the plain functions do (2026-10-27; written out with isnan, uam
                    // emitted 5-11% more code in the vertex shaders).
                    if (id == 79) // NMin
                        def = t + " " + n + "(" + t + " a, " + t + " b) { return min(a, b); }";
                    else if (id == 80) // NMax
                        def = t + " " + n + "(" + t + " a, " + t + " b) { return max(a, b); }";
                    else if (id == 81) // NClamp(x, lo, hi)
                        def = t + " " + n + "(" + t + " x, " + t + " lo, " + t + " hi) { return min(max(x, lo), hi); }";
                    else {
                        result.error = "GLSL.std.450 instruction " + std::to_string(id) + " through spirv_intrinsics";
                        return result;
                    }
                    glsl.replace(p, end + 1 - p, def);
                }
                removeLines(glsl, "#extension GL_EXT_spirv_intrinsics");
            }

            // ---- push constants: the uniform buffer at Deko3DPushUniformSlot (plume_deko3d keeps them there)
            {
                // (its binding decoration is set above; std430 is not a uniform block layout in GL)
                replaceAll(glsl, "layout(push_constant, binding = " + std::to_string(Deko3DPushUniformSlot) + ", std430)",
                           "layout(binding = " + std::to_string(Deko3DPushUniformSlot) + ", std140)");
                replaceAll(glsl, "layout(push_constant, ", "layout(");
            }

            // ---- vertex outputs behind plume_deko3d's constant 905 (Deko3DOptions::vsOutputs, 2026-10-27): each plain
            // "layout(location = N) out TYPE NAME;" is computed into a private copy and stored at the end of main only
            // when bit N of the constant is set (default: all). A pipeline whose pixel shader reads fewer of them gets a
            // variant that neither stores nor computes the rest; one without a pixel shader stores none.
            if (stage == Deko3DSpirvStage::Vertex) {
                struct Output {
                    unsigned location;
                    std::string type, name, decl;
                };
                std::vector<Output> outputs;
                const std::string key = "\nlayout(location = ";
                for (size_t p = glsl.find(key); p != std::string::npos; p = glsl.find(key, p + 1)) {
                    unsigned location = 0;
                    char type[32] = {}, name[128] = {};
                    int consumed = 0;
                    if (sscanf(glsl.c_str() + p + 1, "layout(location = %u) out %31s %127[A-Za-z0-9_];%n", &location, type, name,
                               &consumed) == 3 && consumed > 0 && location < 32)
                        outputs.push_back({location, type, name, glsl.substr(p + 1, size_t(consumed))});
                }
                const size_t mainAt = glsl.find("\nvoid main()\n{\n");
                size_t mainEnd = std::string::npos;
                if (mainAt != std::string::npos) {
                    int depth = 0;
                    for (size_t i = glsl.find('{', mainAt); i < glsl.size(); i++) {
                        if (glsl[i] == '{')
                            depth++;
                        else if (glsl[i] == '}' && --depth == 0) {
                            mainEnd = i;
                            break;
                        }
                    }
                }
                if (!outputs.empty() && mainEnd != std::string::npos) {
                    std::string body = glsl.substr(mainAt, mainEnd - mainAt);
                    // (main returns nothing: every "return;" in it stores first)
                    replaceAll(body, " return;", " { dkStoreOutputs(); return; }");
                    std::string store = "\n#ifndef SPIRV_CROSS_CONSTANT_ID_905\n#define SPIRV_CROSS_CONSTANT_ID_905 4294967295u\n#endif\n"
                                        "const uint dkVsOutputs = uint(SPIRV_CROSS_CONSTANT_ID_905);\n";
                    std::string fn = "void dkStoreOutputs()\n{\n";
                    for (const Output &o : outputs) {
                        store += o.type + " dkOut_" + o.name + ";\n";
                        fn += "    if ((dkVsOutputs & " + std::to_string(1u << o.location) + "u) != 0u)\n        " + o.name +
                              " = dkOut_" + o.name + ";\n";
                    }
                    fn += "}\n";
                    std::string head = glsl.substr(0, mainAt), tail = glsl.substr(mainEnd);
                    for (const Output &o : outputs) {
                        replaceWord(head, o.name, "dkOut_" + o.name);
                        replaceWord(body, o.name, "dkOut_" + o.name);
                        // the declaration keeps its name
                        replaceAll(head, "layout(location = " + std::to_string(o.location) + ") out " + o.type + " dkOut_" + o.name + ";",
                                   o.decl);
                    }
                    glsl = head + store + fn + body + "    dkStoreOutputs();\n" + tail;
                }
            }

            // ---- what uam's GL 4.6 frontend names differently or lacks
            replaceWord(glsl, "gl_VertexIndex", "gl_VertexID");
            if (glsl.find("gl_InstanceIndex") != std::string::npos) {
                replaceWord(glsl, "gl_InstanceIndex", "gl_InstanceID");
                result.notes.push_back("gl_InstanceIndex read as gl_InstanceID (no base instance)");
            }
            removeLines(glsl, "#extension GL_EXT_buffer_reference");
            removeLines(glsl, "#extension GL_EXT_samplerless_texture_functions");
            removeLines(glsl, "#extension GL_ARB_gpu_shader_int64");
            removeLines(glsl, "#extension GL_EXT_shader_explicit_arithmetic_types_int64");
            if (glsl.find("int64_t") != std::string::npos) {
                result.error = "64-bit integers left after the address window rewrite";
                result.glsl = std::move(glsl);  // for the PC tool's report
                return result;
            }
            result.glsl = std::move(glsl);
            result.ok = true;
        }
        catch (const std::exception &e) {
            result.error = std::string("SPIRV-Cross: ") + e.what();
        }
        return result;
    }

    // ---- translation records

    namespace {
        uint64_t fnvMix(uint64_t h, const void *data, size_t size) {
            const uint8_t *p = static_cast<const uint8_t *>(data);
            for (size_t i = 0; i < size; i++) {
                h ^= p[i];
                h *= 0x100000001b3ull;
            }
            return h;
        }
        constexpr uint32_t kRecordMagic = 0x52544B44;  // "DKTR"
    }

    uint64_t Deko3DGlslKey(const std::string &glsl) {
        return fnvMix(0xcbf29ce484222325ull, glsl.data(), glsl.size());
    }

    uint64_t Deko3DTranslationKey(const uint32_t *words, size_t wordCount, const Deko3DSpirvLayout &layout,
                                  const Deko3DSpirvOptions &options) {
        uint64_t h = 0x84222325cbf29ce4ull;
        const uint32_t version = Deko3DTranslatorVersion;
        h = fnvMix(h, &version, sizeof(version));
        h = fnvMix(h, words, wordCount * sizeof(uint32_t));
        for (const Deko3DSpirvLayout::Range &r : layout.ranges) {
            const uint32_t v[6] = {r.set, r.binding, r.count, r.descriptorIndex, r.slot, uint32_t(r.kind)};
            h = fnvMix(h, v, sizeof(v));
        }
        h = fnvMix(h, options.addressWindowMember.data(), options.addressWindowMember.size());
        const uint32_t window[2] = {options.addressWindowSet, options.addressWindowBinding};
        h = fnvMix(h, window, sizeof(window));
        // Never equal to a GLSL key in practice; the salt keeps the two kinds of entry apart in one cache.
        return h ^ 0x5452414E534C4154ull;
    }

    void Deko3DVaryingMasks(const std::string &glsl, uint32_t &inputMask, uint32_t &outputMask) {
        inputMask = outputMask = 0;
        const char *key = "\nlayout(location = ";
        size_t p = 0;
        while ((p = glsl.find(key, p)) != std::string::npos) {
            p += strlen(key);
            char *end = nullptr;
            const unsigned long loc = strtoul(glsl.c_str() + p, &end, 10);
            if (loc < 32 && end && strncmp(end, ") in ", 5) == 0)
                inputMask |= 1u << loc;
            else if (loc < 32 && end && strncmp(end, ") out ", 6) == 0)
                outputMask |= 1u << loc;
        }
    }

    std::vector<uint8_t> Deko3DWriteTranslation(const Deko3DSpirvResult &r) {
        std::vector<uint32_t> w;
        const uint64_t key = Deko3DGlslKey(r.glsl);
        // The specialization constants' defaults, as plume_deko3d reads them from the text.
        std::vector<std::pair<uint32_t, uint32_t>> spec;
        const std::string define = "#define SPIRV_CROSS_CONSTANT_ID_";
        for (size_t p = r.glsl.find(define); p != std::string::npos; p = r.glsl.find(define, p + 1)) {
            char *end = nullptr;
            const unsigned long id = strtoul(r.glsl.c_str() + p + define.size(), &end, 10);
            const unsigned long value = strtoul(end, nullptr, 0);
            spec.push_back({uint32_t(id), uint32_t(value)});
        }
        w.push_back(kRecordMagic);
        w.push_back(Deko3DTranslatorVersion);
        w.push_back(uint32_t(key));
        w.push_back(uint32_t(key >> 32));
        w.push_back(r.uniformMask);
        w.push_back(r.storageMask);
        w.push_back(uint32_t(r.textures.size()));
        w.push_back(uint32_t(r.pushWindows.size()));
        w.push_back(uint32_t(r.vertexInputs.size()));
        w.push_back(uint32_t(spec.size()));
        uint32_t inputMask = 0, outputMask = 0;
        Deko3DVaryingMasks(r.glsl, inputMask, outputMask);
        w.push_back(inputMask);
        w.push_back(outputMask);
        for (const Deko3DSpirvTexture &t : r.textures) {
            const uint32_t v[8] = {t.imageSet, t.imageIndex, uint32_t(t.samplerFrom), t.samplerSet, t.samplerIndex,
                                   t.uniformSlot, t.uniformOffset, 0};
            w.insert(w.end(), v, v + 8);
        }
        for (const auto &pw : r.pushWindows) {
            w.push_back(pw.slot);
            w.push_back(pw.pushOffset);
        }
        w.insert(w.end(), r.vertexInputs.begin(), r.vertexInputs.end());
        for (const auto &[id, value] : spec) {
            w.push_back(id);
            w.push_back(value);
        }
        std::vector<uint8_t> bytes(w.size() * 4);
        memcpy(bytes.data(), w.data(), bytes.size());
        return bytes;
    }

    bool Deko3DReadTranslation(const std::vector<uint8_t> &bytes, Deko3DTranslationRecord &out) {
        if (bytes.size() < 48 || bytes.size() % 4)
            return false;
        std::vector<uint32_t> w(bytes.size() / 4);
        memcpy(w.data(), bytes.data(), bytes.size());
        if (w[0] != kRecordMagic || w[1] != Deko3DTranslatorVersion)
            return false;
        const size_t textures = w[6], pushes = w[7], inputs = w[8], specs = w[9];
        if (12 + textures * 8 + pushes * 2 + inputs + specs * 2 != w.size() || textures > 32 || pushes > Deko3DPushWindowCount)
            return false;
        out = {};
        out.glslKey = uint64_t(w[2]) | (uint64_t(w[3]) << 32);
        out.uniformMask = w[4];
        out.storageMask = w[5];
        out.inputMask = w[10];
        out.outputMask = w[11];
        size_t i = 12;
        for (size_t k = 0; k < textures; k++, i += 8) {
            Deko3DSpirvTexture t;
            t.imageSet = w[i];
            t.imageIndex = w[i + 1];
            t.samplerFrom = Deko3DSpirvTexture::SamplerFrom(w[i + 2]);
            t.samplerSet = w[i + 3];
            t.samplerIndex = w[i + 4];
            t.uniformSlot = w[i + 5];
            t.uniformOffset = w[i + 6];
            out.textures.push_back(t);
        }
        for (size_t k = 0; k < pushes; k++, i += 2)
            out.pushWindows.push_back({w[i], w[i + 1]});
        for (size_t k = 0; k < inputs; k++, i++)
            out.vertexInputs.push_back(w[i]);
        for (size_t k = 0; k < specs; k++, i += 2)
            out.specDefaults.push_back({w[i], w[i + 1]});
        return true;
    }
}
