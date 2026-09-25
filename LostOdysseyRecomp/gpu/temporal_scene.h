#pragma once
#include "temporal_math.h"
#include <bit>
#include <cstdint>

namespace gpu::temporal
{
// Verified position-transform slots from scene captures, including all three
// tire layers in Map3 frame 24389 and battle terrain/objects/skinned layers in
// frames 2871 and 26786, Map16 ground/material passes in frame 18420, and
// static/skinned scene paths in captures 17624-17626 and 21480-21482.
// Also includes source-reviewed player-feedback paths (2026-09-25), plus the
// separately marked, user-accepted screen-sampling batch.
// A slot alone never
// authorizes jitter: renderer also checks viewport/VTE, ordered scene allocation,
// and exact unmodified camera bits. Fullscreen/postprocess/UI shaders are absent.
inline int PositionVPSlot(uint64_t shader) {
    switch(shader) {
    case 0x702c643defe73320ull:case 0x97f07e5d73418e64ull:case 0x99c2b4b0960a9ccdull:return 0;
    // f13429-f13431: alpha-tested depth shares ff9da/a27a geometry; c4-c7
    // feed only position, while its object UV and alpha color remain unchanged.
    case 0x8d3c80b318235b22ull:
    // f5914: stride-56 static depth companion of 799c/eeae; c4-c7 position only.
    case 0x52e4405f97159d2full:
    // f16385-f16387: alpha-tested depth; UV/color outputs are independent of VP.
    case 0xfe3efe042c311110ull:
    // f11745: static depth companion of a936; c4-c7 position only.
    case 0xb2eaed9ab75471f9ull:
    case 0xb030ab4e17a20783ull:case 0xf1b330b3ceea9a3bull:case 0xf7fd88506d704a3dull:return 4;
    case 0x03184cec350bc14eull:case 0x3621e6e696f914c5ull:case 0x4053f2a21dbb92ddull:
    case 0xa27a7234977e0d4aull:case 0xbfe5f796efa9ae95ull:case 0xc13cdd857c57fed9ull:case 0xf4577672c6ee5dd9ull:
    case 0xff9da3984ce8d094ull:case 0x8b5577db3ced3327ull:case 0x400df7c5a60819f5ull:case 0x08dcef32bd434f8cull:
    case 0xfcbb75d0feb3fcb9ull:
    case 0xf63bf6e0d52519a8ull:case 0xc1e8406a5c2ab764ull:case 0x8fe60c14bb586399ull:
    case 0x9f2ddb46a977510bull:case 0x6cbe49f383e54f69ull:
    // Live 4K telemetry + microcode: c7-c10 feed oPos; same camera/depth allocation.
    case 0x0b786a899598ce18ull:
    // Capture 2813-2815: exact c7 camera and position output, 48 draws/frame.
    case 0xe8ec18f1d3eac4dfull:case 0x1ea46291cb1c7298ull:
    // f13429-f13431: material/light companions consume the same main camera.
    // Extra fetch94 carries lighting/scalar data, not position; c7-c10 feed
    // only clip position/copy. Keep independent lighting c4-c6 and object UV.
    case 0x3eb16ad927f44289ull:case 0x83b23507725f85bfull:case 0x6742ec1abe49589eull:
    case 0x0f2b89c7eb1c409eull:case 0xfecf2f9d9bef2702ull:case 0x2a7867b5eed37f8aull:
    case 0x7d403bdef896a97full:case 0x45ed0948b6b701a7ull:
    // f1653: matched depth/material/light geometry and camera; c7-c10 only
    // feed clip position/varyings. World-space lighting uses separate constants.
    case 0x3c86f4a89d220ee8ull:case 0xf3b9f20b3d3a62d5ull:case 0xe7b38eb08c70e5e1ull:
    // f5997-f5999: static material/light companions of b030 depth geometry.
    // c7-c10 feed position and clip copies; world lighting uses c4-c6/c11+.
    case 0x5f0bd44481510be0ull:case 0x310850b1446f0e41ull:case 0xe242d31a3f1acdc4ull:
    // f5912-f5914: clip-copy light pass; c4-c6/c11-c12 lighting stays independent.
    case 0x97b5d441419b5533ull:
    // f16385-f16387: matching alpha-tested light layer, same clip/UV separation.
    case 0x1474db97dfc0afadull:
    // f11745: palace static material & additive lighting accumulation passes; c7-c10 position only.
    case 0x0743de0d691de4c9ull:case 0x25a13c85314d2c4bull:case 0xa2eef6788cd4d60bull:
    case 0xa936005072d293ddull:case 0xc0e0f4c574750856ull:case 0xf91227f682ce9042ull:
    // f2358: stairs / save point static scene & lighting passes; c7-c10 position only.
    case 0x69e9adcf2e1b6887ull:case 0x6a8c2c78737dc94cull:case 0xa20d6099a44e2cd5ull:
    // f2548-f2550: static material companion of f7fd depth; c7-c10
    // feed only position/o4 clip copy. Separate fetch94 lighting is unchanged.
    case 0xa027ab99fa3e3b0dull:return 7;
    case 0x1da1ddc75da8e994ull:case 0x22557143e0f243ddull:case 0x4c87bb5b986defc8ull:case 0xa6c8c11c6dd07144ull:
    case 0xe8c0d438c690c784ull:case 0x576d669b2ad3c898ull:
    case 0x188061ace0615678ull:case 0xdc7f83af67c53ba1ull:case 0x68014a17a2a9a4bdull:
    // Same capture, f7fd depth geometry: c7 is an independent UV transform;
    // c8-c11 feed only position/clip, preserving UV and lighting constants.
    case 0x5d98f5e3bcc3f4aeull:case 0xa9dd56801863b1f0ull:
    // f5914: stride-56 material/light pair, c7 UV and c12+ lighting untouched.
    case 0x799c02c8b6582bfeull:case 0xeeae6424413228d6ull:
    // f2358: matched static companion; c8-c11 position only.
    // f1991/f2163 battle terrain: c8-c11 clip position and copy; c7 UV untouched.
    case 0x8d9770d1bd8ba0faull:
    // Battle companion: c8-c11 clip position and o2 copy; non-VP basis untouched.
    case 0xf6f074ce5d305448ull:
    case 0x6761469677f921c6ull:
    // f2548-f2550: matched static depth geometry; c8-c11 position/o2 only,
    // leaving the independent c7 UV transform and c12 lighting untouched.
    case 0xff769ec7b88e575full:
    // f6131-f6133: late additive floor lighting matches the f7fd depth and
    // ff769 material geometry. c8-c11 feed oPos and o5; PS 4013372b6413788f
    // samples the current scene light resolve through o5.xy/w, so the lookup
    // must follow the same raster jitter. c7 material UV and PS banks stay intact.
    case 0x2078ccaa70d44732ull:return 8;
    // f5446-f5448 enemy skinning: c230-c233 post-skin clip position only.
    case 0x4bd8985d84983b83ull:
    case 0x31bde3e2770db187ull:case 0x7e8492365edcf556ull:return 230;
    case 0x118a37c0d32c0477ull:case 0x3148f81d65d3b5f4ull:case 0xb7557072899a63a1ull:case 0xc84ca5209e98e743ull:
    case 0x0eb223d33f8e8e0cull:case 0x1e9017d2b296f480ull:
    case 0x87a76ceaf1eaec11ull:case 0x81bc335604d04e8bull:return 233;
    // 2026-09-25 feedback batch: exact VS and every observed PS reviewed.
    // These VP paths have no observed clip-XY sampling consumer or known
    // finite-camera mismatch. Detailed identities/holds and synthetic CPU
    // validation scope: docs/notes/jitter-coverage-2026-09-25.md.
    case 0x2d458def192151acull:case 0x6b757ded853a7fc5ull:case 0x6d3d954bb6d86bc1ull:return 0;
    case 0x0fa0396a659f8da5ull:return 1;
    case 0x2b36b5ca7a88912eull:case 0xac81dd5f6ed83c3eull:return 4;
    case 0x03a4238064e6c836ull:case 0x09f67586057d7083ull:case 0x1a2f72d1dce268bdull:
    case 0x22993b734c035ae6ull:case 0x276b01d4190fdc00ull:case 0x3638b6b020b068fcull:
    case 0x3bb0b196f11e64e5ull:case 0x4ce42af298a9baefull:case 0x59006824a7515704ull:
    case 0x6508c631689c4ffaull:case 0x6976f82de60cb915ull:case 0x753287173badc7d1ull:
    case 0x7f2f709e14788599ull:case 0x8060e3f548febc94ull:case 0x8261a0b7daeac888ull:
    case 0x8b986c8d09eab4e4ull:case 0x8f6ce5a4f714294aull:case 0xa0a7fc243e60b248ull:
    case 0xa6314f321efa4d14ull:case 0xae45651b20b50163ull:case 0xb0b143a646a921a7ull:
    case 0xb14ecb62fe79be01ull:case 0xc01a72e0eb5e026aull:case 0xc560d140940528bcull:
    case 0xd6ead6f46d70b19aull:case 0xe8747802c970e598ull:case 0xf5ca0812e57cd43cull:return 7;
    case 0x0e5a12f70e7cb5beull:case 0xf3838aa008bc39d8ull:return 8;
    // 2026-09-25 screen-sampling batch: source-reviewed VP slots and user
    // scene acceptance; this does not establish every producer/consumer path.
    // See docs/notes/jitter-screen-batch-2026-09-25.md.
    case 0x02d8d17463de32cdull:case 0x1b99a8476ec606b1ull:case 0x21b1d8c82e81fa02ull:
    case 0x22225401fc8ea621ull:case 0x23041a74b20c4332ull:case 0x24ac4f2d476bf078ull:
    case 0x29c6ee5dc3e841f5ull:case 0x30c7df4b111290faull:case 0x32f09dcd84b93237ull:
    case 0x3359fe19a89b5e42ull:case 0x35e5f4651ffc54a2ull:case 0x3a818cf89cbff74aull:
    case 0x3ee6416e9416bfc4ull:case 0x3f522a748751d16aull:case 0x418b5eb1b1726eb9ull:
    case 0x4523afe2cc9fe50dull:case 0x48b893348dc956c2ull:case 0x4a25a2f1ae004a7full:
    case 0x5d51dfb03c579740ull:case 0x5ef85743ad497c11ull:case 0x795375250cf5c245ull:
    case 0x7baa15e8628a2d31ull:case 0x840bd920f0539521ull:case 0x912f560f1b64450bull:
    case 0x951ceb61bbce7190ull:case 0x9a771ff60d9fd73full:case 0x9f41368e6ee52741ull:
    case 0x9fa6242c88d0bd9dull:case 0xb8f5cf595e31578bull:case 0xbc5225aaa002037cull:
    case 0xbf8d4de60c64f55dull:case 0xc66b9e0de0e9331cull:case 0xd1c61a2a7b0049d5ull:
    case 0xd24619b1a13523dcull:case 0xf25929da09e30a5cull:return 7;
    case 0x490e7455d880426cull:case 0x8d32020847a4f6b2ull:case 0xb60fba087b51eb53ull:
    case 0xd34f09f9fcce78a1ull:case 0xe0624eec8073b957ull:return 8;
    default:return -1;
    }
}
// e810 has eleven observed PS partners. Only this independently reviewed pair
// may use the slot-7 path, and only with a constant single-texel screen sample.
// Keep it out of the VS-wide table so other consumers cannot self-anchor it.
inline int DrawPositionVPSlot(uint64_t vs, uint64_t ps, bool constantScreenSample = false) {
    if (vs == 0xe810cfacc107fd3cull && ps == 0xfe31f3d6588fde95ull && constantScreenSample) return 7;
    return PositionVPSlot(vs);
}

// Ordered observations from one renderer frame. This associates selected draw
// constants with their actual depth allocation and a later pre-UI scene copy;
// it does not discover shaders, object motion, jitter or camera cuts.
struct SceneAnchor
{
    std::array<uint32_t, 16> vpBits{};
    Viewport viewport{};
    uint64_t depthAllocation = 0;
};
struct SceneResolve
{
    uint64_t frame = 0, ordinal = 0;
    uint32_t address = 0, format = 0, width = 0, height = 0;
    bool fullExtent = false;
};
class SceneObservation
{
public:
    enum class Rejection { None, InvalidCamera, AmbiguousCamera, PartialDepth, RepeatedDepth, InvalidColor, RepeatedColor };
    void Reset(uint64_t frame) { *this = SceneObservation(); frame_ = frame; }
    void ObserveCamera(const SceneAnchor& anchor)
    {
        ++draws_;
        if (draws_ == 1)
        {
            anchor_ = anchor;
            Matrix vp{};
            for (size_t i = 0; i < vp.size(); ++i) vp[i] = std::bit_cast<float>(anchor.vpBits[i]);
            const auto& v = anchor.viewport;
            if (!anchor.depthAllocation || v.x != 0 || v.y != 0 ||
                !Camera::Create(vp, v)) Reject(Rejection::InvalidCamera);
        }
        else if (anchor.vpBits != anchor_.vpBits || anchor.depthAllocation != anchor_.depthAllocation ||
            !SameViewport(anchor.viewport, anchor_.viewport)) Reject(Rejection::AmbiguousCamera);
    }
    void ObserveDepth(uint64_t sourceAllocation, const SceneResolve& resolve)
    {
        // Shadow/other view resolves cannot substitute for the selected allocation.
        if (!draws_ || sourceAllocation != anchor_.depthAllocation) return;
        if (depth_.ordinal) { Reject(Rejection::RepeatedDepth); return; }
        if (resolve.frame != frame_ || !resolve.ordinal || !resolve.fullExtent ||
            resolve.width != anchor_.viewport.width || resolve.height != anchor_.viewport.height)
        { Reject(Rejection::PartialDepth); return; }
        depth_ = resolve;
    }
    bool ObserveColor(const SceneResolve& resolve)
    {
        ++copies_;
        if (copies_ > 1) Reject(Rejection::RepeatedColor);
        if (!depth_.ordinal || resolve.frame != frame_ || resolve.ordinal <= depth_.ordinal ||
            !resolve.fullExtent || resolve.width != depth_.width || resolve.height != depth_.height)
            Reject(Rejection::InvalidColor);
        color_ = resolve;
        return Ready();
    }
    bool Ready() const { return rejection_ == Rejection::None && draws_ && depth_.ordinal && color_.ordinal; }
    uint64_t Frame() const { return frame_; }
    uint32_t Draws() const { return draws_; }
    uint32_t Copies() const { return copies_; }
    Rejection Reason() const { return rejection_; }
    const SceneAnchor& Anchor() const { return anchor_; }
    const SceneResolve& Depth() const { return depth_; }
    const SceneResolve& Color() const { return color_; }
private:
    static bool SameViewport(const Viewport& a, const Viewport& b)
    {
        return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height &&
            a.ndcYSign == b.ndcYSign && a.halfPixelNdcX == b.halfPixelNdcX && a.halfPixelNdcY == b.halfPixelNdcY;
    }
    void Reject(Rejection why) { if (rejection_ == Rejection::None) rejection_ = why; }
    uint64_t frame_ = 0;
    uint32_t draws_ = 0, copies_ = 0;
    Rejection rejection_ = Rejection::None;
    SceneAnchor anchor_{};
    SceneResolve depth_{}, color_{};
};
}
