#pragma once
#include <cmath>
#include <cstdint>
#include <istream>
#include <locale>
#include <sstream>
#include <string>
#include <unordered_set>

namespace gpu::temporal {
struct LiveOptions {
    uint64_t serial=0;
    int aa=3,jitter=1,history=1,bloom=-1,hdr=0,materials=1;
    int stationary=1,coverage=1,acceptance=0,mv_debug=0,mv_consume=1,snap_stationary=0,stationary_color_clip=0,history_fp16=0,stationary_multi_surface=0,moving_bilinear_fallback=0,gpu_timing=0;
    float jitter_scale=1.f;
    float history_weight=.85f,stationary_weight=31.f/33.f,motion_min=.002f,motion_max=.125f;
    float depth_absolute=1e-5f,depth_relative=.01f;
};
// Accepted 4K Uhra policy for the normal TAA path. Keep the live-control
// parser's baseline defaults separate so diagnostic snapshots stay explicit.
inline LiveOptions MainTaaOptions() {
    LiveOptions value;
    value.jitter_scale=.5f;
    value.snap_stationary=1;
    value.stationary_color_clip=1;
    value.stationary_multi_surface=1;
    return value;
}
// A complete snapshot is applied at a render-frame boundary. Reject a malformed
// request as a whole; never mix a partial write with the previous configuration.
inline bool ReadLiveOptions(std::istream& input,LiveOptions& output,std::string& error) {
    LiveOptions value;std::string line;std::unordered_set<std::string> seen;size_t bytes=0;
    auto bad=[&](const char* message){error=message;return false;};
    while(std::getline(input,line)) {
        bytes+=line.size();if(bytes>8192)return bad("Control request exceeds 8192 bytes");
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty())continue;
        auto equals=line.find('=');if(equals==std::string::npos)return bad("Expected key=value");
        auto key=line.substr(0,equals),text=line.substr(equals+1);
        if(!seen.insert(key).second)return bad("Duplicate control field");
        std::istringstream number(text);number.imbue(std::locale::classic());
        auto read=[&](auto& target){return bool(number>>target)&&number.peek()==std::char_traits<char>::eof();};
        bool ok=false;
        if(key=="serial") {if(text.empty()||text.find_first_not_of("0123456789")!=std::string::npos)return bad("Invalid serial");ok=read(value.serial);}
#define LO_LIVE_FIELD(name) else if(key==#name)ok=read(value.name)
        LO_LIVE_FIELD(aa);LO_LIVE_FIELD(jitter);LO_LIVE_FIELD(history);LO_LIVE_FIELD(bloom);
        LO_LIVE_FIELD(hdr);LO_LIVE_FIELD(materials);LO_LIVE_FIELD(stationary);LO_LIVE_FIELD(coverage);
        LO_LIVE_FIELD(acceptance);LO_LIVE_FIELD(mv_debug);LO_LIVE_FIELD(mv_consume);LO_LIVE_FIELD(snap_stationary);
        LO_LIVE_FIELD(stationary_color_clip);LO_LIVE_FIELD(history_fp16);LO_LIVE_FIELD(stationary_multi_surface);LO_LIVE_FIELD(moving_bilinear_fallback);LO_LIVE_FIELD(gpu_timing);
        LO_LIVE_FIELD(jitter_scale);
        LO_LIVE_FIELD(history_weight);LO_LIVE_FIELD(stationary_weight);LO_LIVE_FIELD(motion_min);LO_LIVE_FIELD(motion_max);
        LO_LIVE_FIELD(depth_absolute);LO_LIVE_FIELD(depth_relative);
#undef LO_LIVE_FIELD
        else return bad("Unknown control field");
        if(!ok)return bad("Invalid control value");
    }
    auto range=[](float v,float lo,float hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
    if(!value.serial||value.aa< -1||value.aa>3||value.jitter< -1||value.jitter>1||value.history< -1||value.history>1||value.bloom< -1||value.bloom>1)
        return bad("Invalid serial or rendering mode");
    for(int flag:{value.hdr,value.materials,value.stationary,value.coverage,value.mv_debug,value.mv_consume,value.snap_stationary,value.stationary_color_clip,value.history_fp16,value.stationary_multi_surface,value.moving_bilinear_fallback,value.gpu_timing})
        if(flag<0||flag>1)return bad("Invalid boolean control");
    if(value.acceptance<0||value.acceptance>2||!range(value.jitter_scale,0,1)||!range(value.history_weight,0,.95f)||!range(value.stationary_weight,0,.995f)
       ||!range(value.motion_min,0,16)||!range(value.motion_max,0,16)||value.motion_max<=value.motion_min
       ||!range(value.depth_absolute,0,1)||!range(value.depth_relative,0,1))return bad("Invalid temporal policy range");
    output=value;error.clear();return true;
}
inline std::string LiveJsonString(const std::string& value) {
    std::string result="\"";
    for(unsigned char c:value) {if(c=='"'||c=='\\'){result+='\\';result+=char(c);}else if(c>=32)result+=char(c);else result+=' ';}
    return result+'"';
}
}
