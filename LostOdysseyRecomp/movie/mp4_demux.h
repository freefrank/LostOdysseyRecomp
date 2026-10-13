#pragma once
#include "byte_source.h"
#include <cstdint>
#include <string>
#include <vector>

namespace movie {
// One coded video frame in the file. Times are in the track timescale and already
// include the edit list shift, so pts can be negative for frames before the first
// presented one.
struct Mp4Sample {
    uint64_t offset = 0;  // in the ByteSource
    uint32_t size = 0;
    int64_t dts = 0;
    int64_t pts = 0;      // dts + composition offset
    uint32_t duration = 0;
    bool keyframe = false;
};

struct Mp4Track {
    uint32_t width = 0, height = 0;           // display size (SPS cropping applied)
    uint32_t codedWidth = 0, codedHeight = 0; // macroblock aligned, as decoded
    uint32_t cropLeft = 0, cropTop = 0;       // pixels
    uint32_t timescale = 0;
    uint64_t duration = 0;                    // media duration in timescale units
    uint32_t frameCount = 0;
    uint32_t frameDuration = 0;               // the one stts delta, or 0 when it varies
    uint8_t profile = 0, level = 0;
    uint8_t chromaFormat = 1;                 // SPS chroma_format_idc (1 = 4:2:0)
    uint8_t bitDepth = 8;
    uint8_t nalLengthSize = 4;                // bytes of each NAL length prefix in a sample
    bool avc3 = false;                        // parameter sets may also be in-band
    std::vector<uint8_t> avcC;                // AVCDecoderConfigurationRecord payload
    std::vector<std::vector<uint8_t>> sps, pps;
    std::vector<Mp4Sample> samples;           // decode order
    std::vector<std::string> warnings;
};

// Parses the first H.264 video track of an MP4/MOV. False and *error on any
// malformed or unsupported input; never throws.
bool ParseMp4(ByteSource& source, Mp4Track& track, std::string& error);
}
