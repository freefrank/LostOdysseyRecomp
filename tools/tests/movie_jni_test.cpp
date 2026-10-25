// Production Android backend running against scripted Android classes in a real
// JVM. This exercises JNI ownership/protocol, not H.264 decoding or hardware.
#include <jni.h>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <movie/movie_decoder_android.cpp>
#include <movie/movie_decoder.cpp>

namespace movie { ByteSource::~ByteSource() = default; }
namespace {
thread_local JNIEnv* testEnv = nullptr;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
class Source final : public movie::ByteSource {
public:
    uint64_t Size() const override { return 6; }
    bool ReadAt(uint64_t at, void* dst, size_t n) override {
        const uint8_t raw[]{0,0,0,2,0x65,0x11};
        if (at || n != sizeof(raw)) return false;
        std::memcpy(dst, raw, n);
        return true;
    }
};
}
extern "C" void* SDL_GetAndroidJNIEnv() { return testEnv; }

extern "C" JNIEXPORT void JNICALL Java_MovieCodecTest_run(JNIEnv* env, jclass, jstring name, jboolean software) {
    testEnv = env;
    const char* text = env->GetStringUTFChars(name, nullptr);
    const std::string scenario(text);
    env->ReleaseStringUTFChars(name, text);
    try {
        int64_t scaled;
        Check(movie::Rescale(-1001, 30000, 1000000, scaled) && scaled == -33367, "negative PTS rescale");
        Check(movie::Rescale(scaled, 1000000, 30000, scaled) && scaled == -1001, "negative PTS round trip");
        Check(!movie::Rescale(INT64_MAX, 1, 1000000, scaled), "overflowing timestamp accepted");
        Source source;
        movie::Mp4Track track;
        track.width = track.height = 4; track.codedWidth = 8; track.codedHeight = 6;
        track.timescale = 30000; track.frameDuration = 1001; track.profile = 100;
        track.sps = {{0x67,0x42}}; track.pps = {{0x68}};
        for (int64_t pts : {-1001, 1001, 0}) track.samples.push_back({0,6,pts,pts,1001,true});
        std::atomic<bool> stop{false};
        auto decoder = movie::CreateMovieDecoder(!software);
        decoder->SetStopFlag(&stop);
        std::string error;
        if (scenario == "cancel_before_open") {
            stop.store(true, std::memory_order_relaxed);
            Check(!decoder->Open(track, source, error) && error.empty(), "cancelled open used the codec");
            return;
        }
        Check(decoder->Open(track, source, error), error.c_str());
        movie::Nv12Frame frame;
        if (scenario == "cancel_wait") {
            const auto begin = std::chrono::steady_clock::now();
            std::thread cancel([&] { std::this_thread::sleep_for(std::chrono::milliseconds(40)); stop.store(true, std::memory_order_relaxed); });
            const bool got = decoder->Next(frame, error);
            cancel.join();
            Check(!got && error.empty(), "cancelled wait produced a frame/error");
            Check(std::chrono::steady_clock::now() - begin < std::chrono::seconds(1), "cancelled wait did not return promptly");
            return;
        }
        for (unsigned pass = 0; pass < 2; ++pass) {
            unsigned count = 0;
            while (decoder->Next(frame, error)) {
                Check(count < 3 && frame.pts == (int64_t(count) - 1) * 1001, "output PTS/order changed");
                Check(frame.width == 4 && frame.height == 4 && frame.luma.size() == 16 && frame.chroma.size() == 8, "wrong display crop");
                Check(frame.luma[0] == 18 + count * 16 && frame.luma[15] == 45 + count * 16, "wrong luma stride/crop");
                Check(frame.chroma[0] == 85 + count * 16 && frame.chroma[1] == 165 + count * 16 &&
                    frame.chroma[6] == 90 + count * 16 && frame.chroma[7] == 170 + count * 16, "wrong Cb/Cr plane order/stride");
                ++count;
            }
            if (scenario == "midstream_error") {
                Check(count == 1 && !error.empty(), "midstream error lost or retried");
                Check(decoder->Hardware(), "midstream error switched to software");
                return;
            }
            Check(error.empty(), error.c_str());
            Check(count == 3, "EOS dropped reordered pictures");
            const bool fallback = scenario == "configure_error" || scenario == "no_image" || scenario == "bad_plane" || scenario == "empty_eos";
            Check(decoder->Hardware() == (!software && !fallback && scenario != "api28"), "incorrect hardware classification/fallback");
            if (scenario == "api28") Check(std::string(decoder->AccelerationName()) == "acceleration unknown", "pre-29 codec classified as hardware/software");
            if (!pass) decoder->Rewind();
        }
    } catch (const std::exception& e) {
        env->ThrowNew(env->FindClass("java/lang/AssertionError"), e.what());
    }
}
