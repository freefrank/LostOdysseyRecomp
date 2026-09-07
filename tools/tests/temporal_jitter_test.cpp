#include <gpu/temporal_jitter.h>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace gpu::temporal;
using Constants = std::array<uint32_t, 256 * 4>;
static unsigned checks = 0;
static void Check(bool result, const char* message)
{
    ++checks;
    if (!result) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static double F(uint32_t bits) { return std::bit_cast<float>(bits); }
static Matrix MatrixAt(const Constants& values, int slot)
{
    Matrix matrix{};
    for (unsigned i = 0; i < 16; ++i) matrix[i] = F(values[slot * 4 + i]);
    return matrix;
}

// Independent transcription of d55a20d004031279's depth-UV and reconstruction
// instructions from f14774. Preserve its register/component permutations here
// so a wrong host compensation component or sign cannot satisfy the reference.
struct ShadowResult { std::array<double, 2> uv; Vector projected; };
static ShadowResult Shadow(const Constants& c, double clipX, double clipY, double clipW, double sampledDepth)
{
    const double x = clipX / clipW, y = clipY / clipW;
    ShadowResult result{{x * F(c[0]) + F(c[3]), y * F(c[1]) + F(c[2])}, {}};
    const double linear = 1.0 / (sampledDepth * F(c[6]) - F(c[7]));
    // r0 = depth*c4.zwxy + c5.zwxy.
    Vector r0{linear * F(c[18]) + F(c[22]), linear * F(c[19]) + F(c[23]),
        linear * F(c[16]) + F(c[20]), linear * F(c[17]) + F(c[21])};
    // r0 = (ndcY*depth)*c3.wzxy + r0.yxzw.
    r0 = {y * linear * F(c[15]) + r0[1], y * linear * F(c[14]) + r0[0],
        y * linear * F(c[12]) + r0[2], y * linear * F(c[13]) + r0[3]};
    // r2 = (ndcX*depth)*c2.zwxy + r0.yxzw.
    result.projected = {x * linear * F(c[10]) + r0[1], x * linear * F(c[11]) + r0[0],
        x * linear * F(c[8]) + r0[2], x * linear * F(c[9]) + r0[3]};
    return result;
}

using Float4 = std::array<float,4>;
static Float4 C(const Constants& c, unsigned slot)
{
    return {float(F(c[slot*4])),float(F(c[slot*4+1])),float(F(c[slot*4+2])),float(F(c[slot*4+3]))};
}
static Float4 S(const Float4& value, const char* order)
{
    Float4 result{};
    for (unsigned i=0;i<4;++i) result[i]=value[order[i]=='x'?0:order[i]=='y'?1:order[i]=='z'?2:3];
    return result;
}
static Float4 Mul(float scalar, const Float4& value)
{
    return {scalar*value[0],scalar*value[1],scalar*value[2],scalar*value[3]};
}
static Float4 Mad(float scalar, const Float4& value, const Float4& add)
{
    return {scalar*value[0]+add[0],scalar*value[1]+add[1],scalar*value[2]+add[2],scalar*value[3]+add[3]};
}

// Independently transcribed POSITION paths, including world-space swizzles and
// operation order, from the three f24389 shaders. These do not call Transform
// or the host jitter helper. Synthetic local vertices use both captured tire
// world transforms; this checks shader arithmetic, not raster coverage.
static Float4 TireDepthB030(const Constants& c, Float4 r3)
{
    r3[3]=1;
    auto r2=Mul(r3[3],S(C(c,3),"xywz"));
    r2=Mad(r3[2],S(C(c,2),"wxzy"),S(r2,"zxwy"));
    r2=Mad(r3[1],S(C(c,1),"zwyx"),S(r2,"zxwy"));
    r3=Mad(r3[0],S(C(c,0),"yzxw"),S(r2,"zxwy"));
    r2=Mul(r3[3],C(c,7));
    r2=Mad(r3[1],C(c,6),r2);
    r2=Mad(r3[0],C(c,5),r2);
    return Mad(r3[2],C(c,4),r2);
}
static Float4 TireMaterialFf9(const Constants& c, Float4 r6)
{
    r6[3]=1;
    auto r3=Mul(r6[3],S(C(c,3),"xywz"));
    r3=Mad(r6[2],S(C(c,2),"wxzy"),S(r3,"zxwy"));
    r3=Mad(r6[1],S(C(c,1),"zwyx"),S(r3,"zxwy"));
    r6=Mad(r6[0],S(C(c,0),"yzxw"),S(r3,"zxwy"));
    r3=Mul(r6[3],C(c,10));
    r3=Mad(r6[1],C(c,9),r3);
    r3=Mad(r6[0],C(c,8),r3);
    return Mad(r6[2],C(c,7),r3); // oPos and projected-coordinate o1
}
static Float4 TireLightA27(const Constants& c, Float4 r3)
{
    r3[3]=1;
    auto r0=Mul(r3[3],S(C(c,3),"xywz"));
    r0=Mad(r3[2],S(C(c,2),"wxzy"),S(r0,"zxwy"));
    r0=Mad(r3[1],S(C(c,1),"zwyx"),S(r0,"zxwy"));
    r3=Mad(r3[0],S(C(c,0),"yzxw"),S(r0,"zxwy"));
    r0=Mul(r3[3],C(c,10));
    r0=Mad(r3[1],C(c,9),r0);
    r0=Mad(r3[0],C(c,8),r0);
    return Mad(r3[2],C(c,7),r0); // oPos and shadow-mask coordinate o4
}
static Float4 CharacterMaterial118a(const Constants& c, Float4 r9)
{
    auto r2=Mul(r9[2],C(c,236));
    r2=Mad(r9[0],C(c,235),r2);
    r2=Mad(r9[1],C(c,234),r2);
    return Mad(r9[3],C(c,233),r2); // r2 retained through oPos/o1
}
static Float4 CharacterLight3148(const Constants& c, Float4 r9)
{
    auto r2=Mul(r9[2],C(c,236));
    r2=Mad(r9[0],C(c,235),r2);
    r2=Mad(r9[1],C(c,234),r2);
    return Mad(r9[3],C(c,233),r2); // r2 retained through oPos/o4
}
static void TireMaterialCoverage()
{
    const std::array<uint32_t,16> vp{
        3198114587u,3198176441u,3212458845u,3212475260u,3218741182u,1030373729u,1043781340u,1043793334u,
        0u,1078199118u,3184752743u,3184766616u,1175480082u,1159645321u,1160710128u,1160762570u};
    const std::array<std::array<uint32_t,16>,2> worlds{{
        {0x3f278343,0,0,0,0,0x3f26b4c4,0xbd835a5c,0,0,0x3d835a5c,0x3f26b4c4,0,0x451e8000,0x459d8000,0xc4378000,0x3f800000},
        {0x3f21018b,0xbd815b1c,0xbe2d419f,0,0x3c082306,0x3f1f5bd1,0xbe4e54e2,0,0x3e38bd41,0x3e441df4,0x3f195fc1,0,0x451f7fff,0x459f23a3,0xc435fd34,0x3f800000}}};
    double legacySeparation=0;
    for (const auto& world : worlds)
        for (const auto extent : {Viewport{0,0,1280,720},Viewport{0,0,2560,1440},Viewport{0,0,3840,2160}})
            for (uint64_t frame=0;frame<32;++frame)
            {
                SceneAnchor anchor{vp,extent,11};
                const auto bank=[&](unsigned slot) {
                    Constants c{};std::copy(world.begin(),world.end(),c.begin());
                    std::copy(vp.begin(),vp.end(),c.begin()+slot*4);return c;
                };
                auto base=bank(4),material=bank(7),light=bank(7),character=bank(233),characterLight=bank(233);
                const auto unjitteredMaterial=material;
                Constants ps{};
                const auto a=ApplyDrawJitter(0xb030ab4e17a20783ull,0,frame,true,true,&anchor,11,extent,base.data(),ps.data());
                const auto b=ApplyDrawJitter(0xff9da3984ce8d094ull,0xcca4df9624e783b4ull,frame,true,true,&anchor,11,extent,material.data(),ps.data());
                const auto c=ApplyDrawJitter(0xa27a7234977e0d4aull,0x62f70bc41d61c73bull,frame,true,true,&anchor,11,extent,light.data(),ps.data());
                Check(a.applied && b.applied && c.applied,"three actual tire shader uploads all apply jitter");
                for (const auto vertex : {Float4{-60,-30,5,1},Float4{80,-20,10,1},Float4{0,90,12,1}})
                {
                    const auto depthClip=TireDepthB030(base,vertex);
                    const auto materialClip=TireMaterialFf9(material,vertex);
                    const auto lightClip=TireLightA27(light,vertex);
                    const auto legacyClip=TireMaterialFf9(unjitteredMaterial,vertex);
                    Check(depthClip==materialClip && materialClip==lightClip,"three independently evaluated HLSL position paths agree exactly");
                    Check(materialClip[2]==legacyClip[2] && materialClip[3]==legacyClip[3],"material jitter preserves depth mapping and W");
                    legacySeparation=std::max(legacySeparation,std::abs(double(materialClip[0])/materialClip[3]-double(legacyClip[0])/legacyClip[3])*extent.width*.5);
                    legacySeparation=std::max(legacySeparation,std::abs(double(materialClip[1])/materialClip[3]-double(legacyClip[1])/legacyClip[3])*extent.height*.5);
                }
                const auto d=ApplyDrawJitter(0x118a37c0d32c0477ull,0xbab55ad9437fe925ull,frame,true,true,&anchor,11,extent,character.data(),ps.data());
                const auto e=ApplyDrawJitter(0x3148f81d65d3b5f4ull,0x5fd93b4b34d539beull,frame,true,true,&anchor,11,extent,characterLight.data(),ps.data());
                Check(d.applied && e.applied && CharacterMaterial118a(character,{4957,-638,1,3194})==
                    CharacterLight3148(characterLight,{4957,-638,1,3194}),"character material and lighting projection paths agree");
                auto atlas=bank(7);const auto unchangedAtlas=atlas;
                const auto rejected=ApplyDrawJitter(0xff9da3984ce8d094ull,0xcca4df9624e783b4ull,frame,true,false,&anchor,77,{5,5,60,60},atlas.data(),ps.data());
                Check(!rejected.applied && atlas==unchangedAtlas,"material recognition does not authorize a light-space atlas draw");
            }
    Check(legacySeparation>.3,"three-layer fixture exposes original unjittered material separation");
    std::printf("Tire material coverage: 32 phases, two captured world transforms, three sizes; legacy separation %.6f pixels\n",legacySeparation);
}

int main()
{
    TireMaterialCoverage();
    // Unmodified camera and shadow PS c0..c5 from render f14774, shadow draw154.
    const std::array<uint32_t, 16> capturedVp{
        0xbe9e047a,0xbeb76758,0xbf79e276,0xbf7a227f,0xbfda2754,0x3d84d8c1,0x3e350064,0x3e352ec6,
        0x00000000,0x4043afb1,0xbdf1e1cb,0xbdf21fc7,0x46100af5,0x451c3fed,0x4521a750,0x452270bd};
    const std::array<uint32_t, 24> capturedPs{
        0x3f000000,0xbf000000,0x3f002d83,0x3f00199a,0xc61c1883,0x3a831200,0x3dcd0148,0x38d1ec24,
        0x3925604f,0x39bedf10,0xbc30d092,0x00000000,0xb994c12d,0xb7efb535,0xbb742662,0x00000000,
        0x39b0bda2,0xba41bd88,0xbc6ef0b3,0x00000000,0xbf00035e,0x3fb69166,0x41decc11,0x3f800000};
    Constants originalPs{};
    std::copy(capturedPs.begin(), capturedPs.end(), originalPs.begin());
    SceneAnchor anchor{capturedVp, {0, 0, 2560, 1440}, 37};
    const auto bank = [&](int slot) {
        Constants values{};
        std::copy(capturedVp.begin(), capturedVp.end(), values.begin() + slot * 4);
        return values;
    };
    const float scale[]{1,1,-1}, offset[]{0,0,1};
    const Viewport guest{0,0,1280,720};
    Check(IsJitterViewport(guest, 0x43f, scale, offset), "standard guest camera accepted");
    auto unusual = guest; unusual.width = 1728;
    Check(!IsJitterViewport(unusual, 0x43f, scale, offset), "shadow atlas viewport rejected");
    unusual = guest; unusual.x = 1;
    Check(!IsJitterViewport(unusual, 0x43f, scale, offset), "partial scene viewport rejected");
    Check(!IsJitterViewport(guest, 0x43e, scale, offset), "screen-space VTE rejected");
    const float flipped[]{1,-1,-1}, forward[]{1,1,1}, biased[]{0,0,1.00001f};
    Check(!IsJitterViewport(guest, 0x43f, flipped, offset), "flipped camera rejected");
    Check(!IsJitterViewport(guest, 0x43f, forward, offset), "non-reversed camera rejected");
    Check(!IsJitterViewport(guest, 0x43f, scale, biased), "noncanonical guest depth range rejected");
    // The renderer classifies the guest range first; applying the independent
    // lighting Z offset afterward must neither veto nor modify XY jitter.
    const bool biasedLayerEligible = IsJitterViewport(guest, 0x43f, scale, offset);
    float hostDepthOffset = offset[2]; hostDepthOffset += .00001f;
    Check(biasedLayerEligible && hostDepthOffset == biased[2], "lighting bias preserved outside guest camera classification");

    double legacyError = 0, correctedError = 0;
    for (const auto extent : {Viewport{0,0,1280,720}, Viewport{0,0,1920,1080},
        Viewport{0,0,2560,1440}, Viewport{0,0,3840,2160}})
    {
        anchor.viewport = extent;
        for (uint64_t frame = 0; frame < 32; ++frame)
        {
            auto base = bank(4), light = bank(7), character = bank(233), shadow = bank(0);
            auto ps = originalPs;
            const auto baseResult = ApplyDrawJitter(0xb030ab4e17a20783ull, 0, frame,
                true, true, &anchor, 37, extent, base.data(), ps.data());
            const auto lightResult = ApplyDrawJitter(0xa27a7234977e0d4aull, 0x62f70bc41d61c73bull, frame,
                true, biasedLayerEligible, &anchor, 37, extent, light.data(), ps.data());
            const auto characterResult = ApplyDrawJitter(0x3148f81d65d3b5f4ull, 0x5fd93b4b34d539beull, frame,
                true, true, &anchor, 37, extent, character.data(), ps.data());
            Check(baseResult.applied && lightResult.applied && characterResult.applied, "verified position transforms jitter together");
            Check(std::equal(base.begin()+16, base.begin()+32, light.begin()+28) &&
                std::equal(base.begin()+16, base.begin()+32, character.begin()+233*4), "base/light/character uploaded camera bits agree");
            Check(ps == originalPs, "ordinary materials never receive shadow PS compensation");
            const auto sample = FrameJitter(frame, extent.width, extent.height);
            Check(sample.phase == frame+1 && sample.pixelX >= -.5 && sample.pixelX < .5 &&
                sample.pixelY >= -.5 && sample.pixelY < .5, "all phases remain subpixel at every internal size");
            const auto cycle = FrameJitter(frame+32, extent.width, extent.height);
            Check(sample.ndcX == cycle.ndcX && sample.ndcY == cycle.ndcY, "32-phase period stable");
            const auto before = MatrixAt(bank(4), 4), after = MatrixAt(base, 4);
            for (const auto point : {Vector{0,0,0,1}, Vector{1,2,3,1}, Vector{-4,1,-2,1}})
            {
                const auto oldClip = Transform(point, before), newClip = Transform(point, after);
                Check(newClip[2] == oldClip[2] && newClip[3] == oldClip[3], "projection jitter never changes Z or W");
                Check(std::abs((newClip[0]-oldClip[0])/oldClip[3] * extent.width*.5 - sample.pixelX) < .001 &&
                    std::abs((oldClip[1]-newClip[1])/oldClip[3] * extent.height*.5 - sample.pixelY) < .001,
                    "actual float camera upload produces intended host raster offset");
            }
            const SceneResolve sceneDepth{frame, 247494, 0x9c00000, 4102,
                uint32_t(extent.width), uint32_t(extent.height), true};
            const auto shadowResult = ApplyDrawJitter(0x99c2b4b0960a9ccdull, 0xd55a20d004031279ull,
                frame, true, true, &anchor, 37, extent, shadow.data(), ps.data(), &sceneDepth, &sceneDepth);
            Check(shadowResult.applied && shadowResult.shadowCompensated, "paired shadow raster and reconstruction corrected");
            Check(std::equal(base.begin()+16, base.begin()+32, shadow.begin()), "shadow raster samples the same jittered camera");
            Check(std::equal(ps.begin(), ps.begin()+16, originalPs.begin()) &&
                std::equal(ps.begin()+20, ps.end(), originalPs.begin()+20), "shadow compensation touches only c4");
            for (double depth : {.002, .006, .03})
                for (const auto xy : {std::array<double,2>{-.2,-.3}, std::array<double,2>{.5,.7}})
                {
                    const auto expected = Shadow(originalPs, xy[0]*3, xy[1]*3, 3, depth);
                    const auto legacy = Shadow(originalPs, (xy[0]+sample.ndcX)*3, (xy[1]+sample.ndcY)*3, 3, depth);
                    const auto fixed = Shadow(ps, (xy[0]+sample.ndcX)*3, (xy[1]+sample.ndcY)*3, 3, depth);
                    Check(fixed.uv == legacy.uv, "depth lookup remains on jittered scene grid");
                    Check(fixed.uv != expected.uv, "reconstruction correction does not silently undo depth UV jitter");
                    for (unsigned i = 0; i < 4; ++i)
                    {
                        const double error = std::abs(fixed.projected[i]-expected.projected[i]);
                        correctedError = std::max(correctedError, error);
                        legacyError = std::max(legacyError, std::abs(legacy.projected[i]-expected.projected[i]));
                        Check(error < 2e-5, "d55 swizzled reconstruction stays on unjittered shadow projection");
                    }
                }
        }
    }
    Check(legacyError > .02 && correctedError < legacyError*.001, "fixture exposes legacy shadow drift and bounds corrected error");

    anchor.viewport = {0,0,2560,1440};
    const SceneResolve depth{20,247494,0x9c00000,4102,2560,1440,true};
    const auto noChange = [&](bool enabled, bool compatible, uint64_t vs, const SceneAnchor* camera,
        uint64_t allocation, Viewport extent, JitterRejection expected, const SceneResolve* sampled = nullptr) {
        auto constants = bank(std::max(0, PositionVPSlot(vs))), ps = originalPs;
        const auto old = constants;
        const auto result = ApplyDrawJitter(vs, 0xd55a20d004031279ull, 20, enabled, compatible,
            camera, allocation, extent, constants.data(), ps.data(), &depth, sampled);
        Check(!result.applied && !result.shadowCompensated && result.rejection == expected, "unsupported draw rejected explicitly");
        Check(constants == old && ps == originalPs, "rejected draw leaves both upload banks unchanged");
    };
    noChange(false,true,0x99c2b4b0960a9ccdull,&anchor,37,anchor.viewport,JitterRejection::Disabled,&depth);
    noChange(true,true,0x12345678ull,&anchor,37,anchor.viewport,JitterRejection::UnknownShader);
    noChange(true,false,0x99c2b4b0960a9ccdull,&anchor,37,anchor.viewport,JitterRejection::IncompatibleViewport,&depth);
    noChange(true,true,0x99c2b4b0960a9ccdull,nullptr,37,anchor.viewport,JitterRejection::MissingCamera,&depth);
    auto changed = anchor; changed.vpBits[0] ^= 1;
    noChange(true,true,0x99c2b4b0960a9ccdull,&changed,37,anchor.viewport,JitterRejection::CameraMismatch,&depth);
    noChange(true,true,0x99c2b4b0960a9ccdull,&anchor,38,anchor.viewport,JitterRejection::DepthMismatch,&depth);
    noChange(true,true,0x99c2b4b0960a9ccdull,&anchor,37,{0,0,1280,720},JitterRejection::IncompatibleViewport,&depth);
    noChange(true,true,0x99c2b4b0960a9ccdull,&anchor,37,anchor.viewport,JitterRejection::ShadowDepthMismatch);
    for (unsigned mutation = 0; mutation < 6; ++mutation)
    {
        auto sampled = depth;
        if (mutation == 0) --sampled.frame;
        if (mutation == 1) ++sampled.ordinal;
        if (mutation == 2) sampled.address += 0x1000;
        if (mutation == 3) sampled.format = 6;
        if (mutation == 4) sampled.width /= 2;
        if (mutation == 5) sampled.fullExtent = false;
        noChange(true,true,0x99c2b4b0960a9ccdull,&anchor,37,anchor.viewport,JitterRejection::ShadowDepthMismatch,&sampled);
    }
    // Fetch logical dimensions remain in the guest grid even when the same
    // resolved surface is scaled to 2560x1440. Cropped views retain the parent's
    // provenance but change normalized UVs, so they cannot authorize this fix.
    const auto sizeWord = [](uint32_t width, uint32_t height) { return (width-1) | ((height-1) << 13); };
    Check(IsFullSceneDepthFetch(sizeWord(1280,720), 1u << 9, 1280,720), "full 2D guest depth view accepted at scaled resolution");
    Check(!IsFullSceneDepthFetch(sizeWord(1280,720), 1u << 9, 2560,1440), "physical dimensions cannot substitute for guest fetch dimensions");
    for (unsigned dimension : {0u,2u,3u})
    {
        const bool full = IsFullSceneDepthFetch(sizeWord(1280,720), dimension << 9, 1280,720);
        Check(!full, "non-2D view cannot authorize scene depth reconstruction");
        noChange(true,true,0x99c2b4b0960a9ccdull,&anchor,37,anchor.viewport,JitterRejection::ShadowDepthMismatch,full?&depth:nullptr);
    }
    for (const auto logical : {std::array<uint32_t,2>{640,720}, std::array<uint32_t,2>{1280,360},
        std::array<uint32_t,2>{1281,720}, std::array<uint32_t,2>{1280,721}})
    {
        const bool full = IsFullSceneDepthFetch(sizeWord(logical[0],logical[1]), 1u << 9, 1280,720);
        Check(!full, "cropped or oversized depth fetch cannot inherit parent resolve authorization");
        noChange(true,true,0x99c2b4b0960a9ccdull,&anchor,37,anchor.viewport,JitterRejection::ShadowDepthMismatch,full?&depth:nullptr);
    }
    auto unknownPsVp = bank(0), unknownPs = originalPs;
    const auto unknownPsResult = ApplyDrawJitter(0x99c2b4b0960a9ccdull, 0x12345678ull, 20,
        true,true,&anchor,37,anchor.viewport,unknownPsVp.data(),unknownPs.data());
    Check(unknownPsResult.applied && !unknownPsResult.shadowCompensated && unknownPs == originalPs,
        "unknown PS never acquires the known shadow reconstruction contract");
    Check(!FrameJitter(1,0,1440).phase && !FrameJitter(1,2560,std::numeric_limits<double>::infinity()).phase,
        "invalid physical extent cannot produce jitter");
    auto invalidVp = bank(0), invalidPs = originalPs;
    invalidPs[16] = std::bit_cast<uint32_t>(std::numeric_limits<float>::quiet_NaN());
    const auto invalidBefore = invalidPs;
    const auto invalidResult = ApplyDrawJitter(0x99c2b4b0960a9ccdull,0xd55a20d004031279ull,20,
        true,true,&anchor,37,anchor.viewport,invalidVp.data(),invalidPs.data(),&depth,&depth);
    Check(!invalidResult.applied && invalidResult.rejection == JitterRejection::InvalidConstants &&
        invalidVp == bank(0) && invalidPs == invalidBefore, "invalid PS cannot cause a half-applied correction");
    std::printf("PASS: %u temporal jitter checks; 32 phases at 4 sizes; legacy shadow error %.9g, corrected %.9g\n",
        checks, legacyError, correctedError);
}
