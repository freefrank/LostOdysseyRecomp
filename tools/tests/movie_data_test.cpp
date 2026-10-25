#include <movie/h264_sample.h>
#include <movie/yuv420_frame.h>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>

namespace {
class MemorySource final : public movie::ByteSource {
public:
    std::vector<uint8_t> bytes;
    uint64_t Size() const override { return bytes.size(); }
    bool ReadAt(uint64_t offset, void* out, size_t size) override {
        if (offset > bytes.size() || size > bytes.size() - offset) return false;
        std::memcpy(out, bytes.data() + offset, size);
        return true;
    }
};

void NalSamples() {
    for (uint8_t prefix = 1; prefix <= 4; ++prefix) {
        std::vector<uint8_t> raw(prefix, 0), out;
        raw.back() = 2;
        raw.insert(raw.end(), {0x65, 0x11});
        raw.insert(raw.end(), prefix, 0);
        raw.back() = 1;
        raw.push_back(0x06);
        std::string error;
        movie::AppendH264ParameterSets({{0x67, 0x42}, {0x68}}, out);
        assert(movie::AppendH264Sample(raw, prefix, out, error));
        assert((out == std::vector<uint8_t>{0,0,0,1,0x67,0x42,0,0,0,1,0x68,0,0,0,1,0x65,0x11,0,0,0,1,0x06}));
        raw.pop_back();
        assert(!movie::AppendH264Sample(raw, prefix, out, error));
        assert(!error.empty());
    }
    std::string error;
    std::vector<uint8_t> out;
    assert(!movie::AppendH264Sample({}, 0, out, error));
    assert(!movie::AppendH264Sample({}, 5, out, error));
    assert(!movie::AppendH264Sample(std::vector<uint8_t>{0, 0}, 4, out, error));
}

void YuvPlanes() {
    // Padded 8x6 source, 4x4 display crop at (2,2). The last row's padding
    // deliberately does not exist; NV21 Cb/Cr views overlap by one byte.
    for (uint32_t pixel : {1u, 2u, 3u}) {
        std::vector<uint8_t> y(12 * 5 + 8), cb(6 * pixel * 2 + 3 * pixel + 1), cr(cb.size());
        for (unsigned row = 0; row < 6; ++row)
            for (unsigned x = 0; x < 8; ++x) y[row * 12 + x] = uint8_t(row * 8 + x);
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned x = 0; x < 4; ++x) {
                cb[row * 6 * pixel + x * pixel] = uint8_t(80 + row * 4 + x);
                cr[row * 6 * pixel + x * pixel] = uint8_t(160 + row * 4 + x);
            }
        movie::Yuv420Plane planes[3]{{y, 12, 1}, {cb, 6 * pixel, pixel}, {cr, 6 * pixel, pixel}};
        movie::Nv12Frame frame;
        std::string error;
        assert(movie::CopyYuv420Frame(planes, 8, 6, 2, 2, 4, 4, frame, error));
        assert(frame.width == 4 && frame.height == 4 && frame.lumaPitch == 4 && frame.chromaPitch == 4);
        assert((frame.luma == std::vector<uint8_t>{18,19,20,21,26,27,28,29,34,35,36,37,42,43,44,45}));
        assert((frame.chroma == std::vector<uint8_t>{85,165,86,166,89,169,90,170}));
        assert(!movie::CopyYuv420Frame(planes, 8, 6, 1, 2, 4, 4, frame, error));
        assert(!movie::CopyYuv420Frame(planes, 8, 6, 2, 3, 4, 2, frame, error));
        assert(!movie::CopyYuv420Frame(planes, 8, 6, 6, 2, 4, 4, frame, error));
        planes[2].bytes = planes[2].bytes.first(1);
        assert(!movie::CopyYuv420Frame(planes, 8, 6, 2, 2, 4, 4, frame, error));
    }
    std::vector<uint8_t> y(8 * 6), vu(8 * 2 + 7);
    for (size_t i = 0; i < y.size(); ++i) y[i] = uint8_t(i);
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned x = 0; x < 4; ++x) {
            vu[row * 8 + x * 2] = uint8_t(160 + row * 4 + x);
            // The last Cb sample is not needed by the cropped image.
            if (row * 8 + x * 2 + 1 < vu.size()) vu[row * 8 + x * 2 + 1] = uint8_t(80 + row * 4 + x);
        }
    movie::Yuv420Plane planes[3]{{y, 8, 1}, {std::span(vu).subspan(1), 8, 2}, {vu, 8, 2}};
    movie::Nv12Frame frame;
    std::string error;
    assert(movie::CopyYuv420Frame(planes, 8, 6, 2, 2, 4, 4, frame, error));
    assert((frame.chroma == std::vector<uint8_t>{85,165,86,166,89,169,90,170}));
    // Odd display dimensions are supported when the crop origin is even.
    assert(movie::CopyYuv420Frame(planes, 8, 6, 0, 0, 3, 3, frame, error));
    assert(frame.luma.size() == 9 && frame.chroma.size() == 8);
    planes[0].rowStride = 0;
    assert(!movie::CopyYuv420Frame(planes, 8, 6, 0, 0, 3, 3, frame, error));
}

void Demux(const char* path, movie::Mp4Track& track) {
    MemorySource source;
    std::ifstream file(path, std::ios::binary);
    assert(file);
    source.bytes.assign(std::istreambuf_iterator<char>(file), {});
    std::string error;
    assert(movie::ParseMp4(source, track, error));
    assert(!track.samples.empty() && track.width == 64 && track.height == 64);
    std::vector<uint8_t> raw, annexb;
    for (const auto& s : track.samples) {
        raw.resize(s.size);
        assert(source.ReadAt(s.offset, raw.data(), raw.size()));
        annexb.clear();
        assert(movie::AppendH264Sample(raw, track.nalLengthSize, annexb, error));
        assert(annexb.size() >= 4 && annexb[0] == 0 && annexb[1] == 0 && annexb[2] == 0 && annexb[3] == 1);
    }
}
}

int main(int argc, char** argv) {
    assert(argc == 1 || argc == 3);
    NalSamples();
    YuvPlanes();
    movie::Mp4Track plain, fragmented;
    Demux(argc == 3 ? argv[1] : "data/testsrc-64x64-bframes.mp4", plain);
    Demux(argc == 3 ? argv[2] : "data/testsrc-64x64-bframes-frag.mp4", fragmented);
    assert(plain.frameCount == fragmented.frameCount);
    for (size_t i = 0; i < plain.samples.size(); ++i) {
        const auto& a = plain.samples[i]; const auto& b = fragmented.samples[i];
        assert(a.pts == b.pts && a.dts == b.dts && a.duration == b.duration && a.size == b.size && a.keyframe == b.keyframe);
    }
    std::cout << "movie data: Annex B, cropped/strided YUV420 and " << plain.frameCount << " plain/fragmented samples passed\n";
}
