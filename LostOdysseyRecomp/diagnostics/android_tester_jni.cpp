#if defined(__ANDROID__)
#include "android_tester.h"
#include <gpu/video.h>
#include <jni.h>
#include <sstream>

using ::diagnostics::android_tester::g_recorder;
using ::diagnostics::android_tester::NowNs;

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_freefrank_lostodyssey_RuntimeActivity_nativeTesterGpu(JNIEnv* env, jclass) {
    const auto name = gpu::video::ActiveGpuDeviceName();
    return env->NewStringUTF(name.c_str());
}
extern "C" JNIEXPORT jstring JNICALL
Java_io_github_freefrank_lostodyssey_RuntimeActivity_nativeTesterGpuDriver(JNIEnv* env, jclass) {
    const auto version = gpu::video::ActiveGpuDriverVersion();
    return env->NewStringUTF(version.c_str());
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_io_github_freefrank_lostodyssey_RuntimeActivity_nativeTesterStatus(JNIEnv* env, jclass) {
    const uint64_t now = NowNs();
    // Also enforce the intensive limit while the app is paused or no frame is
    // presented. Stop is harmless if the presentation thread stopped it first.
    if (g_recorder.recording.load() && g_recorder.profile.load() == 2 &&
        g_recorder.Duration(now) >= ::diagnostics::android_tester::Recorder::IntensiveLimitNs)
        g_recorder.Stop(now);
    const jlong values[] = {jlong(g_recorder.recording.load()), jlong(g_recorder.profile.load()),
        jlong(g_recorder.automatic.load()), jlong(g_recorder.thermal.load()), jlong(g_recorder.Started()),
        jlong(g_recorder.Duration(now)), jlong(g_recorder.Dropped()), jlong(g_recorder.TargetFps()), jlong(g_recorder.Threshold())};
    jlongArray array = env->NewLongArray(9);
    if (array) env->SetLongArrayRegion(array, 0, 9, values);
    return array;
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_freefrank_lostodyssey_RuntimeActivity_nativeTesterStop(JNIEnv*, jclass) {
    g_recorder.Stop(NowNs());
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_freefrank_lostodyssey_RuntimeActivity_nativeTesterCollectorFinished(JNIEnv*, jclass) {
    g_recorder.ReleaseBuffer();
    g_recorder.collectorIdle.store(true, std::memory_order_release);
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_freefrank_lostodyssey_RuntimeActivity_nativeTesterDrain(JNIEnv* env, jclass) {
    try {
        const auto frames = g_recorder.Drain();
        std::ostringstream text;
        for (const auto& f : frames)
            text << f.timestampNs << ',' << f.intervalNs << ',' << f.presentWallNs << ','
                 << (f.presentCpuNs ? std::to_string(f.presentCpuNs) : std::string{}) << ','
                 << f.rendererFrame << ',' << f.draws << ',' << f.gpuBatchTotalNs << ',' << f.gpuBatches << ','
                 << f.audioStarved << ',' << f.audioClockResets << ',' << f.xmaFrames << ',' << f.xmaFailures << ','
                 << f.xmaDecodeWallNs << ',' << f.movieFrames << ',' << f.moviePtsMs << ',' << f.movieUpdateWallNs << ','
                 << f.pipelineCreates << ',' << f.pipelineFailures << ',' << f.pipelineWallNs << ',' << f.pipelineVs << ',' << f.pipelinePs << ','
                 << f.movieActive << ',' << f.presented << ',' << f.spike << '\n';
        return env->NewStringUTF(text.str().c_str());
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}
#endif
