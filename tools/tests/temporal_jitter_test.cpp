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

int main()
{
    TireMaterialCoverage();
    BattleCoverage();
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
