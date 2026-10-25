// Decodes an H.264 MP4 through the movie decoder and reports speed and per-frame MD5s.
//   LoMovieDecodeTest <mp4> [--frames N] [--software] [--framemd5 out.txt] [--packets out.txt] [--raw out.nv12]
//   LoMovieDecodeTest --mods <dir> --key <movie key> [...]   (a movie resolved from a mod folder or .loarc)
//   LoMovieDecodeTest --overlay-name <movie key>              (prints the overlay file name of a key)
// --framemd5 writes ffmpeg framemd5 lines over NV12 planes (Y rows, then interleaved UV rows, no padding).
#include <movie/movie_decoder.h>
#include <movie/mp4_demux.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
// Minimal RFC 1321 MD5.
class Md5 {
public:
    Md5() { Reset(); }
    void Reset() {
        a_ = 0x67452301; b_ = 0xefcdab89; c_ = 0x98badcfe; d_ = 0x10325476;
        length_ = 0;
        fill_ = 0;
    }
    void Update(const uint8_t* data, size_t size) {
        length_ += size;
        while (size) {
            const size_t n = std::min(size, sizeof(buffer_) - fill_);
            std::memcpy(buffer_ + fill_, data, n);
            fill_ += n; data += n; size -= n;
            if (fill_ == 64) { Block(buffer_); fill_ = 0; }
        }
    }
    std::string Finish() {
        const uint64_t bits = length_ * 8;
        const uint8_t pad = 0x80;
        Update(&pad, 1);
        const uint8_t zero = 0;
        while (fill_ != 56) Update(&zero, 1);
        uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = uint8_t(bits >> (8 * i));
        Update(len, 8);
        char out[33];
        const uint32_t words[4] = {a_, b_, c_, d_};
        for (int i = 0; i < 16; ++i) std::snprintf(out + i * 2, 3, "%02x", (words[i / 4] >> (8 * (i % 4))) & 0xff);
        return out;
    }
private:
    static uint32_t Rol(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }
    void Block(const uint8_t* p) {
        static const uint32_t K[64] = {
            0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
            0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
            0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
            0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
            0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
            0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
            0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
            0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
        static const int S[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                                  4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
        uint32_t m[16];
        for (int i = 0; i < 16; ++i) m[i] = uint32_t(p[i * 4]) | (uint32_t(p[i * 4 + 1]) << 8) | (uint32_t(p[i * 4 + 2]) << 16) | (uint32_t(p[i * 4 + 3]) << 24);
        uint32_t a = a_, b = b_, c = c_, d = d_;
        for (int i = 0; i < 64; ++i) {
            uint32_t f; int g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
            else { f = c ^ (b | ~d); g = (7 * i) & 15; }
            f = f + a + K[i] + m[g];
            a = d; d = c; c = b;
            b = b + Rol(f, S[i]);
        }
        a_ += a; b_ += b; c_ += c; d_ += d;
    }
    uint32_t a_, b_, c_, d_;
    uint64_t length_;
    uint8_t buffer_[64];
    size_t fill_;
};

std::string FrameMd5(const movie::Nv12Frame& f) {
    Md5 md5;
    // Rows without padding, in case a backend ever reports pitch != width.
    for (uint32_t y = 0; y < f.height; ++y) md5.Update(f.luma.data() + size_t(y) * f.lumaPitch, f.width);
    for (uint32_t y = 0; y < (f.height + 1) / 2; ++y) md5.Update(f.chroma.data() + size_t(y) * f.chromaPitch, (f.width + 1) & ~1u);
    return md5.Finish();
}
}

int main(int argc, char** argv) {
    std::string path, rawPath, md5Path, packetsPath, modsDir, key, overlayKey;
    bool software = false, rewindCheck = false;
    uint64_t maxFrames = ~0ull;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto value = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--frames") maxFrames = std::strtoull(value().c_str(), nullptr, 10);
        else if (a == "--software") software = true;
        else if (a == "--rewind-check") rewindCheck = true;
        else if (a == "--framemd5") md5Path = value();
        else if (a == "--packets") packetsPath = value();
        else if (a == "--raw") rawPath = value();
        else if (a == "--mods") modsDir = value();
        else if (a == "--key") key = value();
        else if (a == "--overlay-name") overlayKey = value();
        else if (!a.empty() && a[0] != '-') path = a;
        else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
    }
    if (!overlayKey.empty()) {
        std::cout << modding::OverlayRelativePath({modding::AssetKind::Movie, overlayKey}).generic_string() << "\n";
        return 0;
    }
    modding::ResolvedAsset asset;
    if (!modsDir.empty()) {
        modding::Initialize(modsDir);
        auto resolved = modding::Resolve({{modding::AssetKind::Movie, key}, {}});
        if (!resolved) { std::fprintf(stderr, "no movie '%s' in %s\n", key.c_str(), modsDir.c_str()); return 1; }
        asset = *resolved;
        std::printf("source: %s (offset %llu, size %llu)\n", modding::DescribeAsset(asset).c_str(),
                    static_cast<unsigned long long>(asset.offset), static_cast<unsigned long long>(asset.size));
    } else if (!path.empty()) {
        asset.path = std::filesystem::path(path);
    } else {
        std::fprintf(stderr, "usage: LoMovieDecodeTest <mp4> [--frames N] [--software] [--framemd5 out.txt] [--packets out.txt]\n");
        return 2;
    }

    std::string error;
    auto source = movie::OpenByteSource(asset, &error);
    if (!source) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
    movie::Mp4Track track;
    if (!movie::ParseMp4(*source, track, error)) { std::fprintf(stderr, "demux: %s\n", error.c_str()); return 1; }
    for (const auto& w : track.warnings) std::printf("warning: %s\n", w.c_str());
    const double fps = track.frameDuration ? double(track.timescale) / track.frameDuration : 0.0;
    std::printf("track: %ux%u (coded %ux%u) profile %u level %u timescale %u duration %llu samples %u frame duration %u (%.3f fps) nal length %u\n",
                track.width, track.height, track.codedWidth, track.codedHeight, track.profile, track.level, track.timescale,
                static_cast<unsigned long long>(track.duration), track.frameCount, track.frameDuration, fps, track.nalLengthSize);
    if (!packetsPath.empty()) {
        std::ofstream out(packetsPath);
        out << "index,dts,pts,duration,size,key\n";
        for (size_t i = 0; i < track.samples.size(); ++i) {
            const auto& s = track.samples[i];
            out << i << ',' << s.dts << ',' << s.pts << ',' << s.duration << ',' << s.size << ',' << (s.keyframe ? 1 : 0) << '\n';
        }
    }

    auto decoder = movie::CreateMovieDecoder(!software);
    if (!decoder) { std::fprintf(stderr, "no movie decoder on this platform\n"); return 1; }
    const auto openStart = std::chrono::steady_clock::now();
    if (!decoder->Open(track, *source, error)) { std::fprintf(stderr, "open: %s\n", error.c_str()); return 1; }
    const double openMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - openStart).count();

    std::ofstream md5Out;
    if (!md5Path.empty()) {
        md5Out.open(md5Path);
        md5Out << "#format: frame checksums\n#version: 2\n#hash: MD5\n#tb 0: 1/" << track.timescale << "\n#media_type 0: video\n"
               << "#codec_id 0: rawvideo\n#dimensions 0: " << track.width << "x" << track.height << "\n#sar 0: 1/1\n"
               << "#stream#, dts,        pts, duration,     size, hash\n";
    }
    std::ofstream rawOut;
    if (!rawPath.empty()) rawOut.open(rawPath, std::ios::binary);
    movie::Nv12Frame frame;
    uint64_t frames = 0;
    double total = 0, worst = 0, first = 0;
    std::vector<double> times;
    std::vector<std::string> firstPass;
    int64_t lastPts = INT64_MIN;
    bool ordered = true;
    const auto wall = std::chrono::steady_clock::now();
    while (frames < maxFrames) {
        const auto t0 = std::chrono::steady_clock::now();
        const bool ok = decoder->Next(frame, error);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (!ok) break;
        if (frames == 0) first = ms; else { total += ms; worst = std::max(worst, ms); times.push_back(ms); }
        if (frame.pts <= lastPts) ordered = false;
        lastPts = frame.pts;
        if (rewindCheck) firstPass.push_back(FrameMd5(frame));
        if (md5Out) {
            const size_t bytes = size_t(frame.width) * frame.height + size_t((frame.width + 1) & ~1u) * ((frame.height + 1) / 2);
            char head[96];
            std::snprintf(head, sizeof(head), "0, %10lld, %10lld, %8u, %8zu, ", static_cast<long long>(frame.pts),
                          static_cast<long long>(frame.pts), track.frameDuration, bytes);
            md5Out << head << FrameMd5(frame) << "\n";
        }
        if (rawOut) {
            for (uint32_t y = 0; y < frame.height; ++y) rawOut.write(reinterpret_cast<const char*>(frame.luma.data() + size_t(y) * frame.lumaPitch), frame.width);
            for (uint32_t y = 0; y < (frame.height + 1) / 2; ++y) rawOut.write(reinterpret_cast<const char*>(frame.chroma.data() + size_t(y) * frame.chromaPitch), (frame.width + 1) & ~1u);
        }
        ++frames;
    }
    const double wallMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wall).count();
    if (!error.empty()) { std::fprintf(stderr, "decode error after %llu frames: %s\n", static_cast<unsigned long long>(frames), error.c_str()); return 1; }
    if (rewindCheck) {
        decoder->Rewind();
        size_t i = 0;
        bool same = true;
        while (i < firstPass.size() && decoder->Next(frame, error)) same = same && FrameMd5(frame) == firstPass[i++];
        std::printf("rewind: %zu of %zu frames again, %s\n", i, firstPass.size(), same && i == firstPass.size() ? "identical" : "DIFFERENT");
        if (!same || i != firstPass.size()) return 1;
    }
    std::sort(times.begin(), times.end());
    const double p95 = times.empty() ? 0.0 : times[size_t(double(times.size() - 1) * 0.95)];
    std::printf("decoder: %s, acceleration: %s, open %.1f ms\n", decoder->Name(), decoder->AccelerationName(), openMs);
    std::printf("frames: %llu, output %ux%u, pts %s\n", static_cast<unsigned long long>(frames), frame.width, frame.height,
                ordered ? "strictly increasing" : "NOT increasing");
    if (frames > 1) {
        std::printf("decode ms/frame: first %.2f, average %.2f (excluding first), p95 %.2f, worst %.2f, wall %.0f ms (%.1f fps)\n", first,
                    total / double(frames - 1), p95, worst, wallMs, double(frames) * 1000.0 / wallMs);
    }
    if (maxFrames == ~0ull && frames != track.frameCount) {
        std::fprintf(stderr, "decoded %llu frames, the file has %u\n",static_cast<unsigned long long>(frames), track.frameCount);
        return 1;
    }
    if (!ordered) return 1;
    return 0;
}
