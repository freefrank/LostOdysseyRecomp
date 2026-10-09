#include "lo_semantics/grid_storage_initialize61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>

namespace lo::semantic::gpu::grid_storage_initialize61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr unsigned Dimension = 88u, PlaneCells = 92u, LastCoordinate = 96u;
constexpr unsigned ReciprocalCoordinate = 100u, CellCount = 104u, Cells = 108u;

double Float(const Registers& s, unsigned index) { return std::bit_cast<double>(s.fpr_bits[index]); }
void Float(Registers& s, unsigned index, double value) { s.fpr_bits[index] = std::bit_cast<std::uint64_t>(value); }
double Single(double value) { return static_cast<float>(value); }
void Load(GuestMemory& memory, Registers& s, unsigned index, std::uint64_t address) {
    Float(s, index, std::bit_cast<float>(memory.ReadU32(Address(address))));
}
void Store(GuestMemory& memory, const Registers& s, unsigned index, std::uint64_t address) {
    memory.WriteU32(Address(address), std::bit_cast<std::uint32_t>(static_cast<float>(Float(s,index))));
}
void Compare(Registers& s, std::uint32_t a, std::uint32_t b) {
    s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b), s.xer_so};
}
std::uint64_t Product(std::uint64_t a, std::uint64_t b) {
    return static_cast<std::uint64_t>(std::int64_t(std::bit_cast<std::int32_t>(Address(a))) *
        std::bit_cast<std::int32_t>(Address(b)));
}
void Initialize(GuestMemory& memory, Dependencies deps, Registers& s) {
    auto& r = s.r;
    r[12] = s.lr; s.lr = 0x82bb1e60u;
    for (unsigned i=29u; i<=31u; ++i) WriteU64(memory,Address(r[1]-16u-8u*(31u-i)),r[i]);
    memory.WriteU32(Address(r[1]-8u),Address(r[12]));
    const auto caller_sp=r[1]; r[1]-=128u; memory.WriteU32(Address(r[1]),Address(caller_sp));
    r[9]=r[4]-1u; r[31]=r[3]; r[7]=Address(r[9]); r[8]=Product(r[4],r[4]);
    memory.WriteU32(Address(r[31]+Dimension),Address(r[4]));
    WriteU64(memory,Address(r[1]+80u),r[7]);
    memory.WriteU32(Address(r[31]+PlaneCells),Address(r[8]));
    r[7]=0xffffffff82000000ull;
    r[11]=r[31]+4u; r[10]=r[31]+28u; r[9]=r[31]+40u;
    r[30]=Product(r[8],r[4]);
    if(s.cached_fp_control & 0x8040u) {
        s.cached_fp_control &= ~0x8040u; deps.fp.SetHostFpControl(s.cached_fp_control);
    }
    Load(memory,s,13u,r[7]+30596u);
    r[7]=0xffffffff82020000ull;
    Float(s,0u,Single(static_cast<double>(std::bit_cast<std::int64_t>(ReadU64(memory,Address(r[1]+80u))))));
    Store(memory,s,0u,r[31]+LastCoordinate);
    Float(s,0u,Single(Float(s,13u)/Float(s,0u))); Store(memory,s,0u,r[31]+ReciprocalCoordinate);

    // Copy bounds in field order: the input is borrowed and may overlap the
    // object, so no host-side vector snapshot replaces these ordered accesses.
    for(unsigned offset=0u;offset<24u;offset+=4u) {
        Load(memory,s,0u,r[5]+offset); Store(memory,s,0u,r[11]+offset);
    }
    // Midpoint = (minimum + maximum) * the image's half constant.
    for(unsigned axis=0u;axis<3u;++axis) {
        Load(memory,s,12u,r[11]+axis*4u); Load(memory,s,0u,r[11]+12u+axis*4u);
        Float(s,0u,Single(Float(s,0u)+Float(s,12u))); Store(memory,s,0u,r[10]+axis*4u);
    }
    Load(memory,s,12u,r[10]); Load(memory,s,11u,r[10]+4u); Load(memory,s,10u,r[10]+8u);
    Load(memory,s,0u,r[7]-1552u);
    for(unsigned axis=0u;axis<3u;++axis) {
        const unsigned reg=12u-axis;
        Float(s,reg,Single(Float(s,reg)*Float(s,0u))); Store(memory,s,reg,r[10]+axis*4u);
    }
    // Half extent: retain the original z/x/y store order and FP scratch values.
    Load(memory,s,11u,r[11]); r[10]=1073676288u;
    Load(memory,s,12u,r[11]+12u); Float(s,12u,Single(Float(s,12u)-Float(s,11u))); Store(memory,s,12u,r[9]);
    Load(memory,s,11u,r[11]+4u); r[10]|=65535u;
    Load(memory,s,12u,r[11]+16u); Float(s,12u,Single(Float(s,12u)-Float(s,11u))); Store(memory,s,12u,r[9]+4u);
    Load(memory,s,11u,r[11]+8u); Load(memory,s,12u,r[11]+20u);
    Float(s,12u,Single(Float(s,12u)-Float(s,11u)));
    Load(memory,s,10u,r[9]); Load(memory,s,11u,r[9]+4u);
    Float(s,10u,Single(Float(s,10u)*Float(s,0u))); Float(s,11u,Single(Float(s,11u)*Float(s,0u)));
    Store(memory,s,12u,r[9]+8u); Store(memory,s,10u,r[9]); Store(memory,s,11u,r[9]+4u);
    Float(s,0u,Single(Float(s,12u)*Float(s,0u))); Store(memory,s,0u,r[9]+8u);

    // Full extents and reciprocal coordinate transforms are separate stored
    // quantities. The two single-precision rounding steps are intentional.
    Load(memory,s,12u,r[31]+16u); Load(memory,s,0u,r[11]); Float(s,0u,Single(Float(s,12u)-Float(s,0u)));
    Load(memory,s,11u,r[31]+20u); Load(memory,s,12u,r[11]+4u); Float(s,12u,Single(Float(s,11u)-Float(s,12u)));
    Load(memory,s,10u,r[31]+24u); Load(memory,s,11u,r[11]+8u);
    Compare(s,Address(r[30]),Address(r[10])); Float(s,11u,Single(Float(s,10u)-Float(s,11u)));
    Store(memory,s,0u,r[31]+52u); Store(memory,s,12u,r[31]+56u); r[29]=~std::uint64_t{0};
    Store(memory,s,11u,r[31]+60u); r[4]=(r[30]<<2u)&0xfffffffcu;
    Load(memory,s,0u,r[31]+LastCoordinate); memory.WriteU32(Address(r[31]+CellCount),Address(r[30]));
    Float(s,13u,Single(Float(s,13u)/Float(s,0u)));
    Load(memory,s,12u,r[31]+52u); Load(memory,s,11u,r[31]+56u);
    Float(s,9u,Single(Float(s,0u)/Float(s,12u))); Load(memory,s,10u,r[31]+60u); Store(memory,s,9u,r[31]+64u);
    Float(s,9u,Single(Float(s,0u)/Float(s,11u))); Float(s,0u,Single(Float(s,0u)/Float(s,10u)));
    Store(memory,s,0u,r[31]+72u); Store(memory,s,9u,r[31]+68u);
    for(unsigned axis=0u;axis<3u;++axis) {
        Float(s,0u,Single(Float(s,13u)*Float(s,12u-axis))); Store(memory,s,0u,r[31]+76u+axis*4u);
    }
    if(s.cr6.gt) r[4]=r[29];
    r[11]=0xffffffff832e0000ull; r[5]=286u;
    r[3]=memory.ReadU32(Address(r[11]-2744u)); r[11]=memory.ReadU32(Address(r[3]));
    r[11]=memory.ReadU32(Address(r[11]+8u)); s.ctr=r[11]; s.lr=0x82bb203cu;
    deps.guest.CallIndirect(Address(s.ctr)&~3u,memory,s);
    Compare(s,Address(r[3]),0u);
    if(!s.cr6.eq) {
        r[11]=r[30]-1u;
        const auto last=std::bit_cast<std::int32_t>(Address(r[11]));
        s.cr6={std::uint8_t(last<0),std::uint8_t(last>0),std::uint8_t(last==0),s.xer_so};
        if(!s.cr6.lt) {
            r[10]=r[11]+1u; r[11]=r[3]; r[9]=r[29]; Compare(s,Address(r[10]),0u);
            if(!s.cr6.eq) {
                s.ctr=r[10];
                do {
                    memory.WriteU32(Address(r[11]),Address(r[9])); r[11]+=4u; --s.ctr;
                } while(Address(s.ctr)!=0u);
            }
        }
    } else r[3]=0u;
    memory.WriteU32(Address(r[31]+Cells),Address(r[3]));
    r[11]=Address(r[3])==0u ? 1u : 0u; r[3]=r[11]^1u;
    r[1]+=128u;
    for(unsigned i=29u;i<=31u;++i) r[i]=ReadU64(memory,Address(r[1]-16u-8u*(31u-i)));
    r[12]=memory.ReadU32(Address(r[1]-8u)); s.lr=r[12];
}
}
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps, Registers& state) {
    if(entry!=0x82bb1e58u) return false;
    Initialize(memory,deps,state); return true;
}
}
