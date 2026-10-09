#include "lo_semantics/crt_random_thread61.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_last_error.h"
#include "lo_semantics/crt_thread_error_routes.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>

namespace lo::semantic::gpu::crt_random_thread61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using crt_context_adapter::ToStream;
using crt_context_adapter::FromStream;
std::uint64_t Signed(std::uint32_t word) {
    return std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(word)));
}
void CompareZero(Registers& s,std::uint64_t word) {
    const auto value=std::bit_cast<std::int32_t>(Address(word));
    s.cr0={std::uint8_t(value<0),std::uint8_t(value>0),std::uint8_t(value==0),s.xer_so};
}
struct TlsBridge final : crt_thread_error_routes::NativeServices {
    Services& services;Registers& full;
    TlsBridge(Services& native,Registers& state):services(native),full(state){}
    void KeTlsGetValue(GuestMemory& m,crt_thread_error_routes::Registers& lower) override {
        FromStream(full,lower);services.KeTlsGetValue(m,full);lower=ToStream(full);
    }
    void KeTlsSetValue(GuestMemory& m,crt_thread_error_routes::Registers& lower) override {
        FromStream(full,lower);services.KeTlsSetValue(m,full);lower=ToStream(full);
    }
};
void Call(GuestAddress entry,GuestAddress continuation,GuestMemory& m,Dependencies d,Registers& s) {
    s.lr=continuation;(void)ApplyAcceptedLower(entry,m,d,s);
}
void Indirect(GuestAddress continuation,GuestMemory& m,Dependencies d,Registers& s) {
    s.ctr=s.r[11];s.lr=continuation;d.services.CallIndirect(Address(s.ctr)&~3u,m,s);
}
void Enter(GuestMemory& m,Registers& s,unsigned frame) {
    const auto caller=s.r[1];s.r[1]-=frame;m.WriteU32(Address(s.r[1]),Address(caller));
}
void ThreadRecord(GuestMemory& m,Dependencies d,Registers& s) {
    auto& r=s.r;r[12]=s.lr;s.lr=0x822ca050u;
    for(unsigned i=29u;i<=31u;++i)WriteU64(m,Address(r[1]-8u*(33u-i)),r[i]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));Enter(m,s,112u);
    Call(0x822ca100u,0x822ca058u,m,d,s);
    r[30]=Signed(0x83210000u);r[29]=r[3];r[31]=m.ReadU32(Address(r[30]+19828u));
    Call(0x822ca128u,0x822ca068u,m,d,s);
    r[11]=r[3];r[3]=r[31];Indirect(0x822ca078u,m,d,s);
    r[31]=r[3];CompareZero(s,r[31]);
    if(s.cr0.eq) {
        r[4]=196u;r[3]=1u;Call(0x82b81778u,0x822ca08cu,m,d,s);
        r[31]=r[3];CompareZero(s,r[31]);
        if(!s.cr0.eq) {
            r[11]=Signed(0x832d0000u);r[3]=m.ReadU32(Address(r[30]+19828u));r[4]=r[31];
            r[11]=m.ReadU32(Address(r[11]+15072u));Indirect(0x822ca0acu,m,d,s);
            CompareZero(s,r[3]);
            if(!s.cr0.eq) {
                r[11]=Signed(0x83210000u);r[10]=1u;r[11]+=21624u;
                m.WriteU32(Address(r[31]+20u),Address(r[10]));m.WriteU32(Address(r[31]+92u),Address(r[11]));
                Call(0x82290aa8u,0x822ca0ccu,m,d,s);
                r[11]=UINT64_MAX;m.WriteU32(Address(r[31]),Address(r[3]));m.WriteU32(Address(r[31]+4u),Address(r[11]));
            } else {
                r[3]=r[31];Call(0x823addc0u,0x822ca0e4u,m,d,s);r[31]=0u;
            }
        }
    }
    r[3]=r[29];Call(0x822ca180u,0x822ca0f0u,m,d,s);r[3]=r[31];r[1]+=112u;
    for(unsigned i=29u;i<=31u;++i)r[i]=ReadU64(m,Address(r[1]-8u*(33u-i)));
    r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
void RequiredRecord(GuestMemory& m,Dependencies d,Registers& s) {
    auto& r=s.r;r[12]=s.lr;m.WriteU32(Address(r[1]-8u),Address(r[12]));WriteU64(m,Address(r[1]-16u),r[31]);Enter(m,s,96u);
    s.lr=0x822ca01cu;ThreadRecord(m,d,s);r[31]=r[3];CompareZero(s,r[31]);
    if(s.cr0.eq){r[3]=16u;s.lr=0x822ca02cu;d.services.FatalRuntimeError(m,s);}
    r[3]=r[31];r[1]+=96u;r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];r[31]=ReadU64(m,Address(r[1]-16u));
}
void Random(GuestMemory& m,Dependencies d,Registers& s) {
    auto& r=s.r;r[12]=s.lr;m.WriteU32(Address(r[1]-8u),Address(r[12]));Enter(m,s,96u);
    s.lr=0x822c9fd0u;RequiredRecord(m,d,s);r[11]=r[3];r[10]=214013u;r[9]=m.ReadU32(Address(r[11]+20u));
    // The product is a full signed-word product before the two 64-bit adds;
    // only the seed store and returned bit slice narrow it to a word.
    r[10]=std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(r[9])))*214013ll);
    r[10]+=39u<<16u;r[10]-=24893u;r[3]=(Address(r[10])>>16u)&0x7fffu;
    m.WriteU32(Address(r[11]+20u),Address(r[10]));r[1]+=96u;r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
}
bool ApplyAcceptedLower(GuestAddress entry,GuestMemory& m,Dependencies d,Registers& s) {
    switch(entry) {
    case 0x822ca100u: {
        crt_last_error::Registers lower{s.r[3],s.r[11],s.r[13],s.xer_so,{s.cr6.lt,s.cr6.gt,s.cr6.eq,s.cr6.so}};
        (void)crt_last_error::Apply(entry,m,lower);s.r[3]=lower.r3;s.r[11]=lower.r11;s.r[13]=lower.r13;s.xer_so=lower.xer_so;
        s.cr6={lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.so};return true;
    }
    case 0x822ca128u:case 0x822ca180u: {
        auto lower=ToStream(s);TlsBridge bridge{d.services,s};
        (void)crt_thread_error_routes::Apply(entry,m,bridge,lower);FromStream(s,lower);return true;
    }
    case 0x82b81778u:return crt_record_allocation_context::Apply(entry,m,d.allocation,s);
    case 0x823addc0u:{auto lower=ToStream(s);const auto ok=crt_free_context::Apply(entry,m,d.release,lower);FromStream(s,lower);return ok;}
    case 0x82290aa8u:s.r[11]=m.ReadU32(Address(s.r[13]+256u));s.r[3]=m.ReadU32(Address(s.r[11]+332u));return true;
    default:return false;
    }
}
bool Apply(GuestAddress entry,GuestMemory& m,Dependencies d,Registers& s) {
    switch(entry) {
    case 0x82bd2c48u:case 0x822c9fc0u:Random(m,d,s);return true;
    case 0x822ca008u:RequiredRecord(m,d,s);return true;
    case 0x822ca048u:ThreadRecord(m,d,s);return true;
    default:return false;
    }
}
}
