#include "lo_semantics/owned_tree_reorder_support61.h"
#include "lo_semantics/owned_tree_visit61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::owned_tree_reorder_support61 {
    namespace {
        using recovery_abi::Address;
        using recovery_abi::ReadU64;
        using recovery_abi::WriteU64;
        void Compare(Registers& s,std::uint64_t a,std::uint64_t b){
            const auto x=Address(a),y=Address(b);
            s.cr6={
                std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so
            };
        }
        void Enter(GuestMemory& m,Registers& s,unsigned first,unsigned frame,GuestAddress continuation){
            auto& r=s.r;
            r[12]=s.lr;
            if(continuation)s.lr=continuation;
            for(unsigned i=first;i<32u;++i)WriteU64(m,Address(r[1]-8u*(33u-i)),r[i]);
            m.WriteU32(Address(r[1]-8u),Address(r[12]));
            const auto old=r[1];
            r[1]-=frame;
            m.WriteU32(Address(r[1]),Address(old));
        }
        void Leave(GuestMemory& m,Registers& s,unsigned first,unsigned frame){
            auto& r=s.r;
            r[1]+=frame;
            r[12]=m.ReadU32(Address(r[1]-8u));
            s.lr=r[12];
            for(unsigned i=first;i<32u;++i)r[i]=ReadU64(m,Address(r[1]-8u*(33u-i)));
        }
        struct Visitor final:crt_close_recursive_buffer_context::GuestServices {
            Dependencies deps;
            explicit Visitor(Dependencies d):deps(d){
            }
            void CallIndirect(GuestAddress target,GuestMemory& m,Registers& s)override{
                if(target==0x82bd1b50u||target==0x82bd2168u||target==0x82bd1b78u)(void)lo::semantic::gpu::owned_tree_reorder_support61::Apply(target,m,deps,s);
                else deps.guest.CallIndirect(target,m,s);
            }
        };
        void CountLeaf(GuestMemory& m,Registers& s){
            auto& r=s.r;
            r[11]=m.ReadU32(Address(r[3]+24u));
            r[11]=Address(r[11])&0xfffffffeu;
            Compare(s,r[11],0u);
            if(s.cr6.eq){
                r[11]=m.ReadU32(Address(r[5]));
                ++r[11];
                m.WriteU32(Address(r[5]),Address(r[11]));
            }
            r[3]=1u;
        }
        // Capacity equality triggers growth; the original then appends using
        // the live descriptor even when the growth return is zero.
        void AppendLeaf(GuestMemory& m,Dependencies d,Registers& s){
            Enter(m,s,30u,112u,0u);
            auto& r=s.r;
            r[30]=r[3];
            r[31]=r[5];
            r[11]=m.ReadU32(Address(r[30]+24u));
            r[11]=Address(r[11])&0xfffffffeu;
            Compare(s,r[11],0u);
            if(s.cr6.eq){
                r[11]=m.ReadU32(Address(r[31]));
                r[10]=m.ReadU32(Address(r[31]+4u));
                Compare(s,r[10],r[11]);
                if(s.cr6.eq){
                    r[4]=1u;
                    r[3]=r[31];
                    s.lr=0x82bd1bc0u;
                    (void)reader_buffer_growth61::Apply(0x82bd2870u,m,{
                        d.guest,d.fp
                    },s);
                }
                r[11]=m.ReadU32(Address(r[31]+4u));
                r[10]=m.ReadU32(Address(r[31]+8u));
                r[11]=(Address(r[11])<<2u)&0xfffffffcu;
                m.WriteU32(Address(r[10]+r[11]),Address(r[30]));
                r[11]=m.ReadU32(Address(r[31]+4u));
                ++r[11];
                m.WriteU32(Address(r[31]+4u),Address(r[11]));
            }
            r[3]=1u;
            Leave(m,s,30u,112u);
        }
        // A leaf becomes six binary32 bounds and one packed range word.
        // Its range packs the byte offset from the index base above a 4-bit
        // (count - 1), retaining the original truncation and FP transfers.
        void ExportLeaf(GuestMemory& m,Dependencies d,Registers& s){
            auto& r=s.r;
            r[11]=r[3];
            r[3]=1u;
            r[10]=m.ReadU32(Address(r[11]+24u));
            r[10]=Address(r[10])&0xfffffffeu;
            Compare(s,r[10],0u);
            if(!s.cr6.eq)return;
            r[10]=m.ReadU32(Address(r[5]));
            if(s.cached_fp_control&0x8040u){
                s.cached_fp_control&=~0x8040u;
                d.fp.SetHostFpControl(s.cached_fp_control);
            }
            s.fpr_bits[0]=std::bit_cast<std::uint64_t>(static_cast<double>(std::bit_cast<float>(m.ReadU32(Address(r[11])))));
            r[9]=m.ReadU32(Address(r[5]+4u));
            r[8]=(Address(r[10])<<1u)&0xfffffffeu;
            r[10]+=r[8];
            r[10]=(Address(r[10])<<3u)&0xfffffff8u;
            r[10]+=r[9];
            m.WriteU32(Address(r[10]),std::bit_cast<std::uint32_t>(static_cast<float>(std::bit_cast<double>(s.fpr_bits[0]))));
            for(unsigned n=1;n<6u;++n){
                s.fpr_bits[0]=std::bit_cast<std::uint64_t>(static_cast<double>(std::bit_cast<float>(m.ReadU32(Address(r[11]+4u*n)))));
                m.WriteU32(Address(r[10]+4u*n),std::bit_cast<std::uint32_t>(static_cast<float>(std::bit_cast<double>(s.fpr_bits[0]))));
            }
            r[10]=m.ReadU32(Address(r[11]+32u));
            r[11]=m.ReadU32(Address(r[11]+36u));
            r[9]=m.ReadU32(Address(r[5]+12u));
            r[8]=m.ReadU32(Address(r[5]));
            --r[11];
            r[10]-=r[9];
            r[9]=m.ReadU32(Address(r[5]+8u));
            r[8]=(Address(r[8])<<2u)&0xfffffffcu;
            r[11]=(Address(r[11])&15u)|((Address(r[10])<<2u)&0xfffffff0u);
            m.WriteU32(Address(r[8]+r[9]),Address(r[11]));
            r[11]=m.ReadU32(Address(r[5]));
            ++r[11];
            m.WriteU32(Address(r[5]),Address(r[11]));
        }
        void Depth(GuestMemory& m,Dependencies d,Registers& s){
            Enter(m,s,32u,96u,0u);
            auto& r=s.r;
            r[11]=0u;
            r[3]=m.ReadU32(Address(r[3]+4u));
            r[6]=r[4];
            r[7]=r[5];
            r[5]=r[1]+84u;
            r[4]=r[1]+80u;
            m.WriteU32(Address(r[1]+80u),Address(r[11]));
            m.WriteU32(Address(r[1]+84u),Address(r[11]));
            s.lr=0x82bdb1f0u;
            Visitor visitor(d);
            (void)owned_tree_visit61::Apply(0x82bda0a8u,m,visitor,s);
            r[3]=m.ReadU32(Address(r[1]+80u));
            Leave(m,s,32u,96u);
        }
        void Children(GuestMemory& m,Dependencies d,Registers& s){
            Enter(m,s,29u,112u,0x82bdb210u);
            auto& r=s.r;
            r[31]=r[4];
            r[30]=r[3];
            r[29]=r[5];
            Compare(s,r[31],0u);
            if(!s.cr6.eq){
                r[3]=m.ReadU32(Address(r[30]+4u));
                r[4]=0u;
                r[5]=r[29];
                s.ctr=r[31];
                s.lr=0x82bdb238u;
                Visitor visitor(d);
                visitor.CallIndirect(Address(s.ctr)&~3u,m,s);
                r[11]=Address(r[3])&255u;
                Compare(s,r[11],0u);
                if(!s.cr6.eq){
                    r[3]=m.ReadU32(Address(r[30]+4u));
                    r[4]=r[31];
                    r[5]=r[29];
                    s.lr=0x82bdb254u;
                    (void)owned_tree_visit61::Apply(0x82bda1a0u,m,visitor,s);
                }
            }
            Leave(m,s,29u,112u);
        }
        // The second owned pointer addresses four bytes past its allocation.
        // Reload owner fields after each mutable guest disposal boundary.
        void Release(GuestMemory& m,Dependencies d,Registers& s){
            Enter(m,s,29u,112u,0x82bd20f8u);
            auto& r=s.r;
            r[31]=r[3];
            s.lr=0x82bd2104u;
            (void)transform_owner_routes61::Apply(0x82bd1558u,m,d,s);
            r[11]=m.ReadU32(Address(r[31]+32u));
            r[30]=0u;
            Compare(s,r[11],0u);
            if(!s.cr6.eq){
                s.lr=0x82bd2118u;
                (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,m,d.guest,s);
                r[11]=m.ReadU32(Address(r[3]));
                r[4]=m.ReadU32(Address(r[31]+32u));
                r[11]=m.ReadU32(Address(r[11]+12u));
                s.ctr=r[11];
                s.lr=0x82bd212cu;
                d.guest.CallIndirect(Address(s.ctr)&~3u,m,s);
                m.WriteU32(Address(r[31]+32u),Address(r[30]));
            }
            r[29]=m.ReadU32(Address(r[31]+24u));
            Compare(s,r[29],0u);
            if(!s.cr6.eq){
                s.lr=0x82bd2140u;
                (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,m,d.guest,s);
                r[11]=m.ReadU32(Address(r[3]));
                r[4]=r[29]-4u;
                r[11]=m.ReadU32(Address(r[11]+12u));
                s.ctr=r[11];
                s.lr=0x82bd2154u;
                d.guest.CallIndirect(Address(s.ctr)&~3u,m,s);
                m.WriteU32(Address(r[31]+24u),Address(r[30]));
            }
            m.WriteU32(Address(r[31]+20u),Address(r[30]));
            m.WriteU32(Address(r[31]+28u),Address(r[30]));
            Leave(m,s,29u,112u);
        }
    }
    bool Apply(GuestAddress entry,GuestMemory& m,Dependencies d,Registers& s){
        switch(entry){
            case 0x82bdb1c0u:Depth(m,d,s);
            break;
            case 0x82bdb208u:Children(m,d,s);
            break;
            case 0x82bd20f0u:Release(m,d,s);
            break;
            case 0x82bd1b50u:CountLeaf(m,s);
            break;
            case 0x82bd2168u:ExportLeaf(m,d,s);
            break;
            case 0x82bd1b78u:AppendLeaf(m,d,s);
            break;
            default:return false;
        }
        return true;
    }
}
