#pragma once
#include "byte_source.h"
#include "mp4_demux.h"
#include <cstdint>
#include <memory>
#include <atomic>
#include <string>
#include <vector>

namespace movie {
// One decoded picture, cropped to the display size, planes without row padding.
// Luma is width x height; chroma is interleaved UV, (width + 1) / 2 pairs per row,
// (height + 1) / 2 rows (lumaPitch == width, chromaPitch == width rounded up to even).
struct Nv12Frame {
    uint32_t width = 0, height = 0, lumaPitch = 0, chromaPitch = 0;
    std::vector<uint8_t> luma, chroma;
    int64_t pts = 0; // track timescale
};

// Decodes the samples of one Mp4Track in display order. The track and the source
// must outlive the decoder. One thread at a time.
class MovieDecoder {
public:
    virtual ~MovieDecoder();
    virtual bool Open(const Mp4Track& track, ByteSource& source, std::string& error) = 0;
    // False at the end of the stream (error empty) or on failure (error set).
    virtual bool Next(Nv12Frame& frame, std::string& error) = 0;
    // Back to the first sample; Next then starts over.
    virtual void Rewind() = 0;
    virtual const char* Name() const = 0;
    // True when frames come out of a GPU video decoder (valid after the first Next).
    virtual bool Hardware() const = 0;
    // Older Android versions cannot reliably classify vendor codecs.
    virtual const char* AccelerationName() const { return Hardware() ? "hardware" : "software"; }
    // Set before Open; the flag must outlive this decoder. Only the flag is
    // touched by other threads: codec calls remain on the decoding thread.
    void SetStopFlag(const std::atomic<bool>* flag) { stopFlag_ = flag; }
protected:
    bool StopRequested() const { return stopFlag_ && stopFlag_->load(std::memory_order_relaxed); }
private:
    const std::atomic<bool>* stopFlag_ = nullptr;
};

// Null when this platform has no backend. preferHardware false forces software.
std::unique_ptr<MovieDecoder> CreateMovieDecoder(bool preferHardware);
}
