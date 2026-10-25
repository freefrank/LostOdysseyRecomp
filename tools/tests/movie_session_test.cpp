// Actual session/clock/publication code with a controllable decoder. File reads
// and MP4 parsing are production; no GPU, guest game code or platform codec.
#include <movie/movie_player.cpp>
#include <cassert>
#include <iostream>

namespace {
std::atomic<bool> allowOpen{false}, allowFrame{false}, cancelled{false};
std::atomic<unsigned> nextCalls{0};
int mode = 0;
class Decoder final : public movie::MovieDecoder {
    bool first_ = true;
public:
    bool Open(const movie::Mp4Track&, movie::ByteSource&, std::string& error) override {
        if (mode == 2) { error = "scripted open failure"; return false; }
        while (!allowOpen.load() && !StopRequested()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (StopRequested()) { cancelled = true; return false; }
        return true;
    }
    bool Next(movie::Nv12Frame& frame, std::string& error) override {
        ++nextCalls;
        if (!first_ && mode == 1) { error = "scripted decode failure"; return false; }
        while ((!allowFrame.load() || !first_) && !StopRequested()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (StopRequested()) { cancelled = true; return false; }
        frame.width = frame.height = frame.lumaPitch = frame.chromaPitch = 64;
        frame.luma.resize(64*64); frame.chroma.resize(64*32); frame.pts = 0;
        first_ = false;
        return true;
    }
    void Rewind() override { first_ = true; }
    const char* Name() const override { return "scripted session decoder"; }
    bool Hardware() const override { return false; }
};

template<class Predicate> void Await(Predicate ready) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!ready() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(ready());
}
}
namespace movie {
MovieDecoder::~MovieDecoder() = default;
std::unique_ptr<MovieDecoder> CreateMovieDecoder(bool) { return std::make_unique<Decoder>(); }
}

int main(int argc, char** argv) {
    modding::ResolvedAsset asset;
    asset.path = argc > 1 ? argv[1] : "data/testsrc-64x64-bframes.mp4";
    constexpr uint32_t guest = 0x10000, id = 1;
    auto attach = [&] {
        auto s = std::make_shared<movie::Session>("test", asset);
        std::lock_guard lock(movie::gMutex);
        movie::gSessions[id] = s; movie::gByMovie[guest] = id;
        return s;
    };
    auto detach = [] {
        std::lock_guard lock(movie::gMutex);
        movie::gSessions.clear(); movie::gByMovie.clear();
    };
    movie::DrawFrame picture;
    {
        auto s = attach();
        s->clock.Shown(0, 100);
        assert(!s->Ready() && movie::GuestUpdateDue(id, 100));
        assert(!movie::FrameForDraw(id, 0, picture));
        allowOpen = true;
        allowFrame = true;
        Await([&] { return s->Ready(); });
        assert(!movie::GuestUpdateDue(id, 100));
        assert(movie::FrameForDraw(id, 0, picture) && picture.frame && picture.serial);
        Await([&] { return nextCalls.load() >= 2; });
        s->RequestStop();
        detach(); s.reset();
        assert(cancelled);
    }
    for (int failure : {1, 2}) {
        mode = failure; cancelled = false;
        auto s = attach();
        Await([&] { return s->Failed(); });
        assert(movie::ReplacementSession(guest) == 0);
        assert(!movie::FrameForDraw(id, 0, picture));
        s->RequestStop(); detach(); s.reset();
    }
    mode = 0; allowOpen = false; allowFrame = false; cancelled = false;
    {
        auto s = attach();
        // The flag is also observed while Open waits for a codec to initialize.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        s->RequestStop(); detach(); s.reset();
        assert(cancelled);
    }
    std::cout << "movie session: startup pacing, publication, midstream/open fallback and cancellation passed\n";
}
