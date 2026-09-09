#pragma once
#include "shader/position_evidence.h"
#include <iomanip>
#include <sstream>

namespace gpu::taa_collection {
inline constexpr const char* ShaderBuild="0.5.0-position-evidence-1";
// Caller passes the existing allowlisted device strings; no arbitrary text.
inline std::string RequestStart(std::string_view backend,std::string_view gpu,std::string_view driver) {
    return "{\"schema\":2,\"build\":\""+std::string(ShaderBuild)+"\",\"backend\":\""+std::string(backend)+
        "\",\"gpu\":\""+std::string(gpu)+"\",\"driver\":\""+std::string(driver)+"\",\"records\":[";
}
inline std::string RecordJson(uint64_t vs,uint64_t ps,uint32_t width,uint32_t height,int slot,
    uint32_t candidates,uint32_t flags,uint32_t rejection,uint32_t draws,
    const position_evidence::Summary& p,uint32_t guards) {
    std::ostringstream out;
    out<<"{\"vs\":\""<<std::hex<<std::setfill('0')<<std::setw(16)<<vs<<"\",\"ps\":\""<<std::setw(16)<<ps<<std::dec<<
        "\",\"width\":"<<width<<",\"height\":"<<height<<",\"slot\":"<<slot<<",\"candidates\":"<<candidates<<
        ",\"flags\":"<<flags<<",\"rejection\":"<<rejection<<",\"draws\":"<<draws<<
        ",\"position\":{\"version\":"<<p.version<<",\"kind\":"<<p.kind<<",\"slot\":"<<p.slot<<
        ",\"issues\":"<<p.issues<<",\"outputs\":"<<p.outputs<<"},\"guards\":"<<guards<<'}';
    return out.str();
}
}
