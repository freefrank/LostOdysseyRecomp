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
    // Map tour 2026-10-01 (Uhra - Army Sewers, Eastern Gohtza Railroad Track):
    // the 8d3c program with a 12-dword vertex stride, a scene depth writer.
    case 0xb9b8056050a4c194ull:
    // f5914: stride-56 static depth companion of 799c/eeae; c4-c7 position only.
    case 0x52e4405f97159d2full:
    // f16385-f16387: alpha-tested depth; UV/color outputs are independent of VP.
    case 0xfe3efe042c311110ull:
    // Map tour 2026-10-01 (Experimental Staff Marine Division): the fe3e program
    // with a 14-dword vertex stride, a scene depth writer.
    case 0xf964d2661094b1a0ull:
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
    case 0xa027ab99fa3e3b0dull:
    // f25276 Burning Cave: static material over f7fd depth; c7-c10 feed only oPos
    // and o4, of which PS 042e reads only W. fetch94 light and c11 eye stay intact.
    case 0x61bc9947f1e88573ull:
    // Map tour 2026-10-01: static materials over 52e4 depth whose c7-c10 feed only
    // oPos and the o4 copy, of which their only seen PS reads just W.
    // 2441 + d7f3: Snow-Covered Trail. f8b1 + e5b7: Astral Square, Numara Palace.
    case 0x24418a5936c2d236ull:case 0xf8b1457ed05cacdfull:
    // 8d66 over b030 depth (Ice Canyon - Ice Gorge, Frozen Trail): the same chain;
    // PS c795 and 9e1c read only W of the o1 copy.
    case 0x8d6658641e3b780dull:return 7;
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
    // f6814 paused cutscene: depth writer e9b8 (PS afd8 reads only the o2 W)
    // and later material d31e use the 52e4 scene camera bits at c8-c11 for
    // oPos and its o2/o5 copy only; c7 UV and c12/c13 light/eye stay intact.
    // PS 4907455386b2b291 samples a same-frame resolve of jittered slot-0
    // passes through o5.xy/w, so that lookup must follow the raster jitter.
    case 0xe9b8dd7e7c5a3425ull:case 0xd31e2122a3b51434ull:
    // Opening battle 2026-10-01: alpha-tested depth writers 7def and c511 use the
    // scene camera at c8-c11 for oPos and its o2/o4 copy only (the c12 eye stays
    // intact). Their nine player-feedback PS partners read only W of the copy and
    // kill on alpha sampled at mesh UVs.
    case 0x7def181705ff29c9ull:case 0xc511136caf4421ebull:
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
// Reviewed sky material VS/PS pairs. Each exact pair shares an earlier depth
// draw's c4-c7 camera at c7-c10; other PS partners of the same VS stay held.
struct SkyMaterialPair { uint64_t vs, ps; bool motionFallback; };
inline constexpr SkyMaterialPair SkyMaterialPairs[]{
    // Grand Staff f3448-f3450 (#67), b030 depth. Previously an unknown depth
    // writer, it keeps the conservative whole-frame motion fallback.
    {0xbda41a11626a545cull, 0xa9e9542e2c60029aull, true},
    // Legacy of the Eastern Tribe f1800-f1802 (#102), f7fd depth. Its PS reads
    // only the clip W copy; object motion replays like the depth companion.
    {0xdb23a2ad4493bbb4ull, 0x02ee5f0608be581aull, false},
    // Map tour 2026-10-01, Gohtza - Southernmost Cape: the #102 sky program with
    // another vertex component order, over f7fd depth; PS 311b reads only clip W.
    {0xcbadff38155833b6ull, 0x311b14004ee00284ull, false},
    // Old Sorceress' Mansion (#121): the #67 VS with the #102 PS over b030 depth,
    // from two runtime suspect logs. Same VS, so the same motion fallback as #67.
    {0xbda41a11626a545cull, 0x02ee5f0608be581aull, true},
    // Ice Canyon - Snowy Plateau (F1 f12139): the #67 VS with PS 1dee over b030
    // depth. 1dee reads only the clip W copy; same VS, same fallback as #67.
    {0xbda41a11626a545cull, 0x1dee52ba32155a53ull, true},
    // Map tour 2026-10-01: the #67 VS over b030 depth with six more PS that read
    // only the clip W copy and sample at mesh UVs (triage_suspect.py review).
    // e086: Numara Palace, Ghost Town, Armored Vehicle, Ipsilon Mountains hut.
    {0xbda41a11626a545cull, 0xe086f5f676c72482ull, true},
    // e2b8: Saman - Main Street, Port of Saman.
    {0xbda41a11626a545cull, 0xe2b89a553d00ef47ull, true},
    // 72bc: Experimental Staff Marine Division, Ice Canyon - Glacier Fang, White Boa.
    {0xbda41a11626a545cull, 0x72bcd05d7ab61ce1ull, true},
    // bbba: Uhra - Amphitheater of the Sky, Grand Staff - Central Connector.
    {0xbda41a11626a545cull, 0xbbbac6693e441760ull, true},
    // 4049: The White Boa - Main Deck.
    {0xbda41a11626a545cull, 0x40496f0784d54689ull, true},
    // 1693: Aurora-Bound Train - Engine Car.
    {0xbda41a11626a545cull, 0x1693d368b809e65dull, true},
    // Sea of Baus battle (#203): the #67 VS over b030 depth with PS fd46, which
    // reads only the clip W copy and samples at mesh UVs. Same fallback as #67.
    {0xbda41a11626a545cull, 0xfd46e0190f5f6c50ull, true},
    // Cutscene tour 2026-10-04: bda4 with PS 6426 is the depth-writing base pass
    // under the 2496/42b1 light; it reads only clip W. db23 with PS 1693 (xx4)
    // reads only W of the o1 copy. Same policies as their VS's other pairs.
    {0xbda41a11626a545cull, 0x642665c9452ccafbull, true},
    {0xdb23a2ad4493bbb4ull, 0x1693d368b809e65dull, false},
};
inline const SkyMaterialPair* FindSkyMaterialPair(uint64_t vs, uint64_t ps) {
    for (const auto& pair : SkyMaterialPairs)
        if (pair.vs == vs && pair.ps == ps) return &pair;
    return nullptr;
}
// Reviewed per-light passes drawn over the geometry of a jittered slot-7 scene
// draw without writing depth. Their c7-c10 feed only oPos and the o4 copy; the
// PS samples tex0 at o4.xy/w (light attenuation at ScreenPosition, like the
// f6131 2078/4013 floor light), so the lookup follows the raster jitter.
struct ScreenLightPair { uint64_t vs, ps; };
inline constexpr ScreenLightPair ScreenLightPairs[]{
    // Old Sorceress' Mansion battle (#212): both over the 4053 floor draw with
    // the same world and camera; unjittered, the GEQUAL depth test made the
    // floor lighting flicker. 9bde is e810 with an extra fetch94 scalar.
    {0xe810cfacc107fd3cull, 0xc44ebbbc0207b5a9ull},
    {0x9bdef27080ca3ab4ull, 0xd122f0139a58bdacull},
    // The same floor light in a battle entered from the Entrance Hall.
    {0xe810cfacc107fd3cull, 0xa800980dfc9e4efeull},
    // Cutscene tour 2026-10-04: the other e810 partners, each the same i4
    // attenuation fetch multiplied into the light (a5c3/dba6 add a spot cone).
    {0xe810cfacc107fd3cull, 0x3b45f8f248182356ull}, {0xe810cfacc107fd3cull, 0x5b11f88a8bb293dfull},
    {0xe810cfacc107fd3cull, 0x78a5c96b2d7eaa91ull}, {0xe810cfacc107fd3cull, 0x7e18a8faa49f6a35ull},
    {0xe810cfacc107fd3cull, 0x827f18c3f617e562ull}, {0xe810cfacc107fd3cull, 0x8bc0c849ae1ec109ull},
    {0xe810cfacc107fd3cull, 0xa5c326bc64cc266aull}, {0xe810cfacc107fd3cull, 0xab5e0c09e5a377f5ull},
    {0xe810cfacc107fd3cull, 0xd0d7801796e1ca87ull}, {0xe810cfacc107fd3cull, 0xdba6015f3b42f468ull},
    {0xe810cfacc107fd3cull, 0xfe31f3d6588fde95ull},
    // Cutscene tour: per-light passes of other vertex formats, same structure
    // (world via c0-c3, camera c7-c10 to oPos and one clip copy, tex0 at it).
    {0x00e3a3a34ae36a53ull, 0xaa4c50854f436570ull}, {0x00e3a3a34ae36a53ull, 0xb08ab38de8f5b470ull},
    {0x2214874b92125316ull, 0x4a2275404b5560b2ull}, {0x2496cf2dd8440be6ull, 0x42b12599b7f6a94eull},
    {0x2ec5f87f29727f06ull, 0xc0894ed7e71f3199ull}, {0x3fbb7967479aa2faull, 0xe2b7e7e5d683411bull},
    {0x6c057b35ec257977ull, 0x6d97859b109fcdebull}, {0x6c057b35ec257977ull, 0xdbb8f8521845fcc5ull},
    {0x83f8d16827cb5fb3ull, 0x3745a935e66fb8b9ull}, {0x83f8d16827cb5fb3ull, 0xb38356d8e0623584ull},
    {0xc2eec5754e89870dull, 0xb598060d9a65ea8bull}, {0xc2eec5754e89870dull, 0xcea9fc0081567198ull},
    {0xd9732b36fcc9dc49ull, 0x08678429d24ed0efull}, {0xda5bafb8e0f5ea2dull, 0x921f6bf1ab35afd7ull},
    {0xdd4737cd63de5942ull, 0x8901785286fadee6ull}, {0xef71c9c08352b01aull, 0x4098c2df329dc2b1ull},
    {0xf1833d2ba6fbb269ull, 0xa60815deff98d748ull}, {0xf7fff3419840491eull, 0x55bf9fedd9777eddull},
    // RT_183B: lights over the 66fe depth writer in ReviewedPairs, left behind it
    // once 66fe was jittered.
    {0x3fc19a69242fafedull, 0x638386a1e1151b03ull}, {0xc559554e1745f863ull, 0x6c26c0428188562dull},
};
inline bool IsScreenLightPair(uint64_t vs, uint64_t ps) {
    for (const auto& pair : ScreenLightPairs)
        if (pair.vs == vs && pair.ps == ps) return true;
    return false;
}
// Other reviewed exact pairs (the 2026-10-04 cutscene tour and later reports),
// with the slot the runtime found the scene camera in. Each VS feeds that matrix
// only to oPos and plain clip copies; each PS reads at most clip W or samples a
// same-frame buffer at the pixel. Notes: docs/notes/jitter-cutscene-tour-2026-10-04.md.
struct ReviewedPair { uint64_t vs, ps; int slot; };
inline constexpr ReviewedPair ReviewedPairs[]{
    // Depth writers and materials that read only clip W (or no clip copy).
    {0x1c0053e696cb6770ull, 0x30731f7aad7cb542ull, 8}, {0x4cca1cb1d14cad0cull, 0xe3a453ca3a9f399bull, 7},
    {0x66fe184a69d65dc5ull, 0x0fed3017c576229dull, 7}, {0xa8d2318208e7c3a0ull, 0x00afc8726e2a884aull, 4},
    {0xac328a81ee03a7c8ull, 0xdfdf2b514e15f620ull, 0}, {0xbd4c84ecc2898862ull, 0xf7ff3169953b3a36ull, 7},
    {0xe275fec97d4e7cfbull, 0x07153d2546a589ebull, 7}, {0xeb5f611c4321708eull, 0x38b953b91dcfd9ceull, 0},
    // Slot-8 per-light pass (c7 is its UV transform).
    {0xcabb0b7ea3077c96ull, 0xb1d4594c12ff35d4ull, 8},
    // Cutscene characters: skinned light/depth passes like 3148/118a (camera
    // c233-c236 after the bone blend); one tex0 attenuation fetch at the clip copy.
    {0x258051387347ab2full, 0x957d8f92546fe31aull, 233}, {0x258051387347ab2full, 0x8724d5f1834d7842ull, 233},
    {0x6261e0eb6b69ec62ull, 0xaab158a074b29bcfull, 233}, {0x6261e0eb6b69ec62ull, 0x0aa1c2c4ec7933c1ull, 233},
    {0x69605181e9299128ull, 0xc4689958cc72c568ull, 233},
    // Experimental Staff Marine Division boats (#307): c189 and 3305 are e7b3 and
    // 6742 with a 14-dword vertex stride, HLSL otherwise identical. The c189 base
    // passes draw over the jittered 52e4/f964 depth (GEQUAL, no depth write) and
    // sample the light attenuation at their o4 clip copy, as e7b3 does with the
    // same two PS. 3305 writes depth; its PS read only W of the o1 clip copy.
    {0xc1896d4be9e73859ull, 0x6ad300f19bb477d0ull, 7}, {0xc1896d4be9e73859ull, 0xb3bdd4cd8b83a350ull, 7},
    {0x330542fa74d064deull, 0x8a7a046c63f1213eull, 7}, {0x330542fa74d064deull, 0x8116c07a39250830ull, 7},
};
inline int ReviewedPairSlot(uint64_t vs, uint64_t ps) {
    for (const auto& pair : ReviewedPairs)
        if (pair.vs == vs && pair.ps == ps) return pair.slot;
    return -1;
}
// e810 stays out of the VS-wide table: only its reviewed PS partners may use
// the slot-7 path, so an unreviewed consumer cannot self-anchor it.
inline int DrawPositionVPSlot(uint64_t vs, uint64_t ps, bool constantScreenSample = false) {
    if (FindSkyMaterialPair(vs, ps)) return 7;
    if (vs == 0xe810cfacc107fd3cull && ps == 0xfe31f3d6588fde95ull && constantScreenSample) return 7;
    if (IsScreenLightPair(vs, ps)) return 7;
    if (const int slot = ReviewedPairSlot(vs, ps); slot >= 0) return slot;
    return PositionVPSlot(vs);
}
// Sky pairs jitter only against a scene camera observed before them; they
// never become the frame's camera anchor themselves.
inline bool RequiresEarlierSceneAnchor(uint64_t vs, uint64_t ps) {
    return FindSkyMaterialPair(vs, ps) != nullptr;
}
inline bool RetainsMotionFallback(uint64_t vs, uint64_t ps) {
    const auto* pair = FindSkyMaterialPair(vs, ps);
    return pair && pair->motionFallback;
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
        RememberCamera(anchor);
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
        RememberDepth(sourceAllocation, resolve);
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
    // Some cutscene shots draw a second full scene view with another camera
    // (#212). Draws without their own anchor (shadow volumes, projections,
    // lights) jitter with the observed camera they use, and a projection may
    // sample that view's depth resolve; the first camera stays the frame's.
    const SceneAnchor& AnchorFor(const uint32_t* vp) const
    {
        for (uint32_t i = 0; i < cameraCount_; ++i)
            if (std::equal(cameras_[i].vpBits.begin(), cameras_[i].vpBits.end(), vp)) return cameras_[i];
        return anchor_;
    }
    const SceneResolve& DepthFor(const SceneResolve* sampled) const
    {
        for (uint32_t i = 0; sampled && i < depthCount_; ++i)
            if (depths_[i].ordinal == sampled->ordinal && depths_[i].address == sampled->address) return depths_[i];
        return depth_;
    }
private:
    void RememberCamera(const SceneAnchor& anchor)
    {
        for (uint32_t i = 0; i < cameraCount_; ++i)
            if (cameras_[i].vpBits == anchor.vpBits && cameras_[i].depthAllocation == anchor.depthAllocation &&
                SameViewport(cameras_[i].viewport, anchor.viewport)) return;
        if (cameraCount_ < cameras_.size()) cameras_[cameraCount_++] = anchor;
    }
    void RememberDepth(uint64_t sourceAllocation, const SceneResolve& resolve)
    {
        if (resolve.frame != frame_ || !resolve.ordinal || !resolve.fullExtent || depthCount_ == depths_.size()) return;
        for (uint32_t i = 0; i < cameraCount_; ++i)
            if (cameras_[i].depthAllocation == sourceAllocation && resolve.width == cameras_[i].viewport.width &&
                resolve.height == cameras_[i].viewport.height) { depths_[depthCount_++] = resolve; return; }
    }
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
    std::array<SceneAnchor, 4> cameras_{};
    std::array<SceneResolve, 8> depths_{};
    uint32_t cameraCount_ = 0, depthCount_ = 0;
};
}
