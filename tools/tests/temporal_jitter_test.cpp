#include <gpu/temporal_jitter.h>
#include <gpu/temporal_suspect.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include "map16_jitter_capture.h"
#include "f5997_jitter_capture.h"
#include "f5912_jitter_capture.h"
#include "f16385_jitter_capture.h"
#include "f2548_jitter_capture.h"
#include "f6131_late_floor_jitter_capture.h"
#include "f6814_cutscene_jitter_capture.h"
#include "f6131_e810_jitter_capture.h"
#include "f3449_sky_jitter_capture.h"
#include "f1800_sky_jitter_capture.h"
#include "issue121_sky_ge1_jitter_capture.h"
#include "issue203_sky_jitter_capture.h"
#include "issue212_light_jitter_capture.h"
#include "tour_cutscene_20261004_capture.h"
#include "issue307_boat_capture.h"
#include "f12139_sky_jitter_capture.h"
#include "f25276_cave_jitter_capture.h"
#include "tour_sky_20261001_capture.h"
#include "tour_batch2_20261001_capture.h"
#include "tour_8d66_20261001_capture.h"
#include "battle_depth_20261001_capture.h"
#include "feedback_mapping_cases.h"
#include "screen_batch_cases.h"

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

// Independent transcription of the reviewed feedback programs. Inputs/constants
// below are synthetic: compact player feedback does not contain vertex buffers
// or constant banks. Preserve source operation order and register swizzles; do
// not use Transform or reconstruct a projection from the production slot table.
static Float4 FeedbackPosition(const Constants& c, Float4 v, const FeedbackMappingCase& shader)
{
    v[3]=1;
    Float4 world{}, clip{};
    if (shader.family==FeedbackProjection::Depth0)
    {
        world=Mul(v[3],S(C(c,7),"wzxy"));
        world=Mad(v[2],S(C(c,6),"wzyx"),S(world,"xywz"));
        world=Mad(v[1],S(C(c,5),"yxzw"),S(world,"zwyx"));
        world=Mad(v[0],S(C(c,4),"zywx"),S(world,"zxwy"));
        clip=Mul(world[2],C(c,3));
        clip=Mad(world[0],S(C(c,2),"wzyx"),S(clip,"wzyx"));
        clip=Mad(world[1],C(c,1),S(clip,"wzyx"));
        return Mad(world[3],C(c,0),clip);
    }
    if (shader.family==FeedbackProjection::Material0)
    {
        world=Mul(v[3],S(C(c,8),"wxyz"));
        world=Mad(v[2],S(C(c,7),"wxyz"),world);
        world=Mad(v[1],S(C(c,6),"wxyz"),world);
        world=Mad(v[0],S(C(c,5),"wxyz"),world);
        clip=Mul(world[0],C(c,3));
        clip=Mad(world[3],C(c,2),clip);
        clip=Mad(world[2],C(c,1),clip);
        return Mad(world[1],C(c,0),clip);
    }
    if (shader.family==FeedbackProjection::Alternate7)
    {
        world=Mul(v[3],S(C(c,3),"wzxy"));
        world=Mad(v[2],S(C(c,2),"wzyx"),S(world,"xywz"));
        world=Mad(v[1],S(C(c,1),"yxzw"),S(world,"zwyx"));
        world=Mad(v[0],S(C(c,0),"zywx"),S(world,"zxwy"));
        clip=Mul(world[2],C(c,10));
        clip=Mad(world[0],C(c,9),clip);
        clip=Mad(world[1],C(c,8),clip);
        return Mad(world[3],C(c,7),clip);
    }
    // 0fa039 uses c5..8 for world position; the static c4/c7/c8
    // families use c0..3. All have the same swizzled world and VP tail.
    const unsigned worldSlot=shader.family==FeedbackProjection::Material1?5:0;
    world=Mul(v[3],S(C(c,worldSlot+3),"xywz"));
    world=Mad(v[2],S(C(c,worldSlot+2),"wxzy"),S(world,"zxwy"));
    world=Mad(v[1],S(C(c,worldSlot+1),"zwyx"),S(world,"zxwy"));
    world=Mad(v[0],S(C(c,worldSlot),"yzxw"),S(world,"zxwy"));
    clip=Mul(world[3],C(c,shader.slot+3));
    clip=Mad(world[1],C(c,shader.slot+2),clip);
    clip=Mad(world[0],C(c,shader.slot+1),clip);
    return Mad(world[2],C(c,shader.slot),clip);
}

// Only the source-reviewed final four-row VP accumulation is modeled here.
// The 40 shader programs have three distinct scalar orders in their HLSL;
// vertex fetch, preceding world operations and PS sampling are outside this test.
static Float4 ScreenBatchClip(const Constants& c, unsigned slot,
    screen_batch_capture::Family family, const Float4& source)
{
    using screen_batch_capture::Family;
    const std::array<unsigned,4> order=family==Family::A ? std::array<unsigned,4>{3,1,0,2} :
        family==Family::B ? std::array<unsigned,4>{0,3,2,1} :
                            std::array<unsigned,4>{2,0,1,3};
    auto clip=Mul(source[order[0]],C(c,slot+3));
    clip=Mad(source[order[1]],C(c,slot+2),clip);
    clip=Mad(source[order[2]],C(c,slot+1),clip);
    return Mad(source[order[3]],C(c,slot),clip);
}
static void ScreenMappingBatch()
{
    std::array<uint64_t,40> seen{};
    size_t unique=0;
    double oldSeparation=0,maxPixelError=0;
    const unsigned firstCheck=checks;
    for (const auto& item:screen_batch_capture::pairs)
    {
        if (std::find(seen.begin(),seen.begin()+unique,item.vs)==seen.begin()+unique)
        {
            Check(unique<seen.size(),"screen batch fixture has at most 40 unique VS");
            if (unique<seen.size()) seen[unique++]=item.vs;
        }
        Check(PositionVPSlot(item.vs)==int(item.slot) &&
            DrawPositionVPSlot(item.vs,item.ps,false)==int(item.slot),
            "screen batch observed VS/PS pair maps to its reviewed camera slot");
        Constants original{},originalPs{};
        for (unsigned i=0;i<original.size();++i)
        {
            original[i]=std::bit_cast<uint32_t>(float(int((i*17)%47)-23)/16.f);
            originalPs[i]=std::bit_cast<uint32_t>(float(int((i*13)%31)-15)/8.f);
        }
        const std::array<float,16> vp{1.3f,.2f,.03f,.01f, -.15f,1.7f,.4f,-.07f,
            .31f,-.22f,.91f,.13f, .25f,-.5f,.75f,4.f};
        std::array<uint32_t,16> vpBits{};
        for (unsigned i=0;i<16;++i)
            vpBits[i]=original[item.slot*4+i]=std::bit_cast<uint32_t>(vp[i]);
        for (const auto extent:{Viewport{0,0,2560,1440},Viewport{0,0,3840,2160}})
            for (uint64_t phase=0;phase<32;++phase)
            {
                const SceneAnchor anchor{vpBits,extent,54};
                auto changed=original,ps=originalPs,legacy=original,legacyPs=originalPs;
                const auto jitter=ApplyDrawJitter(item.vs,item.ps,phase,true,true,&anchor,
                    54,extent,changed.data(),ps.data());
                Check(jitter.applied && jitter.slot==int(item.slot) &&
                    !jitter.shadowCompensated && ps==originalPs,
                    "screen batch jitter changes VS position without changing PS constants");
                const auto omitted=ApplyDrawJitter(0,item.ps,phase,true,true,&anchor,
                    54,extent,legacy.data(),legacyPs.data());
                Check(!omitted.applied && omitted.rejection==JitterRejection::UnknownShader &&
                    legacy==original && legacyPs==originalPs,
                    "screen batch old unmapped control leaves uploads unchanged");
                bool otherConstantsUnchanged=true;
                for (unsigned i=0;i<changed.size();++i)
                    if (i<item.slot*4 || i>=(item.slot+4)*4 || i%4>=2)
                        otherConstantsUnchanged &= changed[i]==original[i];
                Check(otherConstantsUnchanged,
                    "screen batch keeps VP ZW and all non-VP VS constants exact");
                for (const auto source:{Float4{-.4f,.7f,1.2f,1.f},
                    Float4{1.1f,-.6f,2.3f,1.f},Float4{-1.2f,.3f,.8f,1.f}})
                {
                    const auto before=ScreenBatchClip(original,item.slot,item.family,source);
                    const auto after=ScreenBatchClip(changed,item.slot,item.family,source);
                    Check(std::isfinite(before[3]) && std::abs(before[3])>.1 &&
                        after[2]==before[2] && after[3]==before[3],
                        "screen batch independent final clip retains Z and W");
                    for (unsigned axis=0;axis<2;++axis)
                    {
                        const double pixels=(double(after[axis])/after[3]-
                            double(before[axis])/before[3])*(axis?extent.height:extent.width)*
                            (axis?-.5:.5);
                        const double expected=axis?jitter.sample.pixelY:jitter.sample.pixelX;
                        maxPixelError=std::max(maxPixelError,std::abs(pixels-expected));
                        oldSeparation=std::max(oldSeparation,std::abs(pixels));
                        Check(std::abs(pixels-expected)<.003,
                            "screen batch source-reviewed VP accumulation follows shared pixel jitter");
                    }
                }
            }
        const Viewport extent{0,0,2560,1440};
        const SceneAnchor anchor{vpBits,extent,54};
        for (unsigned rejection=0;rejection<6;++rejection)
        {
            auto values=original,ps=originalPs;
            auto candidateAnchor=anchor;
            bool enabled=true,compatible=true;
            const SceneAnchor* selected=&candidateAnchor;
            uint64_t depth=54;
            auto expected=JitterRejection::Disabled;
            if (rejection==0) enabled=false;
            if (rejection==1) {compatible=false;expected=JitterRejection::IncompatibleViewport;}
            if (rejection==2) {selected=nullptr;expected=JitterRejection::MissingCamera;}
            if (rejection==3) {candidateAnchor.vpBits[0]^=1;expected=JitterRejection::CameraMismatch;}
            if (rejection==4) {depth=55;expected=JitterRejection::DepthMismatch;}
            if (rejection==5)
            {
                values[item.slot*4]=candidateAnchor.vpBits[0]=
                    std::bit_cast<uint32_t>(std::numeric_limits<float>::infinity());
                expected=JitterRejection::InvalidConstants;
            }
            const auto untouched=values;
            const auto result=ApplyDrawJitter(item.vs,item.ps,0,enabled,compatible,
                selected,depth,extent,values.data(),ps.data());
            Check(!result.applied && result.rejection==expected &&
                values==untouched && ps==originalPs,
                "screen batch rejection guards leave both uploads untouched");
        }
    }
    Check(unique==40 && oldSeparation>.3 && maxPixelError<.003,
        "screen batch covers 40 unique shaders with visible old offset and bounded pixel error");
    std::printf("Screen mapping batch: %zu observed VS/PS pairs, %zu VS, 32 phases, 1440p/4K; old separation %.6f px, max pixel error %.6f px, %u checks (CPU VP tail only)\n",
        screen_batch_capture::pairs.size(),unique,oldSeparation,maxPixelError,checks-firstCheck);
}

static void FeedbackMappingBatch()
{
    const unsigned startChecks=checks;
    double maxError=0, legacySeparation=0;
    for (const auto& shader : feedbackMappings)
    {
        double shaderLegacySeparation=0;
        Check(PositionVPSlot(shader.vs)==int(shader.slot),"reviewed feedback VS resolves its exact VP slot");
        for (unsigned scene=0;scene<3;++scene)
        {
            Constants original{}, originalPs{};
            for (unsigned i=0;i<original.size();++i)
            {
                // Nonzero, non-symmetric independent world/UV/lighting data.
                original[i]=std::bit_cast<uint32_t>(float(int((i*17+scene*11)%41)-20)/16.f);
                originalPs[i]=std::bit_cast<uint32_t>(float(int((i*13+scene*7)%29)-14)/8.f);
            }
            const std::array<float,16> vp{1.3f,.2f,.03f,.01f, -.15f,1.7f,.4f,-.07f,
                .31f,-.22f,.91f,.13f, .25f+scene*.125f,-.5f,.75f,4.f};
            SceneAnchor anchor{};
            anchor.depthAllocation=37;
            for (unsigned i=0;i<vp.size();++i)
                anchor.vpBits[i]=original[shader.slot*4+i]=std::bit_cast<uint32_t>(vp[i]);
            for (const auto extent : {Viewport{0,0,1280,720},Viewport{0,0,1920,1080},
                Viewport{0,0,2560,1440},Viewport{0,0,3840,2160}})
            {
                anchor.viewport=extent;
                for (uint64_t frame=0;frame<32;++frame)
                {
                    auto values=original, ps=originalPs;
                    const auto result=ApplyDrawJitter(shader.vs,shader.ps,frame,true,true,
                        &anchor,37,extent,values.data(),ps.data());
                    Check(result.applied && !result.shadowCompensated,"reviewed feedback upload applies ordinary scene jitter");
                    Check(ps==originalPs,"feedback mappings never modify PS constants");
                    auto legacy=original, legacyPs=originalPs;
                    const auto legacyResult=ApplyDrawJitter(0,shader.ps,frame,true,true,
                        &anchor,37,extent,legacy.data(),legacyPs.data());
                    Check(legacyResult.rejection==JitterRejection::UnknownShader && legacy==original && legacyPs==originalPs,
                        "legacy unrecognized upload does not jitter either bank");
                    bool unchanged=true;
                    for (unsigned i=0;i<values.size();++i)
                        if (i<shader.slot*4 || i>=(shader.slot+4)*4 || i%4>=2)
                            unchanged &= values[i]==original[i];
                    Check(unchanged,"only VP XY changes; VP ZW, UV, lighting and other VS constants stay exact");
                    for (const auto vertex : {Float4{-.3f,.5f,.2f,1},Float4{.7f,-.2f,.4f,1},Float4{.1f,.9f,-.6f,1}})
                    {
                        const auto before=FeedbackPosition(original,vertex,shader);
                        const auto after=FeedbackPosition(values,vertex,shader);
                        const auto legacyClip=FeedbackPosition(legacy,vertex,shader);
                        Check(std::isfinite(before[3]) && std::abs(before[3])>.05,"synthetic reference has useful clip W");
                        Check(after[2]==before[2] && after[3]==before[3],"independent program preserves clip Z and W (including PS W-only inputs)");
                        for (unsigned axis=0;axis<2;++axis)
                        {
                            const double pixels=(double(after[axis])-before[axis])/before[3]*
                                (axis?extent.height:extent.width)*.5;
                            const double expected=axis?-result.sample.pixelY:result.sample.pixelX;
                            maxError=std::max(maxError,std::abs(pixels-expected));
                            const double separation=std::abs((double(after[axis])-legacyClip[axis])/legacyClip[3])*
                                (axis?extent.height:extent.width)*.5;
                            shaderLegacySeparation=std::max(shaderLegacySeparation,separation);
                            Check(std::abs(pixels-expected)<.003,"independent shader projection follows shared pixel jitter");
                        }
                    }
                }
            }
            // These controls test actual immutable uploads, including when the
            // same camera matrix is present at a different constant slot.
            for (unsigned rejection=0;rejection<7;++rejection)
            {
                auto values=original, ps=originalPs;
                auto rejectedAnchor=anchor;
                bool enabled=true, compatible=true;
                const SceneAnchor* selected=&rejectedAnchor;
                uint64_t depth=37;
                JitterRejection expected=JitterRejection::Disabled;
                if (rejection==0) enabled=false;
                if (rejection==1) { compatible=false; expected=JitterRejection::IncompatibleViewport; }
                if (rejection==2) { selected=nullptr; expected=JitterRejection::MissingCamera; }
                if (rejection==3) { rejectedAnchor.vpBits[0]^=1; expected=JitterRejection::CameraMismatch; }
                if (rejection==4) { depth=38; expected=JitterRejection::DepthMismatch; }
                if (rejection==5)
                {
                    std::copy(anchor.vpBits.begin(),anchor.vpBits.end(),values.begin()+200*4);
                    values[shader.slot*4]^=1;
                    expected=JitterRejection::CameraMismatch;
                }
                if (rejection==6)
                {
                    values[shader.slot*4]=rejectedAnchor.vpBits[0]=std::bit_cast<uint32_t>(std::numeric_limits<float>::infinity());
                    expected=JitterRejection::InvalidConstants;
                }
                const auto untouched=values;
                const auto result=ApplyDrawJitter(shader.vs,shader.ps,5,enabled,compatible,
                    selected,depth,anchor.viewport,values.data(),ps.data());
                Check(!result.applied && result.rejection==expected,"feedback rejection retains the required runtime guard");
                Check(values==untouched && ps==originalPs,"rejected feedback draw leaves both banks byte-for-byte intact");
            }
        }
        Check(shaderLegacySeparation>.48,"each reviewed program exposes the old unjittered projection separation");
        legacySeparation=std::max(legacySeparation,shaderLegacySeparation);
    }
    for (const auto vs : feedbackHeldMappings)
        Check(PositionVPSlot(vs)==-1,"unresolved screen sampling or camera evidence stays unmapped");
    Check(legacySeparation>.48,"unmapped legacy control misses a visible subpixel phase");
    std::printf("Feedback mapping batch: %zu VS, 3 synthetic banks, 32 phases, 720p-4K; max error %.6f px; legacy separation %.6f px; %u checks\n",
        std::size(feedbackMappings),maxError,legacySeparation,checks-startChecks);
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

// Battle f2871 position arithmetic, independently transcribed from HLSL.
// f1b3 (draw1) and f7fd (draw12) have the same position instructions; likewise
// 8b55 (draw79) and 400d (draw96), after renaming their temporary registers.
// Keep the lighting and skinned permutations explicit instead of using Transform.
static Float4 BattleDepth(const Constants& c, Float4 r2)
{
    r2[3]=1;
    auto r1=Mul(r2[3],S(C(c,3),"xywz"));
    r1=Mad(r2[2],S(C(c,2),"wxzy"),S(r1,"zxwy"));
    r1=Mad(r2[1],S(C(c,1),"zwyx"),S(r1,"zxwy"));
    r2=Mad(r2[0],S(C(c,0),"yzxw"),S(r1,"zxwy"));
    r1=Mul(r2[3],C(c,7));
    r1=Mad(r2[1],C(c,6),r1);
    r1=Mad(r2[0],C(c,5),r1);
    return Mad(r2[2],C(c,4),r1);
}
static Float4 BattleMaterial(const Constants& c, Float4 r8)
{
    r8[3]=1;
    auto r3=Mul(r8[3],S(C(c,3),"xywz"));
    r3=Mad(r8[2],S(C(c,2),"wxzy"),S(r3,"zxwy"));
    r3=Mad(r8[1],S(C(c,1),"zwyx"),S(r3,"zxwy"));
    const auto r9=Mad(r8[0],S(C(c,0),"yzxw"),S(r3,"zxwy"));
    r3=Mul(r9[3],C(c,10));
    r3=Mad(r9[1],C(c,9),r3);
    r3=Mad(r9[0],C(c,8),r3);
    return Mad(r9[2],C(c,7),r3); // oPos and o4 in 400d, o1 in 8b55
}
static Float4 BattleLight08dc(const Constants& c, Float4 r4)
{
    r4[3]=1;
    auto r0=Mul(r4[3],S(C(c,3),"xywz"));
    r0=Mad(r4[2],S(C(c,2),"wxzy"),S(r0,"zxwy"));
    r0=Mad(r4[1],S(C(c,1),"zwyx"),S(r0,"zxwy"));
    r4=Mad(r4[0],S(C(c,0),"yzxw"),S(r0,"zxwy"));
    r0=Mul(r4[3],C(c,10));
    r0=Mad(r4[1],C(c,9),r0);
    r0=Mad(r4[0],C(c,8),r0);
    return Mad(r4[2],C(c,7),r0); // retained through oPos/o4; PS667 divides o4.xy/w
}
// Input is the palette-skinned point. These retain the distinct world-transform
// and VP swizzles of draw202 (0eb2) and draw249 (1e90), not just c233 endpoints.
static Float4 BattleSkinned0eb(const Constants& c, Float4 point)
{
    auto r0=Mad(point[2],S(C(c,3),"wzyx"),S(C(c,4),"wzyx"));
    r0=Mad(point[1],S(C(c,2),"yxzw"),S(r0,"zwyx"));
    const auto r6=Mad(point[0],S(C(c,1),"zywx"),S(r0,"zxwy"));
    auto r2=Mul(r6[2],C(c,236));
    r2=Mad(r6[0],C(c,235),r2);
    r2=Mad(r6[1],C(c,234),r2);
    return Mad(r6[3],C(c,233),r2); // r2 -> oPos/o1
}
static Float4 BattleSkinned1e90(const Constants& c, Float4 point)
{
    auto r0=Mad(point[2],S(C(c,3),"wxyz"),S(C(c,4),"wxyz"));
    r0=Mad(point[1],S(C(c,2),"wxyz"),r0);
    const auto r7=Mad(point[0],S(C(c,1),"wxyz"),r0);
    auto r5=Mul(r7[0],C(c,236));
    r5=Mad(r7[3],C(c,235),r5);
    r5=Mad(r7[2],C(c,234),r5);
    return Mad(r7[1],C(c,233),r5); // r7 -> oPos/o1
}
static void BattleCoverage()
{
    const std::array<uint32_t,16> vp{
        1065421082u,1036507293u,1061409510u,1061422356u,1067057496u,3181867434u,3206812392u,3206823155u,
        0u,1077045441u,3174739206u,3174751452u,1127137590u,3281230865u,1146691008u,1146869091u};
    // Actual object draw1 and terrain draw12 c0-c3; skin draw202/249 c1-c4.
    const std::array<std::array<uint32_t,16>,2> worlds{{
        {0x3f800000,0,0,0,0,0x3f800000,0,0,0,0,0x3f800000,0,0,0,0,0x3f800000},
        {0x3f800000,0,0,0,0,0x3f800000,0,0,0,0,0x3f800000,0,0xc4c22ac6,0x44761a64,0x443aa280,0x3f800000}}};
    const std::array<uint32_t,16> skinWorld{
        0x3f800000,0,0,0,0,0x3f800000,0,0,0,0,0x3f800000,0,0xc3a28000,0,0,0x3f800000};
    // Retained first two bone matrices c8-c13, shared by those two draws.
    const std::array<uint32_t,24> bones{
        0x3f7c4405,0xbe05dccc,0x3ddf267b,0xc1a3d828,0x3e03c439,0x3f7dc96e,0x3cd20b20,0xc0e34ce5,
        0xbde415eb,0xbc383e27,0x3f7e6424,0xbf4b42ce,0x3f7c4405,0xbe05dccc,0x3ddf267b,0xc1a3d827,
        0x3e03c439,0x3f7dc96e,0x3cd20b20,0xc0e34ce4,0xbde415eb,0xbc383e27,0x3f7e6424,0xbf4b429e};
    const auto skinnedPoint=[&](Float4 local,float weight) {
        Float4 point{0,0,0,1};
        for (unsigned row=0;row<3;++row)
        {
            Float4 blended{};
            for (unsigned column=0;column<4;++column)
                blended[column]=weight*float(F(bones[row*4+column]))+(1-weight)*float(F(bones[12+row*4+column]));
            // HLSL dot(row.zxyw, local.zxy1); no host Transform/jitter reference.
            point[row]=((blended[2]*local[2]+blended[0]*local[0])+blended[1]*local[1])+blended[3];
        }
        return point;
    };
    double legacySeparation=0,offsetError=0;
    for (const auto extent : {Viewport{0,0,1280,720},Viewport{0,0,1920,1080},
        Viewport{0,0,2560,1440},Viewport{0,0,3840,2160}})
        for (uint64_t frame=0;frame<32;++frame)
        {
            const SceneAnchor anchor{vp,extent,19};
            const auto bank=[&](unsigned slot,unsigned world) {
                Constants c{};
                std::copy(worlds[world].begin(),worlds[world].end(),c.begin());
                std::copy(vp.begin(),vp.end(),c.begin()+slot*4);return c;
            };
            const auto jitter=FrameJitter(frame,extent.width,extent.height);
            for (unsigned world=0;world<2;++world)
            {
                const uint64_t depthVs=world?0xf7fd88506d704a3dull:0xf1b330b3ceea9a3bull;
                const uint64_t materialVs=world?0x400df7c5a60819f5ull:0x8b5577db3ced3327ull;
                auto depth=bank(4,world),material=bank(7,world),light=bank(7,world);
                const auto original=material;
                Constants ps{};
                const auto a=ApplyDrawJitter(depthVs,0,frame,true,true,&anchor,19,extent,depth.data(),ps.data());
                const auto b=ApplyDrawJitter(materialVs,0,frame,true,true,&anchor,19,extent,material.data(),ps.data());
                const auto c=ApplyDrawJitter(0x08dcef32bd434f8cull,0x667ca7db02d6d0a5ull,
                    frame,true,true,&anchor,19,extent,light.data(),ps.data());
                Check(a.applied && b.applied && c.applied,"battle depth/material/lighting accept the captured camera");
                Check(ps==Constants{} && !c.shadowCompensated,"mask-sampling lighting never acquires d55 reconstruction compensation");
                for (const auto local : {Float4{-250,-100,20,1},Float4{120,90,80,1},Float4{10,250,160,1}})
                {
                    const auto d=BattleDepth(depth,local),m=BattleMaterial(material,local),l=BattleLight08dc(light,local);
                    const auto old=BattleMaterial(original,local);
                    Check(d==m && m==l,"independent battle position paths agree exactly");
                    Check(m[2]==old[2] && m[3]==old[3],"battle jitter preserves Z and W");
                    // Both clip-derived varyings must move with the raster position.
                    Check(l[0]/l[3]==d[0]/d[3] && l[1]/l[3]==d[1]/d[3],"terrain mask lookup follows jittered depth coordinates");
                    const double dx=(double(m[0])/m[3]-double(old[0])/old[3])*extent.width*.5;
                    const double dy=(double(old[1])/old[3]-double(m[1])/m[3])*extent.height*.5;
                    offsetError=std::max({offsetError,std::abs(dx-jitter.pixelX),std::abs(dy-jitter.pixelY)});
                    Check(std::abs(dx-jitter.pixelX)<.02 && std::abs(dy-jitter.pixelY)<.02,"battle shader upload produces intended physical jitter");
                    // Negative control: depth corrected while material stays at original VP.
                    legacySeparation=std::max({legacySeparation,std::abs(dx),std::abs(dy)});
                }
            }
            auto skin=bank(233,0),otherSkin=skin;
            std::copy(skinWorld.begin(),skinWorld.end(),skin.begin()+4);
            otherSkin=skin;
            const auto oldSkin=skin;
            Constants ps{};
            const auto a=ApplyDrawJitter(0x0eb223d33f8e8e0cull,0x463f83252b104180ull,frame,true,true,&anchor,19,extent,skin.data(),ps.data());
            const auto b=ApplyDrawJitter(0x1e9017d2b296f480ull,0xbb31c704c2ef724bull,frame,true,true,&anchor,19,extent,otherSkin.data(),ps.data());
            Check(a.applied && b.applied,"both battle skinned position paths receive jitter");
            for (float weight : {0.f,.35f,1.f})
                for (const auto local : {Float4{-20,30,50,1},Float4{10,-40,90,1}})
                {
                    const auto point=skinnedPoint(local,weight);
                    const auto first=BattleSkinned0eb(skin,point),second=BattleSkinned1e90(otherSkin,point);
                    const auto old=BattleSkinned0eb(oldSkin,point);
                    Check(first==second,"distinct skinned world/projection swizzles retain matching clip positions");
                    Check(first[2]==old[2] && first[3]==old[3],"skinned position jitter preserves Z/W");
                    Check(std::abs((double(first[0])/first[3]-double(old[0])/old[3])*extent.width*.5-jitter.pixelX)<.02 &&
                        std::abs((double(old[1])/old[3]-double(first[1])/first[3])*extent.height*.5-jitter.pixelY)<.02,
                        "skinned inputs keep the same intended raster jitter");
                }
            // Recognition must still leave Off and mismatched upload banks untouched.
            for (const auto [vs,slot] : {std::pair{0xf1b330b3ceea9a3bull,4},
                std::pair{0x8b5577db3ced3327ull,7},std::pair{0x400df7c5a60819f5ull,7},
                std::pair{0x08dcef32bd434f8cull,7},std::pair{0x0eb223d33f8e8e0cull,233},std::pair{0x1e9017d2b296f480ull,233}})
                for (unsigned mode=0;mode<4;++mode)
                {
                    auto values=bank(slot,0),originalPs=ps;
                    if (mode==3) values[slot*4]^=1;
                    const auto before=values;
                    const auto result=ApplyDrawJitter(vs,0,frame,mode!=0,mode!=1,&anchor,mode==2?20:19,
                        extent,values.data(),ps.data());
                    Check(!result.applied && values==before && ps==originalPs,"new battle paths preserve Off/viewport/depth/camera rejection bytes");
                }
        }
    Check(legacySeparation>.4,"battle negative control exposes depth versus unjittered material separation");

    // The 13 newly recognized depth writers in f2871 share the original camera.
    // Replay that observation set alongside an existing anchor; a changed camera
    // on a newly recognized material must still reject the entire association.
    const SceneAnchor anchor{vp,{0,0,2560,1440},19};
    for (bool conflicting : {false,true})
    {
        SceneObservation observation;observation.Reset(2871);observation.ObserveCamera(anchor);
        for (unsigned draw=0;draw<13;++draw)
        {
            auto added=anchor;
            if (conflicting && draw==12) added.vpBits[12]^=1;
            observation.ObserveCamera(added);
        }
        observation.ObserveDepth(19,{2871,1,0x9c00000,4102,2560,1440,true});
        observation.ObserveColor({2871,2,0xb0d9000,6,2560,1440,true});
        Check(observation.Draws()==14 && observation.Ready()!=conflicting,"added battle depth writers preserve camera consistency and ambiguity rejection");
        Check(observation.Anchor().vpBits==vp,"observer retains unmodified guest camera bits");
    }
    for (uint64_t screenVs : {0x6318b7b358aa8b45ull,0x73a203360434358cull})
    {
        Constants values{},ps{};std::copy(vp.begin(),vp.end(),values.begin()+233*4);
        const auto before=values;
        const auto result=ApplyDrawJitter(screenVs,0,2871,true,true,&anchor,19,anchor.viewport,values.data(),ps.data());
        Check(!result.applied && result.rejection==JitterRejection::UnknownShader && values==before,
            "residual c233 VP cannot authorize a screen-space shader");
    }
    std::printf("Battle coverage: 32 phases, four sizes; legacy separation %.6f pixels, raster-offset error %.6f pixels\n",legacySeparation,offsetError);
}

struct Map16Output
{
    Float4 position, projected;
    std::array<float,2> materialUv{};
};

// Independent f18420 HLSL transcriptions, including swizzled world transforms.
// Do not use PositionVPSlot or the production Transform for these references.
static Map16Output Map16GroundFcbb(const Constants& c, Float4 r4)
{
    r4[3]=1;
    auto r2=Mul(r4[3],S(C(c,3),"xywz"));
    r2=Mad(r4[2],S(C(c,2),"wxzy"),S(r2,"zxwy"));
    r2=Mad(r4[1],S(C(c,1),"zwyx"),S(r2,"zxwy"));
    const auto r11=Mad(r4[0],S(C(c,0),"yzxw"),S(r2,"zxwy"));
    r2=Mul(r11[3],C(c,10));
    r2=Mad(r11[1],C(c,9),r2);
    r2=Mad(r11[0],C(c,8),r2);
    r4=Mad(r11[2],C(c,7),r2);
    return {r4,r4}; // r4 -> oPos and, without intervening mutation, o4.
}

static Map16Output Map16MaterialE8c0(const Constants& c, Float4 r7)
{
    r7[3]=1;
    auto r4=Mul(r7[3],S(C(c,3),"xywz"));
    r4=Mad(r7[2],S(C(c,2),"wxzy"),S(r4,"zxwy"));
    r4=Mad(r7[1],S(C(c,1),"zwyx"),S(r4,"zxwy"));
    r7=Mad(r7[0],S(C(c,0),"yzxw"),S(r4,"zxwy"));
    r4=Mul(r7[3],C(c,11));
    r4=Mad(r7[1],C(c,10),r4);
    r4=Mad(r7[0],C(c,9),r4);
    r4=Mad(r7[2],C(c,8),r4);
    const auto uv=C(c,7);
    return {r4,r4,{.125f*uv[0]+uv[3],.875f*uv[1]+uv[2]}}; // oPos/o2; o0.xy.
}

static Map16Output Map16Layer576d(const Constants& c, Float4 r4)
{
    r4[3]=1;
    auto r1=Mul(r4[3],S(C(c,3),"xywz"));
    r1=Mad(r4[2],S(C(c,2),"wxzy"),S(r1,"zxwy"));
    r1=Mad(r4[1],S(C(c,1),"zwyx"),S(r1,"zxwy"));
    r4=Mad(r4[0],S(C(c,0),"yzxw"),S(r1,"zxwy"));
    r1=Mul(r4[3],C(c,11));
    r1=Mad(r4[1],C(c,10),r1);
    r1=Mad(r4[0],C(c,9),r1);
    r1=Mad(r4[2],C(c,8),r1);
    const auto uv=C(c,7);
    return {r1,r1,{.125f*uv[0]+uv[3],.875f*uv[1]+uv[2]}}; // oPos/o5; o0.xy.
}

// Both observed 576d PS variants (2ca5/9510) retain this operation order:
// rcp(i5.w), r0.zz*i5.xy -> r0.zw, r0.wz*c0.yx+c0.zw, sample at r0.wz.
static std::array<float,2> Map16SceneUv(const Constants& c, const Float4& i5)
{
    const float reciprocal=1.f/i5[3];
    const float z=reciprocal*i5[0], w=reciprocal*i5[1];
    const auto scaleBias=C(c,0);
    const float outZ=w*scaleBias[1]+scaleBias[2];
    const float outW=z*scaleBias[0]+scaleBias[3];
    return {outW,outZ};
}

static void Map16Coverage()
{
    using namespace map16_capture;
    const auto bank=[](const auto& captured) {
        Constants c{};std::copy(captured.begin(),captured.end(),c.begin());return c;
    };
    std::array<uint32_t,16> vp{};
    std::copy_n(groundVs.begin()+7*4,16,vp.begin());
    const Viewport extent{0,0,3840,2160};
    const SceneAnchor anchor{vp,extent,8};
    const std::array<uint64_t,4> vsHashes{
        0xfcbb75d0feb3fcb9ull,0xe8c0d438c690c784ull,0x576d669b2ad3c898ull,0x576d669b2ad3c898ull};
    const std::array<uint64_t,4> psHashes{
        0x6373dc6f789b2bf1ull,0x5fbcd2ea7c8c6c8eull,0x2ca5e48054767199ull,0x9510a3faf0d67e3full};
    const std::array<Map16Output(*)(const Constants&,Float4),4> evaluate{
        Map16GroundFcbb,Map16MaterialE8c0,Map16Layer576d,Map16Layer576d};
    const std::array<Constants,4> banks{bank(groundVs),bank(materialVs),bank(layerVs),bank(layerVs)};
    const std::array<Constants,4> psBanks{bank(groundPs),bank(materialPs),bank(layerPs1593),bank(layerPs1594)};
    std::array<double,4> legacySeparation{};
    double offsetError=0,uvError=0;
    const unsigned firstCheck=checks;
    for (unsigned path=0;path<banks.size();++path)
    {
        auto original=banks[path];
        // Captured c230 also contains this camera. Patching that unused copy
        // must fail the independent oPos check even if camera matching succeeds.
        std::copy(vp.begin(),vp.end(),original.begin()+230*4);
        const unsigned slot=path==0?7:8;
        Constants originalDepth{};
        std::copy_n(original.begin(),16,originalDepth.begin());
        std::copy(vp.begin(),vp.end(),originalDepth.begin()+4*4);
        for (uint64_t frame=0;frame<32;++frame)
        {
            auto depth=originalDepth,material=original,ps=psBanks[path],depthPs=psBanks[path];
            const auto depthHash=path==0?0xb030ab4e17a20783ull:0xf7fd88506d704a3dull;
            ApplyDrawJitter(depthHash,0,frame,true,true,&anchor,8,extent,depth.data(),depthPs.data());
            ApplyDrawJitter(vsHashes[path],psHashes[path],frame,true,true,&anchor,8,extent,material.data(),ps.data());
            const auto jitter=FrameJitter(frame,extent.width,extent.height);
            for (const auto vertex : {Float4{-60,-30,5,1},Float4{80,-20,10,1},Float4{0,90,12,1}})
            {
                const auto reference=path==0?TireDepthB030(depth,vertex):BattleDepth(depth,vertex);
                const auto current=evaluate[path](material,vertex),legacy=evaluate[path](original,vertex);
                Check(current.position==reference,"Map16 independent material position agrees with its depth pass");
                Check(current.projected==reference,"Map16 clip-derived interpolant follows its raster position");
                Check(current.position[2]==legacy.position[2] && current.position[3]==legacy.position[3],"Map16 material preserves clip Z/W");
                Check(current.materialUv==legacy.materialUv,"Map16 jitter preserves material texture UV transform");
                Check(std::isfinite(reference[3]) && std::abs(reference[3])>1,"Map16 reference point has nondegenerate clip W");
                for (unsigned axis=0;axis<2;++axis)
                {
                    const double dimension=axis==0?extent.width:extent.height;
                    const double expected=axis==0?jitter.pixelX:jitter.pixelY;
                    const double sign=axis==0?1:-1;
                    const double displacement=(double(current.position[axis])/current.position[3]-double(legacy.position[axis])/legacy.position[3])*dimension*.5*sign;
                    legacySeparation[path]=std::max(legacySeparation[path],std::abs(displacement));
                    offsetError=std::max(offsetError,std::abs(displacement-expected));
                    Check(std::abs(displacement-expected)<.01,"Map16 independent clip offset matches requested physical jitter");
                    if (path>=2)
                    {
                        const auto currentUv=Map16SceneUv(ps,current.projected),legacyUv=Map16SceneUv(psBanks[path],legacy.projected);
                        const double sampleDisplacement=(double(currentUv[axis])-legacyUv[axis])*dimension;
                        uvError=std::max(uvError,std::abs(sampleDisplacement-displacement));
                        Check(std::abs(sampleDisplacement-displacement)<.002,"576d screen-color sampling tracks physical raster jitter without inverse compensation");
                    }
                }
            }
            Check(ps==psBanks[path],"Map16 material and scene-color PS constants remain unchanged");
            // Restore only the independently identified VP range: all remaining
            // captured world/normal/UV/light/camera constants must be untouched.
            std::copy_n(original.begin()+slot*4,16,material.begin()+slot*4);
            Check(material==original,"Map16 jitter leaves non-position constants and residual camera banks unchanged");
        }
        Check(legacySeparation[path]>.3,"Map16 unjittered-material negative control exposes the original layer separation");
        for (unsigned reject=0;reject<4;++reject)
        {
            auto material=original,ps=psBanks[path];
            auto viewport=extent;
            if (reject==1) material[slot*4]^=1; // A different camera at the used slot.
            if (reject==2) viewport.width-=1;
            const auto before=material;
            const auto result=ApplyDrawJitter(vsHashes[path],psHashes[path],0,reject!=0,true,&anchor,reject==3?9:8,viewport,material.data(),ps.data());
            Check(!result.applied && material==before && ps==psBanks[path],"Map16 Off/camera/viewport/allocation rejection preserves the upload");
        }
    }
    std::printf("Map16 coverage: %u checks, 32 phases at 3840x2160, ground and two material layers; legacy separation %.6f/%.6f/%.6f/%.6f pixels, clip offset error %.6f, scene-UV error %.6f pixels\n",
        checks-firstCheck,legacySeparation[0],legacySeparation[1],legacySeparation[2],legacySeparation[3],offsetError,uvError);
}

// f1653 3c86's r8 -> r4 position chain is exactly BattleMaterial after
// renaming temporaries; f3b9/e7b38's r5 -> r0 is BattleLight08dc. Their
// world/VP swizzles and accumulation order are identical, not just the slot.
// o1 (3c86) / o4 (light) retain the clip result. Actual paired material PSs
// use only its W; light PSs overwrite XYZ before use. No world ray uses c7.
static void CapturedStaticLayerCoverage()
{
    const std::array<uint32_t,16> vp{
        0xbeb66c64,0x3e7676a4,0x3f79787d,0x3f79b86a,0x3fd8f533,0x3d4f3b82,0x3e51c2d1,0x3e51f891,
        0,0x4044705a,0xbda36b85,0xbda39565,0x4220688f,0xc3cbadb0,0x43836084,0x4388822e};
    // Captured c0-c3 from draw493 (1428 indices) and draw983/1067 (930/1428).
    const std::array<std::array<uint32_t,16>,2> worlds{{
        {0x3f7cd792,0xbe205815,0,0,0x3e205815,0x3f7cd792,0,0,0,0,0x3f800000,0,0x44bb5c1b,0xc3f557fc,0,0x3f800000},
        {0x3f800000,0,0,0,0,0x3f800000,0,0,0,0,0x3f800000,0,0x453a8469,0x44eebffa,0,0x3f800000}}};
    double legacySeparation=0;
    for (const auto& world : worlds)
        for (const auto extent : {Viewport{0,0,1280,720},Viewport{0,0,3840,2160}})
            for (uint64_t frame=0;frame<32;++frame)
            {
                const SceneAnchor anchor{vp,extent,19};
                const auto bank=[&](unsigned slot) {
                    Constants c{};std::copy(world.begin(),world.end(),c.begin());
                    std::copy(vp.begin(),vp.end(),c.begin()+slot*4);return c;
                };
                auto depth=bank(4),material=bank(7),lightA=bank(7),lightB=bank(7);
                const auto original=material;
                Constants ps{};
                Check(ApplyDrawJitter(0xb030ab4e17a20783ull,0,frame,true,true,&anchor,19,extent,depth.data(),ps.data()).applied,
                    "captured static depth accepts scene camera");
                const auto apply=[&](uint64_t vs,uint64_t pixel,Constants& c) {
                    const auto r=ApplyDrawJitter(vs,pixel,frame,true,true,&anchor,19,extent,c.data(),ps.data());
                    Check(r.applied && !r.shadowCompensated && ps==Constants{},"static layer applies only position jitter");
                    for (int mutation=0;mutation<2;++mutation)
                    {
                        auto rejected=original;
                        if (!mutation) rejected[28]^=1;
                        const auto before=rejected;
                        const auto failure=ApplyDrawJitter(vs,pixel,frame,true,true,&anchor,mutation?20:19,
                            extent,rejected.data(),ps.data());
                        Check(!failure.applied && rejected==before && failure.rejection==
                            (mutation?JitterRejection::DepthMismatch:JitterRejection::CameraMismatch),
                            "recognized static layer rejects other camera/allocation without writes");
                    }
                };
                apply(0x3c86f4a89d220ee8ull,0xec90bdc0d9da2ad4ull,material);
                apply(0xf3b9f20b3d3a62d5ull,0x1ed63d51dfbc8863ull,lightA);
                apply(0xe7b38eb08c70e5e1ull,0x1c7eb2610da60b50ull,lightB);
                // Other constants (including world, light/view ray c11-c12)
                // cannot change when the position bank is uploaded.
                for (unsigned i=0;i<material.size();++i)
                    if (i<28 || i>=44)
                        Check(material[i]==original[i] && lightA[i]==original[i] && lightB[i]==original[i],
                            "static lighting constants outside VP remain unchanged");
                for (const auto local : {Float4{-250,-100,20,1},Float4{120,90,80,1},Float4{10,250,160,1}})
                {
                    const auto d=BattleDepth(depth,local),m=BattleMaterial(material,local);
                    const auto a=BattleLight08dc(lightA,local),b=BattleLight08dc(lightB,local);
                    const auto old=BattleMaterial(original,local);
                    Check(d==m && m==a && a==b,"captured static HLSL depth/material/light chains agree");
                    Check(m[2]==old[2] && m[3]==old[3],"static layer clip Z/W and material PS depth remain unchanged");
                    Check(m[0]/m[3]==d[0]/d[3] && m[1]/m[3]==d[1]/d[3] &&
                        a[0]/a[3]==d[0]/d[3] && b[1]/b[3]==d[1]/d[3],
                        "retained clip varyings project onto the same jittered depth grid");
                    legacySeparation=std::max({legacySeparation,
                        std::abs(double(m[0])/m[3]-double(old[0])/old[3])*extent.width*.5,
                        std::abs(double(m[1])/m[3]-double(old[1])/old[3])*extent.height*.5});
                }
            }
    Check(legacySeparation>.3,"static fixture exposes original layer separation");
    std::printf("Captured static layers: %u checks, 32 phases, two worlds, two sizes; legacy separation %.6f pixels\n",checks,legacySeparation);
}


// f5997 5f0/310 use the exact TireMaterialFf9 operation chain (temporary
// register renaming only). a9dd/5d98 retain the same swizzles with c8-c11 VP.
static Float4 F5997LayerC8(const Constants& c, Float4 r6)
{
    r6[3]=1;
    auto r2=Mul(r6[3],S(C(c,3),"xywz"));
    r2=Mad(r6[2],S(C(c,2),"wxzy"),S(r2,"zxwy"));
    r2=Mad(r6[1],S(C(c,1),"zwyx"),S(r2,"zxwy"));
    r6=Mad(r6[0],S(C(c,0),"yzxw"),S(r2,"zxwy"));
    r2=Mul(r6[3],C(c,11));
    r2=Mad(r6[1],C(c,10),r2);
    r2=Mad(r6[0],C(c,9),r2);
    return Mad(r6[2],C(c,8),r2);
}
// e242 has a distinct wxyz world path and x,w,z,y camera accumulation.
static Float4 F5997LayerE242(const Constants& c, Float4 r6)
{
    r6[3]=1;
    auto r0=Mul(r6[3],S(C(c,3),"wxyz"));
    r0=Mad(r6[2],S(C(c,2),"wxyz"),r0);
    r0=Mad(r6[1],S(C(c,1),"wxyz"),r0);
    r6=Mad(r6[0],S(C(c,0),"wxyz"),r0);
    r0=Mul(r6[0],C(c,10));
    r0=Mad(r6[3],C(c,9),r0);
    r0=Mad(r6[2],C(c,8),r0);
    return Mad(r6[1],C(c,7),r0);
}
static void CapturedF5997Layers()
{
    double legacySeparation=0;
    for (const auto& draw:f5997_capture::draws)
    {
        double drawSeparation=0;
        Constants original{}, originalPs{};
        std::copy(draw.vertex.begin(),draw.vertex.end(),original.begin());
        std::copy(draw.pixel.begin(),draw.pixel.end(),originalPs.begin());
        std::array<uint32_t,16> vp{};
        std::copy_n(original.begin()+draw.slot*4,16,vp.begin());
        const auto path=draw.vs==0xe242d31a3f1acdc4ull?F5997LayerE242:
            draw.slot==8?F5997LayerC8:TireMaterialFf9;
        for (const auto extent:{Viewport{0,0,1280,720},Viewport{0,0,3840,2160}})
            for (uint64_t phase=0;phase<32;++phase)
            {
                const SceneAnchor anchor{vp,extent,0x10000};
                auto layer=original,ps=originalPs;
                Constants depth{};
                std::copy_n(original.begin(),16,depth.begin());
                std::copy(vp.begin(),vp.end(),depth.begin()+16);
                Check(ApplyDrawJitter(0xb030ab4e17a20783ull,0,phase,true,true,&anchor,
                    0x10000,extent,depth.data(),ps.data()).applied,"f5997 depth camera accepted");
                const auto applied=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                    0x10000,extent,layer.data(),ps.data());
                Check(applied.applied && !applied.shadowCompensated && ps==originalPs,
                    "f5997 layer applies position jitter without changing paired PS");
                for (unsigned i=0;i<layer.size();++i)
                    if (i<draw.slot*4 || i>=(draw.slot+4)*4 || i%4>=2)
                        Check(layer[i]==original[i],"f5997 non-position constants and VP ZW preserved");
                for (const auto local:{Float4{-250,-100,20,1},Float4{120,90,80,1},Float4{10,250,160,1}})
                {
                    const auto d=TireDepthB030(depth,local),m=path(layer,local),old=path(original,local);
                    Check(d==m,"f5997 independently transcribed material and depth chains agree");
                    Check(m[2]==old[2] && m[3]==old[3],"f5997 clip ZW preserved");
                    drawSeparation=std::max({drawSeparation,
                        std::abs(double(m[0])/m[3]-double(old[0])/old[3])*extent.width*.5,
                        std::abs(double(m[1])/m[3]-double(old[1])/old[3])*extent.height*.5});
                }
                for (unsigned mutation=0;mutation<2;++mutation)
                {
                    auto rejected=original;
                    if (!mutation) rejected[draw.slot*4]^=1;
                    const auto before=rejected;
                    auto rejectedPs=originalPs;
                    const auto result=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                        mutation?0x10001:0x10000,extent,rejected.data(),rejectedPs.data());
                    Check(!result.applied && rejected==before && rejectedPs==originalPs &&
                        result.rejection==(mutation?JitterRejection::DepthMismatch:JitterRejection::CameraMismatch),
                        "f5997 other camera or depth allocation rejected without writes");
                }
            }
        Check(drawSeparation>.3,"f5997 omitted-jitter negative control exposes each draw separation");
        legacySeparation=std::max(legacySeparation,drawSeparation);
    }
    std::printf("Captured f5997 layers: %u checks, nine draws, five shaders, 32 phases, 720p/4K; legacy separation %.6f pixels\n",checks,legacySeparation);
}

// Reviewed f5912 HLSL: 52e position chain is exactly TireDepthB030;
// 799c/eeae exactly F5997LayerC8, and 97b5 exactly TireMaterialFf9.
// Temporary register names differ; world swizzles and accumulation order do not.
static void CapturedF5912Layers()
{
    double legacySeparation=0;
    for (const auto& draw:f5912_capture::draws)
    {
        double drawSeparation=0;
        Constants original{}, originalPs{};
        std::copy(draw.vertex.begin(),draw.vertex.end(),original.begin());
        std::copy(draw.pixel.begin(),draw.pixel.end(),originalPs.begin());
        std::array<uint32_t,16> vp{};
        std::copy_n(original.begin()+draw.slot*4,16,vp.begin());
        const auto path=draw.slot==4?TireDepthB030:
            draw.slot==8?F5997LayerC8:TireMaterialFf9;
        for (const auto extent:{Viewport{0,0,1920,1080},Viewport{0,0,3840,2160}})
            for (uint64_t phase=0;phase<32;++phase)
            {
                const SceneAnchor anchor{vp,extent,0x10000};
                auto layer=original,ps=originalPs;
                Constants depth{};
                std::copy(draw.depth.begin(),draw.depth.end(),depth.begin());
                Check(ApplyDrawJitter(draw.depthVs,0,phase,true,true,&anchor,
                    0x10000,extent,depth.data(),ps.data()).applied,"f5912 depth camera accepted");
                const auto applied=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                    0x10000,extent,layer.data(),ps.data());
                Check(applied.applied && !applied.shadowCompensated && ps==originalPs,
                    "f5912 layer applies position jitter without changing paired PS");
                for (unsigned i=0;i<layer.size();++i)
                    if (i<draw.slot*4 || i>=(draw.slot+4)*4 || i%4>=2)
                        Check(layer[i]==original[i],"f5912 non-position constants and VP ZW preserved");
                for (const auto local:{Float4{-250,-100,20,1},Float4{120,90,80,1},Float4{10,250,160,1}})
                {
                    const auto d=TireDepthB030(depth,local),m=path(layer,local),old=path(original,local);
                    Check(d==m,"f5912 independently transcribed material and depth chains agree");
                    Check(m[2]==old[2] && m[3]==old[3],"f5912 clip ZW preserved");
                    drawSeparation=std::max({drawSeparation,
                        std::abs(double(m[0])/m[3]-double(old[0])/old[3])*extent.width*.5,
                        std::abs(double(m[1])/m[3]-double(old[1])/old[3])*extent.height*.5});
                }
                for (unsigned mutation=0;mutation<2;++mutation)
                {
                    auto rejected=original;
                    if (!mutation) rejected[draw.slot*4]^=1;
                    const auto before=rejected;
                    auto rejectedPs=originalPs;
                    const auto result=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                        mutation?0x10001:0x10000,extent,rejected.data(),rejectedPs.data());
                    Check(!result.applied && rejected==before && rejectedPs==originalPs &&
                        result.rejection==(mutation?JitterRejection::DepthMismatch:JitterRejection::CameraMismatch),
                        "f5912 other camera or depth allocation rejected without writes");
                }
            }
        Check(drawSeparation>.3,"f5912 omitted-jitter negative control exposes each draw separation");
        legacySeparation=std::max(legacySeparation,drawSeparation);
    }
    std::printf("Captured f5912 layers: %u checks, 12 draws across three frames, four shaders, 32 phases, 1080p/4K; legacy separation %.6f pixels\n",checks,legacySeparation);
}

// Exact position instruction chains reviewed in f16385 HLSL: fe3efe uses
// TireDepthB030; 1474 uses TireMaterialFf9, with clip retained in o4.
// Neither path uses camera constants for alpha UV or vertex color.
static void CapturedF16385Layers()
{
    double legacySeparation=0;
    for (const auto& draw:f16385_capture::draws)
    {
        double drawSeparation=0;
        Constants original{}, originalPs{};
        std::copy(draw.vertex.begin(),draw.vertex.end(),original.begin());
        std::copy(draw.pixel.begin(),draw.pixel.end(),originalPs.begin());
        std::array<uint32_t,16> vp{};
        std::copy_n(original.begin()+draw.slot*4,16,vp.begin());
        const auto path=draw.slot==4?TireDepthB030:
            TireMaterialFf9;
        for (const auto extent:{Viewport{0,0,1920,1080},Viewport{0,0,3840,2160}})
            for (uint64_t phase=0;phase<32;++phase)
            {
                const SceneAnchor anchor{vp,extent,0x10000};
                auto layer=original,ps=originalPs;
                Constants depth{};
                std::copy(draw.depth.begin(),draw.depth.end(),depth.begin());
                Check(ApplyDrawJitter(draw.depthVs,0,phase,true,true,&anchor,
                    0x10000,extent,depth.data(),ps.data()).applied,"f16385 depth camera accepted");
                const auto applied=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                    0x10000,extent,layer.data(),ps.data());
                Check(applied.applied && !applied.shadowCompensated && ps==originalPs,
                    "f16385 layer applies position jitter without changing paired PS");
                for (unsigned i=0;i<layer.size();++i)
                    if (i<draw.slot*4 || i>=(draw.slot+4)*4 || i%4>=2)
                        Check(layer[i]==original[i],"f16385 non-position constants and VP ZW preserved");
                for (const auto local:{Float4{-250,-100,20,1},Float4{120,90,80,1},Float4{10,250,160,1}})
                {
                    const auto d=TireDepthB030(depth,local),m=path(layer,local),old=path(original,local);
                    Check(d==m,"f16385 independently transcribed material and depth chains agree");
                    Check(m[2]==old[2] && m[3]==old[3],"f16385 clip ZW preserved");
                    drawSeparation=std::max({drawSeparation,
                        std::abs(double(m[0])/m[3]-double(old[0])/old[3])*extent.width*.5,
                        std::abs(double(m[1])/m[3]-double(old[1])/old[3])*extent.height*.5});
                }
                for (unsigned mutation=0;mutation<2;++mutation)
                {
                    auto rejected=original;
                    if (!mutation) rejected[draw.slot*4]^=1;
                    const auto before=rejected;
                    auto rejectedPs=originalPs;
                    const auto result=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                        mutation?0x10001:0x10000,extent,rejected.data(),rejectedPs.data());
                    Check(!result.applied && rejected==before && rejectedPs==originalPs &&
                        result.rejection==(mutation?JitterRejection::DepthMismatch:JitterRejection::CameraMismatch),
                        "f16385 other camera or depth allocation rejected without writes");
                }
            }
        Check(drawSeparation>.3,"f16385 omitted-jitter negative control exposes each draw separation");
        legacySeparation=std::max(legacySeparation,drawSeparation);
    }
    std::printf("Captured f16385 layers: %u checks, 12 draws across three frames, two shaders, 32 phases, 1080p/4K; legacy separation %.6f pixels\n",checks,legacySeparation);
}

// The f2548 pre-resolve depth geometry uses f7fd c4-c7. Its matching textured
// material draws use a027 c7-c10 or ff769 c8-c11 for clip position. Their
// world/fetch95/index geometry and camera bits match the paired depth draws.
// The reference functions transcribe the captured HLSL instruction order.
static Float4 Battle8dClip(const Constants& c, Float4 r6);
static void CapturedF2548Layers()
{
    double legacySeparation = 0;
    for (const auto& draw : f2548_capture::draws)
    {
        Constants original{}, originalPs{}, originalDepth{};
        std::copy(draw.vertex.begin(), draw.vertex.end(), original.begin());
        std::copy(draw.vertexLate.begin(), draw.vertexLate.end(), original.begin()+254*4);
        std::copy(draw.pixel.begin(), draw.pixel.end(), originalPs.begin());
        std::copy(draw.depth.begin(), draw.depth.end(), originalDepth.begin());
        std::array<uint32_t,16> vp{};
        std::copy_n(original.begin() + draw.slot*4, 16, vp.begin());
        Check(draw.depthVs == 0xf7fd88506d704a3dull &&
              std::equal(vp.begin(), vp.end(), originalDepth.begin()+16),
              "f2548 material and depth share the captured camera");
        const auto materialPath = draw.slot == 7 ? TireLightA27 : Battle8dClip;
        for (const auto extent : {Viewport{0,0,1280,720}, Viewport{0,0,3840,2160}})
            for (uint64_t phase = 0; phase < 32; ++phase)
            {
                const SceneAnchor anchor{vp,extent,54};
                auto layer = original, depth = originalDepth, ps = originalPs;
                Check(ApplyDrawJitter(draw.depthVs,0,phase,true,true,&anchor,
                    54,extent,depth.data(),ps.data()).applied,
                    "f2548 paired depth accepts scene camera");
                const auto applied = ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                    54,extent,layer.data(),ps.data());
                Check(applied.applied && !applied.shadowCompensated && ps == originalPs,
                    "f2548 material changes position without changing pixel constants");
                for (unsigned i = 0; i < layer.size(); ++i)
                    if (i < draw.slot*4 || i >= (draw.slot+4)*4 || i%4 >= 2)
                        Check(layer[i] == original[i],
                            "f2548 UV, world, lighting, and clip ZW constants are preserved");
                for (const auto local : {Float4{-250,-100,20,1}, Float4{120,90,80,1},
                                         Float4{10,250,160,1}})
                {
                    const auto d = TireDepthB030(depth,local);
                    const auto m = materialPath(layer,local);
                    const auto old = materialPath(original,local);
                    Check(d == m, "f2548 independently transcribed depth/material clips agree");
                    Check(m[2] == old[2] && m[3] == old[3],
                        "f2548 material jitter preserves depth and W");
                    legacySeparation = std::max({legacySeparation,
                        std::abs(double(d[0])/d[3]-double(old[0])/old[3])*extent.width*.5,
                        std::abs(double(d[1])/d[3]-double(old[1])/old[3])*extent.height*.5});
                }
                for (unsigned mutation = 0; mutation < 2; ++mutation)
                {
                    auto rejected = original, rejectedPs = originalPs;
                    if (!mutation) rejected[draw.slot*4] ^= 1;
                    const auto before = rejected;
                    const auto result = ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                        mutation ? 55 : 54,extent,rejected.data(),rejectedPs.data());
                    Check(!result.applied && rejected == before && rejectedPs == originalPs &&
                        result.rejection == (mutation ? JitterRejection::DepthMismatch :
                                              JitterRejection::CameraMismatch),
                        "f2548 mismatched camera or depth rejects without writes");
                }
            }
    }
    Check(legacySeparation > .3,
        "f2548 unjittered material separates from jittered depth in negative control");
    std::printf("Captured f2548 layers: %u checks, 11 draws, 32 phases, 720p/4K; legacy separation %.6f pixels\n",
        checks,legacySeparation);
}

// Compact cumulative-register reconstructions (original capture files and
// SHA-256 in out/streamline-fg-p0/fsr-p2-battle-cpu-01/source-samples.json).
// f1991 draw699 and f2163 draw540: c0..3 world, c7 UV, c8..11 VP, c12 light.
struct Battle8dSample {
    const char* source;
    std::array<uint32_t,16> vp;
    std::array<uint32_t,24> other;
    std::array<uint32_t,8> ps;
};
static constexpr std::array<Battle8dSample,2> battle8d{{
    {"f1991/d699",
     {0x3f636cc9,0xbeda98bb,0xbe2cdeff,0x4194f6af,0x3ed6c0e1,0x3f677eb1,0xbda33d39,0x410ca9eb,
      0x3e3f2ba7,0,0x3f7b7fce,0xc1d802f0,0x3e3ebdea,0xbf2b593a,0xbf381fd5,0x428b3729},
     {0x40400000,0x40400000,0x40400000,0x40400000,0,0xbf800000,0,0,
      0x3f800000,0,0,0,0,0,0x3f800000,0,0x80000000,0x80000000,0x3f800000,0,
      0x3ebecc0b,0x3f39b8fd,0xbf14210c,0x425a779b},
     {0,0,0,0x3f800000,0x3f800000,0x3f000000,0,0}},
    {"f2163/d540",
     {0x3f7e1781,0,0xbdf996a3,0x42462fb4,0,0x3f800000,0,0,
      0x3df996a3,0,0x3f7e1781,0xc1c48aee,0x3f800000,0,0,0},
     {0x3f800000,0x3f800000,0x3f800000,0x3f800000,0xbf326e98,0xb382daa5,0,0,
      0x3382daa5,0xbf326e98,0,0,0,0,0x3f326e98,0,0,0,0x3fb7a4ea,0x80000000,
      0,0x3f800000,0,0},
     {0,0,0,0x3f800000,0x40000000,0x3f000000,0xbf800000,0x3f800000}}
}};

// f5446..f5448 draw11: c1..4 world, c5..7 selected skin-palette
// samples, c230..233 VP and c255 select flags. This CPU fixture starts at
// controlled post-skin registers; full bone palette/weights are GPU-lane scope.
struct Battle4bdSample {
    const char* source;
    std::array<uint32_t,16> vp;
    std::array<uint32_t,32> other;
    std::array<uint32_t,8> ps;
};
static constexpr std::array<Battle4bdSample,3> battle4bd{{
    {"f5446/d11",
     {0xbf07a1ae,0x3e6bea93,0x3f6fcd2e,0x3f700aa1,0x3fbd1fc5,0x3da9305b,0x3eabf9a4,0x3eac25b6,
      0,0x4031e81c,0xbdb372bc,0xbdb3a0b8,0x431f1fa5,0xc39290f9,0x434fca00,0x4359ff3f},
     {0xbf7fff6b,0x3b8a3b29,0,0,0xbb8a3b29,0xbf7fff6b,0,0,0,0,0x3f800000,0,
      0x43b2be41,0x40f85762,0,0x3f800000,0x3f7f9ff0,0x3d500000,0x3c996b36,0xbfef1eb0,
      0xbd46fc35,0x3f7e5207,0xbdd407e8,0x412f7af4,0xbcc37b29,0x3dd1db57,0x3f7e9445,0xc178dc50,
      0,0x3f800000,0x3f000000,0xbf800000},
     {0x40400000,0x40400000,0,0,0,0x3f000000,0x3f800000,0}},
    {"f5447/d11",
     {0xbf07a1ae,0x3e6bea93,0x3f6fcd2e,0x3f700aa1,0x3fbd1fc5,0x3da9305b,0x3eabf9a4,0x3eac25b6,
      0,0x4031e81c,0xbdb372bc,0xbdb3a0b8,0x431eff6f,0xc3928a10,0x43500232,0x435a3780},
     {0xbf7fff6b,0x3b8a3b29,0,0,0xbb8a3b29,0xbf7fff6b,0,0,0,0,0x3f800000,0,
      0x43b21abe,0x40dee3c2,0,0x3f800000,0x3f7f9b08,0x3d5116b6,0x3cb23968,0xc00b13dc,
      0xbd473641,0x3f7e6b00,0xbdcc594f,0x41296fb2,0xbcdad935,0x3dc9ddf4,0x3f7ea95b,0xc1817d04,
      0,0x3f800000,0x3f000000,0xbf800000},
     {0x40400000,0x40400000,0,0,0,0x3f000000,0x3f800000,0}},
    {"f5448/d11",
     {0xbf07a1ae,0x3e6bea93,0x3f6fcd2e,0x3f700aa1,0x3fbd1fc5,0x3da9305b,0x3eabf9a4,0x3eac25b6,
      0,0x4031e81c,0xbdb372bc,0xbdb3a0b8,0x431edffe,0xc3928351,0x4350390f,0x435a6e6b},
     {0xbf7fff6b,0x3b8a3b29,0,0,0xbb8a3b29,0xbf7fff6b,0,0,0,0,0x3f800000,0,
      0x43b09bd1,0x40acbcb1,0,0x3f800000,0x3f7f9293,0x3d525913,0x3cd8a699,0xc02950f8,
       0xbd479603,0x3f7e9c89,0xbdbc321e,0x411cba25,0xbcfe226b,0x3db93e0b,0x3f7ed3b1,0xc18eb5b5,
      0,0x3f800000,0x3f000000,0xbf800000},
     {0x40400000,0x40400000,0,0,0,0x3f000000,0x3f800000,0}}
}};

// Independent, final shader swizzles (no Transform/PositionVPSlot in oracle).
static Float4 Battle8dClip(const Constants& c, Float4 r6)
{
    r6[3]=1;
    auto r4=Mul(r6[3],S(C(c,3),"xywz"));
    r4=Mad(r6[2],S(C(c,2),"wxzy"),S(r4,"zxwy"));
    r4=Mad(r6[1],S(C(c,1),"zwyx"),S(r4,"zxwy"));
    r6=Mad(r6[0],S(C(c,0),"yzxw"),S(r4,"zxwy"));
    r4=Mul(r6[3],C(c,11));
    r4=Mad(r6[1],C(c,10),r4);
    r4=Mad(r6[0],C(c,9),r4);
    return Mad(r6[2],C(c,8),r4); // oPos and o2; c7 UV is independent.
}

struct LateFloorOutput { Float4 position, clipCopy; std::array<float,2> materialUv; };
// f6131 VS 2078 copies the c8..11 result to both oPos and o5. Its separate
// r0.xy*c7.xy+c7.wz calculation feeds o0; the PS samples tex0 from i5, not o0.
static LateFloorOutput LateFloor2078(const Constants& c, Float4 local,
    std::array<float,2> fetchedUv)
{
    const auto clip=Battle8dClip(c,local);
    const auto uv=C(c,7);
    return {clip,clip,{fetchedUv[0]*uv[0]+uv[3],fetchedUv[1]*uv[1]+uv[2]}};
}
// f6131 PS 4013: rcp(i5.w), multiply i5.xy, r6.yx*c0.yx+c0.zw,
// then sample tex0 at r6.yx. Keep this independent of the VS upload helper.
static std::array<float,2> LateFloorTex0(const Constants& ps, const Float4& i5)
{
    const float reciprocal=1.f/i5[3];
    const float x=reciprocal*i5[0], y=reciprocal*i5[1];
    const auto c0=C(ps,0);
    const float r6x=y*c0[1]+c0[2], r6y=x*c0[0]+c0[3];
    return {r6y,r6x};
}
static void CapturedF6131LateFloor()
{
    double oldClipSeparation=0, oldTex0Separation=0, maxDepthError=0;
    for (const auto& draw:f6131_late_floor_capture::draws)
    {
        Constants original{}, originalDepth{}, originalPs{};
        std::copy(draw.vertex.begin(),draw.vertex.end(),original.begin());
        std::copy(draw.vertexLate.begin(),draw.vertexLate.end(),original.begin()+254*4);
        std::copy(draw.depth.begin(),draw.depth.end(),originalDepth.begin());
        std::copy(draw.pixel.begin(),draw.pixel.end(),originalPs.begin());
        std::array<uint32_t,16> vp{};
        std::copy_n(original.begin()+8*4,16,vp.begin());
        Check(draw.vs==0x2078ccaa70d44732ull && draw.ps==0x4013372b6413788full &&
            draw.depthVs==0xf7fd88506d704a3dull && draw.slot==8 &&
            std::equal(vp.begin(),vp.end(),originalDepth.begin()+4*4),
            "f6131 late floor uses the captured depth camera and reviewed shader pair");
        for (const auto extent:{Viewport{0,0,2560,1440},Viewport{0,0,3840,2160}})
            for (uint64_t phase=0;phase<32;++phase)
            {
                const SceneAnchor anchor{vp,extent,54};
                auto late=original,depth=originalDepth,ps=originalPs,depthPs=originalPs;
                Check(ApplyDrawJitter(draw.depthVs,0,phase,true,true,&anchor,54,extent,
                    depth.data(),depthPs.data()).applied,"f6131 depth camera accepts jitter");
                const auto result=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                    54,extent,late.data(),ps.data());
                Check(result.applied && result.slot==8 && !result.shadowCompensated &&
                    ps==originalPs,"f6131 late draw jitters slot 8 without changing PS constants");
                for (unsigned i=0;i<late.size();++i)
                    if (i<32 || i>=48 || i%4>=2)
                        Check(late[i]==original[i],"f6131 world, UV, lighting and clip ZW constants stay exact");
                for (const auto local:{Float4{-250,-100,20,1},Float4{120,90,80,1},
                    Float4{10,250,160,1}})
                {
                    const auto d=TireDepthB030(depth,local);
                    const auto current=LateFloor2078(late,local,{.125f,.875f});
                    const auto old=LateFloor2078(original,local,{.125f,.875f});
                    Check(d==current.position && current.clipCopy==current.position,
                        "f6131 independent late clip and copied i5 align with paired depth");
                    Check(current.position[2]==old.position[2] && current.position[3]==old.position[3],
                        "f6131 late draw preserves clip Z and W");
                    Check(current.materialUv==old.materialUv,
                        "f6131 c7 material UV is independent of position jitter");
                    Check(std::isfinite(d[3]) && std::abs(d[3])>1,
                        "f6131 synthetic point has nondegenerate clip W");
                    const auto currentLookup=LateFloorTex0(ps,current.clipCopy);
                    const auto depthLookup=LateFloorTex0(originalPs,d);
                    const auto oldLookup=LateFloorTex0(originalPs,old.clipCopy);
                    for (unsigned axis=0;axis<2;++axis)
                    {
                        const double dimension=axis?extent.height:extent.width;
                        // Host viewport Y and captured PS c0.y both point down.
                        const double clipPixels=(double(d[axis])/d[3]-
                            double(old.position[axis])/old.position[3])*dimension*(axis?-.5:.5);
                        const double tex0Pixels=(double(currentLookup[axis])-
                            double(oldLookup[axis]))*dimension;
                        maxDepthError=std::max(maxDepthError,
                            std::abs((double(currentLookup[axis])-depthLookup[axis])*dimension));
                        oldClipSeparation=std::max(oldClipSeparation,std::abs(clipPixels));
                        oldTex0Separation=std::max(oldTex0Separation,std::abs(tex0Pixels));
                        Check(std::abs((double(currentLookup[axis])-depthLookup[axis])*dimension)<.003,
                            "f6131 clip-derived tex0 samples the paired depth pixel");
                        Check(std::abs(tex0Pixels-clipPixels)<.003,
                            "f6131 tex0 lookup follows the independent clip pixel shift");
                    }
                }
                for (unsigned mutation=0;mutation<2;++mutation)
                {
                    auto rejected=original,rejectedPs=originalPs;
                    if (!mutation) rejected[8*4]^=1;
                    const auto before=rejected;
                    const auto failure=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                        mutation?55:54,extent,rejected.data(),rejectedPs.data());
                    Check(!failure.applied && rejected==before && rejectedPs==originalPs &&
                        failure.rejection==(mutation?JitterRejection::DepthMismatch:
                            JitterRejection::CameraMismatch),
                        "f6131 camera or depth mismatch rejects without changing constants");
                }
            }
    }
    Check(oldClipSeparation>.3 && oldTex0Separation>.3,
        "f6131 unmapped late draw separates from depth and tex0 by a visible subpixel phase");
    std::printf("Captured f6131 late floor: %u checks, five draws, 32 phases, 1440p/4K; old clip %.6f px, tex0 %.6f px, depth lookup error %.6f px\n",
        checks,oldClipSeparation,oldTex0Separation,maxDepthError);
}

// f6814 VS e9b8/d31e transcribe the battle 8d path: world c0..3, camera c8..11,
// result copied to oPos and o2/o5. The 52e4 anchor uses the b030 c4..7 path.
// d31e's PS 4907 uses the f6131 rcp(i5.w)*i5.xy*c0.yx+c0.zw tex0 lookup.
static void CapturedF6814Cutscene()
{
    using namespace f6814_cutscene_capture;
    std::array<uint32_t,16> vp{};
    std::copy_n(anchor.begin()+4*4,16,vp.begin());
    Check(PositionVPSlot(0x52e4405f97159d2full)==4 &&
        std::equal(vp.begin(),vp.end(),producerCamera.begin()),
        "f6814 tex0 producer and 52e4 depth anchor share the captured scene camera");
    double oldClipSeparation=0, oldTex0Separation=0, maxLookupError=0;
    for (const auto& draw:draws)
    {
        Constants original{}, originalPs{}, originalDepth{};
        std::copy(draw.vertex.begin(),draw.vertex.end(),original.begin());
        std::copy(draw.vertexLate.begin(),draw.vertexLate.end(),original.begin()+254*4);
        std::copy(draw.pixel.begin(),draw.pixel.end(),originalPs.begin());
        Check(draw.slot==8 && PositionVPSlot(draw.vs)==8 && DrawPositionVPSlot(draw.vs,draw.ps)==8 &&
            std::equal(vp.begin(),vp.end(),original.begin()+8*4),
            "f6814 late draw uses the captured scene camera at c8..11");
        // The anchor camera with this draw's world rows: the depth this draw tests.
        std::copy(anchor.begin(),anchor.end(),originalDepth.begin());
        std::copy_n(original.begin(),16,originalDepth.begin());
        const bool samplesScene=draw.ps==0x4907455386b2b291ull;
        for (const auto extent:{Viewport{0,0,2560,1440},Viewport{0,0,3840,2160}})
            for (uint64_t phase=0;phase<32;++phase)
            {
                const SceneAnchor sceneAnchor{vp,extent,14};
                auto late=original,depth=originalDepth,ps=originalPs,depthPs=originalPs;
                Check(ApplyDrawJitter(0x52e4405f97159d2full,0,phase,true,true,&sceneAnchor,14,extent,
                    depth.data(),depthPs.data()).applied,"f6814 52e4 depth camera accepts jitter");
                const auto result=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&sceneAnchor,
                    14,extent,late.data(),ps.data());
                Check(result.applied && result.slot==8 && !result.shadowCompensated &&
                    ps==originalPs,"f6814 late draw jitters slot 8 without changing PS constants");
                for (unsigned i=0;i<late.size();++i)
                    if (i<32 || i>=48 || i%4>=2)
                        Check(late[i]==original[i],"f6814 world, UV, lighting and clip ZW constants stay exact");
                for (const auto local:{Float4{-250,-100,20,1},Float4{120,90,80,1},
                    Float4{10,250,160,1}})
                {
                    const auto d=TireDepthB030(depth,local);
                    const auto current=Battle8dClip(late,local);
                    const auto old=Battle8dClip(original,local);
                    Check(d==current,"f6814 late clip and its o2/o5 copy align with the jittered depth");
                    Check(current[2]==old[2] && current[3]==old[3],
                        "f6814 late draw preserves clip Z and W");
                    Check(std::isfinite(d[3]) && std::abs(d[3])>1,
                        "f6814 synthetic point has nondegenerate clip W");
                    for (unsigned axis=0;axis<2;++axis)
                    {
                        const double dimension=axis?extent.height:extent.width;
                        const double clipPixels=(double(d[axis])/d[3]-
                            double(old[axis])/old[3])*dimension*(axis?-.5:.5);
                        oldClipSeparation=std::max(oldClipSeparation,std::abs(clipPixels));
                        if (!samplesScene) continue;
                        // tex0 is the jittered slot-0 producer resolve with this camera.
                        const auto currentLookup=LateFloorTex0(ps,current);
                        const auto producerLookup=LateFloorTex0(originalPs,d);
                        const auto oldLookup=LateFloorTex0(originalPs,old);
                        const double tex0Pixels=(double(currentLookup[axis])-
                            double(oldLookup[axis]))*dimension;
                        const double lookupError=std::abs((double(currentLookup[axis])-
                            producerLookup[axis])*dimension);
                        maxLookupError=std::max(maxLookupError,lookupError);
                        oldTex0Separation=std::max(oldTex0Separation,std::abs(tex0Pixels));
                        Check(lookupError<.003,"f6814 d31e tex0 samples the jittered producer pixel");
                        Check(std::abs(tex0Pixels-clipPixels)<.003,
                            "f6814 d31e tex0 lookup follows the clip pixel shift");
                    }
                }
                for (unsigned mutation=0;mutation<2;++mutation)
                {
                    auto rejected=original,rejectedPs=originalPs;
                    if (!mutation) rejected[8*4]^=1;
                    const auto before=rejected;
                    const auto failure=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&sceneAnchor,
                        mutation?15:14,extent,rejected.data(),rejectedPs.data());
                    Check(!failure.applied && rejected==before && rejectedPs==originalPs &&
                        failure.rejection==(mutation?JitterRejection::DepthMismatch:
                            JitterRejection::CameraMismatch),
                        "f6814 camera or depth mismatch rejects without changing constants");
                }
            }
    }
    Check(oldClipSeparation>.3 && oldTex0Separation>.3,
        "f6814 unmapped draws separate from depth and tex0 by a visible subpixel phase");
    std::printf("Captured f6814 cutscene: %u checks, e9b8/d31e, 32 phases, 1440p/4K; old clip %.6f px, tex0 %.6f px, lookup error %.6f px\n",
        checks,oldClipSeparation,oldTex0Separation,maxLookupError);
}

// f6131 VS e810 lines 405-422: fetched position through c0..3, then the
// distinct c7..10 camera accumulation. The result is copied to oPos and o4.
static Float4 E810Clip(const Constants& c, Float4 point)
{
    point[3]=1;
    auto world=Mul(point[3],S(C(c,3),"wxyz"));
    world=Mad(point[2],S(C(c,2),"wxyz"),world);
    world=Mad(point[1],S(C(c,1),"wxyz"),world);
    world=Mad(point[0],S(C(c,0),"wxyz"),world);
    auto clip=Mul(world[0],C(c,10));
    clip=Mad(world[3],C(c,9),clip);
    clip=Mad(world[2],C(c,8),clip);
    return Mad(world[1],C(c,7),clip);
}
// f6131 PS fe31 lines 404-410: i4.xy/w -> c0 YX scale/bias -> tex0 YX.
static std::array<float,2> E810Tex0(const Constants& ps, Float4 i4)
{
    const float inverseW=1.f/i4[3];
    const Float4 r5{inverseW*i4[0],inverseW*i4[1],0,0};
    const auto c0=C(ps,0);
    const Float4 swizzled{r5[1]*c0[1]+c0[2],r5[0]*c0[0]+c0[3],0,0};
    return {swizzled[1],swizzled[0]};
}
static void CapturedF6131E810ConstantSample()
{
    using namespace f6131_e810_capture;
    const auto& draw=draws[0];
    Constants original{},originalDepth{},originalPs{};
    std::copy(draw.vertex.begin(),draw.vertex.end(),original.begin());
    std::copy(draw.vertexLate.begin(),draw.vertexLate.end(),original.begin()+254*4);
    std::copy(draw.depth.begin(),draw.depth.end(),originalDepth.begin());
    std::copy(draw.pixel.begin(),draw.pixel.end(),originalPs.begin());
    std::array<uint32_t,16> vp{};
    std::copy_n(original.begin()+7*4,16,vp.begin());
    Check(draw.vs==0xe810cfacc107fd3cull && draw.ps==0xfe31f3d6588fde95ull &&
        draw.depthVs==0xb030ab4e17a20783ull && draw.slot==7 &&
        std::equal(vp.begin(),vp.end(),originalDepth.begin()+4*4),
        "f6131 e810 draw746 and depth178 share captured scene VP");
    // Exact texture0 fetch words from draw746. The guest 1x1 RGBA upload uses
    // base 0x17000, 2D fetch, repeat U/V. The no-resolve argument below is
    // a controlled test assumption; the old F1 trace cannot prove cache state.
    constexpr uint32_t fetch0=0x80400002u,fetch1=0x00017086u,fetch2=0u,fetch5=0x00000218u;
    Check(IsSingleTexelScreenFetch(fetch0,fetch1,fetch2,fetch5,false),
        "f6131 e810 texture0 qualifies as a constant one-texel screen sample");
    for (const auto invalid: {std::array<uint32_t,4>{fetch0,fetch1,fetch2,fetch5&~(3u<<9)},
        {fetch0,fetch1&0xfffu,fetch2,fetch5}, {fetch0,fetch1^1u,fetch2,fetch5},
        {fetch0,fetch1,1u,fetch5}, {fetch0|(4u<<10),fetch1,fetch2,fetch5},
        {fetch0|(4u<<13),fetch1,fetch2,fetch5}, {fetch0^3u,fetch1,fetch2,fetch5}})
        Check(!IsSingleTexelScreenFetch(invalid[0],invalid[1],invalid[2],invalid[3],false),
            "e810 wrong dimension, base, format, size, wrap or fetch type is rejected");
    Check(!IsSingleTexelScreenFetch(fetch0,fetch1,fetch2,fetch5,true),
        "e810 same-address resolved surface is not a constant guest upload");
    // The 2026-10-04 cutscene tour reviewed fe31 with the other e810 light
    // partners, so the pair no longer needs the constant-sample gate.
    Check(PositionVPSlot(draw.vs)==-1 && DrawPositionVPSlot(draw.vs,draw.ps,false)==7 &&
        DrawPositionVPSlot(draw.vs,draw.ps,true)==7 &&
        DrawPositionVPSlot(draw.vs,0x0123456789abcdefull,true)==-1,
        "e810 remains outside VS-wide map and accepts only reviewed PS pairs");
    double oldSeparation=0;
    for (const auto extent:{Viewport{0,0,2560,1440},Viewport{0,0,3840,2160}})
        for (uint64_t phase=0;phase<32;++phase)
        {
            const SceneAnchor anchor{vp,extent,54};
            auto depth=originalDepth,layer=original,ps=originalPs,depthPs=originalPs;
            Check(ApplyDrawJitter(draw.depthVs,0,phase,true,true,&anchor,54,extent,
                depth.data(),depthPs.data()).applied,"e810 paired depth accepts shared jitter");
            const auto result=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                54,extent,layer.data(),ps.data(),nullptr,nullptr,1,nullptr,true);
            Check(result.applied && result.slot==7 && !result.shadowCompensated && ps==originalPs,
                "eligible e810/fe31 upload jitters slot 7 without PS edits");
            for (unsigned i=0;i<layer.size();++i)
                if (i<7*4 || i>=11*4 || i%4>=2)
                    Check(layer[i]==original[i],"e810 leaves world, UV, lighting and clip ZW constants intact");
            for (const auto point:{Float4{-250,-100,20,1},Float4{120,90,80,1},
                Float4{10,250,160,1}})
            {
                const auto reference=TireDepthB030(depth,point);
                const auto current=E810Clip(layer,point),old=E810Clip(original,point);
                // HLSL copies the computed r0 to o4 unchanged; the PS uses it as i4.
                Check(current==reference,
                    "e810 independent oPos and copied o4 align with paired depth");
                Check(current[2]==old[2] && current[3]==old[3],
                    "e810 preserves clip Z and W");
                Check(std::isfinite(reference[3]) && std::abs(reference[3])>1,
                    "e810 synthetic point has nondegenerate clip W");
                const auto sample=E810Tex0(ps,current),oldSample=E810Tex0(originalPs,old);
                for (unsigned axis=0;axis<2;++axis)
                {
                    const double pixels=(double(current[axis])/current[3]-double(old[axis])/old[3])*
                        (axis?extent.height:extent.width)*(axis?-.5:.5);
                    const double samplePixels=(double(sample[axis])-oldSample[axis])*(axis?extent.height:extent.width);
                    Check(std::abs(samplePixels-pixels)<.003,
                        "e810 projected tex0 coordinate follows the same screen pixel shift");
                    oldSeparation=std::max(oldSeparation,std::abs(pixels));
                }
            }
            {
                auto rejected=original,rejectedPs=originalPs;
                const auto failure=ApplyDrawJitter(draw.vs,0x0123456789abcdefull,
                    phase,true,true,&anchor,54,extent,rejected.data(),rejectedPs.data(),
                    nullptr,nullptr,1,nullptr,true);
                Check(!failure.applied && failure.rejection==JitterRejection::UnknownShader &&
                    rejected==original && rejectedPs==originalPs,
                    "e810 with an unreviewed PS leaves both banks unchanged");
            }
        }
    Check(oldSeparation>.3,"unmapped e810 control separates from jittered depth by a visible phase");
    std::printf("Captured f6131 e810 constant sample: %u checks, draw746/depth178, 32 phases, 1440p/4K; old separation %.6f px\n",
        checks,oldSeparation);
}
// Captured sky material/depth pairs. f3449 VS bda41 and f1800 VS db23
// (HLSL lines 427-444 in both) have the same independently transcribed
// world/clip arithmetic as TireMaterialFf9; paired depth VS b030 and f7fd
// use TireDepthB030. Captured banks are exact; the local vertices are
// synthetic because the F1 trace does not include the vertex buffer. A case
// may replace the defaults with points that project on screen for its camera.
struct CapturedSkyCase { const char* label; uint64_t vs, ps, depthVs; unsigned draw, depthDraw;
    bool motionFallback; uint64_t otherSkyPs;
    std::array<Float4,3> locals{{{-250,-100,20,1},{120,90,80,1},{10,250,160,1}}}; };
template<class Draw>
static void CapturedSky(const CapturedSkyCase& expected,const Draw& draw)
{
    const auto message=[&](const char* text) { return std::string(expected.label)+" "+text; };
    Constants original{},originalDepth{},originalPs{};
    std::copy(draw.vertex.begin(),draw.vertex.end(),original.begin());
    std::copy(draw.vertexLate.begin(),draw.vertexLate.end(),original.begin()+254*4);
    std::copy(draw.depth.begin(),draw.depth.end(),originalDepth.begin());
    std::copy(draw.pixel.begin(),draw.pixel.end(),originalPs.begin());
    std::array<uint32_t,16> vp{};
    std::copy_n(original.begin()+7*4,16,vp.begin());
    Check(draw.vs==expected.vs && draw.ps==expected.ps &&
        draw.depthVs==expected.depthVs && draw.slot==7 &&
        draw.draw==expected.draw && draw.depthDraw==expected.depthDraw &&
        std::equal(original.begin(),original.begin()+16,originalDepth.begin()) &&
        std::equal(vp.begin(),vp.end(),originalDepth.begin()+4*4),
        message("sky and depth share captured world and scene camera").c_str());
    constexpr uint64_t unmatchedPs=0x12345678ull; // synthetic negative control
    Check(PositionVPSlot(draw.vs)==-1 && DrawPositionVPSlot(draw.vs,draw.ps)==7 &&
        DrawPositionVPSlot(draw.vs,unmatchedPs)==-1 && PositionVPSlot(draw.depthVs)==4,
        message("sky slot 7 is restricted to the reviewed VS/PS pair").c_str());
    Check(DrawPositionVPSlot(draw.vs,expected.otherSkyPs)==-1 &&
        !RequiresEarlierSceneAnchor(draw.vs,expected.otherSkyPs),
        message("a PS reviewed only with another VS does not authorize this VS").c_str());
    Check(RequiresEarlierSceneAnchor(draw.vs,draw.ps) &&
        !RequiresEarlierSceneAnchor(draw.vs,unmatchedPs) && !RequiresEarlierSceneAnchor(draw.depthVs,0),
        message("sky pair never self-anchors while its depth companion still can").c_str());
    Check(RetainsMotionFallback(draw.vs,draw.ps)==expected.motionFallback &&
        !RetainsMotionFallback(draw.vs,unmatchedPs) && !RetainsMotionFallback(draw.depthVs,0),
        message("sky pair keeps its reviewed object-motion policy").c_str());
    const Viewport extent{0,0,3840,2160}; // captured raster extent
    double maxPixelError=0,oldSeparation=0;
    const auto startChecks=checks;
    for (uint64_t phase=0;phase<32;++phase)
    {
        const SceneAnchor anchor{vp,extent,0x10000};
        auto depth=originalDepth,layer=original,ps=originalPs,depthPs=originalPs;
        const auto depthResult=ApplyDrawJitter(draw.depthVs,0,phase,true,true,&anchor,
            anchor.depthAllocation,extent,depth.data(),depthPs.data());
        const auto layerResult=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
            anchor.depthAllocation,extent,layer.data(),ps.data());
        Check(depthResult.applied && layerResult.applied && layerResult.slot==7 &&
            layerResult.rejection==JitterRejection::None && !layerResult.shadowCompensated &&
            ps==originalPs && depthPs==originalPs,
            message("sky and depth accept one phase without changing PS constants").c_str());
        for (unsigned i=0;i<layer.size();++i)
            if (i<7*4 || i>=11*4 || i%4>=2)
                Check(layer[i]==original[i],message("sky keeps non-VP and VP Z/W constants").c_str());
        for (const auto& local:expected.locals)
        {
            const auto reference=TireDepthB030(depth,local);
            const auto current=TireMaterialFf9(layer,local);
            const auto legacy=TireMaterialFf9(original,local);
            Check(reference==current,message("independent material clip agrees with paired depth").c_str());
            Check(current[2]==legacy[2] && current[3]==legacy[3],
                message("sky jitter preserves clip Z and W").c_str());
            Check(std::isfinite(current[3]) && std::abs(current[3])>1,
                message("synthetic local vertex has a usable clip W").c_str());
            for (unsigned axis=0;axis<2;++axis)
            {
                const double dimension=axis?extent.height:extent.width;
                const double pixels=(double(current[axis])/current[3]-
                    double(legacy[axis])/legacy[3])*dimension*(axis?-.5:.5);
                const double expectedPixels=axis?layerResult.sample.pixelY:layerResult.sample.pixelX;
                maxPixelError=std::max(maxPixelError,std::abs(pixels-expectedPixels));
                oldSeparation=std::max(oldSeparation,std::abs(pixels));
                Check(std::abs(pixels-expectedPixels)<.003,
                    message("independent clip shift matches the requested physical jitter").c_str());
            }
        }
    }
    const SceneAnchor anchor{vp,extent,0x10000};
    const auto reject=[&](bool enabled,uint64_t psHash,const SceneAnchor* camera,
        uint64_t depthAllocation,JitterRejection rejection) {
        auto layer=original,ps=originalPs;
        const auto result=ApplyDrawJitter(draw.vs,psHash,9,enabled,true,camera,
            depthAllocation,extent,layer.data(),ps.data());
        Check(!result.applied && result.rejection==rejection &&
            layer==original && ps==originalPs,
            message("disabled or mismatched sky draw leaves both banks unchanged").c_str());
    };
    reject(false,draw.ps,&anchor,anchor.depthAllocation,JitterRejection::Disabled);
    reject(true,unmatchedPs,&anchor,anchor.depthAllocation,JitterRejection::UnknownShader);
    auto otherCamera=anchor;
    otherCamera.vpBits[0]^=1;
    reject(true,draw.ps,&otherCamera,anchor.depthAllocation,JitterRejection::CameraMismatch);
    reject(true,draw.ps,&anchor,anchor.depthAllocation+1,JitterRejection::DepthMismatch);
    Check(oldSeparation>.3,message("old unjittered sky separates from paired depth").c_str());
    std::printf("Captured %s sky: %u checks, draw%u/depth%u, 32 synthetic phases at captured 4K; old separation %.6f px, max jitter error %.6f px\n",
        expected.label,checks-startChecks,expected.draw,expected.depthDraw,oldSeparation,maxPixelError);
}
static void CapturedF3449Sky()
{
    // #67 keeps the whole-frame motion fallback of its former unknown writer.
    // Since #121 the #102 PS is also reviewed for bda41, so the negative
    // control uses the f6131 late-floor PS, reviewed only with 2078.
    CapturedSky({"f3449",0xbda41a11626a545cull,0xa9e9542e2c60029aull,0xb030ab4e17a20783ull,172,16,
        true,0x4013372b6413788full},
        issue67_sky_f3449::draws[0]);
}
// #121 Old Sorceress' Mansion: the #67 sky VS with the #102 sky PS over b030
// depth. Banks come from two reporter runtime logs, not an F1 capture.
static void CapturedIssue121Sky()
{
    for (const auto& draw:issue121_sky_ge1::draws)
        CapturedSky({"#121 ge1",0xbda41a11626a545cull,0x02ee5f0608be581aull,0xb030ab4e17a20783ull,
            draw.draw,draw.depthDraw,true,0x4013372b6413788full},draw);
}
// Ice Canyon - Snowy Plateau (gp5_0_scrw), F1 f12139: the #67 sky VS with the new
// PS 1dee over b030 depth. The default points land far off screen for this sky
// world (|y/w| up to 163), where float32 rounding of the 4K clip alone exceeds
// 0.003 px; these synthetic points project on screen at W 2,300-89,000.
static void CapturedF12139Sky()
{
    CapturedSky({"f12139 gp5",0xbda41a11626a545cull,0x1dee52ba32155a53ull,0xb030ab4e17a20783ull,54,12,
        true,0x4013372b6413788full,{{{0,-3000,7000,1},{14000,-14000,3000,1},{80000,-50000,20000,1}}}},
        gp5_sky_f12139::draws[0]);
}
// Captured VS-wide material/depth pairs: a slot-7 material VS (the
// TireMaterialFf9 chain: world c0-c3, VP c7-c10, possibly a clip copy whose PS
// reads only W) mapped VS-wide over the same geometry as a jittered depth
// prepass with the TireDepthB030 arithmetic. Captured banks are exact; the
// local vertices are synthetic because the F1 trace has no vertex buffer.
struct CapturedMaterialCase { const char* label; uint64_t vs, ps, depthVs; unsigned draw, depthDraw;
    std::array<Float4,3> locals{{{-250,-100,20,1},{120,90,80,1},{10,250,160,1}}}; };
template<class Draw>
static void CapturedMaterialOverDepth(const CapturedMaterialCase& expected,const Draw& draw)
{
    const auto message=[&](const char* text) { return std::string(expected.label)+" "+text; };
    Constants original{},originalDepth{},originalPs{};
    std::copy(draw.vertex.begin(),draw.vertex.end(),original.begin());
    std::copy(draw.vertexLate.begin(),draw.vertexLate.end(),original.begin()+254*4);
    std::copy(draw.depth.begin(),draw.depth.end(),originalDepth.begin());
    std::copy(draw.pixel.begin(),draw.pixel.end(),originalPs.begin());
    std::array<uint32_t,16> vp{};
    std::copy_n(original.begin()+7*4,16,vp.begin());
    Check(draw.vs==expected.vs && draw.ps==expected.ps && draw.depthVs==expected.depthVs &&
        draw.slot==7 && draw.draw==expected.draw && draw.depthDraw==expected.depthDraw &&
        std::equal(original.begin(),original.begin()+16,originalDepth.begin()) &&
        std::equal(vp.begin(),vp.end(),originalDepth.begin()+4*4),
        message("material and depth share captured world and scene camera").c_str());
    Check(PositionVPSlot(draw.vs)==7 && DrawPositionVPSlot(draw.vs,draw.ps)==7 &&
        PositionVPSlot(draw.depthVs)==4 && !RequiresEarlierSceneAnchor(draw.vs,draw.ps) &&
        !RetainsMotionFallback(draw.vs,draw.ps),
        message("material maps VS-wide to slot 7 with the ordinary anchor and motion policy").c_str());
    double oldSeparation=0,maxPixelError=0;
    const auto startChecks=checks;
    for (const auto extent:{Viewport{0,0,2560,1440},Viewport{0,0,3840,2160}})
        for (uint64_t phase=0;phase<32;++phase)
        {
            const SceneAnchor anchor{vp,extent,14};
            auto layer=original,depth=originalDepth,ps=originalPs,depthPs=originalPs;
            Check(ApplyDrawJitter(draw.depthVs,0,phase,true,true,&anchor,14,extent,
                depth.data(),depthPs.data()).applied,message("depth camera accepts jitter").c_str());
            const auto result=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,14,extent,
                layer.data(),ps.data());
            Check(result.applied && result.slot==7 && !result.shadowCompensated && ps==originalPs,
                message("material jitters slot 7 without changing PS constants").c_str());
            for (unsigned i=0;i<layer.size();++i)
                if (i<7*4 || i>=11*4 || i%4>=2)
                    Check(layer[i]==original[i],message("world, eye, light and clip ZW constants stay exact").c_str());
            for (const auto& local:expected.locals)
            {
                const auto d=TireDepthB030(depth,local);
                const auto current=TireMaterialFf9(layer,local);
                const auto old=TireMaterialFf9(original,local);
                Check(d==current,message("material clip and its copy align with the jittered depth").c_str());
                Check(current[2]==old[2] && current[3]==old[3],
                    message("material keeps clip Z and the W its PS reads").c_str());
                Check(std::isfinite(d[3]) && std::abs(d[3])>1,
                    message("synthetic point has nondegenerate clip W").c_str());
                for (unsigned axis=0;axis<2;++axis)
                {
                    const double pixels=(double(current[axis])/current[3]-double(old[axis])/old[3])*
                        (axis?extent.height:extent.width)*(axis?-.5:.5);
                    const double wanted=axis?result.sample.pixelY:result.sample.pixelX;
                    maxPixelError=std::max(maxPixelError,std::abs(pixels-wanted));
                    oldSeparation=std::max(oldSeparation,std::abs(pixels));
                    Check(std::abs(pixels-wanted)<.003,
                        message("material clip shift matches the requested physical jitter").c_str());
                }
            }
            for (unsigned mutation=0;mutation<2;++mutation)
            {
                auto rejected=original,rejectedPs=originalPs;
                if (!mutation) rejected[7*4]^=1;
                const auto before=rejected;
                const auto failure=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                    mutation?15:14,extent,rejected.data(),rejectedPs.data());
                Check(!failure.applied && rejected==before && rejectedPs==originalPs &&
                    failure.rejection==(mutation?JitterRejection::DepthMismatch:
                        JitterRejection::CameraMismatch),
                    message("camera or depth mismatch rejects without changing constants").c_str());
            }
        }
    Check(oldSeparation>.3,message("unmapped material separates from its jittered depth by a visible phase").c_str());
    std::printf("Captured %s: %u checks, draw%u/depth%u, 32 phases, 1440p/4K; old separation %.6f px, max jitter error %.6f px\n",
        expected.label,checks-startChecks,expected.draw,expected.depthDraw,oldSeparation,maxPixelError);
}
// Burning Cave - Strange Rocks (gs4_0_scrw), F1 f25276: VS 61bc HLSL 440-461 is
// the TireMaterialFf9 chain (world c0-c3, VP c7-c10) and copies the clip to o4
// (530-531); depth VS f7fd is TireDepthB030. PS 042e reads only i4.w and samples
// its six textures at mesh UVs, so only the position may move.
static void CapturedF25276Cave()
{
    CapturedMaterialOverDepth({"f25276 cave",0x61bc9947f1e88573ull,0x042ef823a22e1498ull,0xf7fd88506d704a3dull,273,10},
        gs4_cave_f25276::draws[0]);
}
// Map tour 2026-10-01: six more PS partners of the #67 sky VS over b030 depth,
// with banks from runtime suspect lines (first reported draw of each pair).
// b9b8 is the mapped depth VS 8d3c with a 12-dword vertex stride.
static void CapturedTourSky()
{
    for (const auto& draw:tour_sky_20261001::draws)
    {
        char label[32];
        std::snprintf(label,sizeof(label),"tour sky %04x",unsigned(draw.ps>>48));
        CapturedSky({label,0xbda41a11626a545cull,draw.ps,0xb030ab4e17a20783ull,draw.draw,draw.depthDraw,
            true,0x4013372b6413788full},draw);
    }
    Check(PositionVPSlot(0xb9b8056050a4c194ull)==4 && PositionVPSlot(0x8d3c80b318235b22ull)==4,
        "tour depth VS b9b8 maps to slot 4 with 8d3c, its 10-dword stride twin");
}
// Map tour 2026-10-01, second pass: runtime-only shader variants reviewed from
// microcode dumped where they were drawn. 2441 and f8b1 are slot-7 materials
// over 52e4 depth, cbad the #102 sky program over f7fd depth, f964 the mapped
// depth VS fe3e with a 14-dword vertex stride.
static void CapturedTourBatch2()
{
    const auto& draws=tour_batch2_20261001::draws;
    CapturedMaterialOverDepth({"tour 2441",0x24418a5936c2d236ull,0xd7f3f85d208dc73dull,0x52e4405f97159d2full,
        draws[0].draw,draws[0].depthDraw},draws[0]);
    CapturedMaterialOverDepth({"tour f8b1",0xf8b1457ed05cacdfull,0xe5b735783b09888bull,0x52e4405f97159d2full,
        draws[1].draw,draws[1].depthDraw},draws[1]);
    CapturedSky({"tour cbad",0xcbadff38155833b6ull,0x311b14004ee00284ull,0xf7fd88506d704a3dull,
        draws[2].draw,draws[2].depthDraw,false,0x4013372b6413788full},draws[2]);
    Check(PositionVPSlot(0xf964d2661094b1a0ull)==4 && PositionVPSlot(0xfe3efe042c311110ull)==4,
        "tour depth VS f964 maps to slot 4 with fe3e, its 10-dword stride twin");
}
// Map tours 2026-10-01: VS 8d66 (HLSL 430-447) is the TireMaterialFf9 chain over
// b030 depth and copies its clip to o1; PS c795 (Ice Canyon - Ice Gorge) and 9e1c
// (Frozen Trail) read only i1.w and sample at mesh UVs.
static void CapturedTour8d66()
{
    const auto& draws=tour_8d66_20261001::draws;
    CapturedMaterialOverDepth({"tour 8d66/c795",0x8d6658641e3b780dull,0xc7956695c6ba859bull,0xb030ab4e17a20783ull,
        draws[0].draw,draws[0].depthDraw},draws[0]);
    CapturedMaterialOverDepth({"tour 8d66/9e1c",0x8d6658641e3b780dull,0x9e1cd4d452f7380dull,0xb030ab4e17a20783ull,
        draws[1].draw,draws[1].depthDraw},draws[1]);
}
// Sea of Baus battle (#203): the #67 sky VS with PS fd46 over b030 depth, from the
// player's v0.8.10 and v0.8.15 logs. fd46 reads only i1.w and samples at mesh UVs.
// This sky world sits 1,607 units above the battle camera, so the default points
// land far off screen; these synthetic points project on screen at W 3,000-20,000.
static void CapturedIssue203Sky()
{
    const auto& draw=issue203_sky::draws[0];
    CapturedSky({"#203 sea battle",0xbda41a11626a545cull,0xfd46e0190f5f6c50ull,0xb030ab4e17a20783ull,
        draw.draw,draw.depthDraw,true,0x4013372b6413788full,
        {{{3000,200,-1400,1},{8000,-1500,-2500,1},{20000,4000,1000,1}}}},draw);
}
// Synthetic input only: the local point (W 1) whose clip under clip(bank, .)
// lands at the given NDC and W. clip is affine in the point, so three unit
// steps give its columns.
template<class Clip>
static Float4 LocalForClip(Clip clip, const Constants& bank, double ndcX, double ndcY, double w)
{
    const auto origin=clip(bank,Float4{0,0,0,1});
    const unsigned axes[3]{0,1,3};
    const double target[3]{ndcX*w,ndcY*w,w};
    double m[3][3],b[3];
    for (unsigned column=0;column<3;++column)
    {
        Float4 step{0,0,0,1};
        step[column]=1;
        const auto moved=clip(bank,step);
        for (unsigned i=0;i<3;++i) m[i][column]=double(moved[axes[i]])-origin[axes[i]];
    }
    for (unsigned i=0;i<3;++i) b[i]=target[i]-origin[axes[i]];
    const auto det=[](const double a[3][3]) {
        return a[0][0]*(a[1][1]*a[2][2]-a[1][2]*a[2][1])-a[0][1]*(a[1][0]*a[2][2]-a[1][2]*a[2][0])+
            a[0][2]*(a[1][0]*a[2][1]-a[1][1]*a[2][0]); };
    const double d=det(m);
    Float4 local{0,0,0,1};
    for (unsigned k=0;k<3;++k)
    {
        double replaced[3][3];
        for (unsigned i=0;i<3;++i)
            for (unsigned j=0;j<3;++j) replaced[i][j]=j==k?b[i]:m[i][j];
        local[k]=float(det(replaced)/d);
    }
    return local;
}
// Old Sorceress' Mansion battle (#212), from the player's v0.8.15 log: per-light
// floor passes e810/c44e and 9bde/d122 over the 4053 floor draw (slot 7,
// TireMaterialFf9 chain), same world and camera, no depth write, GEQUAL test.
// 9bde HLSL 440-454 is the E810Clip chain and copies the clip to o4; both PS
// sample tex0 (light attenuation) at i4.xy/w through c0 as E810Tex0 does. Draw 3,
// e810/a800, lit the floor around an enemy in a local Entrance Hall battle.
static void CapturedIssue212Light()
{
    double oldSeparation=0,maxPixelError=0;
    const auto startChecks=checks;
    for (const auto& draw:issue212_light::draws)
    {
        Constants original{},originalDepth{},originalPs{};
        std::copy(draw.vertex.begin(),draw.vertex.end(),original.begin());
        std::copy(draw.vertexLate.begin(),draw.vertexLate.end(),original.begin()+254*4);
        std::copy_n(draw.depth.begin(),16,originalDepth.begin());
        std::copy_n(draw.depth.begin()+16,16,originalDepth.begin()+7*4);
        std::copy(draw.pixel.begin(),draw.pixel.end(),originalPs.begin());
        std::array<uint32_t,16> vp{};
        std::copy_n(original.begin()+7*4,16,vp.begin());
        Check(draw.depthVs==0x4053f2a21dbb92ddull && draw.slot==7 &&
            std::equal(original.begin(),original.begin()+16,originalDepth.begin()) &&
            std::equal(vp.begin(),vp.end(),originalDepth.begin()+7*4),
            "#212 light pass and 4053 floor share captured world and slot-7 camera");
        Check(IsScreenLightPair(draw.vs,draw.ps) && PositionVPSlot(draw.vs)==-1 &&
            DrawPositionVPSlot(draw.vs,draw.ps)==7 && PositionVPSlot(draw.depthVs)==7 &&
            !RequiresEarlierSceneAnchor(draw.vs,draw.ps) && !RetainsMotionFallback(draw.vs,draw.ps),
            "#212 exact light pair maps to slot 7 with the ordinary anchor and motion policy");
        Check(DrawPositionVPSlot(draw.vs,0x0123456789abcdefull,true)==-1 &&
            DrawPositionVPSlot(0x9bdef27080ca3ab4ull,0xc44ebbbc0207b5a9ull)==-1,
            "#212 other PS partners of the light VS stay held");
        std::array<Float4,3> locals{};
        const double placement[3][3]{{-.6,-.4,8},{.1,.3,40},{.7,-.2,300}};
        for (unsigned i=0;i<3;++i)
            locals[i]=LocalForClip(E810Clip,original,placement[i][0],placement[i][1],placement[i][2]);
        for (const auto extent:{Viewport{0,0,1114,626},Viewport{0,0,2560,1440},Viewport{0,0,3840,2160}})
            for (uint64_t phase=0;phase<32;++phase)
            {
                const SceneAnchor anchor{vp,extent,21};
                auto depth=originalDepth,layer=original,ps=originalPs,depthPs=originalPs;
                Check(ApplyDrawJitter(draw.depthVs,0,phase,true,true,&anchor,21,extent,
                    depth.data(),depthPs.data()).applied,"#212 4053 floor accepts jitter");
                const auto result=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,21,extent,
                    layer.data(),ps.data());
                Check(result.applied && result.slot==7 && !result.shadowCompensated && ps==originalPs,
                    "#212 light pass jitters slot 7 without changing PS constants");
                for (unsigned i=0;i<layer.size();++i)
                    if (i<7*4 || i>=11*4 || i%4>=2)
                        Check(layer[i]==original[i],"#212 world, light and clip ZW constants stay exact");
                for (const auto& local:locals)
                {
                    const auto floor=TireMaterialFf9(depth,local);
                    const auto current=E810Clip(layer,local),old=E810Clip(original,local);
                    Check(std::isfinite(floor[3]) && floor[3]>1,"#212 synthetic point has nondegenerate clip W");
                    Check(current[2]==old[2] && current[3]==old[3],"#212 light pass keeps clip Z and W");
                    const auto sample=E810Tex0(ps,current),oldSample=E810Tex0(originalPs,old);
                    for (unsigned axis=0;axis<2;++axis)
                    {
                        const double size=axis?extent.height:extent.width,sign=axis?-.5:.5;
                        const double toFloor=(double(current[axis])/current[3]-double(floor[axis])/floor[3])*size*sign;
                        const double pixels=(double(current[axis])/current[3]-double(old[axis])/old[3])*size*sign;
                        const double samplePixels=(double(sample[axis])-oldSample[axis])*size;
                        maxPixelError=std::max(maxPixelError,std::abs(toFloor));
                        oldSeparation=std::max(oldSeparation,std::abs(pixels));
                        Check(std::abs(toFloor)<.003,"#212 light pass clip and its o4 copy align with the jittered floor");
                        Check(std::abs(samplePixels-pixels)<.003,"#212 tex0 lookup follows the same screen pixel shift");
                    }
                }
                for (unsigned mutation=0;mutation<2;++mutation)
                {
                    auto rejected=original,rejectedPs=originalPs;
                    if (!mutation) rejected[7*4]^=1;
                    const auto before=rejected;
                    const auto failure=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                        mutation?22:21,extent,rejected.data(),rejectedPs.data());
                    Check(!failure.applied && rejected==before && rejectedPs==originalPs &&
                        failure.rejection==(mutation?JitterRejection::DepthMismatch:JitterRejection::CameraMismatch),
                        "#212 camera or depth mismatch rejects without changing constants");
                }
            }
    }
    Check(oldSeparation>.3,"#212 unmapped light pass separates from its jittered floor by a visible phase");
    std::printf("Captured #212 light passes: %u checks, three draws, 32 phases, 626p/1440p/4K; old separation %.6f px, max floor error %.6f px\n",
        checks-startChecks,oldSeparation,maxPixelError);
}
// Reviewed exact pairs with their first logged banks. Each maps to the slot the
// runtime found the camera in, jitters only the x/y of that matrix's rows,
// leaves every other constant and the PS bank exact, and gets the same matrix
// as its jittered depth companion. Returns how many carry such a companion.
template<size_t N>
static unsigned CheckLoggedPairs(const tour_cutscene::Draw (&draws)[N])
{
    unsigned companions=0;
    for (const auto& draw:draws)
    {
        Check(DrawPositionVPSlot(draw.vs,draw.ps)==int(draw.slot),"logged pair maps to its logged camera slot");
        Check(RequiresEarlierSceneAnchor(draw.vs,draw.ps)==(FindSkyMaterialPair(draw.vs,draw.ps)!=nullptr),
            "logged pair keeps its table's anchor policy");
        Constants original{},originalPs{};
        std::copy(draw.vertex.begin(),draw.vertex.end(),original.begin());
        std::copy(draw.late.begin(),draw.late.end(),original.begin()+254*4);
        if (draw.slot<=60)
            Check(std::equal(draw.camera.begin(),draw.camera.end(),original.begin()+draw.slot*4),
                "logged camera bank matches the logged vertex bank");
        std::copy(draw.camera.begin(),draw.camera.end(),original.begin()+draw.slot*4);
        std::copy(draw.pixel.begin(),draw.pixel.end(),originalPs.begin());
        std::array<uint32_t,16> vp{};
        std::copy(draw.camera.begin(),draw.camera.end(),vp.begin());
        const int depthSlot=draw.depthVs?PositionVPSlot(draw.depthVs):-1;
        const bool companion=depthSlot>=0 && unsigned(depthSlot)==draw.depthSlot &&
            std::equal(draw.depth.begin()+16,draw.depth.end(),vp.begin());
        companions+=companion;
        for (const auto extent:{Viewport{0,0,1114,626},Viewport{0,0,3840,2160}})
            for (uint64_t phase=0;phase<32;phase+=3)
            {
                const SceneAnchor anchor{vp,extent,31};
                auto layer=original,ps=originalPs;
                const auto result=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,31,extent,layer.data(),ps.data());
                Check(result.applied && result.slot==int(draw.slot) && !result.shadowCompensated && ps==originalPs,
                    "logged pair jitters its slot without changing PS constants");
                for (unsigned i=0;i<layer.size();++i)
                    if (i<draw.slot*4 || i>=draw.slot*4+16 || i%4>=2)
                        Check(layer[i]==original[i],"logged pair leaves every other constant exact");
                if (companion)
                {
                    Constants depth{},depthPs{};
                    std::copy_n(draw.depth.begin(),16,depth.begin());
                    std::copy_n(draw.depth.begin()+16,16,depth.begin()+depthSlot*4);
                    Check(ApplyDrawJitter(draw.depthVs,0,phase,true,true,&anchor,31,extent,depth.data(),depthPs.data()).applied &&
                        std::equal(depth.begin()+depthSlot*4,depth.begin()+depthSlot*4+16,layer.begin()+draw.slot*4),
                        "logged pair and its depth companion get the same jittered camera");
                }
                auto rejected=original,rejectedPs=originalPs;
                rejected[draw.slot*4]^=1;
                const auto before=rejected;
                const auto failure=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,31,extent,rejected.data(),rejectedPs.data());
                Check(!failure.applied && failure.rejection==JitterRejection::CameraMismatch && rejected==before,
                    "logged pair with another camera stays unjittered");
            }
    }
    return companions;
}
// Cutscene tour 2026-10-04: the 45 reviewed pairs (PC, psvita and Mac).
static void CapturedCutsceneTour()
{
    const auto startChecks=checks;
    const unsigned companions=CheckLoggedPairs(tour_cutscene::draws);
    Check(companions>=30,"most tour pairs carry a mapped depth companion with the same camera");
    std::printf("Captured cutscene tour: %u checks, %zu pairs, %u with a depth companion\n",
        checks-startChecks,std::size(tour_cutscene::draws),companions);
}
// #307 Experimental Staff Marine Division boats: c189 and 3305 are e7b3 and
// 6742 (slot 7) with a 14-dword vertex stride. All four pairs draw over the
// jittered 52e4/f964 depth with GEQUAL; the c189 base passes without a depth
// write, 3305 with one. Only these four exact pairs are mapped.
static void CapturedIssue307Boat()
{
    const auto startChecks=checks;
    const unsigned companions=CheckLoggedPairs(issue307_boat::draws);
    Check(std::size(issue307_boat::draws)==4 && companions==4,
        "#307 all four boat pairs draw over a jittered 52e4/f964 depth companion with the same camera");
    for (const auto& draw:issue307_boat::draws)
        Check(draw.slot==7 && !RequiresEarlierSceneAnchor(draw.vs,draw.ps) && !RetainsMotionFallback(draw.vs,draw.ps) &&
            DrawPositionVPSlot(draw.vs,0x0123456789abcdefull)==-1,
            "#307 boat pair keeps the ordinary anchor and motion policy; other PS partners stay held");
    Check(PositionVPSlot(0xc1896d4be9e73859ull)==-1 && PositionVPSlot(0x330542fa74d064deull)==-1 &&
        PositionVPSlot(0xe7b38eb08c70e5e1ull)==7 && PositionVPSlot(0x6742ec1abe49589eull)==7,
        "#307 boat VS are not mapped as a whole; their 10-dword twins keep slot 7");
    std::printf("Captured #307 boats: %u checks, %zu pairs, %u with a depth companion\n",
        checks-startChecks,std::size(issue307_boat::draws),companions);
}
// Opening battle 2026-10-01: depth writers 7def (HLSL 534-543) and c511 (540-549)
// end with oPos = P.x*c11 + P.w*c10 + P.z*c9 + P.y*c8 for a position P built from
// the vertex, the world rows and the c12 eye. No depth companion draws the same
// geometry; the log found the scene camera at c8-c11 on each draw.
static Float4 BattleBillboardClip(const Constants& c, const Float4& p)
{
    auto r=Mul(p[0],C(c,11));
    r=Mad(p[3],C(c,10),r);
    r=Mad(p[2],C(c,9),r);
    return Mad(p[1],C(c,8),r);
}
// Synthetic input only: the P with P.x=1 (c11 holds the translation) whose clip
// lands at the given NDC and W under this bank's camera.
static Float4 BattleBillboardPoint(const Constants& c, double ndcX, double ndcY, double w)
{
    const auto row=[&](unsigned slot,unsigned axis) { return double(C(c,slot)[axis]); };
    const unsigned axes[3]{0,1,3};
    const double target[3]{ndcX*w,ndcY*w,w};
    double m[3][3],b[3];
    for (unsigned i=0;i<3;++i)
    {
        m[i][0]=row(8,axes[i]); m[i][1]=row(9,axes[i]); m[i][2]=row(10,axes[i]);
        b[i]=target[i]-row(11,axes[i]);
    }
    const auto det=[](const double a[3][3]) {
        return a[0][0]*(a[1][1]*a[2][2]-a[1][2]*a[2][1])-a[0][1]*(a[1][0]*a[2][2]-a[1][2]*a[2][0])+
            a[0][2]*(a[1][0]*a[2][1]-a[1][1]*a[2][0]); };
    const double d=det(m);
    double p[3];
    for (unsigned k=0;k<3;++k)
    {
        double replaced[3][3];
        for (unsigned i=0;i<3;++i)
            for (unsigned j=0;j<3;++j) replaced[i][j]=j==k?b[i]:m[i][j];
        p[k]=det(replaced)/d;
    }
    return {1.f,float(p[0]),float(p[1]),float(p[2])};
}
static void CapturedBattleDepthWriters()
{
    double maxPixelError=0,oldSeparation=0;
    const auto startChecks=checks;
    for (const auto& draw:battle_depth_20261001::draws)
    {
        char label[48];
        std::snprintf(label,sizeof(label),"battle %04x/%04x f%u",unsigned(draw.vs>>48),unsigned(draw.ps>>48),draw.frame);
        const auto message=[&](const char* text) { return std::string(label)+" "+text; };
        Constants original{},originalPs{};
        std::copy(draw.vertex.begin(),draw.vertex.end(),original.begin());
        std::copy(draw.vertexLate.begin(),draw.vertexLate.end(),original.begin()+254*4);
        std::copy(draw.pixel.begin(),draw.pixel.end(),originalPs.begin());
        Check(draw.slot==8 && std::equal(draw.camera.begin(),draw.camera.end(),original.begin()+8*4),
            message("log found the scene camera at c8-c11").c_str());
        Check(PositionVPSlot(draw.vs)==8 && DrawPositionVPSlot(draw.vs,draw.ps)==8 &&
            !RequiresEarlierSceneAnchor(draw.vs,draw.ps) && !RetainsMotionFallback(draw.vs,draw.ps),
            message("depth writer maps VS-wide to slot 8 with the ordinary anchor and motion policy").c_str());
        for (const auto extent:{Viewport{0,0,2560,1440},Viewport{0,0,3840,2160}})
            for (uint64_t phase=0;phase<32;++phase)
            {
                const SceneAnchor anchor{draw.camera,extent,14};
                auto values=original,ps=originalPs;
                const auto result=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,14,extent,
                    values.data(),ps.data());
                Check(result.applied && result.slot==8 && !result.shadowCompensated && ps==originalPs,
                    message("depth writer jitters slot 8 without changing PS constants").c_str());
                auto reference=original,referencePs=originalPs;
                Check(ApplyDrawJitter(0x1da1ddc75da8e994ull,0,phase,true,true,&anchor,14,extent,
                    reference.data(),referencePs.data()).applied &&
                    std::equal(values.begin()+8*4,values.begin()+12*4,reference.begin()+8*4),
                    message("depth writer gets the jittered camera of mapped slot-8 VS 1da1").c_str());
                for (unsigned i=0;i<values.size();++i)
                    if (i<8*4 || i>=12*4 || i%4>=2)
                        Check(values[i]==original[i],message("world, eye, UV and clip ZW constants stay exact").c_str());
                for (const auto& target:{std::array<double,3>{.3,-.2,60},{-.55,.4,250},{.1,.65,900}})
                {
                    const auto p=BattleBillboardPoint(original,target[0],target[1],target[2]);
                    const auto current=BattleBillboardClip(values,p);
                    const auto old=BattleBillboardClip(original,p);
                    Check(current[2]==old[2] && current[3]==old[3],
                        message("depth writer keeps clip Z and the W its PS reads").c_str());
                    Check(std::isfinite(old[3]) && std::abs(old[3])>1,
                        message("synthetic point has nondegenerate clip W").c_str());
                    for (unsigned axis=0;axis<2;++axis)
                    {
                        const double pixels=(double(current[axis])/current[3]-double(old[axis])/old[3])*
                            (axis?extent.height:extent.width)*(axis?-.5:.5);
                        const double wanted=axis?result.sample.pixelY:result.sample.pixelX;
                        maxPixelError=std::max(maxPixelError,std::abs(pixels-wanted));
                        oldSeparation=std::max(oldSeparation,std::abs(pixels));
                        Check(std::abs(pixels-wanted)<.003,
                            message("depth writer clip shift matches the requested physical jitter").c_str());
                    }
                }
                for (unsigned mutation=0;mutation<2;++mutation)
                {
                    auto rejected=original,rejectedPs=originalPs;
                    if (!mutation) rejected[8*4]^=1;
                    const auto before=rejected;
                    const auto failure=ApplyDrawJitter(draw.vs,draw.ps,phase,true,true,&anchor,
                        mutation?15:14,extent,rejected.data(),rejectedPs.data());
                    Check(!failure.applied && rejected==before && rejectedPs==originalPs &&
                        failure.rejection==(mutation?JitterRejection::DepthMismatch:JitterRejection::CameraMismatch),
                        message("camera or depth mismatch rejects without changing constants").c_str());
                }
            }
    }
    Check(oldSeparation>.3,"unmapped battle depth writers separate from the jittered scene by a visible phase");
    std::printf("Captured battle depth writers: %u checks, %zu draws (7def, c511), 32 phases, 1440p/4K; old separation %.6f px, max jitter error %.6f px\n",
        checks-startChecks,std::size(battle_depth_20261001::draws),oldSeparation,maxPixelError);
}
// Runtime suspect locator (no F1 capture): camera slot choice, same-frame
// companion lookup, per-pair settling and the fixture-bearing log format.
static void SuspectLocator()
{
    const auto startChecks=checks;
    using namespace issue102_sky_f1800;
    const auto& draw=draws[0];
    Constants material{},pixel{};
    std::copy(draw.vertex.begin(),draw.vertex.end(),material.begin());
    std::copy(draw.vertexLate.begin(),draw.vertexLate.end(),material.begin()+254*4);
    std::copy(draw.pixel.begin(),draw.pixel.end(),pixel.begin());
    std::array<uint32_t,16> camera{},world{},vp{};
    std::copy_n(material.begin()+7*4,16,camera.begin());
    std::copy_n(draw.depth.begin(),16,world.begin());
    std::copy_n(draw.depth.begin()+16,16,vp.begin());
    Check(SceneCameraSlot(material.data(),camera)==7,"suspect camera slot finds the f1800 sky camera at c7");
    auto other=camera; other[0]^=1;
    Check(SceneCameraSlot(material.data(),other)==-1,"suspect camera slot rejects a different camera");
    // A skinned VS whose real VP is at c233 while a stale scene camera stays at c7.
    auto stale=material; std::copy(camera.begin(),camera.end(),stale.begin()+233*4);
    using Action=SuspectDecision::Action;
    const auto decide=[&](SuspectEvidence e,const Constants& c,uint64_t waited=0) {
        return DecideSuspect(e,c.data(),camera,SceneCameraSlot(c.data(),camera),waited);
    };
    const auto skinned=decide({true,true,1,0,233},stale);
    Check(skinned.action==Action::Report && skinned.cameraSlot==233 && skinned.evidenceSlot==233 &&
        skinned.positionKind==1,"proven window is reported even when a stale camera sits earlier");
    auto moved=stale; moved[233*4]^=1;
    Check(decide({true,true,1,0,233},moved).action==Action::Wait,
        "a proven window that differs on this draw waits instead of dismissing the pair");
    Check(decide({true,true,2,0,-1},material).action==Action::Dismiss,"direct-position evidence dismisses for good");
    const auto skyDecision=decide({true,true,1,0,7},material);
    Check(skyDecision.action==Action::Report && skyDecision.cameraSlot==7 && skyDecision.positionKind==1,
        "f1800 sky evidence reports slot 7");
    Check(decide({true,false,0,0,-1},material,SuspectEvidenceFrames-1).action==Action::Wait &&
        decide({true,false,0,0,-1},material,SuspectEvidenceFrames).action==Action::Report,
        "missing evidence waits a bounded number of frames, then reports unproven");
    const auto flagged=decide({true,true,1,4,7},material);
    Check(flagged.action==Action::Report && flagged.positionKind==1 && flagged.evidenceSlot==-1,
        "evidence with analyzer issues is treated as unproven");
    Check(decide({false,false,0,0,-1},material).action==Action::Report,"no collection reports immediately");

    SuspectTracker tracker;
    const SuspectGeometry sky{0xb8f3c00,2484,0,0xc258003},cube{0xb8f3c00,36,0,0xc258003};
    tracker.BeginFrame(10);
    Check(!tracker.FindCompanion(sky),"no companion before a jittered depth writer");
    tracker.ObserveJitteredDepth(sky,draw.depthVs,4,351,world,vp);
    tracker.ObserveJitteredDepth(cube,0x1234,7,352,world,vp);
    const auto* companion=tracker.FindCompanion(sky);
    Check(companion && companion->vs==draw.depthVs && companion->slot==4 && companion->draw==351 &&
        !tracker.FindCompanion({0xb8f3c00,2484,3,0xc258003}),"companion lookup matches exact geometry only");
    tracker.ObserveJitteredDepth(sky,0x5678,4,400,world,vp);
    Check(tracker.FindCompanion(sky)->draw==400,"latest jittered writer of the geometry wins");
    tracker.BeginFrame(10);
    Check(tracker.FindCompanion(sky)!=nullptr,"same frame keeps companions");
    tracker.BeginFrame(11);
    Check(!tracker.FindCompanion(sky) && !tracker.FindCompanion(cube),"a new frame retires companions");
    for (uint32_t i=0;i<SuspectTracker::MaxCompanions+10;++i)
        tracker.ObserveJitteredDepth({i,3,0,0},i,4,i,world,vp);
    Check(tracker.FindCompanion({0,3,0,0}) && !tracker.FindCompanion({SuspectTracker::MaxCompanions,3,0,0}),
        "companion table stays bounded");

    Check(tracker.Waited(draw.vs,draw.ps,100)==0 && tracker.Waited(draw.vs,draw.ps,150)==50 &&
        tracker.Waited(draw.vs,0x1111,150)==0,"evidence wait is counted per pair from its first frame");
    Check(!tracker.Settled(draw.vs,draw.ps),"unseen pair is not settled");
    tracker.Settle(draw.vs,draw.ps,true);
    Check(tracker.Settled(draw.vs,draw.ps) && !tracker.Settled(draw.vs,0x1111),"settled pair is logged once");
    for (uint64_t i=1;i<SuspectTracker::MaxReports;++i) tracker.Settle(i,i,true);
    Check(tracker.Settled(0x9999,0x9999),"report cap stops further lines");

    SuspectTracker lines;
    lines.BeginFrame(1138);
    lines.ObserveJitteredDepth(sky,draw.depthVs,4,351,world,vp);
    const auto text=FormatSuspect({draw.vs,draw.ps,1138,1231,0x700766,7,1,7,sky,lines.FindCompanion(sky),
        material.data(),pixel.data()});
    Check(text[0].starts_with("temporal suspect: kind=depth_writer_after_jittered_geometry vs=db23a2ad4493bbb4 ps=02ee5f0608be581a ") &&
        text[0].find(" companion_vs=f7fd88506d704a3d companion_slot=4 companion_draw=351 same_world=true same_camera=true")!=std::string::npos &&
        text[0].find(" zfunc=6 zwrite=true index_base=0x0b8f3c00 index_count=2484 ")!=std::string::npos,
        "suspect line names the pair, companion and depth state");
    Check(text[1].starts_with("temporal suspect banks: vs=db23a2ad4493bbb4 ps=02ee5f0608be581a material=") &&
        text[1].find(" late=")!=std::string::npos && text[1].find(" companion_vp=-")==std::string::npos &&
        text[1].size()<2200,"banks line carries fixture words within a bounded size");
    const auto alone=FormatSuspect({draw.vs,draw.ps,1,2,0x700766,7,-1,-1,sky,nullptr,material.data(),pixel.data()});
    Check(alone[0].find("kind=depth_writer ")!=std::string::npos && alone[1].ends_with(" companion_world=- companion_vp=-"),
        "a depth writer without companion omits companion banks");
    std::printf("Suspect locator: %u checks, f1800 sky line %zu+%zu bytes\n",checks-startChecks,text[0].size(),text[1].size());
}
static void CapturedF1800Sky()
{
    // #102 replays object motion; the fallback aborted it every frame.
    CapturedSky({"f1800",0xdb23a2ad4493bbb4ull,0x02ee5f0608be581aull,0xf7fd88506d704a3dull,1222,466,
        false,0xa9e9542e2c60029aull},
        issue102_sky_f1800::draws[0]);
}
static float Dot(const Float4& a,const Float4& b)
{
    float result=0;
    for (unsigned i=0;i<4;++i) result+=a[i]*b[i];
    return result;
}
static Float4 Battle4bdClip(const Constants& c)
{
    // Controlled one-bone post-skin registers seeded with captured c5..c7.
    // Preserve c255's observed x=0, y=1 select: xyz from r3/r6, w=1.
    const auto r11=C(c,5),r10=C(c,6),r6=C(c,7);
    auto r1=S(r6,"zxyy"),r3=S(r6,"zxyy");
    r1[3]=r3[3]=float(F(c[255*4+1]));
    const float r0w=Dot(S(r11,"zxyw"),r3);
    const float r3x=Dot(S(r10,"zxyw"),r3);
    const float r1x=Dot(S(r6,"zxyw"),r1);
    r1=Mad(r1x,S(C(c,3),"wzyx"),S(C(c,4),"wzyx"));
    r1=Mad(r3x,S(C(c,2),"yxzw"),S(r1,"zwyx"));
    r3=Mad(r0w,S(C(c,1),"zywx"),S(r1,"zxwy"));
    r1=Mad(r3[2],C(c,233),Mul(r3[0],C(c,232)));
    r1=Mad(r3[1],C(c,231),r1);
    return Mad(r3[3],C(c,230),r1);
}
static Float4 BattleF6Clip(const Constants& c,const Float4& r6)
{
    auto r3=Mul(r6[3],C(c,11));
    r3=Mad(r6[0],C(c,10),r3);
    r3=Mad(r6[2],C(c,9),r3);
    return Mad(r6[1],C(c,8),r3); // oPos and o2; rest of basis outside VP.
}

static void BattleP2CpuJitter()
{
    constexpr uint64_t vs8=0x8d9770d1bd8ba0faull,vs4=0x4bd8985d84983b83ull,
        vsF6=0xf6f074ce5d305448ull;
    const float ndcScale[]{1,1,-1},ndcOffset[]{0,0,1};
    Check(IsJitterViewport({0,0,1280,720},0x43f,ndcScale,ndcOffset),
        "battle guest-camera gate uses verified 720p viewport");
    Check(PositionVPSlot(0xbda41a11626a545cull)<0,
        "mixed-camera shader remains excluded");
    Check(battle4bd[0].vp!=battle4bd[1].vp && battle4bd[1].vp!=battle4bd[2].vp &&
        battle4bd[0].other!=battle4bd[1].other && battle4bd[1].other!=battle4bd[2].other,
        "three historical skinned draws contain evolving camera and palette banks");
    double maxError=0;
    unsigned sampled=0;
    const auto exercise=[&](uint64_t vs,uint64_t psHash,int slot,Constants original,
        const Constants originalPs,const char* source,auto evaluate,const auto& points) {
        std::array<uint32_t,16> vp{};
        std::copy_n(original.begin()+slot*4,16,vp.begin());
        for (const auto extent:{Viewport{0,0,853,480},Viewport{0,0,1280,720}})
        {
            const SceneAnchor anchor{vp,extent,0x10000};
            for (uint64_t phase=0;phase<32;++phase)
            {
                auto upload=original,ps=originalPs;
                const auto result=ApplyDrawJitter(vs,psHash,phase,true,true,&anchor,
                    anchor.depthAllocation,extent,upload.data(),ps.data());
                Check(result.applied && result.slot==slot && !result.shadowCompensated &&
                    result.rejection==JitterRejection::None,"actual battle draw jitter upload accepted");
                Check(ps==originalPs,"battle PS upload bitwise unchanged");
                for (unsigned row=0;row<4;++row)
                {
                    Check(upload[slot*4+row*4+2]==original[slot*4+row*4+2] &&
                        upload[slot*4+row*4+3]==original[slot*4+row*4+3],
                        "battle VP depth and homogeneous coefficients preserved");
                    upload[slot*4+row*4]=original[slot*4+row*4];
                    upload[slot*4+row*4+1]=original[slot*4+row*4+1];
                }
                Check(upload==original,"all non-VP constants including UV, skin palette and guest bank unchanged");
                upload=original;ps=originalPs;
                const auto again=ApplyDrawJitter(vs,psHash,phase,true,true,&anchor,
                    anchor.depthAllocation,extent,upload.data(),ps.data());
                const auto jitter=FrameJitter(phase,extent.width,extent.height);
                Check(again.applied && again.sample.phase==jitter.phase,"actual phase used by battle upload");
                for (const auto& point:points)
                {
                    const auto old=evaluate(original,point),now=evaluate(upload,point);
                    Check(std::isfinite(old[3]) && std::abs(old[3])>0.00001f &&
                        now[2]==old[2] && now[3]==old[3],"independent swizzled clip Z/W unchanged");
                    const double dx=(double(now[0])/now[3]-double(old[0])/old[3])*extent.width*.5;
                    const double dy=(double(old[1])/old[3]-double(now[1])/now[3])*extent.height*.5;
                    maxError=std::max({maxError,std::abs(dx-jitter.pixelX),std::abs(dy-jitter.pixelY)});
                    Check(std::abs(dx-jitter.pixelX)<.03 && std::abs(dy-jitter.pixelY)<.03,
                        "independent final swizzle yields requested XY pixel shift");
                    ++sampled;
                }
            }
            auto altered=anchor;altered.vpBits[0]^=1;
            auto untouched=original,unmodifiedPs=originalPs;
            const auto reject=ApplyDrawJitter(vs,psHash,0,true,true,&altered,
                anchor.depthAllocation,extent,untouched.data(),unmodifiedPs.data());
            Check(!reject.applied && reject.rejection==JitterRejection::CameraMismatch &&
                untouched==original && unmodifiedPs==originalPs,
                "different unmodified camera rejected without upload writes");
            const auto rejectDepth=ApplyDrawJitter(vs,psHash,0,true,true,&anchor,
                anchor.depthAllocation+1,extent,untouched.data(),unmodifiedPs.data());
            Check(!rejectDepth.applied && rejectDepth.rejection==JitterRejection::DepthMismatch &&
                untouched==original && unmodifiedPs==originalPs,
                "different depth allocation rejected without upload writes");
        }
        std::printf("P2 CPU %s VS=%016llx slot=%d phase=32 extents=853x480,1280x720\n",
            source,static_cast<unsigned long long>(vs),slot);
    };
    for (const auto& entry:battle8d)
    {
        Constants vs{},ps{};
        std::copy_n(entry.other.begin(),16,vs.begin());
        std::copy_n(entry.other.begin()+16,4,vs.begin()+7*4);
        std::copy(entry.vp.begin(),entry.vp.end(),vs.begin()+8*4);
        std::copy_n(entry.other.begin()+20,4,vs.begin()+12*4);
        std::copy_n(entry.ps.begin(),4,ps.begin());
        std::copy_n(entry.ps.begin()+4,4,ps.begin()+255*4);
        exercise(vs8,0xa196904547677608ull,8,vs,ps,entry.source,Battle8dClip,
            std::array<Float4,3>{{{-.6f,.3f,.5f,1},{1.2f,-.5f,2.f,1},{.4f,1.1f,-1.f,1}}});
    }
    for (const auto& entry:battle4bd)
    {
        Constants vs{},ps{};
        std::copy_n(entry.other.begin(),28,vs.begin()+4);
        std::copy(entry.vp.begin(),entry.vp.end(),vs.begin()+230*4);
        std::copy_n(entry.other.begin()+28,4,vs.begin()+255*4);
        std::copy_n(entry.ps.begin(),4,ps.begin());
        std::copy_n(entry.ps.begin()+4,4,ps.begin()+255*4);
        Check(F(vs[255*4])==0 && F(vs[255*4+1])==1,"historical skin select flags match controlled post-skin case");
        exercise(vs4,0x8ead384aedf2bb23ull,230,vs,ps,entry.source,
            [](const Constants& c,const Float4&){return Battle4bdClip(c);},
            std::array<Float4,1>{{{0,0,0,1}}});
    }
    Constants effect{},effectPs{};
    for (unsigned i=0;i<effect.size();++i)
        effect[i]=std::bit_cast<uint32_t>(float(int(i%37)-18)*.125f);
    constexpr std::array<float,16> controlledVp{
        1,0,0,0, 0,1,0,0, 0,0,1,1, 0,0,.1f,0};
    for (unsigned i=0;i<16;++i)effect[8*4+i]=std::bit_cast<uint32_t>(controlledVp[i]);
    for (unsigned i=0;i<effectPs.size();++i)
        effectPs[i]=std::bit_cast<uint32_t>(float(int(i%19)-9)*.125f);
    exercise(vsF6,0xa3826242c3338c5full,8,effect,effectPs,"f6-controlled",
        [](const Constants& c,const Float4& point){return BattleF6Clip(c,point);},
        std::array<Float4,3>{{{.7f,.2f,2.f,1},{1.5f,-.5f,3.f,1},{-.4f,.6f,4.f,1}}});
    std::printf("Battle P2 CPU jitter: %u final-clip samples, max pixel error %.6f (controlled f6; no GPU draw/alpha claim)\n",
        sampled,maxError);
}

// Named cases, in full-run order. A new captured case needs one row here;
// fullRun=false marks a flag-only case that the full run skips.
struct NamedCase { const char* flag; void (*run)(); bool fullRun; };
static const NamedCase namedCases[]{
    {"--captured-f5997-layers",CapturedF5997Layers,true},
    {"--captured-f5912-layers",CapturedF5912Layers,true},
    {"--captured-f16385-layers",CapturedF16385Layers,true},
    {"--captured-static-layers",CapturedStaticLayerCoverage,true},
    {"--captured-f2548-layers",CapturedF2548Layers,true},
    {"--captured-f6131-late-floor",CapturedF6131LateFloor,true},
    {"--captured-f6131-e810",CapturedF6131E810ConstantSample,true},
    {"--captured-f6814-cutscene",CapturedF6814Cutscene,true},
    {"--captured-f3449-sky",CapturedF3449Sky,true},
    {"--captured-f1800-sky",CapturedF1800Sky,true},
    {"--captured-issue121-sky",CapturedIssue121Sky,true},
    {"--captured-f12139-sky",CapturedF12139Sky,true},
    {"--captured-f25276-cave",CapturedF25276Cave,true},
    {"--captured-tour-sky",CapturedTourSky,true},
    {"--captured-tour-batch2",CapturedTourBatch2,true},
    {"--captured-tour-8d66",CapturedTour8d66,true},
    {"--captured-issue203-sky",CapturedIssue203Sky,true},
    {"--captured-issue212-light",CapturedIssue212Light,true},
    {"--captured-cutscene-tour",CapturedCutsceneTour,true},
    {"--captured-issue307-boat",CapturedIssue307Boat,true},
    {"--captured-battle-depth",CapturedBattleDepthWriters,true},
    {"--suspect-tracker",SuspectLocator,true},
    {"--feedback-mapping-batch",FeedbackMappingBatch,true},
    {"--screen-mapping-batch",ScreenMappingBatch,true},
    {"--battle-p2-cpu",BattleP2CpuJitter,false},
};

int main(int argc,char** argv)
{
    if (argc==2)
    {
        // --captured-static-layers keeps its old meaning: the static layer
        // captures and their coverage, in full-run order.
        const bool staticLayers=std::strcmp(argv[1],"--captured-static-layers")==0;
        for (const auto& item:namedCases)
        {
            if (staticLayers) { item.run(); if (item.run==CapturedStaticLayerCoverage) return 0; continue; }
            if (std::strcmp(argv[1],item.flag)==0) { item.run(); return 0; }
        }
    }
    for (const auto& item:namedCases)
        if (item.fullRun) item.run();
    TireMaterialCoverage();
    BattleCoverage();
    Map16Coverage();
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
            const auto half = FrameJitter(frame, extent.width, extent.height, .5);
            Check(half.phase == sample.phase && half.pixelX == sample.pixelX*.5 &&
                half.pixelY == sample.pixelY*.5 && half.ndcX == sample.ndcX*.5f &&
                half.ndcY == sample.ndcY*.5f, "jitter scale keeps phase and scales raster and NDC offsets");
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
            // The modulated character shadow (67b1) and the adaptive PCF filter
            // (a195) share d55's c0-c5 reconstruction.
            for (const uint64_t variant : {0x67b10ad6a0e3d811ull, 0xa195c4db25859691ull})
            {
                auto variantVp = bank(0), variantPs = originalPs;
                const auto variantResult = ApplyDrawJitter(0x99c2b4b0960a9ccdull, variant, frame, true, true,
                    &anchor, 37, extent, variantVp.data(), variantPs.data(), &sceneDepth, &sceneDepth);
                Check(variantResult.shadowCompensated && variantVp == shadow && variantPs == ps,
                    "shadow projection variants receive the same reconstruction correction");
            }
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
    auto cachedVp = bank(0), cachedPs = originalPs;
    const auto cachedSample = FrameJitter(5, anchor.viewport.width, anchor.viewport.height);
    const auto cachedResult = ApplyDrawJitter(0x99c2b4b0960a9ccdull, 0x12345678ull, 20,
        true, true, &anchor, 37, anchor.viewport, cachedVp.data(), cachedPs.data(), nullptr, nullptr,
        1, &cachedSample);
    Check(cachedResult.applied && cachedResult.sample.phase == cachedSample.phase &&
        cachedResult.sample.pixelX == cachedSample.pixelX && cachedResult.sample.ndcY == cachedSample.ndcY,
        "draw uses the selected frame jitter instead of recomputing a different phase");
    std::printf("PASS: %u temporal jitter checks; 32 phases at 4 sizes; legacy shadow error %.9g, corrected %.9g\n",
        checks, legacyError, correctedError);
}
