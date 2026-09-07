#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <string>

#if defined(_WIN32) && defined(_MSC_VER)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace xenos::resources {
using Sha256Digest = std::array<uint8_t, 32>;
// Portable SHA-256, also exercised independently when Windows CNG is available.
inline Sha256Digest Sha256Portable(std::span<const uint8_t> bytes) {
    constexpr uint32_t constants[] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t state[] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    auto block = [&](const uint8_t* p) {
        uint32_t w[64];
        for (size_t i=0;i<16;++i) w[i]=uint32_t(p[i*4])<<24|uint32_t(p[i*4+1])<<16|uint32_t(p[i*4+2])<<8|p[i*4+3];
        for (size_t i=16;i<64;++i) {
            const auto x=w[i-15], y=w[i-2];
            w[i]=w[i-16]+(std::rotr(x,7)^std::rotr(x,18)^(x>>3))+w[i-7]+(std::rotr(y,17)^std::rotr(y,19)^(y>>10));
        }
        auto a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
        for (size_t i=0;i<64;++i) {
            const auto t1=h+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^(~e&g))+constants[i]+w[i];
            const auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c));
            h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
    };
    size_t offset=0;
    for (;bytes.size()-offset>=64;offset+=64) block(bytes.data()+offset);
    std::array<uint8_t,128> tail{};
    std::copy(bytes.begin()+offset,bytes.end(),tail.begin());
    const auto remaining=bytes.size()-offset;
    tail[remaining]=0x80;
    const size_t length=remaining<56?64:128;
    const auto bits=uint64_t(bytes.size())*8;
    for (size_t i=0;i<8;++i) tail[length-1-i]=uint8_t(bits>>(i*8));
    block(tail.data()); if (length==128) block(tail.data()+64);
    Sha256Digest result{};
    for (size_t i=0;i<32;++i) result[i]=uint8_t(state[i/4]>>(24-(i%4)*8));
    return result;
}
inline Sha256Digest Sha256(std::span<const uint8_t> bytes) {
#if defined(_WIN32) && defined(_MSC_VER)
    Sha256Digest result{};
    if (bytes.size()<=UINT32_MAX && BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
            const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),result.data(),32)>=0) return result;
#endif
    return Sha256Portable(bytes);
}
inline std::string Sha256Hex(const Sha256Digest& digest) {
    constexpr char digits[]="0123456789abcdef";
    std::string result(64,'0');
    for (size_t i=0;i<digest.size();++i) { result[i*2]=digits[digest[i]>>4]; result[i*2+1]=digits[digest[i]&15]; }
    return result;
}
}
