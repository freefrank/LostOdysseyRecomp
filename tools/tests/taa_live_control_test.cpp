#include "gpu/taa_live_control.h"
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
    using namespace gpu::temporal;
    LiveOptions state;std::string error;
    auto parse=[&](const char* text){std::istringstream input(text);return ReadLiveOptions(input,state,error);};
    auto require=[](bool ok){if(!ok)throw std::runtime_error("live-control assertion failed");};
    if(argc==2&&std::string(argv[1])=="--stationary-weight-range") {
        require(parse("serial=1\nstationary_weight=0.995\n"));require(state.stationary_weight==.995f);
        require(!parse("serial=2\nstationary_weight=0.996\n"));require(state.serial==1);
        require(!parse("serial=3\nhistory_weight=0.996\n"));require(state.serial==1);
        std::cout<<"PASS: stationary weight diagnostic range\n";return 0;
    }
    if(argc==2&&std::string(argv[1])=="--history-fp16") {
        require(parse("serial=1\n"));require(state.history_fp16==0);
        require(parse("serial=2\nhistory_fp16=1\n"));require(state.history_fp16==1);
        require(!parse("serial=3\nhistory_fp16=2\n"));require(state.serial==2&&state.history_fp16==1);
        std::cout<<"PASS: history_fp16 parser\n";return 0;
    }
    if(argc==2&&std::string(argv[1])=="--stationary-color-clip") {
        require(parse("serial=1\n"));require(state.stationary_color_clip==0);
        require(parse("serial=2\nstationary_color_clip=1\n"));require(state.stationary_color_clip==1);
        require(!parse("serial=3\nstationary_color_clip=2\n"));require(state.serial==2&&state.stationary_color_clip==1);
        std::cout<<"PASS: stationary_color_clip parser\n";return 0;
    }
    if(argc==2&&std::string(argv[1])=="--stationary-multi-surface") {
        require(parse("serial=1\n"));require(state.stationary_multi_surface==0);
        require(parse("serial=2\nstationary_multi_surface=1\n"));require(state.stationary_multi_surface==1);
        require(!parse("serial=3\nstationary_multi_surface=2\n"));require(state.serial==2&&state.stationary_multi_surface==1);
        std::cout<<"PASS: stationary_multi_surface parser\n";return 0;
    }
    if(argc==2&&std::string(argv[1])=="--moving-bilinear-fallback") {
        require(parse("serial=1\n"));require(state.moving_bilinear_fallback==0);
        require(parse("serial=2\nmoving_bilinear_fallback=1\n"));require(state.moving_bilinear_fallback==1);
        require(!parse("serial=3\nmoving_bilinear_fallback=2\n"));require(state.serial==2&&state.moving_bilinear_fallback==1);
        std::cout<<"PASS: moving_bilinear_fallback parser\n";return 0;
    }
    if(argc==2&&std::string(argv[1])=="--jitter-scale-timing") {
        require(parse("serial=1\n"));require(state.jitter_scale==1.f&&state.gpu_timing==0);
        require(parse("serial=2\njitter_scale=0.5\ngpu_timing=1\n"));
        require(state.jitter_scale==.5f&&state.gpu_timing==1);
        require(!parse("serial=3\njitter_scale=1.1\n"));require(state.serial==2);
        require(!parse("serial=3\ngpu_timing=2\n"));require(state.serial==2);
        std::cout<<"PASS: jitter scale and GPU timing controls\n";return 0;
    }
    require(parse("serial=12\nhistory_weight=0.9\nacceptance=2\nsnap_stationary=1\n"));
    require(state.serial==12&&state.history_weight==.9f&&state.acceptance==2&&state.snap_stationary==1);
    for(auto invalid:{"serial=-1", "serial=1\nhistory_weight=nan", "serial=1\nunknown=1", "serial=1\nserial=2",
                     "serial=1\nmotion_min=.5\nmotion_max=.1", "serial=1\ncoverage=2", "serial=1\naa=1.5", "serial=1\nhistory_weight=.99"}) {
        require(!parse(invalid));require(state.serial==12&&state.history_weight==.9f);
    }
    require(parse("serial=13\naa=2\ncoverage=0\n"));require(state.aa==2&&state.coverage==0&&state.history_weight==.85f);
    require(LiveJsonString("a\"b\\c\n")=="\"a\\\"b\\\\c \"");
    std::cout<<"PASS: atomic live control parsing, ranges, serials and JSON escaping\n";
}
