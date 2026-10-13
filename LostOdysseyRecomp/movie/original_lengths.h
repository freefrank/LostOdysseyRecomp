#pragma once
#include <cstdint>
#include <string_view>

namespace movie {
// Picture length of each original cutscene video in ms: the last video frame's
// time plus one frame (ffprobe of the --export-assets movies; the audio of some
// runs longer). A replacement within 1% of it is fitted to it.
struct OriginalLength {
    std::string_view key;
    uint32_t ms;
};
inline constexpr OriginalLength kOriginalLengths[] = {
    {"mv_00v", 124749},
    {"mv_01v", 196396},
    {"mv_02v", 120753},
    {"mv_04v", 130533},
    {"mv_05v", 4999},
    {"mv_06v", 38333},
    {"mv_07v", 5999},
    {"mv_08v", 16499},
    {"mv_09v", 43708},
    {"mv_10v", 60766},
    {"mv_11v", 25266},
    {"mv_12v", 22533},
    {"mv_13v", 20299},
    {"mv_14v", 12499},
    {"mv_15v", 54199},
    {"mv_16v", 36533},
    {"mv_17v", 14999},
    {"mv_18v", 76266},
    {"mv_19v", 5499},
    {"mv_20v", 110899},
    {"mv_21v", 63666},
    {"mv_23v", 89966},
    {"mv_24v", 2999},
    {"mv_25v", 15499},
    {"mv_26v", 5999},
    {"mv_27v", 39033},
    {"mv_28v", 5999},
    {"mv_29_1v", 5505},
    {"mv_31v", 25324},
    {"mv_32v", 35999},
    {"mv_33v", 76433},
    {"mv_34v", 15999},
    {"mv_35v", 77877},
    {"mv_36v", 7999},
    {"mv_37v", 27833},
    {"mv_38v", 19999},
    {"mv_39v", 23166},
    {"mv_40v", 12133},
    {"mv_41v", 15999},
    {"mv_42v", 15333},
    {"mv_43v", 17499},
    {"mv_44v", 38999},
    {"mv_45v", 40166},
    {"mv_46v", 10999},
    {"mv_47v", 74433},
    {"mv_48v", 9999},
    {"mv_49v", 55121},
    {"mv_50v", 36799},
    {"mv_52v", 14999},
    {"mv_53v", 15999},
    {"mv_54v", 38333},
    {"mv_55v", 31399},
    {"mv_56v", 74166},
    {"mv_57_1v", 4203},
    {"mv_57_2v", 3936},
    {"mv_57v", 137970},
    {"mv_58v", 12845},
    {"mv_59v", 69135},
    {"mv_60v", 13999},
    {"rt_006v", 56999},
    {"rt_013v", 60927},
    {"rt_132_d12", 4971},
    {"rt_140_d13", 6005},
    {"rt_165_d16", 6272},
    {"the_end_v", 11008},
};

inline uint32_t OriginalLengthMs(std::string_view key) {
    for (const auto& entry : kOriginalLengths)
        if (entry.key == key) return entry.ms;
    return 0;
}
}
