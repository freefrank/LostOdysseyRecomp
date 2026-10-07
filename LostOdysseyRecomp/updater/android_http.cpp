#if defined(__ANDROID__)
#include "http.h"

#include <SDL3/SDL.h>
#include <jni.h>

#include <fstream>
#include <string_view>
#include <vector>

// HTTPS through java.net.HttpURLConnection over JNI: the platform's TLS and
// certificate store, no libcurl. SDL attaches the calling thread to the VM.
// The shader pack download and the app update check use it.
namespace updater
{
namespace
{
constexpr std::string_view UserAgent = "LostOdysseyRecomp-Updater/1.0";

bool IsHttpsUrl(std::string_view url)
{
    constexpr std::string_view prefix = "https://";
    return url.size() >= prefix.size() && url.substr(0, prefix.size()) == prefix;
}

// Deletes a local reference when the scope ends.
struct Local
{
    JNIEnv *env;
    jobject object;
    Local(JNIEnv *e, jobject o) : env(e), object(o) {}
    Local(const Local &) = delete;
    Local &operator=(const Local &) = delete;
    ~Local() { if (object) env->DeleteLocalRef(object); }
    explicit operator bool() const { return object != nullptr; }
};

std::string Utf8(JNIEnv *env, jobject text)
{
    std::string result;
    if (!text) return result;
    if (const char *chars = env->GetStringUTFChars(static_cast<jstring>(text), nullptr))
    {
        result = chars;
        env->ReleaseStringUTFChars(static_cast<jstring>(text), chars);
    }
    return result;
}

// A pending Java exception becomes `error` and is cleared.
bool Failed(JNIEnv *env, std::string &error, std::string_view what)
{
    if (!env->ExceptionCheck()) return false;
    Local exception(env, env->ExceptionOccurred());
    env->ExceptionClear();
    error = what;
    Local type(env, env->FindClass("java/lang/Throwable"));
    const jmethodID toString = type && exception
        ? env->GetMethodID(static_cast<jclass>(type.object), "toString", "()Ljava/lang/String;") : nullptr;
    if (toString)
    {
        Local message(env, env->CallObjectMethod(exception.object, toString));
        if (env->ExceptionCheck()) env->ExceptionClear();
        else if (const auto text = Utf8(env, message.object); !text.empty()) error += ": " + text;
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    return true;
}

// One GET request; Read() returns the body chunk by chunk.
class Connection
{
public:
    explicit Connection(JNIEnv *env) : env_(env) {}
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;
    ~Connection()
    {
        if (stream_)
        {
            env_->CallVoidMethod(stream_, closeStream_);
            if (env_->ExceptionCheck()) env_->ExceptionClear();
            env_->DeleteLocalRef(stream_);
        }
        if (buffer_) env_->DeleteLocalRef(buffer_);
        if (connection_)
        {
            env_->CallVoidMethod(connection_, disconnect_);
            if (env_->ExceptionCheck()) env_->ExceptionClear();
            env_->DeleteLocalRef(connection_);
        }
    }

    // Opens `url`, then checks the status and the final (redirected) scheme.
    bool Open(std::string_view url, int readTimeoutMs, std::string &error)
    {
        if (!IsHttpsUrl(url)) { error = "update URL does not use HTTPS"; return false; }
        Local urlClass(env_, env_->FindClass("java/net/URL"));
        Local connectionClass(env_, env_->FindClass("java/net/HttpURLConnection"));
        Local streamClass(env_, env_->FindClass("java/io/InputStream"));
        if (Failed(env_, error, "Java networking classes are unavailable")) return false;
        const auto urlType = static_cast<jclass>(urlClass.object);
        const auto connectionType = static_cast<jclass>(connectionClass.object);
        const auto streamType = static_cast<jclass>(streamClass.object);
        const jmethodID urlInit = env_->GetMethodID(urlType, "<init>", "(Ljava/lang/String;)V");
        const jmethodID openConnection = env_->GetMethodID(urlType, "openConnection", "()Ljava/net/URLConnection;");
        const jmethodID getProtocol = env_->GetMethodID(urlType, "getProtocol", "()Ljava/lang/String;");
        const jmethodID setConnectTimeout = env_->GetMethodID(connectionType, "setConnectTimeout", "(I)V");
        const jmethodID setReadTimeout = env_->GetMethodID(connectionType, "setReadTimeout", "(I)V");
        const jmethodID setFollow = env_->GetMethodID(connectionType, "setInstanceFollowRedirects", "(Z)V");
        const jmethodID setProperty = env_->GetMethodID(connectionType, "setRequestProperty",
                                                        "(Ljava/lang/String;Ljava/lang/String;)V");
        const jmethodID responseCode = env_->GetMethodID(connectionType, "getResponseCode", "()I");
        const jmethodID getUrl = env_->GetMethodID(connectionType, "getURL", "()Ljava/net/URL;");
        const jmethodID getStream = env_->GetMethodID(connectionType, "getInputStream", "()Ljava/io/InputStream;");
        disconnect_ = env_->GetMethodID(connectionType, "disconnect", "()V");
        read_ = env_->GetMethodID(streamType, "read", "([B)I");
        closeStream_ = env_->GetMethodID(streamType, "close", "()V");
        if (Failed(env_, error, "Java networking methods are unavailable")) return false;

        Local text(env_, env_->NewStringUTF(std::string(url).c_str()));
        Local target(env_, text ? env_->NewObject(urlType, urlInit, text.object) : nullptr);
        if (Failed(env_, error, "invalid download URL") || !target) return false;
        connection_ = env_->CallObjectMethod(target.object, openConnection);
        if (Failed(env_, error, "could not open the connection") || !connection_) return false;
        if (!env_->IsInstanceOf(connection_, connectionType)) { error = "download URL is not HTTP"; return false; }
        Local agentKey(env_, env_->NewStringUTF("User-Agent"));
        Local agent(env_, env_->NewStringUTF(std::string(UserAgent).c_str()));
        env_->CallVoidMethod(connection_, setConnectTimeout, jint(8000));
        env_->CallVoidMethod(connection_, setReadTimeout, jint(readTimeoutMs));
        // HttpURLConnection never follows a redirect to another protocol.
        env_->CallVoidMethod(connection_, setFollow, jboolean(JNI_TRUE));
        env_->CallVoidMethod(connection_, setProperty, agentKey.object, agent.object);
        if (Failed(env_, error, "could not configure the connection")) return false;
        const jint status = env_->CallIntMethod(connection_, responseCode);
        if (Failed(env_, error, "request failed")) return false;
        Local finalUrl(env_, env_->CallObjectMethod(connection_, getUrl));
        Local protocol(env_, finalUrl ? env_->CallObjectMethod(finalUrl.object, getProtocol) : nullptr);
        if (Failed(env_, error, "request failed")) return false;
        if (Utf8(env_, protocol.object) != "https") { error = "update redirect resolved to non-HTTPS URL"; return false; }
        if (status != 200) { error = "update server returned HTTP " + std::to_string(status); return false; }
        stream_ = env_->CallObjectMethod(connection_, getStream);
        if (Failed(env_, error, "could not read the response") || !stream_) return false;
        buffer_ = env_->NewByteArray(jsize(chunk_.size()));
        return !Failed(env_, error, "out of memory") && buffer_;
    }

    // Bytes now in Chunk(); 0 at the end of the body; -1 on an error.
    int Read(std::string &error)
    {
        const jint count = env_->CallIntMethod(stream_, read_, buffer_);
        if (Failed(env_, error, "download interrupted")) return -1;
        if (count <= 0) return 0;
        env_->GetByteArrayRegion(buffer_, 0, count, reinterpret_cast<jbyte *>(chunk_.data()));
        return count;
    }
    const char *Chunk() const { return chunk_.data(); }

private:
    JNIEnv *env_;
    jobject connection_ = nullptr;
    jobject stream_ = nullptr;
    jbyteArray buffer_ = nullptr;
    jmethodID disconnect_ = nullptr, read_ = nullptr, closeStream_ = nullptr;
    std::vector<char> chunk_ = std::vector<char>(256 * 1024);
};

// Runs `body` in its own local reference frame on an attached thread.
template <typename Body>
bool WithJava(std::string &error, Body &&body)
{
    auto *env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    if (!env) { error = "Java VM unavailable"; return false; }
    if (env->PushLocalFrame(32) != 0)
    {
        Failed(env, error, "out of memory");
        return false;
    }
    bool ok = false;
    {
        Connection connection(env);
        ok = body(connection);
    }
    env->PopLocalFrame(nullptr);
    return ok;
}
}

bool ReadResponse(std::string_view url, size_t limit, std::string &body, std::string &error)
{
    body.clear();
    return WithJava(error, [&](Connection &connection) {
        if (!connection.Open(url, 8000, error)) return false;
        for (;;)
        {
            const int count = connection.Read(error);
            if (count < 0) return false;
            if (count == 0) return true;
            if (size_t(count) > limit - body.size()) { error = "update response exceeded its size limit"; return false; }
            body.append(connection.Chunk(), size_t(count));
        }
    });
}

bool DownloadFile(std::string_view url, const std::filesystem::path &destination, uint64_t expectedSize,
                  const DownloadProgress &progress, std::string &error, bool &cancelled)
{
    cancelled = false;
    if (!IsHttpsUrl(url)) { error = "update URL does not use HTTPS"; return false; }
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) { error = "could not create update download"; return false; }
    const bool ok = WithJava(error, [&](Connection &connection) {
        // No overall limit for large files, but a stalled transfer fails.
        if (!connection.Open(url, 30000, error)) return false;
        uint64_t total = 0;
        for (;;)
        {
            const int count = connection.Read(error);
            if (count < 0) return false;
            if (count > 0)
            {
                output.write(connection.Chunk(), count);
                if (!output) { error = "could not write update download"; return false; }
                total += uint64_t(count);
            }
            // As with curl, the callback sees every chunk and can stop the transfer.
            if (!progress(total, expectedSize))
            {
                cancelled = true;
                error = "update cancelled by user";
                return false;
            }
            if (count == 0) return true;
        }
    });
    if (!ok) return false;
    output.flush();
    if (!output) { error = "could not write update download"; return false; }
    return true;
}

bool Download(std::string_view, const std::filesystem::path &, uint64_t, ProgressWindow &, std::string &error,
              bool &cancelled)
{
    cancelled = false;
    error = "app updates are installed manually on Android";
    return false;
}
}
#endif
