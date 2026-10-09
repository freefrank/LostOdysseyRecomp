#include "lo_semantics/grid_transform_routes61.h"
#include "lo_semantics/pointer_fields.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
namespace lo::semantic::gpu::grid_transform_routes61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void InitializeBase(GuestMemory& memory,Registers& state) {
    // Compose the already accepted constant-field algorithm rather than
    // retaining a second generated base-constructor body.
    constexpr std::uint64_t table=0xffffffff820d6bb8ull;
    PointerFieldRegisters fields{};fields.r3=state.r[3];
    constexpr std::array assignments{
        ConstantFieldAssignment{PointerFieldRegister::R10,table},
        ConstantFieldAssignment{PointerFieldRegister::R11,0u}};
    constexpr std::array writes{
        ConstantFieldWrite{0,PointerFieldWidth::Word,static_cast<std::uint32_t>(table)},
        ConstantFieldWrite{4,PointerFieldWidth::Word,0u},
        ConstantFieldWrite{8,PointerFieldWidth::Word,0u},
        ConstantFieldWrite{12,PointerFieldWidth::Word,0u},
        ConstantFieldWrite{16,PointerFieldWidth::Word,0u}};
    InitializeConstantFields(memory,fields,PointerFieldRegister::R3,assignments,writes);
    state.r[10]=fields.r10;state.r[11]=fields.r11;
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Registers& state) {
    if(entry==0x82bd12d0u){InitializeBase(memory,state);return true;}
    if(entry!=0x82bd7950u)return false;
    auto& r=state.r;
    r[12]=state.lr;memory.WriteU32(Address(r[1]-8u),Address(r[12]));
    WriteU64(memory,Address(r[1]-16u),r[31]);
    const auto caller_sp=r[1];r[1]-=96u;memory.WriteU32(Address(r[1]),Address(caller_sp));
    r[31]=r[3];state.lr=0x82bd7968u;InitializeBase(memory,state);
    r[11]=0xffffffff820d0000ull;r[3]=r[31];r[11]+=28096u;
    memory.WriteU32(Address(r[31]),Address(r[11]));
    r[1]+=96u;r[12]=memory.ReadU32(Address(r[1]-8u));state.lr=r[12];
    r[31]=ReadU64(memory,Address(r[1]-16u));return true;
}
}
