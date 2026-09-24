#include "../../fsr/vulkan_memory_policy.h"
#include <array>
#include <cstdio>
#include <cstdlib>

namespace {
constexpr uint32_t Local=1, Visible=2, Coherent=4, Disabled=64;
constexpr lo::fsr::MemoryPropertyBits bits{Local,Visible,Coherent,Disabled};
unsigned checks=0;
void Check(bool value,const char* reason) {
    ++checks;
    if (!value) {std::fprintf(stderr,"FAIL: %s\n",reason);std::exit(1);}
}
template<size_t N>
uint32_t Select(const std::array<uint32_t,N>& flags,uint32_t required,uint32_t mask=UINT32_MAX) {
    return lo::fsr::SelectMemoryType(mask,uint32_t(N),required,bits,[&](uint32_t i){return flags[i];});
}
}
int main() {
    Check(Select(std::array<uint32_t,2>{Visible|Coherent,Local},Visible|Local)==UINT32_MAX,
        "a union of different memory types cannot satisfy a compound request");
    Check(Select(std::array<uint32_t,2>{Visible|Coherent,Local},Visible|Local,2)==UINT32_MAX,
        "local-only memory must never be returned for vkMapMemory");
    Check(Select(std::array<uint32_t,2>{Visible|Coherent,Local},Visible)==0,
        "the SDK's existing host-visible fallback remains available");
    Check(Select(std::array<uint32_t,1>{Local|Visible|Coherent},Local)==0,
        "UMA local allocation accepts a host-visible local heap");
    Check(Select(std::array<uint32_t,1>{Local|Visible|Coherent},Local|Visible)==0,
        "UMA upload allocation remains local and mappable");
    Check(Select(std::array<uint32_t,3>{Local|Visible|Coherent,Local,Visible},Local)==1,
        "discrete allocation still prefers invisible local memory");
    Check(Select(std::array<uint32_t,3>{Local|Visible|Coherent,Local,Visible},Local,1)==0,
        "resource compatibility can require the visible local fallback");
    Check(Select(std::array<uint32_t,3>{Visible,Visible|Coherent,Local|Visible},Visible)==1,
        "coherent memory is preferred among qualifying upload types");
    Check(Select(std::array<uint32_t,3>{Visible|Coherent,Local|Visible,Local|Visible|Coherent},Local|Visible)==2,
        "coherency does not override missing required properties");
    Check(Select(std::array<uint32_t,2>{Local|Visible|Coherent|Disabled,Local|Visible},Local)==1,
        "disabled device-coherent feature is not selected");
    Check(Select(std::array<uint32_t,1>{Local|Disabled},Local)==UINT32_MAX,
        "no feature-legal type reports no match");
    Check(Select(std::array<uint32_t,1>{Local},Visible)==UINT32_MAX,"missing required flags report no match");
    Check(Select(std::array<uint32_t,1>{Local},Local,0)==UINT32_MAX,"empty resource mask reports no match");
    Check(Select(std::array<uint32_t,1>{0},0)==0,"zero required properties accepts compatible unflagged memory");
    std::array<uint32_t,32> many{}; many[31]=Local;
    Check(Select(many,Local,uint32_t(1)<<31)==31,"memory type 31 uses an unsigned bit shift");
    Check(lo::fsr::SelectMemoryType(UINT32_MAX,0,Local,bits,[](uint32_t){return Local;})==UINT32_MAX,
        "empty physical type table reports no match");
    std::printf("FSR Vulkan memory selection: %u checks passed (synthetic properties, CPU only)\n",checks);
}
