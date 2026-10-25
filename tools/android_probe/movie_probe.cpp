#include "movie_probe.h"
#include <movie/movie_decoder.h>
#include <algorithm>
#include <chrono>
#include <sstream>

namespace lo::android_probe {
std::string ProbeMovie(const std::string& path, bool software) {
    using Clock = std::chrono::steady_clock;
    std::ostringstream report;
    report << "Movie decoder probe (no rendering or audio)\n";
    std::string error;
    modding::ResolvedAsset asset;
    asset.path = path;
    auto source = movie::OpenByteSource(asset, &error);
    movie::Mp4Track track;
    if (!source || !movie::ParseMp4(*source, track, error)) return report.str() + "FAIL: " + error + "\n";
    auto decoder = movie::CreateMovieDecoder(!software);
    const auto begin = Clock::now();
    if (!decoder || !decoder->Open(track, *source, error)) return report.str() + "FAIL: " + error + "\n";
    const double openMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    movie::Nv12Frame frame, first;
    std::vector<double> timings;
    int64_t lastPts = INT64_MIN;
    bool ordered = true;
    while (true) {
        const auto start = Clock::now();
        if (!decoder->Next(frame, error)) break;
        timings.push_back(std::chrono::duration<double, std::milli>(Clock::now() - start).count());
        if (timings.size() == 1) first = frame;
        if (frame.pts <= lastPts) ordered = false;
        lastPts = frame.pts;
    }
    report << decoder->Name() << ", " << decoder->AccelerationName() << "\n";
    report << "Frames: " << timings.size() << "/" << track.frameCount << "; ordered PTS: " << ordered
           << "; display: " << frame.width << "x" << frame.height << "; open ms: " << openMs << "\n";
    if (!error.empty()) return report.str() + "FAIL: " + error + "\n";
    if (timings.empty() || timings.size() != track.frameCount || !ordered) return report.str() + "FAIL: frame count/order\n";
    std::sort(timings.begin(), timings.end());
    report << "Decode/conversion p95 ms: " << timings[(timings.size()-1)*95/100] << "; worst ms: " << timings.back() << "\n";
    decoder->Rewind();
    if (!decoder->Next(frame, error) || frame.pts != first.pts || frame.width != first.width || frame.height != first.height ||
        frame.luma != first.luma || frame.chroma != first.chroma)
        return report.str() + "FAIL: rewind first picture " + error + "\n";
    report << "PASS: decode count, PTS order, EOS and first-picture rewind.\n"
           << "Displayed cadence, A/V sync, game playback and hardware throughput require separate tests.\n";
    return report.str();
}
}
