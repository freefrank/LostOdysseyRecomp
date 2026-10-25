#if defined(__ANDROID__)
#include "movie_decoder.h"
#include "h264_sample.h"
#include "yuv420_frame.h"
#include <SDL3/SDL_system.h>
#include <jni.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>

namespace movie {
namespace {
constexpr jint kTryAgain = -1, kFormatChanged = -2, kBuffersChanged = -3;
constexpr jint kEndOfStream = 4, kCodecConfig = 2;
constexpr jint kFlexible420 = 0x7F420888, kImage420 = 35;
constexpr auto kNoProgressTimeout = std::chrono::seconds(5);

class LocalFrame {
public:
    explicit LocalFrame(JNIEnv* env) : env_(env), ok_(env->PushLocalFrame(64) == 0) {}
    ~LocalFrame() { if (ok_) env_->PopLocalFrame(nullptr); }
    explicit operator bool() const { return ok_; }
private:
    JNIEnv* env_;
    bool ok_;
};

enum class Acceleration { Hardware, Unknown, Software };
struct Candidate { std::string name; Acceleration acceleration; };

// Both uses have either from or to == 1000000, so remainder * to fits int64.
// Round once, preserving negative edit-list times and avoiding floating drift.
bool Rescale(int64_t value, uint32_t from, uint32_t to, int64_t& result) {
    if (!from || !to) return false;
    constexpr auto max = std::numeric_limits<int64_t>::max(), min = std::numeric_limits<int64_t>::min();
    const int64_t whole = value / from, remainder = value % from;
    if (whole > max / to || whole < min / to) return false;
    const int64_t fraction = remainder * int64_t(to);
    const int64_t rounded = fraction >= 0 ? (fraction + from / 2) / from : -((-fraction + from / 2) / from);
    const int64_t base = whole * to;
    if ((rounded > 0 && base > max - rounded) || (rounded < 0 && base < min - rounded)) return false;
    result = base + rounded;
    return true;
}

class AndroidDecoder final : public MovieDecoder {
public:
    explicit AndroidDecoder(bool preferHardware) : preferHardware_(preferHardware) {}
    ~AndroidDecoder() override {
        if (codec_ || !types_.empty()) env_ = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        CloseCodec();
        if (env_) for (jclass type : types_) env_->DeleteGlobalRef(type);
    }

    bool Open(const Mp4Track& track, ByteSource& source, std::string& error) override try {
        error.clear();
        if (StopRequested()) return false;
        env_ = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        if (!env_) { error = "Android Java VM unavailable"; return false; }
        CloseCodec();
        if (track.samples.empty() || !track.timescale || !track.width || !track.height ||
            track.bitDepth != 8 || track.chromaFormat != 1 || track.nalLengthSize < 1 || track.nalLengthSize > 4) {
            error = "MediaCodec requires an 8-bit 4:2:0 H.264 track";
            return false;
        }
        LocalFrame locals(env_);
        if (!locals) { Exception(error, "allocating JNI references"); return false; }
        if (!initialized_ && !Initialize(error)) return false;
        track_ = &track; source_ = &source;
        candidates_.clear(); nextCandidate_ = 0; delivered_ = false; pendingError_.clear();
        int64_t first = track.samples.front().pts;
        uint64_t maxSample = 0;
        for (const auto& s : track.samples) { first = std::min(first, s.pts); maxSample = std::max<uint64_t>(maxSample, s.size); }
        if (!Rescale(first, track.timescale, 1000000, first) || first == std::numeric_limits<int64_t>::min()) {
            error = "H.264 timestamp is out of range"; return false;
        }
        biasUs_ = first < 0 ? -first : 0;
        // A zero-length NAL also grows a length prefix into a start code.
        const uint64_t maxInput = maxSample + (4 - track.nalLengthSize) * (maxSample / track.nalLengthSize);
        if (maxInput > uint64_t(std::numeric_limits<jint>::max())) { error = "H.264 sample exceeds MediaCodec input size"; return false; }
        maxInput_ = jint(maxInput);
        csd_[0].clear(); csd_[1].clear();
        AppendH264ParameterSets(track.sps, csd_[0]);
        AppendH264ParameterSets(track.pps, csd_[1]);
        if (!FindCandidates(error)) return false;
        return OpenNextCandidate(error);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }

    bool Next(Nv12Frame& frame, std::string& error) override try {
        error.clear();
        if (StopRequested()) return false;
        env_ = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        if (!env_) { error = "Android Java VM unavailable"; return false; }
        if (!pendingError_.empty()) { error = pendingError_; return false; }
        if (!codec_) { error = "MediaCodec is not open"; return false; }
        LocalFrame locals(env_);
        if (!locals) { Exception(error, "allocating JNI references"); return false; }
        for (;;) {
            if (ReadPicture(frame, error)) { delivered_ = true; return true; }
            if (StopRequested() || error.empty() || delivered_) return false;
            // A codec may configure successfully but offer unusable raw output.
            // Try the next candidate only before publishing any picture; after
            // that, the existing player falls back to the running guest WMV.
            if (!OpenNextCandidate(error)) return false;
        }
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }

    void Rewind() override {
        if (!codec_) return;
        env_ = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        if (!env_) { pendingError_ = "Android Java VM unavailable"; return; }
        LocalFrame locals(env_);
        if (!locals) { Exception(pendingError_, "allocating JNI references"); return; }
        delivered_ = false;
        pendingError_.clear();
        // Reconfigure instead of an early flush, which can lose codec-specific
        // data. CSD is submitted once by MediaCodec.start(), including rewind.
        Configure(current_, pendingError_);
    }

    const char* Name() const override { return name_.c_str(); }
    bool Hardware() const override { return current_.acceleration == Acceleration::Hardware; }
    const char* AccelerationName() const override {
        switch (current_.acceleration) {
        case Acceleration::Hardware: return "hardware (reported by Android)";
        case Acceleration::Software: return "software";
        default: return "acceleration unknown";
        }
    }

private:
    bool Exception(std::string& error, const char* operation) {
        if (!env_->ExceptionCheck()) return false;
        jthrowable ex = env_->ExceptionOccurred();
        env_->ExceptionClear();
        error = operation;
        jclass type = ex ? env_->GetObjectClass(ex) : nullptr;
        jmethodID text = type ? env_->GetMethodID(type, "toString", "()Ljava/lang/String;") : nullptr;
        jstring message = text ? static_cast<jstring>(env_->CallObjectMethod(ex, text)) : nullptr;
        if (env_->ExceptionCheck()) env_->ExceptionClear();
        else if (message) {
            if (const char* chars = env_->GetStringUTFChars(message, nullptr)) {
                error += ": "; error += chars;
                env_->ReleaseStringUTFChars(message, chars);
            }
        }
        if (env_->ExceptionCheck()) env_->ExceptionClear();
        if (message) env_->DeleteLocalRef(message);
        if (type) env_->DeleteLocalRef(type);
        if (ex) env_->DeleteLocalRef(ex);
        return true;
    }

    jclass Type(const char* name, std::string& error) {
        jclass local = env_->FindClass(name);
        if (Exception(error, name) || !local) return nullptr;
        auto global = static_cast<jclass>(env_->NewGlobalRef(local));
        env_->DeleteLocalRef(local);
        if (Exception(error, name) || !global) return nullptr;
        types_.push_back(global);
        return global;
    }

    jmethodID Method(jclass type, const char* name, const char* signature, std::string& error, bool isStatic = false) {
        jmethodID id = isStatic ? env_->GetStaticMethodID(type, name, signature) : env_->GetMethodID(type, name, signature);
        if (Exception(error, name)) return nullptr;
        if (!id) error = std::string("Android method unavailable: ") + name;
        return id;
    }

    bool Initialize(std::string& error) {
        codecType_ = Type("android/media/MediaCodec", error);
        if (!codecType_) return false;
        formatType_ = Type("android/media/MediaFormat", error);
        if (!formatType_) return false;
        listType_ = Type("android/media/MediaCodecList", error);
        if (!listType_) return false;
        infoType_ = Type("android/media/MediaCodec$BufferInfo", error);
        if (!infoType_) return false;
        const jclass codecInfo = Type("android/media/MediaCodecInfo", error);
        if (!codecInfo) return false;
        const jclass caps = Type("android/media/MediaCodecInfo$CodecCapabilities", error);
        if (!caps) return false;
        const jclass image = Type("android/media/Image", error);
        if (!image) return false;
        const jclass plane = Type("android/media/Image$Plane", error);
        if (!plane) return false;
        const jclass rect = Type("android/graphics/Rect", error);
        if (!rect) return false;
        const jclass buffer = Type("java/nio/Buffer", error);
        if (!buffer) return false;
        const jclass version = Type("android/os/Build$VERSION", error);
        if (!version) return false;
        const jfieldID sdk = env_->GetStaticFieldID(version, "SDK_INT", "I");
        if (Exception(error, "reading Android API level") || !sdk) return false;
        sdk_ = env_->GetStaticIntField(version, sdk);
        if (Exception(error, "reading Android API level")) return false;
#define LO_METHOD(member, type, name, sig) if (!(member = Method(type, name, sig, error))) return false
#define LO_STATIC(member, type, name, sig) if (!(member = Method(type, name, sig, error, true))) return false
#define LO_FIELD(member, type, name, sig) member = env_->GetFieldID(type, name, sig); if (Exception(error, name) || !member) return false
        LO_STATIC(createCodec_, codecType_, "createByCodecName", "(Ljava/lang/String;)Landroid/media/MediaCodec;");
        LO_METHOD(configure_, codecType_, "configure", "(Landroid/media/MediaFormat;Landroid/view/Surface;Landroid/media/MediaCrypto;I)V");
        LO_METHOD(start_, codecType_, "start", "()V");
        LO_METHOD(stop_, codecType_, "stop", "()V");
        LO_METHOD(release_, codecType_, "release", "()V");
        LO_METHOD(dequeueInput_, codecType_, "dequeueInputBuffer", "(J)I");
        LO_METHOD(inputBuffer_, codecType_, "getInputBuffer", "(I)Ljava/nio/ByteBuffer;");
        LO_METHOD(queueInput_, codecType_, "queueInputBuffer", "(IIIJI)V");
        LO_METHOD(dequeueOutput_, codecType_, "dequeueOutputBuffer", "(Landroid/media/MediaCodec$BufferInfo;J)I");
        LO_METHOD(outputImage_, codecType_, "getOutputImage", "(I)Landroid/media/Image;");
        LO_METHOD(releaseOutput_, codecType_, "releaseOutputBuffer", "(IZ)V");
        LO_STATIC(createFormat_, formatType_, "createVideoFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;");
        LO_METHOD(setInt_, formatType_, "setInteger", "(Ljava/lang/String;I)V");
        LO_METHOD(setFloat_, formatType_, "setFloat", "(Ljava/lang/String;F)V");
        LO_METHOD(setBuffer_, formatType_, "setByteBuffer", "(Ljava/lang/String;Ljava/nio/ByteBuffer;)V");
        LO_METHOD(listInit_, listType_, "<init>", "(I)V");
        LO_METHOD(listInfos_, listType_, "getCodecInfos", "()[Landroid/media/MediaCodecInfo;");
        LO_METHOD(codecName_, codecInfo, "getName", "()Ljava/lang/String;");
        LO_METHOD(isEncoder_, codecInfo, "isEncoder", "()Z");
        LO_METHOD(codecTypes_, codecInfo, "getSupportedTypes", "()[Ljava/lang/String;");
        LO_METHOD(codecCaps_, codecInfo, "getCapabilitiesForType", "(Ljava/lang/String;)Landroid/media/MediaCodecInfo$CodecCapabilities;");
        LO_METHOD(supports_, caps, "isFormatSupported", "(Landroid/media/MediaFormat;)Z");
        if (sdk_ >= 29) {
            LO_METHOD(isHardware_, codecInfo, "isHardwareAccelerated", "()Z");
            LO_METHOD(isSoftware_, codecInfo, "isSoftwareOnly", "()Z");
        }
        LO_METHOD(infoInit_, infoType_, "<init>", "()V");
        LO_FIELD(outSize_, infoType_, "size", "I");
        LO_FIELD(outPts_, infoType_, "presentationTimeUs", "J");
        LO_FIELD(outFlags_, infoType_, "flags", "I");
        LO_METHOD(imageFormat_, image, "getFormat", "()I");
        LO_METHOD(imageWidth_, image, "getWidth", "()I");
        LO_METHOD(imageHeight_, image, "getHeight", "()I");
        LO_METHOD(imageCrop_, image, "getCropRect", "()Landroid/graphics/Rect;");
        LO_METHOD(imagePlanes_, image, "getPlanes", "()[Landroid/media/Image$Plane;");
        LO_METHOD(imageClose_, image, "close", "()V");
        LO_METHOD(planeBuffer_, plane, "getBuffer", "()Ljava/nio/ByteBuffer;");
        LO_METHOD(planeRow_, plane, "getRowStride", "()I");
        LO_METHOD(planePixel_, plane, "getPixelStride", "()I");
        LO_METHOD(bufferPosition_, buffer, "position", "()I");
        LO_METHOD(bufferLimit_, buffer, "limit", "()I");
        LO_FIELD(rectLeft_, rect, "left", "I");
        LO_FIELD(rectTop_, rect, "top", "I");
        LO_FIELD(rectRight_, rect, "right", "I");
        LO_FIELD(rectBottom_, rect, "bottom", "I");
#undef LO_FIELD
#undef LO_STATIC
#undef LO_METHOD
        initialized_ = true;
        return true;
    }

    bool Text(jstring value, std::string& text, std::string& error) {
        if (!value) { error = "Android returned a null string"; return false; }
        const char* chars = env_->GetStringUTFChars(value, nullptr);
        if (Exception(error, "reading Android string") || !chars) return false;
        text = chars;
        env_->ReleaseStringUTFChars(value, chars);
        return true;
    }

    bool SetInt(jobject format, const char* key, jint value, std::string& error) {
        jstring text = env_->NewStringUTF(key);
        if (Exception(error, key) || !text) return false;
        env_->CallVoidMethod(format, setInt_, text, value);
        env_->DeleteLocalRef(text);
        return !Exception(error, key);
    }

    jobject MakeFormat(std::string& error) {
        jstring mime = env_->NewStringUTF("video/avc");
        if (Exception(error, "creating H.264 format") || !mime) return nullptr;
        jobject format = env_->CallStaticObjectMethod(formatType_, createFormat_, mime, jint(track_->width), jint(track_->height));
        env_->DeleteLocalRef(mime);
        if (Exception(error, "creating H.264 format") || !format) return nullptr;
        if (!SetInt(format, "color-format", kFlexible420, error) || !SetInt(format, "max-input-size", maxInput_, error)) return nullptr;
        const jint profile = track_->profile == 66 ? 1 : track_->profile == 77 ? 2 : track_->profile == 88 ? 4 : track_->profile == 100 ? 8 : 0;
        if (profile && !SetInt(format, "profile", profile, error)) return nullptr;
        if (track_->frameDuration) {
            jstring key = env_->NewStringUTF("frame-rate");
            if (Exception(error, "setting frame rate") || !key) return nullptr;
            env_->CallVoidMethod(format, setFloat_, key, jfloat(double(track_->timescale) / track_->frameDuration));
            env_->DeleteLocalRef(key);
            if (Exception(error, "setting frame rate")) return nullptr;
        }
        return format;
    }

    bool FindCandidates(std::string& error) {
        jobject format = MakeFormat(error);
        if (!format) return false;
        // REGULAR_CODECS excludes components that require secure/tunneled playback.
        jobject list = env_->NewObject(listType_, listInit_, jint(0));
        if (Exception(error, "enumerating H.264 decoders") || !list) return false;
        auto infos = static_cast<jobjectArray>(env_->CallObjectMethod(list, listInfos_));
        if (Exception(error, "enumerating H.264 decoders") || !infos) return false;
        const jsize count = env_->GetArrayLength(infos);
        for (jsize i = 0; i < count; ++i) {
            LocalFrame locals(env_);
            if (!locals) { Exception(error, "allocating codec references"); return false; }
            jobject info = env_->GetObjectArrayElement(infos, i);
            if (Exception(error, "reading codec info") || !info) return false;
            const bool encoder = env_->CallBooleanMethod(info, isEncoder_);
            if (Exception(error, "reading codec info")) return false;
            if (encoder) continue;
            auto types = static_cast<jobjectArray>(env_->CallObjectMethod(info, codecTypes_));
            if (Exception(error, "reading codec types") || !types) return false;
            bool avc = false;
            for (jsize j = 0; j < env_->GetArrayLength(types); ++j) {
                auto type = static_cast<jstring>(env_->GetObjectArrayElement(types, j));
                std::string name;
                if (Exception(error, "reading codec type") || !Text(type, name, error)) return false;
                env_->DeleteLocalRef(type);
                if (name == "video/avc") { avc = true; break; }
            }
            if (!avc) continue;
            jstring mime = env_->NewStringUTF("video/avc");
            if (Exception(error, "reading codec capabilities") || !mime) return false;
            jobject caps = env_->CallObjectMethod(info, codecCaps_, mime);
            // Some vendor entries advertise AVC but fail their capability query.
            std::string unsupported;
            if (Exception(unsupported, "reading codec capabilities") || !caps) continue;
            const bool supported = env_->CallBooleanMethod(caps, supports_, format);
            if (Exception(unsupported, "checking codec format") || !supported) continue;
            auto name = static_cast<jstring>(env_->CallObjectMethod(info, codecName_));
            Candidate candidate;
            if (Exception(error, "reading codec name") || !Text(name, candidate.name, error)) return false;
            candidate.acceleration = Acceleration::Unknown;
            if (sdk_ >= 29) {
                const bool hardware = env_->CallBooleanMethod(info, isHardware_);
                if (Exception(error, "classifying codec")) return false;
                const bool software = env_->CallBooleanMethod(info, isSoftware_);
                if (Exception(error, "classifying codec")) return false;
                if (software) candidate.acceleration = Acceleration::Software;
                else if (hardware) candidate.acceleration = Acceleration::Hardware;
            } else if (candidate.name.starts_with("OMX.google.") || candidate.name.starts_with("c2.android.")) {
                // Recognize platform software codecs; other pre-29 names stay
                // unknown, rather than assuming that every vendor codec is HW.
                candidate.acceleration = Acceleration::Software;
            }
            if (preferHardware_ || candidate.acceleration == Acceleration::Software) candidates_.push_back(std::move(candidate));
        }
        std::stable_sort(candidates_.begin(), candidates_.end(), [](const auto& a, const auto& b) {
            return a.acceleration < b.acceleration;
        });
        if (candidates_.empty()) { error = "no suitable MediaCodec H.264 decoder"; return false; }
        return true;
    }

    bool OpenNextCandidate(std::string& error) {
        while (!StopRequested() && nextCandidate_ < candidates_.size()) {
            LocalFrame locals(env_);
            if (!locals) { Exception(error, "allocating codec references"); return false; }
            const Candidate candidate = candidates_[nextCandidate_++];
            if (Configure(candidate, error)) { error.clear(); return true; }
        }
        return false;
    }

    bool Configure(const Candidate& candidate, std::string& error) {
        CloseCodec();
        current_ = candidate;
        jobject format = MakeFormat(error);
        if (!format) return false;
        for (unsigned i = 0; i < 2; ++i) {
            if (csd_[i].empty()) continue; // avc3 may carry parameter sets in-band
            jobject bytes = env_->NewDirectByteBuffer(csd_[i].data(), jlong(csd_[i].size()));
            if (Exception(error, "preparing H.264 parameter sets") || !bytes) return false;
            jstring key = env_->NewStringUTF(i ? "csd-1" : "csd-0");
            if (Exception(error, "preparing H.264 parameter sets") || !key) return false;
            env_->CallVoidMethod(format, setBuffer_, key, bytes);
            if (Exception(error, "setting H.264 parameter sets")) return false;
        }
        jstring name = env_->NewStringUTF(candidate.name.c_str());
        if (Exception(error, "creating H.264 decoder") || !name) return false;
        jobject codec = env_->CallStaticObjectMethod(codecType_, createCodec_, name);
        if (Exception(error, "creating H.264 decoder") || !codec) return false;
        codec_ = env_->NewGlobalRef(codec);
        if (Exception(error, "retaining H.264 decoder") || !codec_) {
            env_->CallVoidMethod(codec, release_);
            if (env_->ExceptionCheck()) env_->ExceptionClear();
            return false;
        }
        env_->CallVoidMethod(codec_, configure_, format, nullptr, nullptr, jint(0));
        if (Exception(error, "configuring H.264 decoder")) { CloseCodec(); return false; }
        env_->CallVoidMethod(codec_, start_);
        if (Exception(error, "starting H.264 decoder")) { CloseCodec(); return false; }
        started_ = true;
        jobject info = env_->NewObject(infoType_, infoInit_);
        if (Exception(error, "allocating codec buffer info") || !info) { CloseCodec(); return false; }
        bufferInfo_ = env_->NewGlobalRef(info);
        if (Exception(error, "retaining codec buffer info") || !bufferInfo_) { CloseCodec(); return false; }
        name_ = "MediaCodec H.264 (" + candidate.name + ")";
        next_ = 0; inputEos_ = outputEos_ = false;
        lastPts_ = std::numeric_limits<int64_t>::min();
        return true;
    }

    void CloseCodec() {
        if (env_ && codec_) {
            if (started_) {
                env_->CallVoidMethod(codec_, stop_);
                if (env_->ExceptionCheck()) env_->ExceptionClear();
            }
            env_->CallVoidMethod(codec_, release_);
            if (env_->ExceptionCheck()) env_->ExceptionClear();
            env_->DeleteGlobalRef(codec_);
            codec_ = nullptr;
        }
        if (env_ && bufferInfo_) { env_->DeleteGlobalRef(bufferInfo_); bufferInfo_ = nullptr; }
        started_ = false;
    }

    bool Feed(jint index, std::string& error) {
        if (next_ == track_->samples.size()) {
            env_->CallVoidMethod(codec_, queueInput_, index, jint(0), jint(0), jlong(0), kEndOfStream);
            if (Exception(error, "queuing H.264 end of stream")) return false;
            inputEos_ = true;
            return true;
        }
        const auto& sample = track_->samples[next_];
        raw_.resize(sample.size);
        if (!source_->ReadAt(sample.offset, raw_.data(), raw_.size())) { error = "cannot read a video sample"; return false; }
        annexb_.clear();
        if (!AppendH264Sample(raw_, track_->nalLengthSize, annexb_, error)) return false;
        jobject bytes = env_->CallObjectMethod(codec_, inputBuffer_, index);
        if (Exception(error, "getting H.264 input buffer") || !bytes) return false;
        auto* dst = static_cast<uint8_t*>(env_->GetDirectBufferAddress(bytes));
        const jlong capacity = env_->GetDirectBufferCapacity(bytes);
        if (Exception(error, "mapping H.264 input buffer")) { env_->DeleteLocalRef(bytes); return false; }
        if (!dst || capacity < 0 || uint64_t(capacity) < annexb_.size() || annexb_.size() > size_t(std::numeric_limits<jint>::max())) {
            env_->DeleteLocalRef(bytes); error = "MediaCodec input buffer is too small"; return false;
        }
        std::memcpy(dst, annexb_.data(), annexb_.size());
        env_->DeleteLocalRef(bytes);
        int64_t pts;
        if (!Rescale(sample.pts, track_->timescale, 1000000, pts) || pts > std::numeric_limits<int64_t>::max() - biasUs_) {
            error = "H.264 timestamp is out of range"; return false;
        }
        env_->CallVoidMethod(codec_, queueInput_, index, jint(0), jint(annexb_.size()), jlong(pts + biasUs_), jint(0));
        if (Exception(error, "queuing H.264 sample")) return false;
        ++next_;
        return true;
    }

    bool CopyImage(jobject image, Nv12Frame& frame, std::string& error) {
        const jint format = env_->CallIntMethod(image, imageFormat_);
        if (Exception(error, "reading image format")) return false;
        const jint w = env_->CallIntMethod(image, imageWidth_);
        if (Exception(error, "reading image width")) return false;
        const jint h = env_->CallIntMethod(image, imageHeight_);
        if (Exception(error, "reading image height")) return false;
        jobject crop = env_->CallObjectMethod(image, imageCrop_);
        if (Exception(error, "reading image crop") || !crop) return false;
        const jint left = env_->GetIntField(crop, rectLeft_), top = env_->GetIntField(crop, rectTop_);
        const jint right = env_->GetIntField(crop, rectRight_), bottom = env_->GetIntField(crop, rectBottom_);
        if (Exception(error, "reading image crop")) return false;
        if (format != kImage420 || w <= 0 || h <= 0 || w > 16384 || h > 16384 || left < 0 || top < 0 ||
            right <= left || bottom <= top || right > w || bottom > h ||
            uint32_t(right - left) != track_->width || uint32_t(bottom - top) != track_->height) {
            error = "MediaCodec returned an unsupported image format or crop"; return false;
        }
        auto array = static_cast<jobjectArray>(env_->CallObjectMethod(image, imagePlanes_));
        if (Exception(error, "reading image planes") || !array || env_->GetArrayLength(array) != 3) {
            if (error.empty()) error = "MediaCodec returned no YUV420 planes";
            return false;
        }
        Yuv420Plane planes[3];
        for (unsigned p = 0; p < 3; ++p) {
            jobject plane = env_->GetObjectArrayElement(array, jsize(p));
            if (Exception(error, "reading image plane") || !plane) return false;
            jobject bytes = env_->CallObjectMethod(plane, planeBuffer_);
            if (Exception(error, "reading image plane buffer") || !bytes) return false;
            const jint row = env_->CallIntMethod(plane, planeRow_);
            if (Exception(error, "reading image row stride")) return false;
            const jint pixel = env_->CallIntMethod(plane, planePixel_);
            if (Exception(error, "reading image pixel stride")) return false;
            const jint position = env_->CallIntMethod(bytes, bufferPosition_);
            if (Exception(error, "reading image buffer position")) return false;
            const jint limit = env_->CallIntMethod(bytes, bufferLimit_);
            if (Exception(error, "reading image buffer limit")) return false;
            const auto* base = static_cast<const uint8_t*>(env_->GetDirectBufferAddress(bytes));
            const jlong capacity = env_->GetDirectBufferCapacity(bytes);
            if (Exception(error, "mapping image plane")) return false;
            if (!base || position < 0 || limit < position || limit > capacity || row <= 0 || pixel <= 0) {
                error = "MediaCodec returned an invalid image plane"; return false;
            }
            planes[p] = {{base + position, size_t(limit - position)}, uint32_t(row), uint32_t(pixel)};
        }
        return CopyYuv420Frame(planes, uint32_t(w), uint32_t(h), uint32_t(left), uint32_t(top),
                              uint32_t(right - left), uint32_t(bottom - top), frame, error);
    }

    // All output ownership is returned even if copying/allocating a frame fails.
    bool ReadOutput(jint index, jint flags, Nv12Frame& frame, std::string& error) {
        jobject image = nullptr;
        bool ok = false;
        try {
            image = env_->CallObjectMethod(codec_, outputImage_, index);
            if (!Exception(error, "getting decoded image")) {
                if (image) ok = CopyImage(image, frame, error);
                else error = "MediaCodec offers no readable YUV420 image";
            }
        } catch (const std::exception& e) { error = e.what(); }
        if (image) {
            env_->CallVoidMethod(image, imageClose_);
            if (Exception(error, "closing decoded image")) ok = false;
        }
        env_->CallVoidMethod(codec_, releaseOutput_, index, jboolean(false));
        if (Exception(error, "releasing decoded image")) ok = false;
        if (flags & kEndOfStream) outputEos_ = true;
        return ok;
    }

    bool ReadPicture(Nv12Frame& frame, std::string& error) {
        if (outputEos_) return false;
        auto progress = std::chrono::steady_clock::now();
        jlong waitUs = 0;
        while (!StopRequested()) {
            const jint index = env_->CallIntMethod(codec_, dequeueOutput_, bufferInfo_, waitUs);
            if (Exception(error, "dequeueing decoded picture")) return false;
            if (index >= 0) {
                const jint flags = env_->GetIntField(bufferInfo_, outFlags_);
                const jint size = env_->GetIntField(bufferInfo_, outSize_);
                const jlong timestamp = env_->GetLongField(bufferInfo_, outPts_);
                if (Exception(error, "reading decoded picture info")) return false;
                if (size > 0 && !(flags & kCodecConfig)) {
                    if (!ReadOutput(index, flags, frame, error)) return false;
                    if (timestamp < std::numeric_limits<int64_t>::min() + biasUs_ ||
                        !Rescale(timestamp - biasUs_, 1000000, track_->timescale, frame.pts) || frame.pts < lastPts_) {
                        error = "MediaCodec returned invalid presentation timestamps"; return false;
                    }
                    lastPts_ = frame.pts;
                    return true;
                }
                env_->CallVoidMethod(codec_, releaseOutput_, index, jboolean(false));
                if (Exception(error, "releasing empty codec output")) return false;
                if (flags & kEndOfStream) {
                    outputEos_ = true;
                    if (!delivered_) error = "MediaCodec produced no pictures";
                    return false;
                }
                progress = std::chrono::steady_clock::now();
            } else if (index == kFormatChanged) {
                // Image.getCropRect()/getPlanes() describe each actual output.
                progress = std::chrono::steady_clock::now();
            } else if (index != kTryAgain && index != kBuffersChanged) {
                error = "unexpected MediaCodec output status"; return false;
            }
            if (!inputEos_) {
                const jint input = env_->CallIntMethod(codec_, dequeueInput_, jlong(0));
                if (Exception(error, "dequeueing H.264 input")) return false;
                if (input >= 0) {
                    if (!Feed(input, error)) return false;
                    progress = std::chrono::steady_clock::now();
                    waitUs = 0;
                    continue;
                }
                if (input != kTryAgain) { error = "unexpected MediaCodec input status"; return false; }
            }
            if (std::chrono::steady_clock::now() - progress > kNoProgressTimeout) {
                error = "MediaCodec made no progress for 5 seconds"; return false;
            }
            waitUs = 10000; // bounded wait, not a busy loop; stop is checked next
        }
        return false;
    }

    bool preferHardware_, initialized_ = false, started_ = false, delivered_ = false;
    bool inputEos_ = false, outputEos_ = false;
    JNIEnv* env_ = nullptr; // refreshed per entry point; never used concurrently
    std::vector<jclass> types_;
    jclass codecType_ = nullptr, formatType_ = nullptr, listType_ = nullptr, infoType_ = nullptr;
    jobject codec_ = nullptr, bufferInfo_ = nullptr;
    jint sdk_ = 0, maxInput_ = 0;
    const Mp4Track* track_ = nullptr;
    ByteSource* source_ = nullptr;
    std::vector<Candidate> candidates_;
    Candidate current_{"unopened", Acceleration::Unknown};
    size_t nextCandidate_ = 0, next_ = 0;
    int64_t biasUs_ = 0, lastPts_ = std::numeric_limits<int64_t>::min();
    std::string name_ = "MediaCodec H.264", pendingError_;
    std::vector<uint8_t> raw_, annexb_, csd_[2];
    jmethodID createCodec_{}, configure_{}, start_{}, stop_{}, release_{}, dequeueInput_{}, inputBuffer_{}, queueInput_{};
    jmethodID dequeueOutput_{}, outputImage_{}, releaseOutput_{};
    jmethodID createFormat_{}, setInt_{}, setFloat_{}, setBuffer_{};
    jmethodID listInit_{}, listInfos_{}, codecName_{}, isEncoder_{}, codecTypes_{}, codecCaps_{}, supports_{};
    jmethodID isHardware_{}, isSoftware_{}, infoInit_{}, imageFormat_{}, imageWidth_{}, imageHeight_{}, imageCrop_{};
    jmethodID imagePlanes_{}, imageClose_{}, planeBuffer_{}, planeRow_{}, planePixel_{}, bufferPosition_{}, bufferLimit_{};
    jfieldID outSize_{}, outPts_{}, outFlags_{}, rectLeft_{}, rectTop_{}, rectRight_{}, rectBottom_{};
};
}

std::unique_ptr<MovieDecoder> CreateAndroidMovieDecoder(bool preferHardware) {
    return std::make_unique<AndroidDecoder>(preferHardware);
}
}
#endif
