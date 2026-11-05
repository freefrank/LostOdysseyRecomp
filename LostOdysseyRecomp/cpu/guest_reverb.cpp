// The game's XAudio reverb (sub_82CCA0B0: r3 = the reverb, r4 = its two input channel pointers, r5 = its output
// channel pointers): eight filters per channel (recompiled calls), then 256 samples through about 20 delay lines and
// all-pass sections, then a vector mix into the outputs. With one mixing worker it is half of that worker, about 11%
// of a core in the busy street, and the game's audio costs about 11% of the frame rate there (the three cores are
// full). Most of the recompiled loop's time went to its own stack: the PPC code keeps about 40 pointers and partial
// sums in its frame (r1 + 80..372) and reloads them every sample, and every single-precision operation went through
// double and back.
//
// The body below is generated from the recompiled listing by tools/switch/ppc2native.py (arguments: the listing,
// 82CCA0B0 17872 sub_82CCA0B0 82CCAB38 82CCB5B0 82CCAB34): the same instructions in the same order, with four
// differences only. In the 256-sample loop, the 145 memory words at fixed addresses (coefficients, delay lengths and
// line indices of its delay lines) are C++ locals: each is an offset in the reverb, found by evaluating the setup code
// before the loop (several of the loop's pointers name the same word, so they share one local), read once
// before the loop; stores still write memory as well. The recompiled loop reloaded all of them every sample. The
// frame's private slots (below r1 + 384, and the caller's r1 + 20) are C++ locals, which the compiler keeps in
// registers. fadds / fsubs / fmuls run in float: the recompiled code rounds
// them through double to float, and for +, - and * double rounding through a format of 53 bits is innocuous for
// 24-bit operands, so the results are the same. fmadds is computed in double and rounded to float, as the recompiled
// code does (its fused fmadd on doubles). The vector loop keeps lanes in guest element order and uses the same
// operations (fmla for vmaddfp, as the recompiled build fuses it), with flushing on as there.
// Buffers at r1 + 384 and above stay in guest memory: the filters write them.
//
// LO_NATIVE_REVERB=0 keeps the recompiled code; 2 runs both from the same state and compares the reverb's memory
// (0x4B9D0 bytes) and its four outputs (first 20 differences, a count every 2^12 calls).
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <arm_neon.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" PPC_FUNC(__imp__sub_82CCA0B0);

namespace
{
    int NativeReverbMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_REVERB");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    inline float F(uint32_t bits) { return std::bit_cast<float>(bits); }
    inline uint32_t U(float value) { return std::bit_cast<uint32_t>(value); }

    inline float32x4_t LoadVector(uint8_t* base, uint32_t address)
    {
        return vreinterpretq_f32_u8(vrev32q_u8(vld1q_u8(base + address)));
    }

    inline void StoreVector(uint8_t* base, uint32_t address, float32x4_t value)
    {
        vst1q_u8(base + address, vrev32q_u8(vreinterpretq_u8_f32(value)));
    }

    // lvlx: the bytes from address to the end of its 16-byte block, in the vector's first elements; zeros after.
    inline float32x4_t LoadVectorLeft(uint8_t* base, uint32_t address)
    {
        uint8_t bytes[16] = {};
        const uint32_t count = 16 - (address & 15);
        std::memcpy(bytes, base + address, count);
        return vreinterpretq_f32_u8(vrev32q_u8(vld1q_u8(bytes)));
    }

    // Two of the loop's fixed addresses the same (see tools/switch/ppc2native.py): the promotion would be wrong there.
    void LoopAliased(uint32_t address)
    {
        static std::atomic<bool> logged{false};
        if (!logged.exchange(true)) LOG_ERROR("[reverb] two fixed loop addresses meet at {:#x}", address);
    }

    void Native(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t entrySp = ctx.r1.u32;
        const uint32_t object = ctx.r3.u32; // the reverb: the loop's fixed addresses are offsets in it
        const uint64_t entryLr = ctx.lr;
        uint32_t sp = entrySp - 17872;
        PPC_STORE_U32(sp, entrySp); // stwu's back chain
        uint32_t r0 = ctx.r0.u32, r3 = ctx.r3.u32, r4 = ctx.r4.u32, r5 = ctx.r5.u32, r6 = ctx.r6.u32, r7 = ctx.r7.u32, r8 = ctx.r8.u32, r9 = ctx.r9.u32, r10 = ctx.r10.u32, r11 = ctx.r11.u32, r12 = ctx.r12.u32, r14 = ctx.r14.u32, r15 = ctx.r15.u32, r16 = ctx.r16.u32, r17 = ctx.r17.u32, r18 = ctx.r18.u32, r19 = ctx.r19.u32, r20 = ctx.r20.u32, r21 = ctx.r21.u32, r22 = ctx.r22.u32, r23 = ctx.r23.u32, r24 = ctx.r24.u32, r25 = ctx.r25.u32, r26 = ctx.r26.u32, r27 = ctx.r27.u32, r28 = ctx.r28.u32, r29 = ctx.r29.u32, r30 = ctx.r30.u32, r31 = ctx.r31.u32;
        float f0 = float(ctx.f0.f64), f1 = float(ctx.f1.f64), f2 = float(ctx.f2.f64), f3 = float(ctx.f3.f64), f4 = float(ctx.f4.f64), f5 = float(ctx.f5.f64), f6 = float(ctx.f6.f64), f7 = float(ctx.f7.f64), f8 = float(ctx.f8.f64), f9 = float(ctx.f9.f64), f10 = float(ctx.f10.f64), f11 = float(ctx.f11.f64), f12 = float(ctx.f12.f64), f13 = float(ctx.f13.f64), f14 = float(ctx.f14.f64), f15 = float(ctx.f15.f64), f16 = float(ctx.f16.f64), f17 = float(ctx.f17.f64), f18 = float(ctx.f18.f64), f19 = float(ctx.f19.f64), f20 = float(ctx.f20.f64), f21 = float(ctx.f21.f64), f22 = float(ctx.f22.f64), f23 = float(ctx.f23.f64), f24 = float(ctx.f24.f64), f25 = float(ctx.f25.f64), f26 = float(ctx.f26.f64), f27 = float(ctx.f27.f64), f28 = float(ctx.f28.f64), f29 = float(ctx.f29.f64), f30 = float(ctx.f30.f64), f31 = float(ctx.f31.f64);
        float32x4_t v0 = vdupq_n_f32(0), v1 = vdupq_n_f32(0), v2 = vdupq_n_f32(0), v3 = vdupq_n_f32(0), v4 = vdupq_n_f32(0), v5 = vdupq_n_f32(0), v6 = vdupq_n_f32(0), v7 = vdupq_n_f32(0), v8 = vdupq_n_f32(0), v9 = vdupq_n_f32(0), v10 = vdupq_n_f32(0), v11 = vdupq_n_f32(0), v12 = vdupq_n_f32(0), v13 = vdupq_n_f32(0), v31 = vdupq_n_f32(0);
        uint32_t s80 = 0, s84 = 0, s88 = 0, s92 = 0, s96 = 0, s100 = 0, s104 = 0, s108 = 0, s112 = 0, s116 = 0, s120 = 0, s124 = 0, s128 = 0, s132 = 0, s136 = 0, s140 = 0, s144 = 0, s148 = 0, s152 = 0, s156 = 0, s160 = 0, s164 = 0, s168 = 0, s172 = 0, s176 = 0, s180 = 0, s184 = 0, s188 = 0, s192 = 0, s196 = 0, s200 = 0, s204 = 0, s208 = 0, s212 = 0, s216 = 0, s220 = 0, s224 = 0, s228 = 0, s232 = 0, s236 = 0, s240 = 0, s244 = 0, s248 = 0, s252 = 0, s256 = 0, s260 = 0, s264 = 0, s268 = 0, s272 = 0, s276 = 0, s280 = 0, s284 = 0, s288 = 0, s292 = 0, s296 = 0, s300 = 0, s304 = 0, s308 = 0, s312 = 0, s316 = 0, s320 = 0, s324 = 0, s328 = 0, s332 = 0, s336 = 0, s340 = 0, s344 = 0, s348 = 0, s352 = 0, s356 = 0, s360 = 0, s364 = 0, s368 = 0, s372 = 0, s17892 = 0;
        uint32_t m0 = 0, m1 = 0, m2 = 0, m3 = 0, m4 = 0, m5 = 0, m6 = 0, m7 = 0, m8 = 0, m9 = 0, m10 = 0, m11 = 0, m12 = 0, m13 = 0, m14 = 0, m15 = 0, m16 = 0, m17 = 0, m18 = 0, m19 = 0, m20 = 0, m21 = 0, m22 = 0, m23 = 0, m24 = 0, m25 = 0, m26 = 0, m27 = 0, m28 = 0, m29 = 0, m30 = 0, m31 = 0, m32 = 0, m33 = 0, m34 = 0, m35 = 0, m36 = 0, m37 = 0, m38 = 0, m39 = 0, m40 = 0, m41 = 0, m42 = 0, m43 = 0, m44 = 0, m45 = 0, m46 = 0, m47 = 0, m48 = 0, m49 = 0, m50 = 0, m51 = 0, m52 = 0, m53 = 0, m54 = 0, m55 = 0, m56 = 0, m57 = 0, m58 = 0, m59 = 0, m60 = 0, m61 = 0, m62 = 0, m63 = 0, m64 = 0, m65 = 0, m66 = 0, m67 = 0, m68 = 0, m69 = 0, m70 = 0, m71 = 0, m72 = 0, m73 = 0, m74 = 0, m75 = 0, m76 = 0, m77 = 0, m78 = 0, m79 = 0, m80 = 0, m81 = 0, m82 = 0, m83 = 0, m84 = 0, m85 = 0, m86 = 0, m87 = 0, m88 = 0, m89 = 0, m90 = 0, m91 = 0, m92 = 0, m93 = 0, m94 = 0, m95 = 0, m96 = 0, m97 = 0, m98 = 0, m99 = 0, m100 = 0, m101 = 0, m102 = 0, m103 = 0, m104 = 0, m105 = 0, m106 = 0, m107 = 0, m108 = 0, m109 = 0, m110 = 0, m111 = 0, m112 = 0, m113 = 0, m114 = 0, m115 = 0, m116 = 0, m117 = 0, m118 = 0, m119 = 0, m120 = 0, m121 = 0, m122 = 0, m123 = 0, m124 = 0, m125 = 0, m126 = 0, m127 = 0, m128 = 0, m129 = 0, m130 = 0, m131 = 0, m132 = 0, m133 = 0, m134 = 0, m135 = 0, m136 = 0, m137 = 0, m138 = 0, m139 = 0, m140 = 0, m141 = 0, m142 = 0, m143 = 0, m144 = 0;
        int32_t cmpA = 0, cmpB = 0;
        uint32_t cmpUA = 0, cmpUB = 0;
        bool cmpSigned = true;
        // Unconditional: the cached mode can be stale when other code ran on a copy of the context (Check).
        ctx.fpscr.disableFlushModeUnconditional();
#define FLUSH_ON ctx.fpscr.enableFlushMode()
#define CALL(function, returnAddress) do { \
            ctx.r0.u64 = r0; \
            ctx.r3.u64 = r3; \
            ctx.r4.u64 = r4; \
            ctx.r5.u64 = r5; \
            ctx.r6.u64 = r6; \
            ctx.r7.u64 = r7; \
            ctx.r8.u64 = r8; \
            ctx.r9.u64 = r9; \
            ctx.r10.u64 = r10; \
            ctx.r11.u64 = r11; \
            ctx.r12.u64 = r12; \
            ctx.f0.f64 = double(f0); \
            ctx.f1.f64 = double(f1); \
            ctx.f2.f64 = double(f2); \
            ctx.f3.f64 = double(f3); \
            ctx.f4.f64 = double(f4); \
            ctx.f5.f64 = double(f5); \
            ctx.f6.f64 = double(f6); \
            ctx.f7.f64 = double(f7); \
            ctx.f8.f64 = double(f8); \
            ctx.f9.f64 = double(f9); \
            ctx.f10.f64 = double(f10); \
            ctx.f11.f64 = double(f11); \
            ctx.f12.f64 = double(f12); \
            ctx.f13.f64 = double(f13); \
            ctx.r1.u64 = sp; ctx.lr = (returnAddress); \
            function(ctx, base); \
            r0 = ctx.r0.u32; \
            r3 = ctx.r3.u32; \
            r4 = ctx.r4.u32; \
            r5 = ctx.r5.u32; \
            r6 = ctx.r6.u32; \
            r7 = ctx.r7.u32; \
            r8 = ctx.r8.u32; \
            r9 = ctx.r9.u32; \
            r10 = ctx.r10.u32; \
            r11 = ctx.r11.u32; \
            r12 = ctx.r12.u32; \
            f0 = float(ctx.f0.f64); \
            f1 = float(ctx.f1.f64); \
            f2 = float(ctx.f2.f64); \
            f3 = float(ctx.f3.f64); \
            f4 = float(ctx.f4.f64); \
            f5 = float(ctx.f5.f64); \
            f6 = float(ctx.f6.f64); \
            f7 = float(ctx.f7.f64); \
            f8 = float(ctx.f8.f64); \
            f9 = float(ctx.f9.f64); \
            f10 = float(ctx.f10.f64); \
            f11 = float(ctx.f11.f64); \
            f12 = float(ctx.f12.f64); \
            f13 = float(ctx.f13.f64); \
            ctx.fpscr.disableFlushMode(); \
        } while (0)
        // mflr r12 (no effect here)
        // bl 0x82b7a6b0 (register saves: not needed)
        r12 = sp + uint32_t(-152); // addi r12,r1,-152
        // bl 0x82b7b130 (register saves: not needed)
        // ld r12,-4096(r1) (stack probe)
        // ld r12,-8192(r1) (stack probe)
        // ld r12,-12288(r1) (stack probe)
        // ld r12,-16384(r1) (stack probe)
        // stwu r1,-17872(r1) (frame set up at entry)
        r30 = r3; // mr r30,r3
        r11 = PPC_FAST_LOAD_U32(uint32_t(r30 + 100)); // lwz r11,100(r30)
        s17892 = r30; // stw r30,17892(r1)
        cmpA = int32_t(r11); cmpB = 1; cmpSigned = true; // cmpwi cr6,r11,1
        if (cmpSigned ? (cmpA == cmpB) : (cmpUA == cmpUB)) goto loc_82CCA104; // beq cr6,0x82cca104
        cmpA = int32_t(r11); cmpB = 2; cmpSigned = true; // cmpwi cr6,r11,2
        if (cmpSigned ? (cmpA == cmpB) : (cmpUA == cmpUB)) goto loc_82CCA0FC; // beq cr6,0x82cca0fc
        r11 = PPC_FAST_LOAD_U32(r4); // lwz r11,0(r4)
        r29 = PPC_FAST_LOAD_U32(uint32_t(r4 + 4)); // lwz r29,4(r4)
        goto loc_82CCA10C; // b 0x82cca10c
        loc_82CCA0FC:;
        r11 = PPC_FAST_LOAD_U32(uint32_t(r4 + 4)); // lwz r11,4(r4)
        goto loc_82CCA108; // b 0x82cca108
        loc_82CCA104:;
        r11 = PPC_FAST_LOAD_U32(r4); // lwz r11,0(r4)
        loc_82CCA108:;
        r29 = r11; // mr r29,r11
        loc_82CCA10C:;
        r10 = PPC_FAST_LOAD_U32(r5); // lwz r10,0(r5)
        s148 = r11; // stw r11,148(r1)
        s140 = r29; // stw r29,140(r1)
        s316 = r10; // stw r10,316(r1)
        r10 = PPC_FAST_LOAD_U32(uint32_t(r5 + 4)); // lwz r10,4(r5)
        s308 = r10; // stw r10,308(r1)
        r10 = PPC_FAST_LOAD_U32(uint32_t(r5 + 12)); // lwz r10,12(r5)
        s300 = r10; // stw r10,300(r1)
        r10 = PPC_FAST_LOAD_U32(uint32_t(r5 + 16)); // lwz r10,16(r5)
        s292 = r10; // stw r10,292(r1)
        // dcbt r0,r11 (no effect here)
        // dcbt r0,r29 (no effect here)
        r10 = uint32_t(128); // li r10,128
        // dcbt r10,r11 (no effect here)
        // dcbt r10,r29 (no effect here)
        r5 = sp + uint32_t(7648); // addi r5,r1,7648
        r4 = r11; // mr r4,r11
        r3 = r30 + uint32_t(104); // addi r3,r30,104
        CALL(sub_82CC8A40, 0x82cca158u); // bl 0x82cc8a40
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r6 = uint32_t(256); // li r6,256
        r5 = sp + uint32_t(9824); // addi r5,r1,9824
        r4 = sp + uint32_t(7648); // addi r4,r1,7648
        r3 = r3 + uint32_t(168); // addi r3,r3,168
        CALL(sub_82CC90D8, 0x82cca170u); // bl 0x82cc90d8
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r7 = uint32_t(256); // li r7,256
        r6 = sp + uint32_t(6608); // addi r6,r1,6608
        r5 = sp + uint32_t(4544); // addi r5,r1,4544
        r4 = sp + uint32_t(9824); // addi r4,r1,9824
        r3 = r3 + uint32_t(240); // addi r3,r3,240
        CALL(sub_82CC9208, 0x82cca18cu); // bl 0x82cc9208
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r4 = sp + uint32_t(4544); // addi r4,r1,4544
        r3 = r3 + uint32_t(2332); // addi r3,r3,2332
        CALL(sub_82CC92D8, 0x82cca19cu); // bl 0x82cc92d8
        r11 = sp + uint32_t(14207); // addi r11,r1,14207
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r31 = r11 & 0xffffff80u; // rlwinm r31,r11,0,0,24
        r7 = uint32_t(256); // li r7,256
        r6 = sp + uint32_t(13040); // addi r6,r1,13040
        r4 = sp + uint32_t(4544); // addi r4,r1,4544
        r3 = r3 + uint32_t(2868); // addi r3,r3,2868
        r5 = r31; // mr r5,r31
        CALL(sub_82CC9560, 0x82cca1c0u); // bl 0x82cc9560
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r4 = r31; // mr r4,r31
        r3 = r3 + uint32_t(11104); // addi r3,r3,11104
        CALL(sub_82CC9630, 0x82cca1d0u); // bl 0x82cc9630
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r5 = sp + uint32_t(3504); // addi r5,r1,3504
        r4 = r31; // mr r4,r31
        r3 = r3 + uint32_t(12152); // addi r3,r3,12152
        CALL(sub_82CC9900, 0x82cca1e4u); // bl 0x82cc9900
        r11 = sp + uint32_t(15375); // addi r11,r1,15375
        r10 = sp + uint32_t(16543); // addi r10,r1,16543
        r11 = r11 & 0xffffff80u; // rlwinm r11,r11,0,0,24
        r5 = r10 & 0xffffff80u; // rlwinm r5,r10,0,0,24
        r6 = r11 + uint32_t(8); // addi r6,r11,8
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r4 = sp + uint32_t(3504); // addi r4,r1,3504
        r3 = r3 + uint32_t(14224); // addi r3,r3,14224
        s220 = r5; // stw r5,220(r1)
        s208 = r6; // stw r6,208(r1)
        CALL(sub_82CC9A68, 0x82cca210u); // bl 0x82cc9a68
        r3 = r30 + uint32_t(65536); // addis r3,r30,1
        r5 = sp + uint32_t(7648); // addi r5,r1,7648
        r4 = r29; // mr r4,r29
        r3 = r3 + uint32_t(136); // addi r3,r3,136
        CALL(sub_82CC8A40, 0x82cca224u); // bl 0x82cc8a40
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r6 = uint32_t(256); // li r6,256
        r5 = sp + uint32_t(9824); // addi r5,r1,9824
        r4 = sp + uint32_t(7648); // addi r4,r1,7648
        r3 = r3 + uint32_t(200); // addi r3,r3,200
        CALL(sub_82CC90D8, 0x82cca23cu); // bl 0x82cc90d8
        r7 = uint32_t(256); // li r7,256
        r6 = sp + uint32_t(3504); // addi r6,r1,3504
        r5 = sp + uint32_t(2464); // addi r5,r1,2464
        r4 = sp + uint32_t(9824); // addi r4,r1,9824
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r3 = r3 + uint32_t(18360); // addi r3,r3,18360
        CALL(sub_82CC9208, 0x82cca258u); // bl 0x82cc9208
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r4 = sp + uint32_t(2464); // addi r4,r1,2464
        r3 = r3 + uint32_t(20452); // addi r3,r3,20452
        CALL(sub_82CC92D8, 0x82cca268u); // bl 0x82cc92d8
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r7 = uint32_t(256); // li r7,256
        r6 = sp + uint32_t(12000); // addi r6,r1,12000
        r5 = sp + uint32_t(1424); // addi r5,r1,1424
        r4 = sp + uint32_t(2464); // addi r4,r1,2464
        r3 = r3 + uint32_t(20988); // addi r3,r3,20988
        CALL(sub_82CC9560, 0x82cca284u); // bl 0x82cc9560
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r4 = sp + uint32_t(1424); // addi r4,r1,1424
        r3 = r3 + uint32_t(29224); // addi r3,r3,29224
        CALL(sub_82CC9630, 0x82cca294u); // bl 0x82cc9630
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r5 = sp + uint32_t(384); // addi r5,r1,384
        r4 = sp + uint32_t(1424); // addi r4,r1,1424
        r3 = r3 + uint32_t(30272); // addi r3,r3,30272
        CALL(sub_82CC9900, 0x82cca2a8u); // bl 0x82cc9900
        r11 = sp + uint32_t(7775); // addi r11,r1,7775
        r10 = sp + uint32_t(9951); // addi r10,r1,9951
        r11 = r11 & 0xffffff80u; // rlwinm r11,r11,0,0,24
        r5 = r10 & 0xffffff80u; // rlwinm r5,r10,0,0,24
        r6 = r11 + uint32_t(8); // addi r6,r11,8
        r3 = r30 + uint32_t(131072); // addis r3,r30,2
        r4 = sp + uint32_t(384); // addi r4,r1,384
        r3 = r3 + uint32_t(32344); // addi r3,r3,32344
        s212 = r5; // stw r5,212(r1)
        s216 = r6; // stw r6,216(r1)
        s204 = r5; // stw r5,204(r1)
        CALL(sub_82CC9A68, 0x82cca2d8u); // bl 0x82cc9a68
        r11 = uint32_t(131072); // lis r11,2
        f0 = F(PPC_FAST_LOAD_U32(uint32_t(r30 + 96))); // lfs f0,96(r30)
        r10 = uint32_t(131072); // lis r10,2
        r11 = r11 | 18352u; // ori r11,r11,18352
        r10 = r10 | 236u; // ori r10,r10,236
        r9 = uint32_t(131072); // lis r9,2
        r8 = uint32_t(131072); // lis r8,2
        r9 = r9 | 18356u; // ori r9,r9,18356
        f13 = F(PPC_FAST_LOAD_U32(uint32_t(r30 + r11))); // lfsx f13,r30,r11
        r11 = sp + uint32_t(1424); // addi r11,r1,1424
        f12 = F(PPC_FAST_LOAD_U32(uint32_t(r30 + r10))); // lfsx f12,r30,r10
        r10 = sp + uint32_t(4544); // addi r10,r1,4544
        r11 = r11 - r31; // subf r11,r31,r11
        r8 = r8 | 232u; // ori r8,r8,232
        f11 = F(PPC_FAST_LOAD_U32(uint32_t(r30 + r9))); // lfsx f11,r30,r9
        r9 = sp + uint32_t(3504); // addi r9,r1,3504
        r7 = sp + uint32_t(2464); // addi r7,r1,2464
        r5 = sp + uint32_t(6612); // addi r5,r1,6612
        s136 = r11; // stw r11,136(r1)
        r11 = r10 - r31; // subf r11,r31,r10
        f10 = F(PPC_FAST_LOAD_U32(uint32_t(r30 + r8))); // lfsx f10,r30,r8
        r8 = sp + uint32_t(2468); // addi r8,r1,2468
        r7 = r7 - r31; // subf r7,r31,r7
        r6 = sp + uint32_t(6608); // addi r6,r1,6608
        r22 = r31 + uint32_t(4); // addi r22,r31,4
        s104 = r11; // stw r11,104(r1)
        r11 = r9 - r31; // subf r11,r31,r9
        r4 = sp + uint32_t(1428); // addi r4,r1,1428
        s128 = r7; // stw r7,128(r1)
        r7 = r6 - r31; // subf r7,r31,r6
        r3 = sp + uint32_t(4548); // addi r3,r1,4548
        r30 = sp + uint32_t(3508); // addi r30,r1,3508
        s152 = r22; // stw r22,152(r1)
        s184 = r11; // stw r11,184(r1)
        r11 = r8 - r31; // subf r11,r31,r8
        r29 = sp + uint32_t(2472); // addi r29,r1,2472
        r28 = sp + uint32_t(6616); // addi r28,r1,6616
        s112 = r7; // stw r7,112(r1)
        r27 = sp + uint32_t(1432); // addi r27,r1,1432
        r26 = sp + uint32_t(4552); // addi r26,r1,4552
        s172 = r11; // stw r11,172(r1)
        r11 = r5 - r31; // subf r11,r31,r5
        r25 = sp + uint32_t(3512); // addi r25,r1,3512
        r24 = sp + uint32_t(2476); // addi r24,r1,2476
        r23 = sp + uint32_t(6620); // addi r23,r1,6620
        s168 = r11; // stw r11,168(r1)
        r11 = r4 - r31; // subf r11,r31,r4
        s132 = r11; // stw r11,132(r1)
        r11 = r3 - r31; // subf r11,r31,r3
        r10 = sp + uint32_t(4556); // addi r10,r1,4556
        r9 = sp + uint32_t(5608); // addi r9,r1,5608
        r8 = sp + uint32_t(408); // addi r8,r1,408
        r7 = sp + uint32_t(2484); // addi r7,r1,2484
        s180 = r11; // stw r11,180(r1)
        r11 = r30 - r31; // subf r11,r31,r30
        s80 = r10; // stw r10,80(r1)
        r10 = sp + uint32_t(3516); // addi r10,r1,3516
        r6 = sp + uint32_t(6628); // addi r6,r1,6628
        r5 = sp + uint32_t(1444); // addi r5,r1,1444
        r4 = sp + uint32_t(4564); // addi r4,r1,4564
        s200 = r11; // stw r11,200(r1)
        r11 = r29 - r31; // subf r11,r31,r29
        s84 = r10; // stw r10,84(r1)
        r10 = sp + uint32_t(2480); // addi r10,r1,2480
        r3 = sp + uint32_t(3524); // addi r3,r1,3524
        r30 = sp + uint32_t(2488); // addi r30,r1,2488
        r29 = sp + uint32_t(6632); // addi r29,r1,6632
        s188 = r11; // stw r11,188(r1)
        r11 = r28 - r31; // subf r11,r31,r28
        s100 = r10; // stw r10,100(r1)
        r10 = sp + uint32_t(6624); // addi r10,r1,6624
        r28 = sp + uint32_t(1448); // addi r28,r1,1448
        r22 = sp + uint32_t(388); // addi r22,r1,388
        r21 = sp + uint32_t(5592); // addi r21,r1,5592
        s176 = r11; // stw r11,176(r1)
        r11 = r27 - r31; // subf r11,r31,r27
        s96 = r10; // stw r10,96(r1)
        r10 = sp + uint32_t(1440); // addi r10,r1,1440
        r27 = sp + uint32_t(4568); // addi r27,r1,4568
        r20 = sp + uint32_t(392); // addi r20,r1,392
        r19 = sp + uint32_t(5596); // addi r19,r1,5596
        s144 = r11; // stw r11,144(r1)
        r11 = r26 - r31; // subf r11,r31,r26
        s92 = r10; // stw r10,92(r1)
        r10 = sp + uint32_t(4560); // addi r10,r1,4560
        r26 = sp + uint32_t(3528); // addi r26,r1,3528
        r18 = sp + uint32_t(396); // addi r18,r1,396
        r17 = sp + uint32_t(5600); // addi r17,r1,5600
        s196 = r11; // stw r11,196(r1)
        r11 = r25 - r31; // subf r11,r31,r25
        s88 = r10; // stw r10,88(r1)
        r10 = sp + uint32_t(3520); // addi r10,r1,3520
        r25 = sp + uint32_t(5584); // addi r25,r1,5584
        r16 = sp + uint32_t(400); // addi r16,r1,400
        r15 = sp + uint32_t(5604); // addi r15,r1,5604
        s192 = r11; // stw r11,192(r1)
        r11 = r24 - r31; // subf r11,r31,r24
        r24 = sp + uint32_t(384); // addi r24,r1,384
        r14 = sp + uint32_t(404); // addi r14,r1,404
        r7 = r7 - r31; // subf r7,r31,r7
        r6 = r6 - r31; // subf r6,r31,r6
        s160 = r11; // stw r11,160(r1)
        r11 = r23 - r31; // subf r11,r31,r23
        r23 = sp + uint32_t(5588); // addi r23,r1,5588
        s108 = r11; // stw r11,108(r1)
        r11 = sp + uint32_t(1436); // addi r11,r1,1436
        r11 = r11 - r31; // subf r11,r31,r11
        s164 = r11; // stw r11,164(r1)
        r11 = s80; // lwz r11,80(r1)
        s80 = r9; // stw r9,80(r1)
        r11 = r11 - r31; // subf r11,r31,r11
        s116 = r11; // stw r11,116(r1)
        r11 = s84; // lwz r11,84(r1)
        s84 = r8; // stw r8,84(r1)
        r8 = r10 - r31; // subf r8,r31,r10
        r11 = r11 - r31; // subf r11,r31,r11
        s120 = r11; // stw r11,120(r1)
        r11 = s100; // lwz r11,100(r1)
        r11 = r11 - r31; // subf r11,r31,r11
        s124 = r11; // stw r11,124(r1)
        r11 = s96; // lwz r11,96(r1)
        r11 = r11 - r31; // subf r11,r31,r11
        s96 = r11; // stw r11,96(r1)
        r11 = s92; // lwz r11,92(r1)
        r11 = r11 - r31; // subf r11,r31,r11
        s100 = r11; // stw r11,100(r1)
        r11 = s88; // lwz r11,88(r1)
        r9 = r11 - r31; // subf r9,r31,r11
        r11 = r15 - r31; // subf r11,r31,r15
        r5 = r5 - r31; // subf r5,r31,r5
        r4 = r4 - r31; // subf r4,r31,r4
        r3 = r3 - r31; // subf r3,r31,r3
        r30 = r30 - r31; // subf r30,r31,r30
        s92 = r11; // stw r11,92(r1)
        r11 = r14 - r31; // subf r11,r31,r14
        r29 = r29 - r31; // subf r29,r31,r29
        r28 = r28 - r31; // subf r28,r31,r28
        r27 = r27 - r31; // subf r27,r31,r27
        r26 = r26 - r31; // subf r26,r31,r26
        s88 = r11; // stw r11,88(r1)
        r25 = r25 - r31; // subf r25,r31,r25
        r11 = s80; // lwz r11,80(r1)
        r24 = r24 - r31; // subf r24,r31,r24
        r23 = r23 - r31; // subf r23,r31,r23
        r11 = r11 - r31; // subf r11,r31,r11
        r22 = r22 - r31; // subf r22,r31,r22
        r21 = r21 - r31; // subf r21,r31,r21
        r20 = r20 - r31; // subf r20,r31,r20
        r19 = r19 - r31; // subf r19,r31,r19
        s80 = r11; // stw r11,80(r1)
        r18 = r18 - r31; // subf r18,r31,r18
        r11 = s84; // lwz r11,84(r1)
        r17 = r17 - r31; // subf r17,r31,r17
        r16 = r16 - r31; // subf r16,r31,r16
        r11 = r11 - r31; // subf r11,r31,r11
        r10 = uint32_t(0); // li r10,0
        s84 = r11; // stw r11,84(r1)
        r11 = s152; // lwz r11,152(r1)
        loc_82CCA550:;
        r31 = sp + uint32_t(1424); // addi r31,r1,1424
        r15 = s136; // lwz r15,136(r1)
        r14 = s128; // lwz r14,128(r1)
        f9 = F(PPC_FAST_LOAD_U32(uint32_t(r11 + -4))); // lfs f9,-4(r11)
        f9 = f9 * f12; // fmuls f9,f9,f12
        f7 = F(PPC_FAST_LOAD_U32(uint32_t(r11 + 4))); // lfs f7,4(r11)
        f5 = F(PPC_FAST_LOAD_U32(uint32_t(r11 + 8))); // lfs f5,8(r11)
        f7 = f7 * f12; // fmuls f7,f7,f12
        f5 = f5 * f12; // fmuls f5,f5,f12
        f4 = F(PPC_FAST_LOAD_U32(uint32_t(r10 + r31))); // lfsx f4,r10,r31
        r31 = s144; // lwz r31,144(r1)
        f8 = F(PPC_FAST_LOAD_U32(uint32_t(r15 + r11))); // lfsx f8,r15,r11
        r15 = s132; // lwz r15,132(r1)
        f2 = F(PPC_FAST_LOAD_U32(uint32_t(r14 + r11))); // lfsx f2,r14,r11
        r14 = s112; // lwz r14,112(r1)
        f4 = f4 * f11; // fmuls f4,f4,f11
        f8 = f8 * f11; // fmuls f8,f8,f11
        f3 = F(PPC_FAST_LOAD_U32(uint32_t(r31 + r11))); // lfsx f3,r31,r11
        r31 = sp + uint32_t(2464); // addi r31,r1,2464
        f6 = F(PPC_FAST_LOAD_U32(uint32_t(r15 + r11))); // lfsx f6,r15,r11
        r15 = sp + uint32_t(4544); // addi r15,r1,4544
        f6 = f6 * f11; // fmuls f6,f6,f11
        f1 = F(PPC_FAST_LOAD_U32(uint32_t(r14 + r11))); // lfsx f1,r14,r11
        f3 = f3 * f11; // fmuls f3,f3,f11
        r14 = s104; // lwz r14,104(r1)
        f2 = float(std::fma(double(f2), double(f13), double(f1))); // fmadds f2,f2,f13,f1
        f1 = F(PPC_FAST_LOAD_U32(r11)); // lfs f1,0(r11)
        f30 = F(PPC_FAST_LOAD_U32(uint32_t(r10 + r31))); // lfsx f30,r10,r31
        r31 = s172; // lwz r31,172(r1)
        f29 = F(PPC_FAST_LOAD_U32(uint32_t(r10 + r15))); // lfsx f29,r10,r15
        r15 = sp + uint32_t(3504); // addi r15,r1,3504
        f9 = float(std::fma(double(f30), double(f13), double(f9))); // fmadds f9,f30,f13,f9
        f4 = float(std::fma(double(f29), double(f10), double(f4))); // fmadds f4,f29,f10,f4
        f31 = F(PPC_FAST_LOAD_U32(uint32_t(r14 + r11))); // lfsx f31,r14,r11
        r14 = s184; // lwz r14,184(r1)
        f8 = float(std::fma(double(f31), double(f10), double(f8))); // fmadds f8,f31,f10,f8
        f28 = F(PPC_FAST_LOAD_U32(uint32_t(r31 + r11))); // lfsx f28,r31,r11
        r31 = s180; // lwz r31,180(r1)
        f7 = float(std::fma(double(f28), double(f13), double(f7))); // fmadds f7,f28,f13,f7
        f31 = F(PPC_FAST_LOAD_U32(uint32_t(r14 + r11))); // lfsx f31,r14,r11
        r14 = s168; // lwz r14,168(r1)
        f27 = F(PPC_FAST_LOAD_U32(uint32_t(r31 + r11))); // lfsx f27,r31,r11
        r31 = s188; // lwz r31,188(r1)
        f6 = float(std::fma(double(f27), double(f10), double(f6))); // fmadds f6,f27,f10,f6
        f27 = F(PPC_FAST_LOAD_U32(uint32_t(r10 + r15))); // lfsx f27,r10,r15
        r15 = s200; // lwz r15,200(r1)
        f2 = float(std::fma(double(f12), double(f1), double(f2))); // fmadds f2,f12,f1,f2
        f4 = f4 + f27; // fadds f4,f4,f27
        f29 = F(PPC_FAST_LOAD_U32(uint32_t(r14 + r11))); // lfsx f29,r14,r11
        f8 = f8 + f31; // fadds f8,f8,f31
        f26 = F(PPC_FAST_LOAD_U32(uint32_t(r31 + r11))); // lfsx f26,r31,r11
        r31 = s196; // lwz r31,196(r1)
        f5 = float(std::fma(double(f26), double(f13), double(f5))); // fmadds f5,f26,f13,f5
        r14 = s160; // lwz r14,160(r1)
        f7 = f7 + f29; // fadds f7,f7,f29
        f30 = F(PPC_FAST_LOAD_U32(uint32_t(r31 + r11))); // lfsx f30,r31,r11
        r31 = sp + uint32_t(6608); // addi r31,r1,6608
        f3 = float(std::fma(double(f30), double(f10), double(f3))); // fmadds f3,f30,f10,f3
        f30 = F(PPC_FAST_LOAD_U32(uint32_t(r15 + r11))); // lfsx f30,r15,r11
        r15 = s176; // lwz r15,176(r1)
        f27 = F(PPC_FAST_LOAD_U32(uint32_t(r14 + r11))); // lfsx f27,r14,r11
        f6 = f6 + f30; // fadds f6,f6,f30
        f28 = F(PPC_FAST_LOAD_U32(uint32_t(r10 + r31))); // lfsx f28,r10,r31
        r31 = sp + uint32_t(5584); // addi r31,r1,5584
        f9 = f9 + f28; // fadds f9,f9,f28
        f26 = F(PPC_FAST_LOAD_U32(uint32_t(r15 + r11))); // lfsx f26,r15,r11
        r15 = s192; // lwz r15,192(r1)
        f5 = f5 + f26; // fadds f5,f5,f26
        f28 = F(PPC_FAST_LOAD_U32(uint32_t(r15 + r11))); // lfsx f28,r15,r11
        r15 = sp + uint32_t(384); // addi r15,r1,384
        f3 = f3 + f28; // fadds f3,f3,f28
        f9 = f9 * f0; // fmuls f9,f9,f0
        PPC_FAST_STORE_U32(uint32_t(r10 + r31), U(f9)); // stfsx f9,r10,r31
        f9 = f4 * f0; // fmuls f9,f4,f0
        PPC_FAST_STORE_U32(uint32_t(r10 + r15), U(f9)); // stfsx f9,r10,r15
        f9 = f2 * f0; // fmuls f9,f2,f0
        PPC_FAST_STORE_U32(uint32_t(r25 + r11), U(f9)); // stfsx f9,r25,r11
        f9 = f8 * f0; // fmuls f9,f8,f0
        PPC_FAST_STORE_U32(uint32_t(r24 + r11), U(f9)); // stfsx f9,r24,r11
        f9 = f7 * f0; // fmuls f9,f7,f0
        PPC_FAST_STORE_U32(uint32_t(r23 + r11), U(f9)); // stfsx f9,r23,r11
        r31 = s164; // lwz r31,164(r1)
        f9 = f6 * f0; // fmuls f9,f6,f0
        PPC_FAST_STORE_U32(uint32_t(r22 + r11), U(f9)); // stfsx f9,r22,r11
        f9 = f5 * f0; // fmuls f9,f5,f0
        PPC_FAST_STORE_U32(uint32_t(r21 + r11), U(f9)); // stfsx f9,r21,r11
        f9 = f3 * f0; // fmuls f9,f3,f0
        PPC_FAST_STORE_U32(uint32_t(r20 + r11), U(f9)); // stfsx f9,r20,r11
        r10 = r10 + uint32_t(32); // addi r10,r10,32
        f9 = F(PPC_FAST_LOAD_U32(uint32_t(r11 + 12))); // lfs f9,12(r11)
        f8 = F(PPC_FAST_LOAD_U32(uint32_t(r31 + r11))); // lfsx f8,r31,r11
        r31 = s100; // lwz r31,100(r1)
        f9 = f9 * f12; // fmuls f9,f9,f12
        f7 = F(PPC_FAST_LOAD_U32(uint32_t(r11 + 16))); // lfs f7,16(r11)
        f8 = f8 * f11; // fmuls f8,f8,f11
        f5 = F(PPC_FAST_LOAD_U32(uint32_t(r11 + 20))); // lfs f5,20(r11)
        f7 = f7 * f12; // fmuls f7,f7,f12
        f4 = F(PPC_FAST_LOAD_U32(uint32_t(r5 + r11))); // lfsx f4,r5,r11
        f5 = f5 * f12; // fmuls f5,f5,f12
        f30 = F(PPC_FAST_LOAD_U32(uint32_t(r9 + r11))); // lfsx f30,r9,r11
        f6 = F(PPC_FAST_LOAD_U32(uint32_t(r31 + r11))); // lfsx f6,r31,r11
        r31 = s116; // lwz r31,116(r1)
        f6 = f6 * f11; // fmuls f6,f6,f11
        f29 = F(PPC_FAST_LOAD_U32(uint32_t(r7 + r11))); // lfsx f29,r7,r11
        f4 = f4 * f11; // fmuls f4,f4,f11
        f3 = F(PPC_FAST_LOAD_U32(uint32_t(r11 + 24))); // lfs f3,24(r11)
        f3 = f3 * f12; // fmuls f3,f3,f12
        f28 = F(PPC_FAST_LOAD_U32(uint32_t(r4 + r11))); // lfsx f28,r4,r11
        f2 = F(PPC_FAST_LOAD_U32(uint32_t(r28 + r11))); // lfsx f2,r28,r11
        f1 = F(PPC_FAST_LOAD_U32(uint32_t(r31 + r11))); // lfsx f1,r31,r11
        r31 = s124; // lwz r31,124(r1)
        f9 = float(std::fma(double(f27), double(f13), double(f9))); // fmadds f9,f27,f13,f9
        f26 = F(PPC_FAST_LOAD_U32(uint32_t(r30 + r11))); // lfsx f26,r30,r11
        f8 = float(std::fma(double(f1), double(f10), double(f8))); // fmadds f8,f1,f10,f8
        f25 = F(PPC_FAST_LOAD_U32(uint32_t(r27 + r11))); // lfsx f25,r27,r11
        f2 = f2 * f11; // fmuls f2,f2,f11
        f23 = F(PPC_FAST_LOAD_U32(uint32_t(r26 + r11))); // lfsx f23,r26,r11
        f5 = float(std::fma(double(f29), double(f13), double(f5))); // fmadds f5,f29,f13,f5
        f29 = F(PPC_FAST_LOAD_U32(uint32_t(r3 + r11))); // lfsx f29,r3,r11
        f31 = F(PPC_FAST_LOAD_U32(uint32_t(r31 + r11))); // lfsx f31,r31,r11
        r31 = s108; // lwz r31,108(r1)
        f7 = float(std::fma(double(f31), double(f13), double(f7))); // fmadds f7,f31,f13,f7
        f31 = F(PPC_FAST_LOAD_U32(uint32_t(r8 + r11))); // lfsx f31,r8,r11
        f6 = float(std::fma(double(f30), double(f10), double(f6))); // fmadds f6,f30,f10,f6
        f30 = F(PPC_FAST_LOAD_U32(uint32_t(r6 + r11))); // lfsx f30,r6,r11
        f4 = float(std::fma(double(f28), double(f10), double(f4))); // fmadds f4,f28,f10,f4
        f28 = F(PPC_FAST_LOAD_U32(uint32_t(r29 + r11))); // lfsx f28,r29,r11
        f3 = float(std::fma(double(f26), double(f13), double(f3))); // fmadds f3,f26,f13,f3
        f24 = F(PPC_FAST_LOAD_U32(uint32_t(r31 + r11))); // lfsx f24,r31,r11
        r31 = s120; // lwz r31,120(r1)
        f9 = f9 + f24; // fadds f9,f9,f24
        f2 = float(std::fma(double(f25), double(f10), double(f2))); // fmadds f2,f25,f10,f2
        f5 = f5 + f30; // fadds f5,f5,f30
        f27 = F(PPC_FAST_LOAD_U32(uint32_t(r31 + r11))); // lfsx f27,r31,r11
        r31 = s96; // lwz r31,96(r1)
        f8 = f8 + f27; // fadds f8,f8,f27
        f6 = f6 + f31; // fadds f6,f6,f31
        f4 = f4 + f29; // fadds f4,f4,f29
        f3 = f3 + f28; // fadds f3,f3,f28
        f1 = F(PPC_FAST_LOAD_U32(uint32_t(r31 + r11))); // lfsx f1,r31,r11
        r31 = s92; // lwz r31,92(r1)
        f7 = f7 + f1; // fadds f7,f7,f1
        f9 = f9 * f0; // fmuls f9,f9,f0
        PPC_FAST_STORE_U32(uint32_t(r19 + r11), U(f9)); // stfsx f9,r19,r11
        f2 = f2 + f23; // fadds f2,f2,f23
        f9 = f8 * f0; // fmuls f9,f8,f0
        PPC_FAST_STORE_U32(uint32_t(r18 + r11), U(f9)); // stfsx f9,r18,r11
        f9 = f7 * f0; // fmuls f9,f7,f0
        PPC_FAST_STORE_U32(uint32_t(r17 + r11), U(f9)); // stfsx f9,r17,r11
        f9 = f6 * f0; // fmuls f9,f6,f0
        PPC_FAST_STORE_U32(uint32_t(r16 + r11), U(f9)); // stfsx f9,r16,r11
        f9 = f5 * f0; // fmuls f9,f5,f0
        PPC_FAST_STORE_U32(uint32_t(r31 + r11), U(f9)); // stfsx f9,r31,r11
        r31 = s88; // lwz r31,88(r1)
        f9 = f4 * f0; // fmuls f9,f4,f0
        PPC_FAST_STORE_U32(uint32_t(r31 + r11), U(f9)); // stfsx f9,r31,r11
        r31 = s80; // lwz r31,80(r1)
        f9 = f3 * f0; // fmuls f9,f3,f0
        PPC_FAST_STORE_U32(uint32_t(r31 + r11), U(f9)); // stfsx f9,r31,r11
        r31 = s84; // lwz r31,84(r1)
        f9 = f2 * f0; // fmuls f9,f2,f0
        PPC_FAST_STORE_U32(uint32_t(r31 + r11), U(f9)); // stfsx f9,r31,r11
        r11 = r11 + uint32_t(32); // addi r11,r11,32
        cmpA = int32_t(r10); cmpB = 1024; cmpSigned = true; // cmpwi cr6,r10,1024
        if (cmpSigned ? (cmpA < cmpB) : (cmpUA < cmpUB)) goto loc_82CCA550; // blt cr6,0x82cca550
        r11 = s17892; // lwz r11,17892(r1)
        r31 = s17892; // lwz r31,17892(r1)
        r10 = r11 + uint32_t(196608); // addis r10,r11,3
        r10 = r10 + uint32_t(-29064); // addi r10,r10,-29064
        s144 = r10; // stw r10,144(r1)
        r10 = r11 + uint32_t(196608); // addis r10,r11,3
        r11 = r11 + uint32_t(196608); // addis r11,r11,3
        r10 = r10 + uint32_t(-26992); // addi r10,r10,-26992
        r11 = r11 + uint32_t(-24920); // addi r11,r11,-24920
        s132 = r10; // stw r10,132(r1)
        s356 = r11; // stw r11,356(r1)
        r11 = uint32_t(131072); // lis r11,2
        r11 = r11 | 38556u; // ori r11,r11,38556
        s120 = r11; // stw r11,120(r1)
        r11 = uint32_t(131072); // lis r11,2
        r11 = r11 | 40624u; // ori r11,r11,40624
        s108 = r11; // stw r11,108(r1)
        r11 = uint32_t(131072); // lis r11,2
        r11 = r11 | 40656u; // ori r11,r11,40656
        s124 = r11; // stw r11,124(r1)
        r11 = uint32_t(131072); // lis r11,2
        r11 = r11 | 40676u; // ori r11,r11,40676
        s116 = r11; // stw r11,116(r1)
        r11 = uint32_t(0); // li r11,0
        s104 = r11; // stw r11,104(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(-29052); // addi r11,r11,-29052
        s192 = r11; // stw r11,192(r1)
        r18 = s144; // lwz r18,144(r1)
        r30 = r18 + uint32_t(16); // addi r30,r18,16
        r11 = s120; // lwz r11,120(r1)
        r11 = r31 + r11; // add r11,r31,r11
        s176 = r11; // stw r11,176(r1)
        r11 = s108; // lwz r11,108(r1)
        r11 = r31 + r11; // add r11,r31,r11
        s200 = r11; // stw r11,200(r1)
        r11 = s124; // lwz r11,124(r1)
        r11 = r31 + r11; // add r11,r31,r11
        s168 = r11; // stw r11,168(r1)
        r11 = s116; // lwz r11,116(r1)
        r11 = r31 + r11; // add r11,r31,r11
        s184 = r11; // stw r11,184(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(-24828); // addi r11,r11,-24828
        s196 = r11; // stw r11,196(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(-8412); // addi r11,r11,-8412
        s188 = r11; // stw r11,188(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(-24816); // addi r11,r11,-24816
        s124 = r11; // stw r11,124(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(-8384); // addi r11,r11,-8384
        s180 = r11; // stw r11,180(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(-6312); // addi r11,r11,-6312
        s172 = r11; // stw r11,172(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(-4244); // addi r11,r11,-4244
        s256 = r11; // stw r11,256(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(-4212); // addi r11,r11,-4212
        s228 = r11; // stw r11,228(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(-4192); // addi r11,r11,-4192
        s232 = r11; // stw r11,232(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(-4160); // addi r11,r11,-4160
        s332 = r11; // stw r11,332(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(12256); // addi r11,r11,12256
        s240 = r11; // stw r11,240(r1)
        r11 = r10; // rotlwi r11,r10,0
        r29 = r11 + uint32_t(16); // addi r29,r11,16
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(-4148); // addi r11,r11,-4148
        s116 = r11; // stw r11,116(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(12284); // addi r11,r11,12284
        s364 = r11; // stw r11,364(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(20504); // addi r11,r11,20504
        r10 = r31 + uint32_t(196608); // addis r10,r31,3
        r9 = r31 + uint32_t(196608); // addis r9,r31,3
        r10 = r10 + uint32_t(-8396); // addi r10,r10,-8396
        r8 = r31 + uint32_t(196608); // addis r8,r31,3
        s248 = r11; // stw r11,248(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r28 = r10 + uint32_t(16); // addi r28,r10,16
        r11 = r11 + uint32_t(28720); // addi r11,r11,28720
        s324 = r10; // stw r10,324(r1)
        r25 = r31 + uint32_t(196608); // addis r25,r31,3
        r10 = r31 + uint32_t(262144); // addis r10,r31,4
        r24 = r31 + uint32_t(196608); // addis r24,r31,3
        r9 = r9 + uint32_t(-6324); // addi r9,r9,-6324
        s340 = r11; // stw r11,340(r1)
        r11 = r31 + uint32_t(262144); // addis r11,r31,4
        r7 = r31 + uint32_t(196608); // addis r7,r31,3
        r11 = r11 + uint32_t(-28600); // addi r11,r11,-28600
        r8 = r8 + uint32_t(20488); // addi r8,r8,20488
        r10 = r10 + uint32_t(168); // addi r10,r10,168
        s296 = r9; // stw r9,296(r1)
        r25 = r25 + uint32_t(12272); // addi r25,r25,12272
        r24 = r24 + uint32_t(28708); // addi r24,r24,28708
        s224 = r11; // stw r11,224(r1)
        r11 = r31 + uint32_t(262144); // addis r11,r31,4
        r7 = r7 + uint32_t(-24836); // addi r7,r7,-24836
        s312 = r8; // stw r8,312(r1)
        r11 = r11 + uint32_t(-24484); // addi r11,r11,-24484
        s152 = r10; // stw r10,152(r1)
        r6 = r31 + uint32_t(196608); // addis r6,r31,3
        s336 = r25; // stw r25,336(r1)
        r5 = r31 + uint32_t(196608); // addis r5,r31,3
        s344 = r24; // stw r24,344(r1)
        r22 = r31 + uint32_t(196608); // addis r22,r31,3
        r21 = r31 + uint32_t(196608); // addis r21,r31,3
        s372 = r11; // stw r11,372(r1)
        r11 = r31 + uint32_t(262144); // addis r11,r31,4
        r4 = r31 + uint32_t(196608); // addis r4,r31,3
        r11 = r11 + uint32_t(-8072); // addi r11,r11,-8072
        r3 = r31 + uint32_t(262144); // addis r3,r31,4
        r23 = r31 + uint32_t(262144); // addis r23,r31,4
        r6 = r6 + uint32_t(-24896); // addi r6,r6,-24896
        r5 = r5 + uint32_t(-4228); // addi r5,r5,-4228
        s108 = r11; // stw r11,108(r1)
        r11 = r31 + uint32_t(262144); // addis r11,r31,4
        r22 = r22 + uint32_t(-24864); // addi r22,r22,-24864
        r11 = r11 + uint32_t(-8036); // addi r11,r11,-8036
        r21 = r21 + uint32_t(-4196); // addi r21,r21,-4196
        r4 = r4 + uint32_t(-4168); // addi r4,r4,-4168
        r27 = r9 + uint32_t(16); // addi r27,r9,16
        r3 = r3 + uint32_t(-24492); // addi r3,r3,-24492
        s264 = r11; // stw r11,264(r1)
        r11 = r31 + uint32_t(262144); // addis r11,r31,4
        r23 = r23 + uint32_t(-28612); // addi r23,r23,-28612
        r11 = r11 + uint32_t(184); // addi r11,r11,184
        s348 = r11; // stw r11,348(r1)
        r11 = r31 + uint32_t(262144); // addis r11,r31,4
        r11 = r11 + uint32_t(8400); // addi r11,r11,8400
        s272 = r11; // stw r11,272(r1)
        r11 = r31 + uint32_t(262144); // addis r11,r31,4
        r11 = r11 + uint32_t(16616); // addi r11,r11,16616
        s368 = r11; // stw r11,368(r1)
        r11 = r31 + uint32_t(262144); // addis r11,r31,4
        r11 = r11 + uint32_t(20732); // addi r11,r11,20732
        s280 = r11; // stw r11,280(r1)
        r11 = r31 + uint32_t(327680); // addis r11,r31,5
        r11 = r11 + uint32_t(-28392); // addi r11,r11,-28392
        s120 = r11; // stw r11,120(r1)
        r11 = r31 + uint32_t(262144); // addis r11,r31,4
        r11 = r11 + uint32_t(20744); // addi r11,r11,20744
        s164 = r11; // stw r11,164(r1)
        r11 = r31 + uint32_t(262144); // addis r11,r31,4
        r11 = r11 + uint32_t(-24472); // addi r11,r11,-24472
        s160 = r11; // stw r11,160(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(-8420); // addi r11,r11,-8420
        s288 = r11; // stw r11,288(r1)
        r11 = r31 + uint32_t(196608); // addis r11,r31,3
        r11 = r11 + uint32_t(-4252); // addi r11,r11,-4252
        s328 = r11; // stw r11,328(r1)
        r11 = r7 + uint32_t(24); // addi r11,r7,24
        r26 = r8 + uint32_t(20); // addi r26,r8,20
        r20 = s212; // lwz r20,212(r1)
        r8 = r31 + uint32_t(262144); // addis r8,r31,4
        s320 = r23; // stw r23,320(r1)
        r10 = r31 + uint32_t(262144); // addis r10,r31,4
        r8 = r8 + uint32_t(20724); // addi r8,r8,20724
        r10 = r10 + uint32_t(-8076); // addi r10,r10,-8076
        r9 = r31 + uint32_t(262144); // addis r9,r31,4
        r19 = uint32_t(256); // li r19,256
        r9 = r9 + uint32_t(16604); // addi r9,r9,16604
        s268 = r8; // stw r8,268(r1)
        r8 = r31 + uint32_t(327680); // addis r8,r31,5
        s360 = r10; // stw r10,360(r1)
        r10 = r31 + uint32_t(262144); // addis r10,r31,4
        r8 = r8 + uint32_t(-28396); // addi r8,r8,-28396
        r10 = r10 + uint32_t(-8048); // addi r10,r10,-8048
        s112 = r19; // stw r19,112(r1)
        s84 = r9; // stw r9,84(r1)
        r25 = r25 + uint32_t(16); // addi r25,r25,16
        r9 = r3 + uint32_t(24); // addi r9,r3,24
        r24 = r24 + uint32_t(16); // addi r24,r24,16
        s276 = r8; // stw r8,276(r1)
        r8 = uint32_t(0); // li r8,0
        s136 = r10; // stw r10,136(r1)
        r10 = r31 + uint32_t(262144); // addis r10,r31,4
        r23 = r23 + uint32_t(16); // addi r23,r23,16
        r10 = r10 + uint32_t(8388); // addi r10,r10,8388
        s80 = r8; // stw r8,80(r1)
        r8 = s204; // lwz r8,204(r1)
        r17 = r8 - r20; // subf r17,r20,r8
        s88 = r10; // stw r10,88(r1)
        r10 = r31 + uint32_t(196608); // addis r10,r31,3
        r10 = r10 + uint32_t(12248); // addi r10,r10,12248
        s100 = r17; // stw r17,100(r1)
        r17 = s220; // lwz r17,220(r1)
        r17 = r17 - r20; // subf r17,r20,r17
        s304 = r10; // stw r10,304(r1)
        r10 = r4 + uint32_t(24); // addi r10,r4,24
        s92 = r17; // stw r17,92(r1)
        r17 = s208; // lwz r17,208(r1)
        r17 = r17 - r20; // subf r17,r20,r17
        s96 = r17; // stw r17,96(r1)
        r17 = s216; // lwz r17,216(r1)
        r20 = r17 - r20; // subf r20,r20,r17
        r17 = uint32_t(-2113929216); // lis r17,-32256
        s128 = r20; // stw r20,128(r1)
        r20 = uint32_t(-2112094208); // lis r20,-32228
        f0 = F(PPC_FAST_LOAD_U32(uint32_t(r17 + 2556))); // lfs f0,2556(r17)
        s204 = U(f0); // stfs f0,204(r1)
        f18 = F(PPC_FAST_LOAD_U32(uint32_t(r20 + -21900))); // lfs f18,-21900(r20)
        m0 = PPC_FAST_LOAD_U32(uint32_t(object + 171792));
        m1 = PPC_FAST_LOAD_U32(uint32_t(object + 299288));
        m2 = PPC_FAST_LOAD_U32(uint32_t(object + 192460));
        m3 = PPC_FAST_LOAD_U32(uint32_t(object + 254072));
        m4 = PPC_FAST_LOAD_U32(uint32_t(object + 167560));
        m5 = PPC_FAST_LOAD_U32(uint32_t(object + 237672));
        m6 = PPC_FAST_LOAD_U32(uint32_t(object + 282888));
        m7 = PPC_FAST_LOAD_U32(uint32_t(object + 167556));
        m8 = PPC_FAST_LOAD_U32(uint32_t(object + 169628));
        m9 = PPC_FAST_LOAD_U32(uint32_t(object + 171696));
        m10 = PPC_FAST_LOAD_U32(uint32_t(object + 171728));
        m11 = PPC_FAST_LOAD_U32(uint32_t(object + 171748));
        m12 = PPC_FAST_LOAD_U32(uint32_t(object + 171780));
        m13 = PPC_FAST_LOAD_U32(uint32_t(object + 188196));
        m14 = PPC_FAST_LOAD_U32(uint32_t(object + 188224));
        m15 = PPC_FAST_LOAD_U32(uint32_t(object + 190296));
        m16 = PPC_FAST_LOAD_U32(uint32_t(object + 192364));
        m17 = PPC_FAST_LOAD_U32(uint32_t(object + 192396));
        m18 = PPC_FAST_LOAD_U32(uint32_t(object + 192416));
        m19 = PPC_FAST_LOAD_U32(uint32_t(object + 192448));
        m20 = PPC_FAST_LOAD_U32(uint32_t(object + 208864));
        m21 = PPC_FAST_LOAD_U32(uint32_t(object + 208892));
        m22 = PPC_FAST_LOAD_U32(uint32_t(object + 217112));
        m23 = PPC_FAST_LOAD_U32(uint32_t(object + 225328));
        m24 = PPC_FAST_LOAD_U32(uint32_t(object + 233544));
        m25 = PPC_FAST_LOAD_U32(uint32_t(object + 237660));
        m26 = PPC_FAST_LOAD_U32(uint32_t(object + 254108));
        m27 = PPC_FAST_LOAD_U32(uint32_t(object + 262328));
        m28 = PPC_FAST_LOAD_U32(uint32_t(object + 270544));
        m29 = PPC_FAST_LOAD_U32(uint32_t(object + 278760));
        m30 = PPC_FAST_LOAD_U32(uint32_t(object + 282876));
        m31 = PPC_FAST_LOAD_U32(uint32_t(object + 169632));
        m32 = PPC_FAST_LOAD_U32(uint32_t(object + 169616));
        m33 = PPC_FAST_LOAD_U32(uint32_t(object + 169624));
        m34 = PPC_FAST_LOAD_U32(uint32_t(object + 169620));
        m35 = PPC_FAST_LOAD_U32(uint32_t(object + 171688));
        m36 = PPC_FAST_LOAD_U32(uint32_t(object + 171692));
        m37 = PPC_FAST_LOAD_U32(uint32_t(object + 171704));
        m38 = PPC_FAST_LOAD_U32(uint32_t(object + 171708));
        m39 = PPC_FAST_LOAD_U32(uint32_t(object + 171712));
        m40 = PPC_FAST_LOAD_U32(uint32_t(object + 171724));
        m41 = PPC_FAST_LOAD_U32(uint32_t(object + 171720));
        m42 = PPC_FAST_LOAD_U32(uint32_t(object + 171716));
        m43 = PPC_FAST_LOAD_U32(uint32_t(object + 171736));
        m44 = PPC_FAST_LOAD_U32(uint32_t(object + 171740));
        m45 = PPC_FAST_LOAD_U32(uint32_t(object + 171744));
        m46 = PPC_FAST_LOAD_U32(uint32_t(object + 171756));
        m47 = PPC_FAST_LOAD_U32(uint32_t(object + 171764));
        m48 = PPC_FAST_LOAD_U32(uint32_t(object + 171768));
        m49 = PPC_FAST_LOAD_U32(uint32_t(object + 171752));
        m50 = PPC_FAST_LOAD_U32(uint32_t(object + 171796));
        m51 = PPC_FAST_LOAD_U32(uint32_t(object + 171776));
        m52 = PPC_FAST_LOAD_U32(uint32_t(object + 171784));
        m53 = PPC_FAST_LOAD_U32(uint32_t(object + 171788));
        m54 = PPC_FAST_LOAD_U32(uint32_t(object + 171772));
        m55 = PPC_FAST_LOAD_U32(uint32_t(object + 188188));
        m56 = PPC_FAST_LOAD_U32(uint32_t(object + 188192));
        m57 = PPC_FAST_LOAD_U32(uint32_t(object + 188204));
        m58 = PPC_FAST_LOAD_U32(uint32_t(object + 188208));
        m59 = PPC_FAST_LOAD_U32(uint32_t(object + 188228));
        m60 = PPC_FAST_LOAD_U32(uint32_t(object + 188212));
        m61 = PPC_FAST_LOAD_U32(uint32_t(object + 188220));
        m62 = PPC_FAST_LOAD_U32(uint32_t(object + 188216));
        m63 = PPC_FAST_LOAD_U32(uint32_t(object + 190300));
        m64 = PPC_FAST_LOAD_U32(uint32_t(object + 190284));
        m65 = PPC_FAST_LOAD_U32(uint32_t(object + 190292));
        m66 = PPC_FAST_LOAD_U32(uint32_t(object + 190288));
        m67 = PPC_FAST_LOAD_U32(uint32_t(object + 192356));
        m68 = PPC_FAST_LOAD_U32(uint32_t(object + 192360));
        m69 = PPC_FAST_LOAD_U32(uint32_t(object + 192372));
        m70 = PPC_FAST_LOAD_U32(uint32_t(object + 192376));
        m71 = PPC_FAST_LOAD_U32(uint32_t(object + 192380));
        m72 = PPC_FAST_LOAD_U32(uint32_t(object + 192388));
        m73 = PPC_FAST_LOAD_U32(uint32_t(object + 192384));
        m74 = PPC_FAST_LOAD_U32(uint32_t(object + 192392));
        m75 = PPC_FAST_LOAD_U32(uint32_t(object + 192404));
        m76 = PPC_FAST_LOAD_U32(uint32_t(object + 192408));
        m77 = PPC_FAST_LOAD_U32(uint32_t(object + 192412));
        m78 = PPC_FAST_LOAD_U32(uint32_t(object + 192424));
        m79 = PPC_FAST_LOAD_U32(uint32_t(object + 192432));
        m80 = PPC_FAST_LOAD_U32(uint32_t(object + 192436));
        m81 = PPC_FAST_LOAD_U32(uint32_t(object + 192420));
        m82 = PPC_FAST_LOAD_U32(uint32_t(object + 192464));
        m83 = PPC_FAST_LOAD_U32(uint32_t(object + 192444));
        m84 = PPC_FAST_LOAD_U32(uint32_t(object + 192452));
        m85 = PPC_FAST_LOAD_U32(uint32_t(object + 192456));
        m86 = PPC_FAST_LOAD_U32(uint32_t(object + 192440));
        m87 = PPC_FAST_LOAD_U32(uint32_t(object + 208856));
        m88 = PPC_FAST_LOAD_U32(uint32_t(object + 208860));
        m89 = PPC_FAST_LOAD_U32(uint32_t(object + 208872));
        m90 = PPC_FAST_LOAD_U32(uint32_t(object + 208876));
        m91 = PPC_FAST_LOAD_U32(uint32_t(object + 208896));
        m92 = PPC_FAST_LOAD_U32(uint32_t(object + 208880));
        m93 = PPC_FAST_LOAD_U32(uint32_t(object + 208888));
        m94 = PPC_FAST_LOAD_U32(uint32_t(object + 208884));
        m95 = PPC_FAST_LOAD_U32(uint32_t(object + 217116));
        m96 = PPC_FAST_LOAD_U32(uint32_t(object + 217096));
        m97 = PPC_FAST_LOAD_U32(uint32_t(object + 217108));
        m98 = PPC_FAST_LOAD_U32(uint32_t(object + 217100));
        m99 = PPC_FAST_LOAD_U32(uint32_t(object + 217104));
        m100 = PPC_FAST_LOAD_U32(uint32_t(object + 225332));
        m101 = PPC_FAST_LOAD_U32(uint32_t(object + 225324));
        m102 = PPC_FAST_LOAD_U32(uint32_t(object + 225316));
        m103 = PPC_FAST_LOAD_U32(uint32_t(object + 225320));
        m104 = PPC_FAST_LOAD_U32(uint32_t(object + 233548));
        m105 = PPC_FAST_LOAD_U32(uint32_t(object + 233532));
        m106 = PPC_FAST_LOAD_U32(uint32_t(object + 233540));
        m107 = PPC_FAST_LOAD_U32(uint32_t(object + 233536));
        m108 = PPC_FAST_LOAD_U32(uint32_t(object + 237676));
        m109 = PPC_FAST_LOAD_U32(uint32_t(object + 237664));
        m110 = PPC_FAST_LOAD_U32(uint32_t(object + 237668));
        m111 = PPC_FAST_LOAD_U32(uint32_t(object + 237656));
        m112 = PPC_FAST_LOAD_U32(uint32_t(object + 237652));
        m113 = PPC_FAST_LOAD_U32(uint32_t(object + 254068));
        m114 = PPC_FAST_LOAD_U32(uint32_t(object + 254076));
        m115 = PPC_FAST_LOAD_U32(uint32_t(object + 254080));
        m116 = PPC_FAST_LOAD_U32(uint32_t(object + 254088));
        m117 = PPC_FAST_LOAD_U32(uint32_t(object + 254092));
        m118 = PPC_FAST_LOAD_U32(uint32_t(object + 254096));
        m119 = PPC_FAST_LOAD_U32(uint32_t(object + 254104));
        m120 = PPC_FAST_LOAD_U32(uint32_t(object + 254100));
        m121 = PPC_FAST_LOAD_U32(uint32_t(object + 254112));
        m122 = PPC_FAST_LOAD_U32(uint32_t(object + 262312));
        m123 = PPC_FAST_LOAD_U32(uint32_t(object + 262324));
        m124 = PPC_FAST_LOAD_U32(uint32_t(object + 262320));
        m125 = PPC_FAST_LOAD_U32(uint32_t(object + 262316));
        m126 = PPC_FAST_LOAD_U32(uint32_t(object + 262332));
        m127 = PPC_FAST_LOAD_U32(uint32_t(object + 270532));
        m128 = PPC_FAST_LOAD_U32(uint32_t(object + 270540));
        m129 = PPC_FAST_LOAD_U32(uint32_t(object + 270536));
        m130 = PPC_FAST_LOAD_U32(uint32_t(object + 270548));
        m131 = PPC_FAST_LOAD_U32(uint32_t(object + 278748));
        m132 = PPC_FAST_LOAD_U32(uint32_t(object + 278756));
        m133 = PPC_FAST_LOAD_U32(uint32_t(object + 278764));
        m134 = PPC_FAST_LOAD_U32(uint32_t(object + 278752));
        m135 = PPC_FAST_LOAD_U32(uint32_t(object + 282880));
        m136 = PPC_FAST_LOAD_U32(uint32_t(object + 282884));
        m137 = PPC_FAST_LOAD_U32(uint32_t(object + 282872));
        m138 = PPC_FAST_LOAD_U32(uint32_t(object + 282892));
        m139 = PPC_FAST_LOAD_U32(uint32_t(object + 282868));
        m140 = PPC_FAST_LOAD_U32(uint32_t(object + 299284));
        m141 = PPC_FAST_LOAD_U32(uint32_t(object + 299292));
        m142 = PPC_FAST_LOAD_U32(uint32_t(object + 299296));
        m143 = PPC_FAST_LOAD_U32(uint32_t(object + 299304));
        m144 = PPC_FAST_LOAD_U32(uint32_t(object + 299308));
        {
            uint32_t fixed[145] = {uint32_t(object + 171792), uint32_t(object + 299288), uint32_t(object + 192460), uint32_t(object + 254072), uint32_t(object + 167560), uint32_t(object + 237672), uint32_t(object + 282888), uint32_t(object + 167556), uint32_t(object + 169628), uint32_t(object + 171696), uint32_t(object + 171728), uint32_t(object + 171748), uint32_t(object + 171780), uint32_t(object + 188196), uint32_t(object + 188224), uint32_t(object + 190296), uint32_t(object + 192364), uint32_t(object + 192396), uint32_t(object + 192416), uint32_t(object + 192448), uint32_t(object + 208864), uint32_t(object + 208892), uint32_t(object + 217112), uint32_t(object + 225328), uint32_t(object + 233544), uint32_t(object + 237660), uint32_t(object + 254108), uint32_t(object + 262328), uint32_t(object + 270544), uint32_t(object + 278760), uint32_t(object + 282876), uint32_t(object + 169632), uint32_t(object + 169616), uint32_t(object + 169624), uint32_t(object + 169620), uint32_t(object + 171688), uint32_t(object + 171692), uint32_t(object + 171704), uint32_t(object + 171708), uint32_t(object + 171712), uint32_t(object + 171724), uint32_t(object + 171720), uint32_t(object + 171716), uint32_t(object + 171736), uint32_t(object + 171740), uint32_t(object + 171744), uint32_t(object + 171756), uint32_t(object + 171764), uint32_t(object + 171768), uint32_t(object + 171752), uint32_t(object + 171796), uint32_t(object + 171776), uint32_t(object + 171784), uint32_t(object + 171788), uint32_t(object + 171772), uint32_t(object + 188188), uint32_t(object + 188192), uint32_t(object + 188204), uint32_t(object + 188208), uint32_t(object + 188228), uint32_t(object + 188212), uint32_t(object + 188220), uint32_t(object + 188216), uint32_t(object + 190300), uint32_t(object + 190284), uint32_t(object + 190292), uint32_t(object + 190288), uint32_t(object + 192356), uint32_t(object + 192360), uint32_t(object + 192372), uint32_t(object + 192376), uint32_t(object + 192380), uint32_t(object + 192388), uint32_t(object + 192384), uint32_t(object + 192392), uint32_t(object + 192404), uint32_t(object + 192408), uint32_t(object + 192412), uint32_t(object + 192424), uint32_t(object + 192432), uint32_t(object + 192436), uint32_t(object + 192420), uint32_t(object + 192464), uint32_t(object + 192444), uint32_t(object + 192452), uint32_t(object + 192456), uint32_t(object + 192440), uint32_t(object + 208856), uint32_t(object + 208860), uint32_t(object + 208872), uint32_t(object + 208876), uint32_t(object + 208896), uint32_t(object + 208880), uint32_t(object + 208888), uint32_t(object + 208884), uint32_t(object + 217116), uint32_t(object + 217096), uint32_t(object + 217108), uint32_t(object + 217100), uint32_t(object + 217104), uint32_t(object + 225332), uint32_t(object + 225324), uint32_t(object + 225316), uint32_t(object + 225320), uint32_t(object + 233548), uint32_t(object + 233532), uint32_t(object + 233540), uint32_t(object + 233536), uint32_t(object + 237676), uint32_t(object + 237664), uint32_t(object + 237668), uint32_t(object + 237656), uint32_t(object + 237652), uint32_t(object + 254068), uint32_t(object + 254076), uint32_t(object + 254080), uint32_t(object + 254088), uint32_t(object + 254092), uint32_t(object + 254096), uint32_t(object + 254104), uint32_t(object + 254100), uint32_t(object + 254112), uint32_t(object + 262312), uint32_t(object + 262324), uint32_t(object + 262320), uint32_t(object + 262316), uint32_t(object + 262332), uint32_t(object + 270532), uint32_t(object + 270540), uint32_t(object + 270536), uint32_t(object + 270548), uint32_t(object + 278748), uint32_t(object + 278756), uint32_t(object + 278764), uint32_t(object + 278752), uint32_t(object + 282880), uint32_t(object + 282884), uint32_t(object + 282872), uint32_t(object + 282892), uint32_t(object + 282868), uint32_t(object + 299284), uint32_t(object + 299292), uint32_t(object + 299296), uint32_t(object + 299304), uint32_t(object + 299308)};
            std::sort(fixed, fixed + 145);
            for (uint32_t i = 1; i < 145; ++i) if (fixed[i] - fixed[i - 1] < 4) { LoopAliased(fixed[i]); break; }
        }
        goto loc_82CCAB3C; // b 0x82ccab3c
        loc_82CCAB38:;
        r19 = s112; // lwz r19,112(r1)
        loc_82CCAB3C:;
        r20 = s104; // lwz r20,104(r1)
        r20 = r20 & 0x0000001fu; // clrlwi r20,r20,27
        cmpA = int32_t(r20); cmpB = 0; cmpSigned = true; // cmpwi cr6,r20,0
        if (cmpSigned ? (cmpA != cmpB) : (cmpUA != cmpUB)) goto loc_82CCAB5C; // bne cr6,0x82ccab5c
        r20 = s148; // lwz r20,148(r1)
        // dcbt r19,r20 (no effect here)
        r20 = s140; // lwz r20,140(r1)
        // dcbt r19,r20 (no effect here)
        loc_82CCAB5C:;
        r15 = s124; // lwz r15,124(r1)
        r17 = sp + uint32_t(5584); // addi r17,r1,5584
        r20 = s120; // lwz r20,120(r1)
        f8 = F(PPC_FAST_LOAD_U32(uint32_t(r18 + 8))); // lfs f8,8(r18)
        r19 = PPC_FAST_LOAD_U32(r18); // lwz r19,0(r18)
        f14 = F(PPC_FAST_LOAD_U32(uint32_t(r18 + 4))); // lfs f14,4(r18)
        r16 = s92; // lwz r16,92(r1)
        r14 = sp + uint32_t(13040); // addi r14,r1,13040
        f5 = F(m0); // lfs f5,0(r15) [fixed]
        r15 = s116; // lwz r15,116(r1)
        f13 = F(m1); // lfs f13,0(r20) [fixed]
        r20 = s108; // lwz r20,108(r1)
        f15 = F(PPC_FAST_LOAD_U32(uint32_t(r16 + r8))); // lfsx f15,r16,r8
        r16 = sp + uint32_t(384); // addi r16,r1,384
        f4 = F(m2); // lfs f4,0(r15) [fixed]
        r15 = s160; // lwz r15,160(r1)
        f12 = F(m3); // lfs f12,0(r20) [fixed]
        r20 = m4; // lwz r20,0(r30) [fixed]
        r20 = r20 + uint32_t(2); // addi r20,r20,2
        f11 = F(m5); // lfs f11,0(r15) [fixed]
        r15 = s80; // lwz r15,80(r1)
        r18 = std::rotl(r20, 2) & 0xfffffffcu; // rlwinm r18,r20,2,0,29
        r20 = s164; // lwz r20,164(r1)
        f10 = F(PPC_FAST_LOAD_U32(uint32_t(r17 + r15))); // lfsx f10,r17,r15
        r17 = s128; // lwz r17,128(r1)
        f0 = F(m6); // lfs f0,0(r20) [fixed]
        r20 = sp + uint32_t(12000); // addi r20,r1,12000
        r17 = r17 + r8; // add r17,r17,r8
        f17 = f10 + f0; // fadds f17,f10,f0
        f9 = F(PPC_FAST_LOAD_U32(uint32_t(r16 + r15))); // lfsx f9,r16,r15
        f9 = f9 + f11; // fadds f9,f9,f11
        s252 = r17; // stw r17,252(r1)
        r17 = s192; // lwz r17,192(r1)
        f7 = F(m7); // lfs f7,0(r17) [fixed]
        r17 = s176; // lwz r17,176(r1)
        f31 = F(m8); // lfs f31,0(r17) [fixed]
        r17 = s200; // lwz r17,200(r1)
        f30 = F(m9); // lfs f30,0(r17) [fixed]
        r17 = s168; // lwz r17,168(r1)
        f29 = F(m10); // lfs f29,0(r17) [fixed]
        r17 = s184; // lwz r17,184(r1)
        f28 = F(m11); // lfs f28,0(r17) [fixed]
        r17 = s196; // lwz r17,196(r1)
        f27 = F(m12); // lfs f27,0(r17) [fixed]
        r17 = s188; // lwz r17,188(r1)
        f3 = F(m13); // lfs f3,0(r17) [fixed]
        r17 = s180; // lwz r17,180(r1)
        f26 = F(m14); // lfs f26,0(r17) [fixed]
        r17 = s172; // lwz r17,172(r1)
        f25 = F(m15); // lfs f25,0(r17) [fixed]
        r17 = s256; // lwz r17,256(r1)
        f24 = F(m16); // lfs f24,0(r17) [fixed]
        r17 = s228; // lwz r17,228(r1)
        f23 = F(m17); // lfs f23,0(r17) [fixed]
        r17 = s232; // lwz r17,232(r1)
        f22 = F(m18); // lfs f22,0(r17) [fixed]
        r17 = s332; // lwz r17,332(r1)
        f21 = F(m19); // lfs f21,0(r17) [fixed]
        r17 = s240; // lwz r17,240(r1)
        f2 = F(m20); // lfs f2,0(r17) [fixed]
        r17 = s364; // lwz r17,364(r1)
        f20 = F(m21); // lfs f20,0(r17) [fixed]
        r17 = s248; // lwz r17,248(r1)
        f19 = F(m22); // lfs f19,0(r17) [fixed]
        r17 = s340; // lwz r17,340(r1)
        f0 = F(m23); // lfs f0,0(r17) [fixed]
        r17 = s224; // lwz r17,224(r1)
        s352 = U(f0); // stfs f0,352(r1)
        f0 = F(m24); // lfs f0,0(r17) [fixed]
        r17 = s372; // lwz r17,372(r1)
        s80 = U(f0); // stfs f0,80(r1)
        f11 = F(m25); // lfs f11,0(r17) [fixed]
        r17 = s264; // lwz r17,264(r1)
        f0 = F(m26); // lfs f0,0(r17) [fixed]
        r17 = s348; // lwz r17,348(r1)
        s156 = U(f0); // stfs f0,156(r1)
        f0 = F(m27); // lfs f0,0(r17) [fixed]
        r17 = s272; // lwz r17,272(r1)
        s236 = U(f0); // stfs f0,236(r1)
        f0 = F(m28); // lfs f0,0(r17) [fixed]
        s260 = U(f0); // stfs f0,260(r1)
        r17 = s368; // lwz r17,368(r1)
        f16 = f9 + f4; // fadds f16,f9,f4
        r16 = s100; // lwz r16,100(r1)
        f17 = f17 + f5; // fadds f17,f17,f5
        f0 = F(m29); // lfs f0,0(r17) [fixed]
        r17 = s280; // lwz r17,280(r1)
        f1 = F(PPC_FAST_LOAD_U32(uint32_t(r16 + r20))); // lfsx f1,r16,r20
        r20 = s96; // lwz r20,96(r1)
        s244 = U(f0); // stfs f0,244(r1)
        f10 = F(PPC_FAST_LOAD_U32(uint32_t(r16 + r14))); // lfsx f10,r16,r14
        f0 = F(m30); // lfs f0,0(r17) [fixed]
        f16 = f16 * f18; // fmuls f16,f16,f18
        s284 = U(f0); // stfs f0,284(r1)
        f17 = f17 * f18; // fmuls f17,f17,f18
        f0 = F(PPC_FAST_LOAD_U32(uint32_t(r20 + r8))); // lfsx f0,r20,r8
        r20 = s128; // lwz r20,128(r1)
        f6 = F(PPC_FAST_LOAD_U32(uint32_t(r20 + r8))); // lfsx f6,r20,r8
        r20 = m4; // lwz r20,0(r30) [fixed]
        r20 = r20 - r19; // subf r20,r19,r20
        r20 = r20 & 0x000001ffu; // clrlwi r20,r20,23
        r20 = r20 + uint32_t(2); // addi r20,r20,2
        r20 = std::rotl(r20, 2) & 0xfffffffcu; // rlwinm r20,r20,2,0,29
        f9 = F(PPC_FAST_LOAD_U32(uint32_t(r20 + r30))); // lfsx f9,r20,r30
        f8 = float(std::fma(double(f8), double(f9), double(f15))); // fmadds f8,f8,f9,f15
        f8 = f8 + f13; // fadds f8,f8,f13
        f8 = f8 + f12; // fadds f8,f8,f12
        PPC_FAST_STORE_U32(uint32_t(r18 + r30), U(f8)); // stfsx f8,r18,r30
        r20 = m4; // lwz r20,0(r30) [fixed]
        r18 = s144; // lwz r18,144(r1)
        r20 = r20 + uint32_t(1); // addi r20,r20,1
        r20 = r20 & 0x000001ffu; // clrlwi r20,r20,23
        m4 = r20; PPC_FAST_STORE_U32(r30, m4); // stw r20,0(r30) [fixed]
        f9 = float(std::fma(double(f14), double(f8), double(f9))); // fmadds f9,f14,f8,f9
        r20 = s132; // lwz r20,132(r1)
        m7 = U(f9); PPC_FAST_STORE_U32(uint32_t(r18 + 12), m7); // stfs f9,12(r18) [fixed]
        r19 = m31; // lwz r19,0(r29) [fixed]
        f9 = f10 + f31; // fadds f9,f10,f31
        r17 = m32; // lwz r17,0(r20) [fixed]
        f15 = F(m33); // lfs f15,8(r20) [fixed]
        f14 = F(m34); // lfs f14,4(r20) [fixed]
        r19 = r19 - r17; // subf r19,r17,r19
        r19 = r19 & 0x000001ffu; // clrlwi r19,r19,23
        r17 = r19 + uint32_t(2); // addi r17,r19,2
        r19 = m31; // lwz r19,0(r29) [fixed]
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r17 = std::rotl(r17, 2) & 0xfffffffcu; // rlwinm r17,r17,2,0,29
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f8 = F(PPC_FAST_LOAD_U32(uint32_t(r17 + r29))); // lfsx f8,r17,r29
        f7 = float(std::fma(double(f15), double(f8), double(f7))); // fmadds f7,f15,f8,f7
        PPC_FAST_STORE_U32(uint32_t(r19 + r29), U(f7)); // stfsx f7,r19,r29
        r19 = m31; // lwz r19,0(r29) [fixed]
        r19 = r19 + uint32_t(1); // addi r19,r19,1
        r19 = r19 & 0x000001ffu; // clrlwi r19,r19,23
        m31 = r19; PPC_FAST_STORE_U32(r29, m31); // stw r19,0(r29) [fixed]
        f8 = float(std::fma(double(f7), double(f14), double(f8))); // fmadds f8,f7,f14,f8
        m8 = U(f8); PPC_FAST_STORE_U32(uint32_t(r20 + 12), m8); // stfs f8,12(r20) [fixed]
        r20 = s356; // lwz r20,356(r1)
        r19 = r20 + uint32_t(12); // addi r19,r20,12
        r17 = m35; // lwz r17,0(r20) [fixed]
        f8 = F(m36); // lfs f8,4(r20) [fixed]
        r17 = r17 & 0x00000001u; // clrlwi r17,r17,31
        f7 = F(m37); // lfs f7,4(r19) [fixed]
        r17 = r17 + uint32_t(4); // addi r17,r17,4
        r17 = std::rotl(r17, 2) & 0xfffffffcu; // rlwinm r17,r17,2,0,29
        f31 = F(PPC_FAST_LOAD_U32(uint32_t(r17 + r20))); // lfsx f31,r17,r20
        m38 = U(f7); PPC_FAST_STORE_U32(uint32_t(r19 + 8), m38); // stfs f7,8(r19) [fixed]
        f8 = float(std::fma(double(f31), double(f8), double(f9))); // fmadds f8,f31,f8,f9
        m37 = U(f9); PPC_FAST_STORE_U32(uint32_t(r19 + 4), m37); // stfs f9,4(r19) [fixed]
        m9 = U(f8); PPC_FAST_STORE_U32(uint32_t(r20 + 8), m9); // stfs f8,8(r20) [fixed]
        r20 = m39; // lwz r20,0(r6) [fixed]
        f7 = F(m40); // lfs f7,12(r6) [fixed]
        r19 = r20 & 0x00000001u; // clrlwi r19,r20,31
        f8 = F(m41); // lfs f8,8(r6) [fixed]
        r20 = r6 + uint32_t(20); // addi r20,r6,20
        f31 = F(m42); // lfs f31,4(r6) [fixed]
        r19 = r19 + uint32_t(6); // addi r19,r19,6
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f15 = F(m43); // lfs f15,4(r20) [fixed]
        f9 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r6))); // lfsx f9,r19,r6
        m44 = U(f15); PPC_FAST_STORE_U32(uint32_t(r20 + 8), m44); // stfs f15,8(r20) [fixed]
        f7 = f7 * f9; // fmuls f7,f7,f9
        f9 = float(std::fma(double(f8), double(f9), double(f30))); // fmadds f9,f8,f9,f30
        m43 = U(f9); PPC_FAST_STORE_U32(uint32_t(r20 + 4), m43); // stfs f9,4(r20) [fixed]
        r20 = r22 + uint32_t(16); // addi r20,r22,16
        f12 = f13 - f12; // fsubs f12,f13,f12
        f30 = F(PPC_FAST_LOAD_U32(r8)); // lfs f30,0(r8)
        f9 = float(std::fma(double(f9), double(f31), double(f7))); // fmadds f9,f9,f31,f7
        m10 = U(f9); PPC_FAST_STORE_U32(uint32_t(r6 + 16), m10); // stfs f9,16(r6) [fixed]
        r19 = m45; // lwz r19,0(r22) [fixed]
        f9 = F(m46); // lfs f9,12(r22) [fixed]
        f8 = F(m47); // lfs f8,4(r20) [fixed]
        r19 = r19 & 0x00000001u; // clrlwi r19,r19,31
        r19 = r19 + uint32_t(5); // addi r19,r19,5
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f7 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r22))); // lfsx f7,r19,r22
        f9 = float(std::fma(double(f7), double(f9), double(f29))); // fmadds f9,f7,f9,f29
        m47 = U(f9); PPC_FAST_STORE_U32(uint32_t(r20 + 4), m47); // stfs f9,4(r20) [fixed]
        m48 = U(f8); PPC_FAST_STORE_U32(uint32_t(r20 + 8), m48); // stfs f8,8(r20) [fixed]
        f8 = F(m49); // lfs f8,8(r22) [fixed]
        f9 = f8 * f9; // fmuls f9,f8,f9
        m11 = U(f9); PPC_FAST_STORE_U32(uint32_t(r22 + 4), m11); // stfs f9,4(r22) [fixed]
        r20 = m50; // lwz r20,0(r11) [fixed]
        f8 = F(m51); // lfs f8,4(r7) [fixed]
        r19 = m52; // lwz r19,12(r7) [fixed]
        f7 = F(m53); // lfs f7,16(r7) [fixed]
        r17 = r20; // mr r17,r20
        r20 = r20 - r19; // subf r20,r19,r20
        r19 = m54; // lwz r19,0(r7) [fixed]
        r20 = r20 & 0x00000fffu; // clrlwi r20,r20,20
        r19 = r17 - r19; // subf r19,r19,r17
        r17 = r20 + uint32_t(2); // addi r17,r20,2
        r20 = r19 & 0x00000fffu; // clrlwi r20,r19,20
        r19 = std::rotl(r17, 2) & 0xfffffffcu; // rlwinm r19,r17,2,0,29
        r20 = r20 + uint32_t(2); // addi r20,r20,2
        r20 = std::rotl(r20, 2) & 0xfffffffcu; // rlwinm r20,r20,2,0,29
        f9 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r11))); // lfsx f9,r19,r11
        f7 = f9 * f7; // fmuls f7,f9,f7
        f9 = F(s204); // lfs f9,204(r1)
        f31 = f28 * f9; // fmuls f31,f28,f9
        f13 = F(PPC_FAST_LOAD_U32(uint32_t(r20 + r11))); // lfsx f13,r20,r11
        f13 = f13 * f8; // fmuls f13,f13,f8
        m12 = U(f13); PPC_FAST_STORE_U32(uint32_t(r7 + 8), m12); // stfs f13,8(r7) [fixed]
        m0 = U(f7); PPC_FAST_STORE_U32(uint32_t(r7 + 20), m0); // stfs f7,20(r7) [fixed]
        r20 = m50; // lwz r20,0(r11) [fixed]
        r20 = r20 + uint32_t(2); // addi r20,r20,2
        r20 = std::rotl(r20, 2) & 0xfffffffcu; // rlwinm r20,r20,2,0,29
        PPC_FAST_STORE_U32(uint32_t(r20 + r11), U(f31)); // stfsx f31,r20,r11
        r20 = m50; // lwz r20,0(r11) [fixed]
        r20 = r20 + uint32_t(1); // addi r20,r20,1
        r20 = r20 & 0x00000fffu; // clrlwi r20,r20,20
        m50 = r20; PPC_FAST_STORE_U32(r11, m50); // stw r20,0(r11) [fixed]
        r20 = s288; // lwz r20,288(r1)
        r19 = m55; // lwz r19,0(r20) [fixed]
        f13 = F(m56); // lfs f13,4(r20) [fixed]
        r19 = r19 & 0x00000001u; // clrlwi r19,r19,31
        r19 = r19 + uint32_t(4); // addi r19,r19,4
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f8 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r20))); // lfsx f8,r19,r20
        f13 = f8 * f13; // fmuls f13,f8,f13
        m13 = U(f13); PPC_FAST_STORE_U32(uint32_t(r20 + 8), m13); // stfs f13,8(r20) [fixed]
        r20 = r20 + uint32_t(12); // addi r20,r20,12
        f13 = F(m57); // lfs f13,4(r20) [fixed]
        m58 = U(f13); PPC_FAST_STORE_U32(uint32_t(r20 + 8), m58); // stfs f13,8(r20) [fixed]
        m57 = U(f27); PPC_FAST_STORE_U32(uint32_t(r20 + 4), m57); // stfs f27,4(r20) [fixed]
        r19 = m59; // lwz r19,0(r28) [fixed]
        r20 = s324; // lwz r20,324(r1)
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r14 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r14,r19,2,0,29
        r19 = m59; // lwz r19,0(r28) [fixed]
        r17 = m60; // lwz r17,0(r20) [fixed]
        f8 = F(m61); // lfs f8,8(r20) [fixed]
        f7 = F(m62); // lfs f7,4(r20) [fixed]
        r19 = r19 - r17; // subf r19,r17,r19
        r19 = r19 & 0x000001ffu; // clrlwi r19,r19,23
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f13 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r28))); // lfsx f13,r19,r28
        f12 = float(std::fma(double(f8), double(f13), double(f12))); // fmadds f12,f8,f13,f12
        f12 = f12 + f30; // fadds f12,f12,f30
        PPC_FAST_STORE_U32(uint32_t(r14 + r28), U(f12)); // stfsx f12,r14,r28
        r19 = m59; // lwz r19,0(r28) [fixed]
        r19 = r19 + uint32_t(1); // addi r19,r19,1
        r19 = r19 & 0x000001ffu; // clrlwi r19,r19,23
        f13 = float(std::fma(double(f12), double(f7), double(f13))); // fmadds f13,f12,f7,f13
        f9 = f22 * f9; // fmuls f9,f22,f9
        m59 = r19; PPC_FAST_STORE_U32(r28, m59); // stw r19,0(r28) [fixed]
        m14 = U(f13); PPC_FAST_STORE_U32(uint32_t(r20 + 12), m14); // stfs f13,12(r20) [fixed]
        r20 = s296; // lwz r20,296(r1)
        r17 = m63; // lwz r17,0(r27) [fixed]
        r19 = r17; // mr r19,r17
        r14 = m64; // lwz r14,0(r20) [fixed]
        f12 = F(m65); // lfs f12,8(r20) [fixed]
        f8 = F(m66); // lfs f8,4(r20) [fixed]
        r17 = r17 - r14; // subf r17,r14,r17
        r14 = r19 + uint32_t(2); // addi r14,r19,2
        r19 = r17 & 0x000001ffu; // clrlwi r19,r17,23
        r17 = std::rotl(r14, 2) & 0xfffffffcu; // rlwinm r17,r14,2,0,29
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f13 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r27))); // lfsx f13,r19,r27
        f12 = float(std::fma(double(f12), double(f13), double(f26))); // fmadds f12,f12,f13,f26
        PPC_FAST_STORE_U32(uint32_t(r17 + r27), U(f12)); // stfsx f12,r17,r27
        r19 = m63; // lwz r19,0(r27) [fixed]
        r19 = r19 + uint32_t(1); // addi r19,r19,1
        r19 = r19 & 0x000001ffu; // clrlwi r19,r19,23
        m63 = r19; PPC_FAST_STORE_U32(r27, m63); // stw r19,0(r27) [fixed]
        f12 = float(std::fma(double(f12), double(f8), double(f13))); // fmadds f12,f12,f8,f13
        m15 = U(f12); PPC_FAST_STORE_U32(uint32_t(r20 + 12), m15); // stfs f12,12(r20) [fixed]
        r20 = s328; // lwz r20,328(r1)
        f13 = f1 + f25; // fadds f13,f1,f25
        r19 = r20 + uint32_t(12); // addi r19,r20,12
        r17 = m67; // lwz r17,0(r20) [fixed]
        f12 = F(m68); // lfs f12,4(r20) [fixed]
        r17 = r17 & 0x00000001u; // clrlwi r17,r17,31
        f8 = F(m69); // lfs f8,4(r19) [fixed]
        r17 = r17 + uint32_t(4); // addi r17,r17,4
        r17 = std::rotl(r17, 2) & 0xfffffffcu; // rlwinm r17,r17,2,0,29
        f7 = F(PPC_FAST_LOAD_U32(uint32_t(r17 + r20))); // lfsx f7,r17,r20
        m69 = U(f13); PPC_FAST_STORE_U32(uint32_t(r19 + 4), m69); // stfs f13,4(r19) [fixed]
        f12 = float(std::fma(double(f7), double(f12), double(f13))); // fmadds f12,f7,f12,f13
        m70 = U(f8); PPC_FAST_STORE_U32(uint32_t(r19 + 8), m70); // stfs f8,8(r19) [fixed]
        m16 = U(f12); PPC_FAST_STORE_U32(uint32_t(r20 + 8), m16); // stfs f12,8(r20) [fixed]
        r20 = m71; // lwz r20,0(r5) [fixed]
        f12 = F(m72); // lfs f12,8(r5) [fixed]
        r19 = r20 & 0x00000001u; // clrlwi r19,r20,31
        f8 = F(m73); // lfs f8,4(r5) [fixed]
        r20 = r5 + uint32_t(20); // addi r20,r5,20
        f7 = F(m74); // lfs f7,12(r5) [fixed]
        r19 = r19 + uint32_t(6); // addi r19,r19,6
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f31 = F(m75); // lfs f31,4(r20) [fixed]
        f13 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r5))); // lfsx f13,r19,r5
        f12 = float(std::fma(double(f12), double(f13), double(f24))); // fmadds f12,f12,f13,f24
        m76 = U(f31); PPC_FAST_STORE_U32(uint32_t(r20 + 8), m76); // stfs f31,8(r20) [fixed]
        m75 = U(f12); PPC_FAST_STORE_U32(uint32_t(r20 + 4), m75); // stfs f12,4(r20) [fixed]
        r20 = r21 + uint32_t(16); // addi r20,r21,16
        f8 = f12 * f8; // fmuls f8,f12,f8
        f13 = float(std::fma(double(f13), double(f7), double(f8))); // fmadds f13,f13,f7,f8
        m17 = U(f13); PPC_FAST_STORE_U32(uint32_t(r5 + 16), m17); // stfs f13,16(r5) [fixed]
        r19 = m77; // lwz r19,0(r21) [fixed]
        f13 = F(m78); // lfs f13,12(r21) [fixed]
        f12 = F(m79); // lfs f12,4(r20) [fixed]
        r19 = r19 & 0x00000001u; // clrlwi r19,r19,31
        r19 = r19 + uint32_t(5); // addi r19,r19,5
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f8 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r21))); // lfsx f8,r19,r21
        f13 = float(std::fma(double(f8), double(f13), double(f23))); // fmadds f13,f8,f13,f23
        m79 = U(f13); PPC_FAST_STORE_U32(uint32_t(r20 + 4), m79); // stfs f13,4(r20) [fixed]
        m80 = U(f12); PPC_FAST_STORE_U32(uint32_t(r20 + 8), m80); // stfs f12,8(r20) [fixed]
        f12 = F(m81); // lfs f12,8(r21) [fixed]
        f13 = f13 * f12; // fmuls f13,f13,f12
        m18 = U(f13); PPC_FAST_STORE_U32(uint32_t(r21 + 4), m18); // stfs f13,4(r21) [fixed]
        r20 = m82; // lwz r20,0(r10) [fixed]
        f12 = F(m83); // lfs f12,4(r4) [fixed]
        r19 = m84; // lwz r19,12(r4) [fixed]
        f8 = F(m85); // lfs f8,16(r4) [fixed]
        r17 = r20; // mr r17,r20
        r20 = r20 - r19; // subf r20,r19,r20
        r19 = m86; // lwz r19,0(r4) [fixed]
        r20 = r20 & 0x00000fffu; // clrlwi r20,r20,20
        r19 = r17 - r19; // subf r19,r19,r17
        r17 = r20 + uint32_t(2); // addi r17,r20,2
        r20 = r19 & 0x00000fffu; // clrlwi r20,r19,20
        r20 = r20 + uint32_t(2); // addi r20,r20,2
        r19 = std::rotl(r17, 2) & 0xfffffffcu; // rlwinm r19,r17,2,0,29
        r20 = std::rotl(r20, 2) & 0xfffffffcu; // rlwinm r20,r20,2,0,29
        f13 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r10))); // lfsx f13,r19,r10
        f7 = F(PPC_FAST_LOAD_U32(uint32_t(r20 + r10))); // lfsx f7,r20,r10
        f13 = f8 * f13; // fmuls f13,f8,f13
        f12 = f7 * f12; // fmuls f12,f7,f12
        m2 = U(f13); PPC_FAST_STORE_U32(uint32_t(r4 + 20), m2); // stfs f13,20(r4) [fixed]
        m19 = U(f12); PPC_FAST_STORE_U32(uint32_t(r4 + 8), m19); // stfs f12,8(r4) [fixed]
        r20 = m82; // lwz r20,0(r10) [fixed]
        f13 = f0 + f20; // fadds f13,f0,f20
        r20 = r20 + uint32_t(2); // addi r20,r20,2
        r20 = std::rotl(r20, 2) & 0xfffffffcu; // rlwinm r20,r20,2,0,29
        PPC_FAST_STORE_U32(uint32_t(r20 + r10), U(f9)); // stfsx f9,r20,r10
        r20 = m82; // lwz r20,0(r10) [fixed]
        r20 = r20 + uint32_t(1); // addi r20,r20,1
        r20 = r20 & 0x00000fffu; // clrlwi r20,r20,20
        m82 = r20; PPC_FAST_STORE_U32(r10, m82); // stw r20,0(r10) [fixed]
        r20 = s304; // lwz r20,304(r1)
        r19 = m87; // lwz r19,0(r20) [fixed]
        f12 = F(m88); // lfs f12,4(r20) [fixed]
        r19 = r19 & 0x00000001u; // clrlwi r19,r19,31
        r19 = r19 + uint32_t(4); // addi r19,r19,4
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f9 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r20))); // lfsx f9,r19,r20
        f12 = f9 * f12; // fmuls f12,f9,f12
        m20 = U(f12); PPC_FAST_STORE_U32(uint32_t(r20 + 8), m20); // stfs f12,8(r20) [fixed]
        r20 = r20 + uint32_t(12); // addi r20,r20,12
        f12 = F(m89); // lfs f12,4(r20) [fixed]
        m90 = U(f12); PPC_FAST_STORE_U32(uint32_t(r20 + 8), m90); // stfs f12,8(r20) [fixed]
        m89 = U(f21); PPC_FAST_STORE_U32(uint32_t(r20 + 4), m89); // stfs f21,4(r20) [fixed]
        r19 = m91; // lwz r19,0(r25) [fixed]
        r20 = s336; // lwz r20,336(r1)
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r14 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r14,r19,2,0,29
        r19 = m91; // lwz r19,0(r25) [fixed]
        r17 = m92; // lwz r17,0(r20) [fixed]
        f9 = F(m93); // lfs f9,8(r20) [fixed]
        f8 = F(m94); // lfs f8,4(r20) [fixed]
        r19 = r19 - r17; // subf r19,r17,r19
        r19 = r19 & 0x000007ffu; // clrlwi r19,r19,21
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f12 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r25))); // lfsx f12,r19,r25
        f9 = float(std::fma(double(f12), double(f9), double(f6))); // fmadds f9,f12,f9,f6
        f9 = f9 + f2; // fadds f9,f9,f2
        f9 = f9 + f3; // fadds f9,f9,f3
        PPC_FAST_STORE_U32(uint32_t(r14 + r25), U(f9)); // stfsx f9,r14,r25
        r19 = m91; // lwz r19,0(r25) [fixed]
        r19 = r19 + uint32_t(1); // addi r19,r19,1
        r19 = r19 & 0x000007ffu; // clrlwi r19,r19,21
        m91 = r19; PPC_FAST_STORE_U32(r25, m91); // stw r19,0(r25) [fixed]
        f12 = float(std::fma(double(f9), double(f8), double(f12))); // fmadds f12,f9,f8,f12
        m21 = U(f12); PPC_FAST_STORE_U32(uint32_t(r20 + 12), m21); // stfs f12,12(r20) [fixed]
        r20 = s312; // lwz r20,312(r1)
        r17 = m95; // lwz r17,0(r26) [fixed]
        r19 = r17; // mr r19,r17
        r14 = m96; // lwz r14,0(r20) [fixed]
        f9 = F(m97); // lfs f9,12(r20) [fixed]
        f8 = F(m98); // lfs f8,4(r20) [fixed]
        r17 = r17 - r14; // subf r17,r14,r17
        f7 = F(m99); // lfs f7,8(r20) [fixed]
        r14 = r19 + uint32_t(2); // addi r14,r19,2
        r19 = r17 & 0x000007ffu; // clrlwi r19,r17,21
        r17 = std::rotl(r14, 2) & 0xfffffffcu; // rlwinm r17,r14,2,0,29
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f12 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r26))); // lfsx f12,r19,r26
        f9 = f12 * f9; // fmuls f9,f12,f9
        f9 = float(std::fma(double(f8), double(f13), double(f9))); // fmadds f9,f8,f13,f9
        f13 = float(std::fma(double(f12), double(f7), double(f13))); // fmadds f13,f12,f7,f13
        PPC_FAST_STORE_U32(uint32_t(r17 + r26), U(f13)); // stfsx f13,r17,r26
        r19 = m95; // lwz r19,0(r26) [fixed]
        r19 = r19 + uint32_t(1); // addi r19,r19,1
        r19 = r19 & 0x000007ffu; // clrlwi r19,r19,21
        m95 = r19; PPC_FAST_STORE_U32(r26, m95); // stw r19,0(r26) [fixed]
        m22 = U(f9); PPC_FAST_STORE_U32(uint32_t(r20 + 16), m22); // stfs f9,16(r20) [fixed]
        r20 = s344; // lwz r20,344(r1)
        r17 = m100; // lwz r17,0(r24) [fixed]
        f12 = F(m101); // lfs f12,8(r20) [fixed]
        r14 = m102; // lwz r14,0(r20) [fixed]
        r19 = r17; // mr r19,r17
        f9 = F(m103); // lfs f9,4(r20) [fixed]
        r17 = r17 - r14; // subf r17,r14,r17
        f8 = F(s352); // lfs f8,352(r1)
        r14 = r19 + uint32_t(2); // addi r14,r19,2
        r19 = r17 & 0x000007ffu; // clrlwi r19,r17,21
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f13 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r24))); // lfsx f13,r19,r24
        r19 = std::rotl(r14, 2) & 0xfffffffcu; // rlwinm r19,r14,2,0,29
        f12 = float(std::fma(double(f13), double(f12), double(f19))); // fmadds f12,f13,f12,f19
        PPC_FAST_STORE_U32(uint32_t(r19 + r24), U(f12)); // stfsx f12,r19,r24
        r19 = m100; // lwz r19,0(r24) [fixed]
        r19 = r19 + uint32_t(1); // addi r19,r19,1
        r19 = r19 & 0x000007ffu; // clrlwi r19,r19,21
        f13 = float(std::fma(double(f12), double(f9), double(f13))); // fmadds f13,f12,f9,f13
        f9 = F(s80); // lfs f9,80(r1)
        f10 = f10 + f9; // fadds f10,f10,f9
        m100 = r19; PPC_FAST_STORE_U32(r24, m100); // stw r19,0(r24) [fixed]
        m23 = U(f13); PPC_FAST_STORE_U32(uint32_t(r20 + 12), m23); // stfs f13,12(r20) [fixed]
        r19 = m104; // lwz r19,0(r23) [fixed]
        r20 = s320; // lwz r20,320(r1)
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r14 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r14,r19,2,0,29
        r19 = m104; // lwz r19,0(r23) [fixed]
        r17 = m105; // lwz r17,0(r20) [fixed]
        f12 = F(m106); // lfs f12,8(r20) [fixed]
        f9 = F(m107); // lfs f9,4(r20) [fixed]
        r19 = r19 - r17; // subf r19,r17,r19
        r19 = r19 & 0x000003ffu; // clrlwi r19,r19,22
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f13 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r23))); // lfsx f13,r19,r23
        f12 = float(std::fma(double(f13), double(f12), double(f0))); // fmadds f12,f13,f12,f0
        f12 = f12 + f8; // fadds f12,f12,f8
        PPC_FAST_STORE_U32(uint32_t(r14 + r23), U(f12)); // stfsx f12,r14,r23
        r19 = m104; // lwz r19,0(r23) [fixed]
        r19 = r19 + uint32_t(1); // addi r19,r19,1
        r19 = r19 & 0x000003ffu; // clrlwi r19,r19,22
        m104 = r19; PPC_FAST_STORE_U32(r23, m104); // stw r19,0(r23) [fixed]
        f13 = float(std::fma(double(f12), double(f9), double(f13))); // fmadds f13,f12,f9,f13
        m24 = U(f13); PPC_FAST_STORE_U32(uint32_t(r20 + 12), m24); // stfs f13,12(r20) [fixed]
        r20 = m108; // lwz r20,0(r9) [fixed]
        r19 = m109; // lwz r19,12(r3) [fixed]
        f9 = F(m110); // lfs f9,16(r3) [fixed]
        r17 = r20; // mr r17,r20
        f12 = F(m111); // lfs f12,4(r3) [fixed]
        r20 = r20 - r19; // subf r20,r19,r20
        r19 = m112; // lwz r19,0(r3) [fixed]
        r20 = r20 & 0x00000fffu; // clrlwi r20,r20,20
        r19 = r17 - r19; // subf r19,r19,r17
        r17 = r20 + uint32_t(2); // addi r17,r20,2
        r20 = r19 & 0x00000fffu; // clrlwi r20,r19,20
        r19 = std::rotl(r17, 2) & 0xfffffffcu; // rlwinm r19,r17,2,0,29
        r20 = r20 + uint32_t(2); // addi r20,r20,2
        r20 = std::rotl(r20, 2) & 0xfffffffcu; // rlwinm r20,r20,2,0,29
        f13 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r9))); // lfsx f13,r19,r9
        f13 = f13 * f9; // fmuls f13,f13,f9
        f8 = F(PPC_FAST_LOAD_U32(uint32_t(r20 + r9))); // lfsx f8,r20,r9
        f12 = f8 * f12; // fmuls f12,f8,f12
        m5 = U(f13); PPC_FAST_STORE_U32(uint32_t(r3 + 20), m5); // stfs f13,20(r3) [fixed]
        m25 = U(f12); PPC_FAST_STORE_U32(uint32_t(r3 + 8), m25); // stfs f12,8(r3) [fixed]
        r20 = m108; // lwz r20,0(r9) [fixed]
        r20 = r20 + uint32_t(2); // addi r20,r20,2
        r20 = std::rotl(r20, 2) & 0xfffffffcu; // rlwinm r20,r20,2,0,29
        PPC_FAST_STORE_U32(uint32_t(r20 + r9), U(f10)); // stfsx f10,r20,r9
        r20 = m108; // lwz r20,0(r9) [fixed]
        r20 = r20 + uint32_t(1); // addi r20,r20,1
        r20 = r20 & 0x00000fffu; // clrlwi r20,r20,20
        m108 = r20; PPC_FAST_STORE_U32(r9, m108); // stw r20,0(r9) [fixed]
        r20 = s360; // lwz r20,360(r1)
        r19 = m113; // lwz r19,0(r20) [fixed]
        f13 = F(m114); // lfs f13,8(r20) [fixed]
        f12 = F(m115); // lfs f12,12(r20) [fixed]
        f13 = f13 * f11; // fmuls f13,f13,f11
        r17 = r19 & 0x00000001u; // clrlwi r17,r19,31
        r19 = r20 + uint32_t(16); // addi r19,r20,16
        r17 = r17 + uint32_t(5); // addi r17,r17,5
        r17 = std::rotl(r17, 2) & 0xfffffffcu; // rlwinm r17,r17,2,0,29
        f10 = F(m116); // lfs f10,4(r19) [fixed]
        f9 = F(PPC_FAST_LOAD_U32(uint32_t(r17 + r20))); // lfsx f9,r17,r20
        m117 = U(f10); PPC_FAST_STORE_U32(uint32_t(r19 + 8), m117); // stfs f10,8(r19) [fixed]
        f13 = float(std::fma(double(f9), double(f12), double(f13))); // fmadds f13,f9,f12,f13
        m116 = U(f11); PPC_FAST_STORE_U32(uint32_t(r19 + 4), m116); // stfs f11,4(r19) [fixed]
        m3 = U(f13); PPC_FAST_STORE_U32(uint32_t(r20 + 4), m3); // stfs f13,4(r20) [fixed]
        r20 = s104; // lwz r20,104(r1)
        r20 = r20 & 0x0000001fu; // clrlwi r20,r20,27
        cmpA = int32_t(r20); cmpB = 0; cmpSigned = true; // cmpwi cr6,r20,0
        if (cmpSigned ? (cmpA != cmpB) : (cmpUA != cmpUB)) goto loc_82CCB320; // bne cr6,0x82ccb320
        r20 = s112; // lwz r20,112(r1)
        r20 = r20 + uint32_t(128); // addi r20,r20,128
        s112 = r20; // stw r20,112(r1)
        loc_82CCB320:;
        r20 = s136; // lwz r20,136(r1)
        f11 = f2 - f3; // fsubs f11,f2,f3
        f13 = F(s156); // lfs f13,156(r1)
        f13 = f6 + f13; // fadds f13,f6,f13
        r17 = m118; // lwz r17,0(r20) [fixed]
        f10 = F(m119); // lfs f10,8(r20) [fixed]
        f9 = F(m120); // lfs f9,4(r20) [fixed]
        r20 = r20 + uint32_t(16); // addi r20,r20,16
        r19 = m121; // lwz r19,0(r20) [fixed]
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r14 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r14,r19,2,0,29
        r19 = m121; // lwz r19,0(r20) [fixed]
        r19 = r19 - r17; // subf r19,r17,r19
        r19 = r19 & 0x000007ffu; // clrlwi r19,r19,21
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f12 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r20))); // lfsx f12,r19,r20
        f11 = float(std::fma(double(f12), double(f10), double(f11))); // fmadds f11,f12,f10,f11
        f0 = f11 + f0; // fadds f0,f11,f0
        PPC_FAST_STORE_U32(uint32_t(r14 + r20), U(f0)); // stfsx f0,r14,r20
        r19 = m121; // lwz r19,0(r20) [fixed]
        r19 = r19 + uint32_t(1); // addi r19,r19,1
        r19 = r19 & 0x000007ffu; // clrlwi r19,r19,21
        m121 = r19; PPC_FAST_STORE_U32(r20, m121); // stw r19,0(r20) [fixed]
        f0 = float(std::fma(double(f0), double(f9), double(f12))); // fmadds f0,f0,f9,f12
        r20 = s136; // lwz r20,136(r1)
        r19 = s152; // lwz r19,152(r1)
        m26 = U(f0); PPC_FAST_STORE_U32(uint32_t(r20 + 12), m26); // stfs f0,12(r20) [fixed]
        r20 = r19 + uint32_t(20); // addi r20,r19,20
        r14 = m122; // lwz r14,0(r19) [fixed]
        f12 = F(m123); // lfs f12,12(r19) [fixed]
        f10 = F(m124); // lfs f10,8(r19) [fixed]
        f11 = F(m125); // lfs f11,4(r19) [fixed]
        r17 = m126; // lwz r17,0(r20) [fixed]
        r14 = r17 - r14; // subf r14,r14,r17
        r17 = r17 + uint32_t(2); // addi r17,r17,2
        s156 = r17; // stw r17,156(r1)
        r17 = r14 & 0x000007ffu; // clrlwi r17,r14,21
        r17 = r17 + uint32_t(2); // addi r17,r17,2
        r17 = std::rotl(r17, 2) & 0xfffffffcu; // rlwinm r17,r17,2,0,29
        f0 = F(PPC_FAST_LOAD_U32(uint32_t(r17 + r20))); // lfsx f0,r17,r20
        f12 = f0 * f12; // fmuls f12,f0,f12
        f0 = float(std::fma(double(f0), double(f10), double(f13))); // fmadds f0,f0,f10,f13
        f12 = float(std::fma(double(f13), double(f11), double(f12))); // fmadds f12,f13,f11,f12
        f11 = F(s236); // lfs f11,236(r1)
        r14 = s156; // lwz r14,156(r1)
        r14 = std::rotl(r14, 2) & 0xfffffffcu; // rlwinm r14,r14,2,0,29
        PPC_FAST_STORE_U32(uint32_t(r14 + r20), U(f0)); // stfsx f0,r14,r20
        r17 = m126; // lwz r17,0(r20) [fixed]
        r17 = r17 + uint32_t(1); // addi r17,r17,1
        r17 = r17 & 0x000007ffu; // clrlwi r17,r17,21
        m126 = r17; PPC_FAST_STORE_U32(r20, m126); // stw r17,0(r20) [fixed]
        m27 = U(f12); PPC_FAST_STORE_U32(uint32_t(r19 + 16), m27); // stfs f12,16(r19) [fixed]
        r19 = s88; // lwz r19,88(r1)
        r20 = r19 + uint32_t(16); // addi r20,r19,16
        r14 = m127; // lwz r14,0(r19) [fixed]
        f13 = F(m128); // lfs f13,8(r19) [fixed]
        f12 = F(m129); // lfs f12,4(r19) [fixed]
        r17 = m130; // lwz r17,0(r20) [fixed]
        r19 = r17; // mr r19,r17
        r17 = r17 - r14; // subf r17,r14,r17
        r14 = r19 + uint32_t(2); // addi r14,r19,2
        r19 = r17 & 0x000007ffu; // clrlwi r19,r17,21
        r17 = std::rotl(r14, 2) & 0xfffffffcu; // rlwinm r17,r14,2,0,29
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        f0 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r20))); // lfsx f0,r19,r20
        f13 = float(std::fma(double(f13), double(f0), double(f11))); // fmadds f13,f13,f0,f11
        PPC_FAST_STORE_U32(uint32_t(r17 + r20), U(f13)); // stfsx f13,r17,r20
        r19 = m130; // lwz r19,0(r20) [fixed]
        r19 = r19 + uint32_t(1); // addi r19,r19,1
        r19 = r19 & 0x000007ffu; // clrlwi r19,r19,21
        m130 = r19; PPC_FAST_STORE_U32(r20, m130); // stw r19,0(r20) [fixed]
        f0 = float(std::fma(double(f13), double(f12), double(f0))); // fmadds f0,f13,f12,f0
        r20 = s88; // lwz r20,88(r1)
        r19 = s84; // lwz r19,84(r1)
        m28 = U(f0); PPC_FAST_STORE_U32(uint32_t(r20 + 12), m28); // stfs f0,12(r20) [fixed]
        r20 = r19 + uint32_t(16); // addi r20,r19,16
        r14 = m131; // lwz r14,0(r19) [fixed]
        f13 = F(m132); // lfs f13,8(r19) [fixed]
        r17 = m133; // lwz r17,0(r20) [fixed]
        f12 = F(m134); // lfs f12,4(r19) [fixed]
        r19 = r17 - r14; // subf r19,r14,r17
        f0 = F(s244); // lfs f0,244(r1)
        r17 = r17 + uint32_t(2); // addi r17,r17,2
        r19 = r19 & 0x000003ffu; // clrlwi r19,r19,22
        f11 = f1 + f0; // fadds f11,f1,f0
        r17 = std::rotl(r17, 2) & 0xfffffffcu; // rlwinm r17,r17,2,0,29
        f10 = F(s260); // lfs f10,260(r1)
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r14 = s92; // lwz r14,92(r1)
        r16 = r16 + uint32_t(4); // addi r16,r16,4
        PPC_FAST_STORE_U32(r8, U(f16)); // stfs f16,0(r8)
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        PPC_FAST_STORE_U32(uint32_t(r14 + r8), U(f17)); // stfsx f17,r14,r8
        r14 = s96; // lwz r14,96(r1)
        s100 = r16; // stw r16,100(r1)
        f0 = F(PPC_FAST_LOAD_U32(uint32_t(r19 + r20))); // lfsx f0,r19,r20
        r16 = s104; // lwz r16,104(r1)
        f13 = float(std::fma(double(f0), double(f13), double(f6))); // fmadds f13,f0,f13,f6
        r16 = r16 + uint32_t(1); // addi r16,r16,1
        PPC_FAST_STORE_U32(uint32_t(r14 + r8), U(f5)); // stfsx f5,r14,r8
        r14 = s252; // lwz r14,252(r1)
        r8 = r8 + uint32_t(4); // addi r8,r8,4
        s104 = r16; // stw r16,104(r1)
        PPC_FAST_STORE_U32(r14, U(f4)); // stfs f4,0(r14)
        f13 = f13 + f10; // fadds f13,f13,f10
        PPC_FAST_STORE_U32(uint32_t(r17 + r20), U(f13)); // stfsx f13,r17,r20
        r19 = m133; // lwz r19,0(r20) [fixed]
        r19 = r19 + uint32_t(1); // addi r19,r19,1
        r19 = r19 & 0x000003ffu; // clrlwi r19,r19,22
        m133 = r19; PPC_FAST_STORE_U32(r20, m133); // stw r19,0(r20) [fixed]
        f0 = float(std::fma(double(f13), double(f12), double(f0))); // fmadds f0,f13,f12,f0
        r20 = s84; // lwz r20,84(r1)
        r19 = s268; // lwz r19,268(r1)
        m29 = U(f0); PPC_FAST_STORE_U32(uint32_t(r20 + 12), m29); // stfs f0,12(r20) [fixed]
        r20 = r19 + uint32_t(24); // addi r20,r19,24
        r16 = m135; // lwz r16,12(r19) [fixed]
        f12 = F(m136); // lfs f12,16(r19) [fixed]
        f13 = F(m137); // lfs f13,4(r19) [fixed]
        r17 = m138; // lwz r17,0(r20) [fixed]
        r14 = r17; // mr r14,r17
        r17 = r17 - r16; // subf r17,r16,r17
        r16 = m139; // lwz r16,0(r19) [fixed]
        r17 = r17 & 0x00000fffu; // clrlwi r17,r17,20
        r16 = r14 - r16; // subf r16,r16,r14
        r14 = r17 + uint32_t(2); // addi r14,r17,2
        r17 = r16 & 0x00000fffu; // clrlwi r17,r16,20
        r16 = std::rotl(r14, 2) & 0xfffffffcu; // rlwinm r16,r14,2,0,29
        r17 = r17 + uint32_t(2); // addi r17,r17,2
        r17 = std::rotl(r17, 2) & 0xfffffffcu; // rlwinm r17,r17,2,0,29
        f0 = F(PPC_FAST_LOAD_U32(uint32_t(r16 + r20))); // lfsx f0,r16,r20
        f0 = f0 * f12; // fmuls f0,f0,f12
        f10 = F(PPC_FAST_LOAD_U32(uint32_t(r17 + r20))); // lfsx f10,r17,r20
        f13 = f10 * f13; // fmuls f13,f10,f13
        m6 = U(f0); PPC_FAST_STORE_U32(uint32_t(r19 + 20), m6); // stfs f0,20(r19) [fixed]
        m30 = U(f13); PPC_FAST_STORE_U32(uint32_t(r19 + 8), m30); // stfs f13,8(r19) [fixed]
        r19 = m138; // lwz r19,0(r20) [fixed]
        f13 = F(s284); // lfs f13,284(r1)
        r19 = r19 + uint32_t(2); // addi r19,r19,2
        r19 = std::rotl(r19, 2) & 0xfffffffcu; // rlwinm r19,r19,2,0,29
        PPC_FAST_STORE_U32(uint32_t(r19 + r20), U(f11)); // stfsx f11,r19,r20
        r19 = m138; // lwz r19,0(r20) [fixed]
        r19 = r19 + uint32_t(1); // addi r19,r19,1
        r19 = r19 & 0x00000fffu; // clrlwi r19,r19,20
        m138 = r19; PPC_FAST_STORE_U32(r20, m138); // stw r19,0(r20) [fixed]
        r20 = s276; // lwz r20,276(r1)
        r19 = m140; // lwz r19,0(r20) [fixed]
        f0 = F(m141); // lfs f0,8(r20) [fixed]
        f0 = f0 * f13; // fmuls f0,f0,f13
        f12 = F(m142); // lfs f12,12(r20) [fixed]
        r17 = r19 & 0x00000001u; // clrlwi r17,r19,31
        r19 = r20 + uint32_t(16); // addi r19,r20,16
        r17 = r17 + uint32_t(5); // addi r17,r17,5
        r17 = std::rotl(r17, 2) & 0xfffffffcu; // rlwinm r17,r17,2,0,29
        f11 = F(m143); // lfs f11,4(r19) [fixed]
        f10 = F(PPC_FAST_LOAD_U32(uint32_t(r17 + r20))); // lfsx f10,r17,r20
        m144 = U(f11); PPC_FAST_STORE_U32(uint32_t(r19 + 8), m144); // stfs f11,8(r19) [fixed]
        f0 = float(std::fma(double(f10), double(f12), double(f0))); // fmadds f0,f10,f12,f0
        m143 = U(f13); PPC_FAST_STORE_U32(uint32_t(r19 + 4), m143); // stfs f13,4(r19) [fixed]
        m1 = U(f0); PPC_FAST_STORE_U32(uint32_t(r20 + 4), m1); // stfs f0,4(r20) [fixed]
        r20 = r15 + uint32_t(4); // addi r20,r15,4
        s80 = r20; // stw r20,80(r1)
        cmpA = int32_t(r20); cmpB = 1024; cmpSigned = true; // cmpwi cr6,r20,1024
        if (cmpSigned ? (cmpA < cmpB) : (cmpUA < cmpUB)) goto loc_82CCAB38; // blt cr6,0x82ccab38
        r27 = s220; // lwz r27,220(r1)
        r3 = r31 + uint32_t(327680); // addis r3,r31,5
        r4 = r27; // mr r4,r27
        r3 = r3 + uint32_t(-28368); // addi r3,r3,-28368
        CALL(sub_82CC9798, 0x82ccb5c8u); // bl 0x82cc9798
        r3 = r31 + uint32_t(327680); // addis r3,r31,5
        r4 = r27; // mr r4,r27
        r3 = r3 + uint32_t(-26296); // addi r3,r3,-26296
        CALL(sub_82CC9798, 0x82ccb5d8u); // bl 0x82cc9798
        r30 = s212; // lwz r30,212(r1)
        r3 = r31 + uint32_t(327680); // addis r3,r31,5
        r4 = r30; // mr r4,r30
        r3 = r3 + uint32_t(-24224); // addi r3,r3,-24224
        CALL(sub_82CC9798, 0x82ccb5ecu); // bl 0x82cc9798
        r3 = r31 + uint32_t(327680); // addis r3,r31,5
        r4 = r30; // mr r4,r30
        r3 = r3 + uint32_t(-22152); // addi r3,r3,-22152
        CALL(sub_82CC9798, 0x82ccb5fcu); // bl 0x82cc9798
        r4 = s208; // lwz r4,208(r1)
        r3 = r31 + uint32_t(327680); // addis r3,r31,5
        r29 = r4 + uint32_t(-8); // addi r29,r4,-8
        r3 = r3 + uint32_t(-20080); // addi r3,r3,-20080
        r5 = r29; // mr r5,r29
        CALL(sub_82CC9B10, 0x82ccb614u); // bl 0x82cc9b10
        r4 = s216; // lwz r4,216(r1)
        r3 = r31 + uint32_t(327680); // addis r3,r31,5
        r28 = r4 + uint32_t(-8); // addi r28,r4,-8
        r3 = r3 + uint32_t(-19024); // addi r3,r3,-19024
        r5 = r28; // mr r5,r28
        CALL(sub_82CC9B10, 0x82ccb62cu); // bl 0x82cc9b10
        r8 = uint32_t(92); // li r8,92
        r11 = uint32_t(-2094923776); // lis r11,-31966
        r9 = s148; // lwz r9,148(r1)
        r10 = r30; // mr r10,r30
        r6 = s292; // lwz r6,292(r1)
        r11 = r11 + uint32_t(-30448); // addi r11,r11,-30448
        r7 = s308; // lwz r7,308(r1)
        r3 = r27 - r9; // subf r3,r9,r27
        v0 = LoadVectorLeft(base, uint32_t(r31 + r8)); // lvlx v0,r31,r8
        r8 = s140; // lwz r8,140(r1)
        v0 = vdupq_laneq_f32(v0, 0); // vspltw v0,v0,0
        r24 = r29 - r9; // subf r24,r9,r29
        r31 = r30 - r9; // subf r31,r9,r30
        v13 = LoadVector(base, (uint32_t(r11) & ~15u)); // lvx128 v13,r0,r11
        r25 = r8 - r30; // subf r25,r30,r8
        r27 = r27 - r30; // subf r27,r30,r27
        r29 = r29 - r30; // subf r29,r30,r29
        FLUSH_ON; v13 = vsubq_f32(v13, v0); // vsubfp v13,v13,v0
        r26 = r8 - r9; // subf r26,r9,r8
        r8 = s316; // lwz r8,316(r1)
        r23 = r28 - r9; // subf r23,r9,r28
        FLUSH_ON; v12 = vaddq_f32(v0, v0); // vaddfp v12,v0,v0
        r11 = r9 + uint32_t(16); // addi r11,r9,16
        r9 = s300; // lwz r9,300(r1)
        r30 = r28 - r30; // subf r30,r30,r28
        r4 = uint32_t(32); // li r4,32
        r28 = uint32_t(-16); // li r28,-16
        r5 = uint32_t(16); // li r5,16
        loc_82CCB69C:;
        v10 = LoadVector(base, (uint32_t(r3 + r11) & ~15u)); // lvx128 v10,r3,r11
        r4 = r4 + uint32_t(-1); // addi r4,r4,-1
        v9 = LoadVector(base, (uint32_t(r10) & ~15u)); // lvx128 v9,r0,r10
        FLUSH_ON; v10 = vmulq_f32(v0, v10); // vmulfp128 v10,v0,v10
        v11 = LoadVector(base, (uint32_t(r27 + r10) & ~15u)); // lvx128 v11,r27,r10
        FLUSH_ON; v9 = vmulq_f32(v0, v9); // vmulfp128 v9,v0,v9
        v8 = LoadVector(base, (uint32_t(r31 + r11) & ~15u)); // lvx128 v8,r31,r11
        FLUSH_ON; v11 = vmulq_f32(v0, v11); // vmulfp128 v11,v0,v11
        FLUSH_ON; v8 = vmulq_f32(v0, v8); // vmulfp128 v8,v0,v8
        v7 = LoadVector(base, (uint32_t(r11) & ~15u)); // lvx128 v7,r0,r11
        v6 = LoadVector(base, (uint32_t(r25 + r10) & ~15u)); // lvx128 v6,r25,r10
        cmpUA = r4; cmpUB = 0u; cmpSigned = false; // cmplwi cr6,r4,0
        v5 = LoadVector(base, (uint32_t(r11 + r28) & ~15u)); // lvx128 v5,r11,r28
        v4 = LoadVector(base, (uint32_t(r26 + r11) & ~15u)); // lvx128 v4,r26,r11
        v3 = LoadVector(base, (uint32_t(r29 + r10) & ~15u)); // lvx128 v3,r29,r10
        v2 = LoadVector(base, (uint32_t(r24 + r11) & ~15u)); // lvx128 v2,r24,r11
        FLUSH_ON; v3 = vmulq_f32(v12, v3); // vmulfp128 v3,v12,v3
        v1 = LoadVector(base, (uint32_t(r30 + r10) & ~15u)); // lvx128 v1,r30,r10
        FLUSH_ON; v2 = vmulq_f32(v12, v2); // vmulfp128 v2,v12,v2
        v31 = LoadVector(base, (uint32_t(r23 + r11) & ~15u)); // lvx128 v31,r23,r11
        r10 = r10 + uint32_t(32); // addi r10,r10,32
        r11 = r11 + uint32_t(32); // addi r11,r11,32
        FLUSH_ON; v10 = vfmaq_f32(v10, v13, v7); // vmaddfp v10,v13,v7,v10
        FLUSH_ON; v9 = vfmaq_f32(v9, v13, v6); // vmaddfp v9,v13,v6,v9
        FLUSH_ON; v11 = vfmaq_f32(v11, v13, v5); // vmaddfp v11,v13,v5,v11
        FLUSH_ON; v8 = vfmaq_f32(v8, v13, v4); // vmaddfp v8,v13,v4,v8
        FLUSH_ON; v7 = vmulq_f32(v12, v1); // vmulfp128 v7,v12,v1
        FLUSH_ON; v6 = vmulq_f32(v12, v31); // vmulfp128 v6,v12,v31
        StoreVector(base, (uint32_t(r9) & ~15u), v3); // stvx128 v3,r0,r9
        StoreVector(base, (uint32_t(r9 + r5) & ~15u), v2); // stvx128 v2,r9,r5
        r9 = r9 + uint32_t(32); // addi r9,r9,32
        StoreVector(base, (uint32_t(r8 + r5) & ~15u), v10); // stvx128 v10,r8,r5
        StoreVector(base, (uint32_t(r7) & ~15u), v9); // stvx128 v9,r0,r7
        StoreVector(base, (uint32_t(r8) & ~15u), v11); // stvx128 v11,r0,r8
        r8 = r8 + uint32_t(32); // addi r8,r8,32
        StoreVector(base, (uint32_t(r7 + r5) & ~15u), v8); // stvx128 v8,r7,r5
        r7 = r7 + uint32_t(32); // addi r7,r7,32
        StoreVector(base, (uint32_t(r6) & ~15u), v7); // stvx128 v7,r0,r6
        StoreVector(base, (uint32_t(r6 + r5) & ~15u), v6); // stvx128 v6,r6,r5
        r6 = r6 + uint32_t(32); // addi r6,r6,32
        if (cmpSigned ? (cmpA != cmpB) : (cmpUA != cmpUB)) goto loc_82CCB69C; // bne cr6,0x82ccb69c
        sp = sp + uint32_t(17872); // addi r1,r1,17872
        r12 = sp + uint32_t(-152); // addi r12,r1,-152
        // bl 0x82b7b17c (register saves: not needed)
        goto done; // b 0x82b7a700

    done:
#undef CALL
#undef FLUSH_ON
        ctx.r0.u64 = r0;
        ctx.r3.u64 = r3;
        ctx.r4.u64 = r4;
        ctx.r5.u64 = r5;
        ctx.r6.u64 = r6;
        ctx.r7.u64 = r7;
        ctx.r8.u64 = r8;
        ctx.r9.u64 = r9;
        ctx.r10.u64 = r10;
        ctx.r11.u64 = r11;
        ctx.r12.u64 = r12;
        ctx.f0.f64 = double(f0);
        ctx.f1.f64 = double(f1);
        ctx.f2.f64 = double(f2);
        ctx.f3.f64 = double(f3);
        ctx.f4.f64 = double(f4);
        ctx.f5.f64 = double(f5);
        ctx.f6.f64 = double(f6);
        ctx.f7.f64 = double(f7);
        ctx.f8.f64 = double(f8);
        ctx.f9.f64 = double(f9);
        ctx.f10.f64 = double(f10);
        ctx.f11.f64 = double(f11);
        ctx.f12.f64 = double(f12);
        ctx.f13.f64 = double(f13);
        ctx.r1.u64 = entrySp;
        ctx.lr = entryLr;
        (void)cmpA; (void)cmpB; (void)cmpUA; (void)cmpUB; (void)cmpSigned;
    }

    // The reverb's own memory: its last filter's state starts at +0x4B5B0, and its filters are 0x420 apart there.
    // Not one byte further: other threads change the objects after it, and the check restores this range.
    constexpr uint32_t kReverbBytes = 0x4B9D0;
    constexpr uint32_t kOutputBytes = 1024;
    constexpr uint32_t kOutputSlots[4] = {0, 4, 12, 16}; // r5 words the vector loop writes through

    void Check(PPCContext& ctx, uint8_t* base)
    {
        static std::atomic<uint32_t> calls{0}, differences{0};
        const uint32_t reverb = ctx.r3.u32, outputs = ctx.r5.u32;
        uint32_t out[4];
        for (uint32_t i = 0; i < 4; ++i) out[i] = PPC_LOAD_U32(outputs + kOutputSlots[i]);
        thread_local std::vector<uint8_t> before, expected;
        before.assign(base + reverb, base + reverb + kReverbBytes);
        for (uint32_t i = 0; i < 4; ++i) before.insert(before.end(), base + out[i], base + out[i] + kOutputBytes);
        PPCContext guest = ctx;
        __imp__sub_82CCA0B0(guest, base);
        ctx.fpscr.loadFromHost(); // the copy changed the CPU's flush mode behind ctx's cached one
        expected.assign(base + reverb, base + reverb + kReverbBytes);
        for (uint32_t i = 0; i < 4; ++i) expected.insert(expected.end(), base + out[i], base + out[i] + kOutputBytes);
        std::memcpy(base + reverb, before.data(), kReverbBytes);
        for (uint32_t i = 0; i < 4; ++i) std::memcpy(base + out[i], before.data() + kReverbBytes + i * kOutputBytes, kOutputBytes);
        Native(ctx, base);
        const uint32_t n = calls.fetch_add(1) + 1;
        auto compare = [&](uint32_t guestAddress, const uint8_t* want, uint32_t size, const char* what) {
            if (std::memcmp(base + guestAddress, want, size) == 0) return;
            uint32_t at = 0;
            while (at + 4 <= size && std::memcmp(base + guestAddress + at, want + at, 4) == 0) at += 4;
            if (differences.fetch_add(1) < 20)
                LOG_ERROR("[reverb] sub_82CCA0B0 differs: {} +{:#x} native {:08X} recompiled {:08X}", what, at,
                    PPC_LOAD_U32(guestAddress + at), __builtin_bswap32(*reinterpret_cast<const uint32_t*>(want + at)));
        };
        compare(reverb, expected.data(), kReverbBytes, "reverb");
        for (uint32_t i = 0; i < 4; ++i) compare(out[i], expected.data() + kReverbBytes + i * kOutputBytes, kOutputBytes, "output");
        if (ctx.r1.u32 != guest.r1.u32) LOG_ERROR("[reverb] r1 {:#x} vs {:#x}", ctx.r1.u32, guest.r1.u32);
        if ((n & 0xFFF) == 0)
            LOG_ERROR("[reverb] sub_82CCA0B0 checked {} calls, {} differ", n, differences.load());
    }
}

PPC_FUNC(sub_82CCA0B0)
{
    switch (NativeReverbMode())
    {
    case 0: __imp__sub_82CCA0B0(ctx, base); return;
    case 2: Check(ctx, base); return;
    default: Native(ctx, base); return;
    }
}
