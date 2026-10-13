#include "mp4_demux.h"
#include <algorithm>
#include <cstring>
#include <exception>

namespace movie {
namespace {
constexpr uint32_t kMaxSamples = 1u << 24;
constexpr uint32_t kMaxSampleBytes = 128u << 20;
constexpr uint64_t kMaxMoovBytes = 256u << 20;
constexpr uint64_t kMaxMoofBytes = 16u << 20;
constexpr uint32_t kMaxDimension = 16384;

constexpr uint32_t Fourcc(char a, char b, char c, char d) {
    return (uint32_t(uint8_t(a)) << 24) | (uint32_t(uint8_t(b)) << 16) | (uint32_t(uint8_t(c)) << 8) | uint32_t(uint8_t(d));
}

struct Fail {
    std::string message;
};

struct Span {
    const uint8_t* data = nullptr;
    size_t size = 0;
};

// Big-endian reader over a span; every read is bounds checked.
class Reader {
public:
    explicit Reader(Span span) : span_(span) {}
    size_t Left() const { return span_.size - pos_; }
    size_t Pos() const { return pos_; }
    uint8_t U8() { Need(1); return span_.data[pos_++]; }
    uint16_t U16() { Need(2); const uint16_t v = uint16_t((span_.data[pos_] << 8) | span_.data[pos_ + 1]); pos_ += 2; return v; }
    uint32_t U32() {
        Need(4);
        const auto* p = span_.data + pos_;
        pos_ += 4;
        return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
    }
    uint64_t U64() { const uint64_t hi = U32(); return (hi << 32) | U32(); }
    void Skip(size_t n) { Need(n); pos_ += n; }
    Span Take(size_t n) { Need(n); Span s{span_.data + pos_, n}; pos_ += n; return s; }
    Span Rest() { return Take(Left()); }
    void Need(size_t n) const {
        if (n > Left()) throw Fail{"truncated box"};
    }
private:
    Span span_;
    size_t pos_ = 0;
};

struct Box {
    uint32_t type = 0;
    Span payload;
};

// Next child box of a span of boxes. False at the end.
bool NextBox(Reader& r, Box& box) {
    if (r.Left() == 0) return false;
    if (r.Left() < 8) throw Fail{"truncated box header"};
    uint64_t size = r.U32();
    box.type = r.U32();
    size_t header = 8;
    if (size == 1) {
        size = r.U64();
        header = 16;
    } else if (size == 0) {
        size = r.Left() + header;
    }
    if (size < header || size - header > r.Left()) throw Fail{"box extends past its parent"};
    box.payload = r.Take(size_t(size - header));
    return true;
}

// Full-box version and flags.
uint8_t Version(Reader& r) {
    const uint32_t vf = r.U32();
    return uint8_t(vf >> 24);
}

// Entry count whose entries of entrySize bytes must still fit in the reader.
uint32_t Count(Reader& r, size_t entrySize, uint32_t limit = kMaxSamples) {
    const uint32_t n = r.U32();
    if (n > limit) throw Fail{"absurd entry count"};
    if (entrySize && uint64_t(n) * entrySize > r.Left()) throw Fail{"table extends past its box"};
    return n;
}

class BitReader {
public:
    explicit BitReader(const std::vector<uint8_t>& d) : d_(d) {}
    uint32_t Bit() {
        if (pos_ >= d_.size() * 8) throw Fail{"truncated SPS"};
        const uint32_t b = (d_[pos_ >> 3] >> (7 - (pos_ & 7))) & 1;
        ++pos_;
        return b;
    }
    uint32_t Bits(unsigned n) { uint32_t v = 0; while (n--) v = (v << 1) | Bit(); return v; }
    uint32_t Ue() {
        unsigned zeros = 0;
        while (Bit() == 0) if (++zeros > 31) throw Fail{"bad SPS exp-golomb code"};
        return zeros ? ((1u << zeros) - 1 + Bits(zeros)) : 0;
    }
    int32_t Se() {
        const uint32_t k = Ue();
        return (k & 1) ? int32_t((k + 1) >> 1) : -int32_t(k >> 1);
    }
private:
    const std::vector<uint8_t>& d_;
    size_t pos_ = 0;
};

// Display size, bit depth and chroma format of an SPS NAL (with its header byte).
void ParseSps(const std::vector<uint8_t>& nal, Mp4Track& t) {
    if (nal.size() < 5) throw Fail{"SPS too short"};
    std::vector<uint8_t> rbsp;
    rbsp.reserve(nal.size());
    int zeros = 0;
    for (size_t i = 1; i < nal.size(); ++i) {
        if (zeros >= 2 && nal[i] == 3) { zeros = 0; continue; }
        zeros = nal[i] == 0 ? zeros + 1 : 0;
        rbsp.push_back(nal[i]);
    }
    BitReader b(rbsp);
    const uint32_t profile = b.Bits(8);
    b.Bits(16);
    b.Ue();
    uint32_t chroma = 1, depth = 8;
    switch (profile) {
    case 100: case 110: case 122: case 244: case 44: case 83: case 86: case 118: case 128: case 138: case 139: case 134: case 135: {
        chroma = b.Ue();
        if (chroma > 3) throw Fail{"bad SPS chroma format"};
        if (chroma == 3) b.Bit();
        depth = b.Ue() + 8;
        b.Ue();
        b.Bit();
        if (b.Bit()) {
            for (unsigned i = 0; i < (chroma != 3 ? 8u : 12u); ++i) {
                if (!b.Bit()) continue;
                int32_t last = 8, next = 8;
                for (unsigned j = 0; j < (i < 6 ? 16u : 64u); ++j) {
                    if (next != 0) next = (last + b.Se() + 256) % 256;
                    last = next == 0 ? last : next;
                }
            }
        }
        break;
    }
    default: break;
    }
    b.Ue();
    const uint32_t pocType = b.Ue();
    if (pocType == 0) {
        b.Ue();
    } else if (pocType == 1) {
        b.Bit(); b.Se(); b.Se();
        const uint32_t n = b.Ue();
        if (n > 255) throw Fail{"bad SPS"};
        for (uint32_t i = 0; i < n; ++i) b.Se();
    }
    b.Ue();
    b.Bit();
    const uint32_t mbW = b.Ue() + 1, mbH = b.Ue() + 1;
    const uint32_t frameMbsOnly = b.Bit();
    if (!frameMbsOnly) b.Bit();
    b.Bit();
    uint32_t cl = 0, cr = 0, ct = 0, cb = 0;
    if (b.Bit()) { cl = b.Ue(); cr = b.Ue(); ct = b.Ue(); cb = b.Ue(); }
    if (mbW > kMaxDimension / 16 || mbH > kMaxDimension / 16) throw Fail{"SPS size out of range"};
    const uint32_t unitX = (chroma == 1 || chroma == 2) ? 2 : 1;
    const uint32_t unitY = (chroma == 1 ? 2 : 1) * (2 - frameMbsOnly);
    t.codedWidth = mbW * 16;
    t.codedHeight = mbH * 16 * (2 - frameMbsOnly);
    const uint64_t cropW = uint64_t(cl + cr) * unitX, cropH = uint64_t(ct + cb) * unitY;
    if (cropW >= t.codedWidth || cropH >= t.codedHeight) throw Fail{"SPS cropping exceeds the picture"};
    t.width = uint32_t(t.codedWidth - cropW);
    t.height = uint32_t(t.codedHeight - cropH);
    t.cropLeft = cl * unitX;
    t.cropTop = ct * unitY;
    t.chromaFormat = uint8_t(chroma);
    t.bitDepth = uint8_t(depth);
}

struct Edit {
    uint64_t segment = 0;
    int64_t mediaTime = 0;
    int16_t rate = 1;
};

struct Tables {
    std::vector<uint32_t> sampleSizes;
    std::vector<std::pair<uint32_t, uint32_t>> stts;      // count, delta
    std::vector<std::pair<uint32_t, int32_t>> ctts;       // count, offset
    struct Run { uint32_t firstChunk, perChunk; };
    std::vector<Run> stsc;
    std::vector<uint64_t> chunkOffsets;
    std::vector<uint32_t> sync;
    bool hasSizes = false, hasStss = false, hasStco = false;
    std::vector<Edit> edits;
    uint32_t trackId = 0;
};

// Fragmented MP4 (moov/mvex, then moof + mdat pairs; Topaz Video AI writes these).
struct Trex {
    uint32_t trackId = 0, duration = 0, size = 0, flags = 0;
};

void ParseAvc1(Span entry, Mp4Track& t) {
    Reader r(entry);
    r.Skip(6 + 2 + 16);
    const uint32_t w = r.U16(), h = r.U16();
    r.Skip(4 + 4 + 4 + 2 + 32 + 2 + 2);
    bool found = false;
    Box box;
    while (NextBox(r, box)) {
        if (box.type != Fourcc('a', 'v', 'c', 'C')) continue;
        Reader c(box.payload);
        if (c.Left() < 7) throw Fail{"avcC too short"};
        t.avcC.assign(box.payload.data, box.payload.data + box.payload.size);
        const uint8_t version = c.U8();
        if (version != 1) throw Fail{"unsupported avcC version"};
        t.profile = c.U8();
        c.U8();
        t.level = c.U8();
        t.nalLengthSize = uint8_t((c.U8() & 3) + 1);
        const uint32_t numSps = c.U8() & 0x1f;
        for (uint32_t i = 0; i < numSps; ++i) {
            const Span s = c.Take(c.U16());
            t.sps.emplace_back(s.data, s.data + s.size);
        }
        const uint32_t numPps = c.U8();
        for (uint32_t i = 0; i < numPps; ++i) {
            const Span s = c.Take(c.U16());
            t.pps.emplace_back(s.data, s.data + s.size);
        }
        found = true;
    }
    if (!found) throw Fail{"avc1 entry without an avcC box"};
    t.width = w;
    t.height = h;
    t.codedWidth = (w + 15) & ~15u;
    t.codedHeight = (h + 15) & ~15u;
    if (!t.sps.empty()) {
        try {
            ParseSps(t.sps[0], t);
        } catch (const Fail& f) {
            t.warnings.push_back(f.message + "; using the sample entry size");
        }
    }
    if (t.width == 0 || t.height == 0 || t.width > kMaxDimension || t.height > kMaxDimension) throw Fail{"bad video size"};
}

// Parses one trak; false when it is not an H.264 video track (t untouched beyond partial fill).
bool ParseTrak(Span trak, Mp4Track& t, Tables& tab) {
    Reader r(trak);
    Box box;
    bool video = false, avc = false;
    Mp4Track local;
    Tables ltab;
    while (NextBox(r, box)) {
        if (box.type == Fourcc('t', 'k', 'h', 'd')) {
            Reader h(box.payload);
            const uint8_t v = Version(h);
            h.Skip(v == 1 ? 16 : 8);
            ltab.trackId = h.U32();
        } else if (box.type == Fourcc('e', 'd', 't', 's')) {
            Reader e(box.payload);
            Box eb;
            while (NextBox(e, eb)) {
                if (eb.type != Fourcc('e', 'l', 's', 't')) continue;
                Reader l(eb.payload);
                const uint8_t v = Version(l);
                const uint32_t n = Count(l, v == 1 ? 20 : 12, 4096);
                for (uint32_t i = 0; i < n; ++i) {
                    Edit ed;
                    if (v == 1) { ed.segment = l.U64(); ed.mediaTime = int64_t(l.U64()); }
                    else { ed.segment = l.U32(); ed.mediaTime = int32_t(l.U32()); }
                    ed.rate = int16_t(l.U16());
                    l.U16();
                    ltab.edits.push_back(ed);
                }
            }
        } else if (box.type == Fourcc('m', 'd', 'i', 'a')) {
            Reader m(box.payload);
            Box mb;
            while (NextBox(m, mb)) {
                if (mb.type == Fourcc('m', 'd', 'h', 'd')) {
                    Reader h(mb.payload);
                    const uint8_t v = Version(h);
                    if (v == 1) { h.Skip(16); local.timescale = h.U32(); local.duration = h.U64(); }
                    else { h.Skip(8); local.timescale = h.U32(); local.duration = h.U32(); }
                } else if (mb.type == Fourcc('h', 'd', 'l', 'r')) {
                    Reader h(mb.payload);
                    h.Skip(8);
                    video = h.U32() == Fourcc('v', 'i', 'd', 'e');
                } else if (mb.type == Fourcc('m', 'i', 'n', 'f')) {
                    Reader mi(mb.payload);
                    Box ib;
                    while (NextBox(mi, ib)) {
                        if (ib.type != Fourcc('s', 't', 'b', 'l')) continue;
                        Reader s(ib.payload);
                        Box sb;
                        while (NextBox(s, sb)) {
                            Reader q(sb.payload);
                            if (sb.type == Fourcc('s', 't', 's', 'd')) {
                                Version(q);
                                const uint32_t n = q.U32();
                                if (n == 0 || n > 64) throw Fail{"bad stsd entry count"};
                                if (n > 1) local.warnings.push_back("several sample descriptions; using the first");
                                Box eb;
                                if (!NextBox(q, eb)) throw Fail{"empty stsd"};
                                if (eb.type == Fourcc('a', 'v', 'c', '1') || eb.type == Fourcc('a', 'v', 'c', '3')) {
                                    local.avc3 = eb.type == Fourcc('a', 'v', 'c', '3');
                                    ParseAvc1(eb.payload, local);
                                    avc = true;
                                }
                            } else if (sb.type == Fourcc('s', 't', 't', 's')) {
                                Version(q);
                                const uint32_t n = Count(q, 8);
                                for (uint32_t i = 0; i < n; ++i) { const uint32_t c = q.U32(); ltab.stts.emplace_back(c, q.U32()); }
                            } else if (sb.type == Fourcc('c', 't', 't', 's')) {
                                Version(q);
                                const uint32_t n = Count(q, 8);
                                for (uint32_t i = 0; i < n; ++i) { const uint32_t c = q.U32(); ltab.ctts.emplace_back(c, int32_t(q.U32())); }
                            } else if (sb.type == Fourcc('s', 't', 's', 'z')) {
                                Version(q);
                                const uint32_t constant = q.U32();
                                const uint32_t n = q.U32();
                                if (n > kMaxSamples) throw Fail{"too many samples"};
                                if (constant == 0 && uint64_t(n) * 4 > q.Left()) throw Fail{"stsz extends past its box"};
                                ltab.sampleSizes.resize(n);
                                for (uint32_t i = 0; i < n; ++i) ltab.sampleSizes[i] = constant ? constant : q.U32();
                                ltab.hasSizes = true;
                            } else if (sb.type == Fourcc('s', 't', 'z', '2')) {
                                Version(q);
                                q.Skip(3);
                                const uint32_t field = q.U8();
                                const uint32_t n = q.U32();
                                if (n > kMaxSamples) throw Fail{"too many samples"};
                                if (field != 4 && field != 8 && field != 16) throw Fail{"bad stz2 field size"};
                                if (uint64_t(n) * field > uint64_t(q.Left()) * 8) throw Fail{"stz2 extends past its box"};
                                ltab.sampleSizes.resize(n);
                                for (uint32_t i = 0; i < n; ++i) {
                                    if (field == 16) ltab.sampleSizes[i] = q.U16();
                                    else if (field == 8) ltab.sampleSizes[i] = q.U8();
                                    else if ((i & 1) == 0) { const uint8_t b = q.U8(); ltab.sampleSizes[i] = b >> 4; if (i + 1 < n) ltab.sampleSizes[i + 1] = b & 15; }
                                }
                                ltab.hasSizes = true;
                            } else if (sb.type == Fourcc('s', 't', 's', 'c')) {
                                Version(q);
                                const uint32_t n = Count(q, 12);
                                for (uint32_t i = 0; i < n; ++i) {
                                    Tables::Run run;
                                    run.firstChunk = q.U32();
                                    run.perChunk = q.U32();
                                    q.U32();
                                    ltab.stsc.push_back(run);
                                }
                            } else if (sb.type == Fourcc('s', 't', 'c', 'o') || sb.type == Fourcc('c', 'o', '6', '4')) {
                                Version(q);
                                const bool wide = sb.type == Fourcc('c', 'o', '6', '4');
                                const uint32_t n = Count(q, wide ? 8 : 4);
                                ltab.chunkOffsets.resize(n);
                                for (uint32_t i = 0; i < n; ++i) ltab.chunkOffsets[i] = wide ? q.U64() : q.U32();
                                ltab.hasStco = true;
                            } else if (sb.type == Fourcc('s', 't', 's', 's')) {
                                Version(q);
                                const uint32_t n = Count(q, 4);
                                ltab.sync.resize(n);
                                for (uint32_t i = 0; i < n; ++i) ltab.sync[i] = q.U32();
                                ltab.hasStss = true;
                            }
                        }
                    }
                }
            }
        }
    }
    if (!video || !avc) return false;
    t = std::move(local);
    tab = std::move(ltab);
    return true;
}

// Samples listed in the moov sample tables.
void BuildSamples(Mp4Track& t, Tables& tab, uint64_t fileSize) {
    if (!tab.hasSizes || !tab.hasStco || tab.stsc.empty() || tab.stts.empty()) throw Fail{"missing sample table box"};
    const size_t n = tab.sampleSizes.size();
    if (n == 0) throw Fail{"video track has no samples"};
    t.samples.resize(n);

    // Sizes and file offsets from stsc + stco.
    for (size_t i = 0, sample = 0; i < tab.chunkOffsets.size() && sample < n; ++i) {
        const uint32_t chunk = uint32_t(i + 1);
        size_t run = 0;
        while (run + 1 < tab.stsc.size() && tab.stsc[run + 1].firstChunk <= chunk) ++run;
        if (tab.stsc[run].firstChunk > chunk) throw Fail{"stsc does not start at chunk 1"};
        uint64_t offset = tab.chunkOffsets[i];
        for (uint32_t k = 0; k < tab.stsc[run].perChunk && sample < n; ++k, ++sample) {
            const uint32_t size = tab.sampleSizes[sample];
            if (size > kMaxSampleBytes) throw Fail{"absurd sample size"};
            if (offset > fileSize || size > fileSize - offset) throw Fail{"sample lies past the end of the file"};
            t.samples[sample].offset = offset;
            t.samples[sample].size = size;
            offset += size;
        }
        if (i + 1 == tab.chunkOffsets.size() && sample < n) throw Fail{"chunk tables cover fewer samples than stsz"};
    }

    // Timing.
    size_t sample = 0;
    int64_t dts = 0;
    for (const auto& [count, delta] : tab.stts) {
        for (uint32_t k = 0; k < count && sample < n; ++k, ++sample) {
            t.samples[sample].dts = dts;
            t.samples[sample].duration = delta;
            dts += delta;
        }
    }
    if (sample < n) throw Fail{"stts covers fewer samples than stsz"};
    sample = 0;
    for (const auto& [count, offset] : tab.ctts) {
        for (uint32_t k = 0; k < count && sample < n; ++k, ++sample) t.samples[sample].pts = t.samples[sample].dts + offset;
    }
    for (; sample < n; ++sample) t.samples[sample].pts = t.samples[sample].dts;

    // Keyframes.
    if (!tab.hasStss) {
        for (auto& s : t.samples) s.keyframe = true;
    } else {
        for (uint32_t number : tab.sync) {
            if (number >= 1 && number <= n) t.samples[number - 1].keyframe = true;
        }
    }
}

// One moof: the runs of trackId's fragments, appended in order. nextDts carries
// the decode time between fragments that have no tfdt.
void ParseMoof(Span moof, uint64_t moofOffset, uint64_t fileSize, uint32_t trackId, const Trex& trex,
               int64_t& nextDts, Mp4Track& t) {
    Reader r(moof);
    Box box;
    while (NextBox(r, box)) {
        if (box.type != Fourcc('t', 'r', 'a', 'f')) continue;
        Reader tr(box.payload);
        Box tb;
        bool mine = false;
        uint64_t base = moofOffset, dataEnd = 0;
        bool haveDataEnd = false;
        uint32_t defDuration = trex.duration, defSize = trex.size, defFlags = trex.flags;
        while (NextBox(tr, tb)) {
            Reader q(tb.payload);
            if (tb.type == Fourcc('t', 'f', 'h', 'd')) {
                const uint32_t flags = q.U32() & 0xFFFFFF;
                const uint32_t id = q.U32();
                mine = trackId == 0 || id == trackId;
                if (flags & 0x01) base = q.U64();
                if (flags & 0x02) q.U32();
                if (flags & 0x08) defDuration = q.U32();
                if (flags & 0x10) defSize = q.U32();
                if (flags & 0x20) defFlags = q.U32();
            } else if (!mine) {
                continue;
            } else if (tb.type == Fourcc('t', 'f', 'd', 't')) {
                const uint8_t v = Version(q);
                nextDts = int64_t(v == 1 ? q.U64() : q.U32());
            } else if (tb.type == Fourcc('t', 'r', 'u', 'n')) {
                const uint32_t flags = q.U32() & 0xFFFFFF;
                const uint32_t count = q.U32();
                if (count > kMaxSamples || t.samples.size() + count > kMaxSamples) throw Fail{"too many samples"};
                uint64_t data = haveDataEnd ? dataEnd : base;
                if (flags & 0x01) data = base + int64_t(int32_t(q.U32()));
                uint32_t firstFlags = defFlags;
                const bool hasFirstFlags = (flags & 0x04) != 0;
                if (hasFirstFlags) firstFlags = q.U32();
                for (uint32_t i = 0; i < count; ++i) {
                    Mp4Sample s;
                    s.duration = (flags & 0x100) ? q.U32() : defDuration;
                    s.size = (flags & 0x200) ? q.U32() : defSize;
                    const uint32_t sampleFlags = (flags & 0x400) ? q.U32() : (i == 0 && hasFirstFlags ? firstFlags : defFlags);
                    // Signed in either version, as FFmpeg reads it.
                    const int64_t offset = (flags & 0x800) ? int64_t(int32_t(q.U32())) : 0;
                    if (s.size > kMaxSampleBytes) throw Fail{"absurd sample size"};
                    if (data > fileSize || s.size > fileSize - data) throw Fail{"sample lies past the end of the file"};
                    s.offset = data;
                    data += s.size;
                    s.dts = nextDts;
                    nextDts += s.duration;
                    s.pts = s.dts + offset;
                    s.keyframe = (sampleFlags & 0x10000) == 0; // sample_is_non_sync_sample
                    t.samples.push_back(s);
                }
                dataEnd = data;
                haveDataEnd = true;
            }
        }
    }
}

bool ReadBox(ByteSource& src, uint64_t offset, uint64_t end, uint32_t& type, uint64_t& header, uint64_t& size);

// Samples of every moof in the file, after any listed in the moov. A
// truncated last fragment (an interrupted export) ends the list.
void AppendFragments(ByteSource& src, uint64_t fileSize, uint32_t trackId, const Trex& trex, Mp4Track& t) {
    int64_t nextDts = t.samples.empty() ? 0 : t.samples.back().dts + t.samples.back().duration;
    std::vector<uint8_t> moof;
    for (uint64_t offset = 0; offset < fileSize;) {
        uint32_t type = 0;
        uint64_t header = 0, size = 0;
        if (!ReadBox(src, offset, fileSize, type, header, size) || size < header || size > fileSize - offset) {
            t.warnings.push_back("the file ends inside a box; later fragments ignored");
            break;
        }
        if (type == Fourcc('m', 'o', 'o', 'f')) {
            if (size - header > kMaxMoofBytes) throw Fail{"moof is absurdly large"};
            moof.resize(size_t(size - header));
            if (!src.ReadAt(offset + header, moof.data(), moof.size())) throw Fail{"cannot read moof"};
            ParseMoof(Span{moof.data(), moof.size()}, offset, fileSize, trackId, trex, nextDts, t);
        }
        offset += size;
    }
}

// Frame duration, edit list and count, once every sample is known.
void FinishSamples(Mp4Track& t, const Tables& tab, uint32_t movieTimescale) {
    const size_t n = t.samples.size();
    if (n == 0) throw Fail{"video track has no samples"};
    t.frameDuration = t.samples[0].duration;
    for (const auto& s : t.samples)
        if (s.duration != t.frameDuration) { t.frameDuration = 0; break; }

    // Edit list: only a leading empty edit and/or a start media_time.
    int64_t shift = 0;
    const auto& edits = tab.edits;
    if (!edits.empty()) {
        size_t i = 0;
        int64_t empty = 0;
        if (edits[0].mediaTime == -1) {
            if (movieTimescale != 0) empty = int64_t((long double)edits[0].segment * t.timescale / movieTimescale + 0.5L);
            i = 1;
        }
        const size_t rest = edits.size() - i;
        if (rest == 1 && edits[i].mediaTime >= 0 && edits[i].rate == 1 && (i == 0 || movieTimescale != 0)) {
            shift = empty - edits[i].mediaTime;
        } else if (rest == 0 && i == 1) {
            t.warnings.push_back("edit list with only an empty edit; ignored");
        } else {
            t.warnings.push_back("unsupported edit list; ignored");
        }
    } else {
        // No edit list (fragmented files from FFmpeg, Topaz): the first frame
        // shown starts the video, as a replacement lines up from it.
        int64_t first = t.samples[0].pts;
        for (const auto& s : t.samples) first = std::min(first, s.pts);
        shift = -first;
    }
    if (shift != 0) {
        for (auto& s : t.samples) {
            s.dts += shift;
            s.pts += shift;
        }
    }
    t.frameCount = uint32_t(n);
}

bool ReadBox(ByteSource& src, uint64_t offset, uint64_t end, uint32_t& type, uint64_t& header, uint64_t& size) {
    uint8_t h[16];
    if (end - offset < 8) return false;
    if (!src.ReadAt(offset, h, 8)) return false;
    size = (uint64_t(h[0]) << 24) | (uint64_t(h[1]) << 16) | (uint64_t(h[2]) << 8) | h[3];
    type = Fourcc(char(h[4]), char(h[5]), char(h[6]), char(h[7]));
    header = 8;
    if (size == 1) {
        if (end - offset < 16 || !src.ReadAt(offset + 8, h + 8, 8)) return false;
        size = 0;
        for (int i = 8; i < 16; ++i) size = (size << 8) | h[i];
        header = 16;
    } else if (size == 0) {
        size = end - offset;
    }
    return true;
}
}

bool ParseMp4(ByteSource& source, Mp4Track& track, std::string& error) {
    try {
        track = Mp4Track();
        const uint64_t fileSize = source.Size();
        uint64_t offset = 0;
        bool found = false;
        while (offset < fileSize && !found) {
            uint32_t type = 0;
            uint64_t header = 0, size = 0;
            if (!ReadBox(source, offset, fileSize, type, header, size)) throw Fail{"cannot read a box header"};
            if (size < header || size > fileSize - offset) throw Fail{"top-level box extends past the end of the file"};
            if (type == Fourcc('m', 'o', 'o', 'v')) {
                const uint64_t payload = size - header;
                if (payload > kMaxMoovBytes) throw Fail{"moov is absurdly large"};
                std::vector<uint8_t> moov(static_cast<size_t>(payload));
                if (!source.ReadAt(offset + header, moov.data(), moov.size())) throw Fail{"cannot read moov"};
                Reader r(Span{moov.data(), moov.size()});
                Box box;
                uint32_t movieTimescale = 0;
                Tables tab;
                std::vector<Trex> trex;
                bool fragmented = false;
                while (NextBox(r, box)) {
                    if (box.type == Fourcc('m', 'v', 'e', 'x')) {
                        fragmented = true;
                        Reader m(box.payload);
                        Box mb;
                        while (NextBox(m, mb)) {
                            if (mb.type != Fourcc('t', 'r', 'e', 'x')) continue;
                            Reader q(mb.payload);
                            Version(q);
                            Trex x;
                            x.trackId = q.U32();
                            q.U32(); // default_sample_description_index
                            x.duration = q.U32();
                            x.size = q.U32();
                            x.flags = q.U32();
                            trex.push_back(x);
                        }
                    } else if (box.type == Fourcc('m', 'v', 'h', 'd')) {
                        Reader h(box.payload);
                        const uint8_t v = Version(h);
                        h.Skip(v == 1 ? 16 : 8);
                        movieTimescale = h.U32();
                    } else if (box.type == Fourcc('t', 'r', 'a', 'k')) {
                        // mvhd normally precedes trak; a late one only affects an empty edit.
                        if (!found) found = ParseTrak(box.payload, track, tab);
                    }
                }
                if (!found) throw Fail{"no H.264 (avc1/avc3) video track"};
                if (track.timescale == 0) throw Fail{"media timescale is zero"};
                if (!fragmented || (tab.hasSizes && !tab.sampleSizes.empty())) BuildSamples(track, tab, fileSize);
                if (fragmented) {
                    Trex defaults;
                    for (const auto& x : trex)
                        if (x.trackId == tab.trackId) defaults = x;
                    AppendFragments(source, fileSize, tab.trackId, defaults, track);
                }
                FinishSamples(track, tab, movieTimescale);
            }
            offset += size;
        }
        if (!found) throw Fail{"no moov box"};
        return true;
    } catch (const Fail& f) {
        error = f.message;
    } catch (const std::exception& e) {
        error = e.what();
    } catch (...) {
        error = "unexpected error while parsing the MP4";
    }
    track = Mp4Track();
    return false;
}
}
