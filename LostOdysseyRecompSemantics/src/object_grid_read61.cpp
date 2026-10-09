#include "lo_semantics/object_grid_read61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/crt_reader_cleanup_callers_context.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::object_grid_read61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(Registers& s,std::uint64_t a,std::uint64_t b) {
    const auto x=Address(a),y=Address(b);
    s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
}
void Seek(GuestMemory& m,Registers& s) {
    auto& r=s.r;
    r[11]=m.ReadU32(Address(r[3]));r[10]=m.ReadU32(Address(r[11]+8u));
    Compare(s,r[4],r[10]);
    if(!s.cr6.lt)return;
    m.WriteU32(Address(r[11]+4u),Address(r[4]));r[11]=m.ReadU32(Address(r[3]));
    r[10]=m.ReadU32(Address(r[11]+4u));r[11]=m.ReadU32(Address(r[11]));r[11]+=r[10];
    m.WriteU32(Address(r[3]+16u),Address(r[11]));
}
void Lower(GuestAddress entry,GuestAddress next,GuestMemory& m,Dependencies d,Registers& s) {
    s.lr=next;
    switch(entry) {
    case 0x82df2150u:(void)crt_stream_close_shared_lower::Apply(entry,m,d.reader.accepted,s);break;
    case 0x82b85d88u: {
        auto stream=crt_context_adapter::ToStream(s);
        (void)crt_stream_bulk_close_routes::Apply(entry,m,d.reader.accepted.close,stream);
        crt_context_adapter::FromStream(s,stream);break;
    }
    case 0x82bd0798u:(void)crt_close_recursive_buffer_context::Apply(entry,m,d.reader.guest,s);break;
    case 0x82bd0d28u:(void)crt_close_reader_callers_context::Apply(entry,m,d.reader,s);break;
    case 0x82bd0a30u:Seek(m,s);break;
    case 0x82bd09c0u:(void)crt_reader_units61::Apply(entry,m,d.reader.guest,s);break;
    case 0x82baf600u:(void)object_sort_reader61::Apply(entry,m,{d.reader.guest,d.fp},s);break;
    case 0x82bd0df0u:(void)crt_reader_cleanup_callers_context::Apply(entry,m,d.reader.guest,s);break;
    default:(void)object_sort_support61::Apply(entry,m,{d.reader.guest,d.fp},s);break;
    }
}
void Refill(GuestAddress next,GuestMemory& m,Dependencies d,Registers& s) {
    if(!s.cr6.eq)return;
    auto& r=s.r;r[3]=r[31];Lower(0x82bd09c0u,next,m,d,s);
    m.WriteU8(Address(r[31]+25u),std::uint8_t(r[3]));m.WriteU8(Address(r[31]+24u),std::uint8_t(r[27]));
}
// Read a single MSB-first bit. The explicit-ID loop reuses the shifted mask
// in r11; its accumulator r29 is prepared before refill, just as in the ABI.
void Bit(GuestMemory& m,Registers& s,unsigned output) {
    auto& r=s.r;r[11]=m.ReadU8(Address(r[31]+24u));r[10]=m.ReadU8(Address(r[31]+25u));
    r[9]=r[11];r[11]>>=1u;r[10]&=r[9];r[10]=r[10]==0u?1u:0u;
    m.WriteU8(Address(r[31]+24u),std::uint8_t(r[11]));r[output]=r[10]^1u;
}
void Restore(GuestMemory& m,Registers& s) {
    auto& r=s.r;r[1]+=160u;
    for(unsigned i=26u;i<=31u;++i)r[i]=ReadU64(m,Address(r[1]-16u-8u*(31u-i)));
    r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
void Read(GuestMemory& m,Dependencies d,Registers& s) {
    auto& r=s.r;r[12]=s.lr;s.lr=0x82bb20a0u;
    for(unsigned i=26u;i<=31u;++i)WriteU64(m,Address(r[1]-16u-8u*(31u-i)),r[i]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));const auto caller_sp=r[1];r[1]-=160u;
    m.WriteU32(Address(r[1]),Address(caller_sp));r[30]=r[4];r[28]=r[3];r[26]=r[5];Compare(s,r[30],0u);
    if(!s.cr6.eq) {
        r[11]=r[30];r[10]=r[11];
        do {r[9]=m.ReadU8(Address(r[11]));++r[11];Compare(s,r[9],0u);}while(!s.cr6.eq);
        r[11]=Address(r[11]-r[10]-1u);Compare(s,r[11],0u);
        if(!s.cr6.eq) {
            r[11]=0xffffffff820d0000ull;r[3]=r[30];r[4]=r[11]+24808u;
            Lower(0x82df2150u,0x82bb20f4u,m,d,s);Compare(s,r[3],0u);
            if(s.cr6.eq)r[11]=0u;
            else {Lower(0x82b85d88u,0x82bb2108u,m,d,s);r[11]=1u;}
        }
        r[11]&=255u;Compare(s,r[11],0u);
        if(s.cr6.eq){r[3]=0u;Restore(m,s);return;}
    }
    // Cursor/histogram are persistent globals shared with the coordinate codec.
    r[11]=0xffffffff832e0000ull;r[9]=~std::uint64_t{0};r[10]=r[11]-15288u;
    r[11]=0xffffffff83210000ull+24972u;
    m.WriteU32(Address(r[11]-8u),Address(r[9]));m.WriteU32(Address(r[11]-4u),Address(r[9]));m.WriteU32(Address(r[11]),Address(r[9]));
    r[9]=0u;r[11]=32u;s.ctr=r[11];
    do {m.WriteU32(Address(r[10]),Address(r[9]));r[10]+=4u;--s.ctr;}while(Address(s.ctr)!=0u);
    r[11]=m.ReadU32(Address(r[28]+104u));r[10]=0u;Compare(s,r[11],0u);
    if(s.cr6.gt) {
        r[11]=0u;r[9]=~std::uint64_t{0};
        do {
            r[8]=m.ReadU32(Address(r[28]+108u));++r[10];m.WriteU32(Address(r[8]+r[11]),Address(r[9]));r[11]+=4u;
            r[8]=m.ReadU32(Address(r[28]+104u));Compare(s,r[10],r[8]);
        }while(s.cr6.lt);
    }
    r[31]=r[26];Compare(s,r[26],0u);
    if(s.cr6.eq) {
        Lower(0x82bd0798u,0x82bb21a0u,m,d,s);r[11]=m.ReadU32(Address(r[3]));r[5]=1u;r[4]=28u;
        r[11]=m.ReadU32(Address(r[11]));s.ctr=r[11];s.lr=0x82bb21b8u;
        d.reader.guest.CallIndirect(Address(s.ctr)&~3u,m,s);Compare(s,r[3],0u);
        if(!s.cr6.eq){r[4]=r[30];Lower(0x82bd0d28u,0x82bb21c8u,m,d,s);r[31]=r[3];}
        else r[31]=0u;
        r[4]=0u;r[3]=r[31];Lower(0x82bd0a30u,0x82bb21e0u,m,d,s);
    }
    r[29]=0u;r[27]=128u;
    for(;;) {
        r[11]=m.ReadU8(Address(r[31]+24u));Compare(s,r[11],0u);Refill(0x82bb21fcu,m,d,s);
        Bit(m,s,10u);Compare(s,r[10],0u);
        if(!s.cr6.eq)r[30]=r[29];
        else {
            r[30]=32u;r[10]=0u;
            do {
                r[11]&=255u;--r[30];r[29]=(r[10]<<1u)&0xfffffffeu;Compare(s,r[11],0u);
                Refill(0x82bb2260u,m,d,s);
                // Compare count before consuming this bit; Bit leaves CR6 live.
                r[11]=m.ReadU8(Address(r[31]+24u));Compare(s,r[30],0u);
                Bit(m,s,10u);r[10]|=r[29];
            }while(!s.cr6.eq);
            r[30]=r[10];r[29]=r[10];
        }
        ++r[29];const auto color=std::bit_cast<std::int32_t>(Address(r[30]));
        s.cr6={std::uint8_t(color<-1),std::uint8_t(color>-1),std::uint8_t(color==-1),s.xer_so};
        if(s.cr6.eq)break;
        r[3]=r[1]+80u;Lower(0x82bd2a08u,0x82bb22b8u,m,d,s);
        r[4]=r[31];r[3]=r[1]+80u;r[5]=m.ReadU32(Address(r[28]+88u));Lower(0x82baf600u,0x82bb22c8u,m,d,s);
        r[11]=r[3];Compare(s,r[11],0u);
        if(!s.cr6.eq) {
            r[10]=0u;
            do {
                r[9]=m.ReadU32(Address(r[1]+88u));--r[11];r[8]=m.ReadU32(Address(r[28]+108u));Compare(s,r[11],0u);
                r[9]=m.ReadU32(Address(r[10]+r[9]));r[10]+=4u;r[9]=(r[9]<<2u)&0xfffffffcu;
                m.WriteU32(Address(r[9]+r[8]),Address(r[30]));
            }while(!s.cr6.eq);
        }
        r[3]=r[1]+80u;Lower(0x82bd2c08u,0x82bb2304u,m,d,s);
    }
    r[11]=m.ReadU32(Address(r[28]+104u));r[29]=0u;Compare(s,r[11],0u);
    if(s.cr6.gt) {
        r[30]=0u;
        do {
            r[11]=m.ReadU8(Address(r[31]+24u));Compare(s,r[11],0u);Refill(0x82bb2330u,m,d,s);
            Bit(m,s,11u);Compare(s,r[11],0u);
            if(s.cr6.eq) {
                r[11]=m.ReadU32(Address(r[28]+108u));r[10]=m.ReadU32(Address(r[11]+r[30]));r[10]|=0x80000000u;
                m.WriteU32(Address(r[11]+r[30]),Address(r[10]));
            }
            r[11]=m.ReadU32(Address(r[28]+104u));++r[29];r[30]+=4u;Compare(s,r[29],r[11]);
        }while(s.cr6.lt);
    }
    Compare(s,r[26],0u);
    if(s.cr6.eq) {
        r[3]=r[31];Lower(0x82bd0df0u,0x82bb239cu,m,d,s);Lower(0x82bd0798u,0x82bb23a0u,m,d,s);
        r[11]=m.ReadU32(Address(r[3]));r[4]=r[31];r[11]=m.ReadU32(Address(r[11]+12u));s.ctr=r[11];s.lr=0x82bb23b4u;
        d.reader.guest.CallIndirect(Address(s.ctr)&~3u,m,s);
    }
    r[3]=1u;Restore(m,s);
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& state) {
    if(entry==0x82bd0a30u){Seek(memory,state);return true;}
    if(entry!=0x82bb2098u)return false;
    Read(memory,deps,state);return true;
}
}
