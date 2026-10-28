#include "discord/client.h"
#include "discord/ipc_transport.h"
#include "discord/protocol.h"
#include "os/json.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

using namespace std::chrono_literals;
using namespace discord_rpc;
using Json = nlohmann::json;

namespace
{
void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

template<class Predicate>
bool Wait(Predicate predicate, std::chrono::milliseconds timeout = 2500ms)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate())
    {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(2ms);
    }
    return true;
}

std::string PrivateEndpoint()
{
    static std::atomic<unsigned> counter{0};
#ifdef _WIN32
    return "\\\\.\\pipe\\lo-discord-test-" + std::to_string(ProcessId()) + "-" + std::to_string(++counter);
#else
    return "/tmp/lo-discord-test-" + std::to_string(ProcessId()) + "-" + std::to_string(++counter);
#endif
}

enum class Behavior { Normal, Silent, AssetError, Close, Malformed };

class FakeServer
{
public:
    explicit FakeServer(std::string endpoint = PrivateEndpoint(), Behavior behavior = Behavior::Normal)
        : endpoint_(std::move(endpoint)), behavior_(behavior)
    {
#ifdef _WIN32
        pipe_ = CreateNamedPipeA(endpoint_.c_str(), PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT, 1, 65536, 65536, 0, nullptr);
        Check(pipe_ != INVALID_HANDLE_VALUE, "create private named pipe");
#else
        listener_ = socket(AF_UNIX, SOCK_STREAM, 0);
        Check(listener_ >= 0, "create private socket");
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        Check(endpoint_.size() < sizeof(address.sun_path), "private socket length");
        std::memcpy(address.sun_path, endpoint_.c_str(), endpoint_.size() + 1);
        Check(bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind private socket");
        Check(listen(listener_, 1) == 0, "listen private socket");
        fcntl(listener_, F_SETFL, O_NONBLOCK);
#endif
        thread_ = std::thread([this] { Run(); });
    }

    ~FakeServer()
    {
        stopping_ = true;
        thread_.join();
#ifdef _WIN32
        CloseHandle(pipe_);
#else
        if (peer_ >= 0) close(peer_);
        close(listener_);
        unlink(endpoint_.c_str());
#endif
    }

    const std::string& Endpoint() const { return endpoint_; }

    std::vector<Frame> Frames() const
    {
        std::lock_guard lock(mutex_);
        return frames_;
    }

    std::vector<Json> Activities() const
    {
        std::vector<Json> result;
        for (const auto& frame : Frames())
        {
            if (frame.opcode != Opcode::Frame) continue;
            auto value = Json::parse(frame.payload);
            if (value.value("cmd", "") == "SET_ACTIVITY") result.push_back(std::move(value));
        }
        return result;
    }

    size_t Handshakes() const
    {
        const auto frames = Frames();
        return size_t(std::count_if(frames.begin(), frames.end(), [](const auto& frame) {
            return frame.opcode == Opcode::Handshake;
        }));
    }

    std::vector<std::chrono::steady_clock::time_point> HandshakeTimes() const
    {
        std::lock_guard lock(mutex_);
        return handshakeTimes_;
    }

private:
    bool Accept()
    {
#ifdef _WIN32
        const auto result = ConnectNamedPipe(pipe_, nullptr);
        return result || GetLastError() == ERROR_PIPE_CONNECTED;
#else
        peer_ = accept(listener_, nullptr, nullptr);
        if (peer_ < 0) return false;
        fcntl(peer_, F_SETFL, O_NONBLOCK);
#ifdef SO_NOSIGPIPE
        int noSignal = 1;
        setsockopt(peer_, SOL_SOCKET, SO_NOSIGPIPE, &noSignal, sizeof(noSignal));
#endif
        return true;
#endif
    }

    int Read(std::span<uint8_t> bytes)
    {
#ifdef _WIN32
        DWORD count = 0;
        if (ReadFile(pipe_, bytes.data(), DWORD(bytes.size()), &count, nullptr)) return int(count);
        return GetLastError() == ERROR_NO_DATA ? 0 : -1;
#else
        const auto count = recv(peer_, bytes.data(), bytes.size(), 0);
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
        return count > 0 ? int(count) : -1;
#endif
    }

    void Disconnect()
    {
#ifdef _WIN32
        DisconnectNamedPipe(pipe_);
#else
        if (peer_ >= 0) close(peer_);
        peer_ = -1;
#endif
    }

    void Send(Opcode opcode, const std::string& payload)
    {
        const auto bytes = EncodeFrame(opcode, payload);
        // Deliberately fragment both the binary header and JSON payload.
        for (size_t offset = 0; offset < bytes.size();)
        {
            const auto length = std::min(size_t(3), bytes.size() - offset);
#ifdef _WIN32
            DWORD count = 0;
            if (!WriteFile(pipe_, bytes.data() + offset, DWORD(length), &count, nullptr) || !count) return;
#else
#ifdef MSG_NOSIGNAL
            constexpr int flags = MSG_NOSIGNAL;
#else
            constexpr int flags = 0;
#endif
            const auto count = send(peer_, bytes.data() + offset, length, flags);
            if (count <= 0) return;
#endif
            offset += size_t(count);
        }
    }

    void Handle(const Frame& frame)
    {
        {
            std::lock_guard lock(mutex_);
            frames_.push_back(frame);
            if (frame.opcode == Opcode::Handshake)
                handshakeTimes_.push_back(std::chrono::steady_clock::now());
        }
        if (behavior_ == Behavior::Silent) return;
        if (frame.opcode == Opcode::Handshake)
        {
            if (behavior_ == Behavior::Malformed) { Send(Opcode::Frame, "{"); return; }
            Send(Opcode::Frame, R"({"cmd":"DISPATCH","evt":"READY","data":{"v":1,"user":{"username":"not retained"}}})");
            Send(Opcode::Ping, R"({"ping":123})");
        }
        if (frame.opcode == Opcode::Frame)
        {
            const auto command = Json::parse(frame.payload);
            if (command["args"]["activity"].is_null()) return;
            if (behavior_ == Behavior::Close) { Send(Opcode::Close, R"({"code":1000})"); return; }
            if (behavior_ == Behavior::AssetError && !rejectedAsset_)
            {
                rejectedAsset_ = true;
                Send(Opcode::Frame, Json{{"cmd", "SET_ACTIVITY"}, {"evt", "ERROR"},
                    {"nonce", command["nonce"]}, {"data", {{"code", 4000}, {"message", "Invalid large_image asset"}}}}.dump());
            }
            else Send(Opcode::Frame, Json{{"cmd", "SET_ACTIVITY"}, {"evt", nullptr},
                {"nonce", command["nonce"]}, {"data", nullptr}}.dump());
        }
    }

    void Run()
    {
        FrameDecoder decoder;
        std::array<uint8_t, 4096> incoming{};
        bool connected = false;
        while (!stopping_)
        {
            if (!connected) connected = Accept();
            if (connected)
            {
                const auto count = Read(incoming);
                if (count < 0)
                {
                    Disconnect();
                    decoder.Reset();
                    connected = false;
                }
                else if (count > 0)
                {
                    if (!decoder.Append(std::span(incoming.data(), size_t(count)))) return;
                    while (auto frame = decoder.Pop()) Handle(*frame);
                }
            }
            std::this_thread::sleep_for(2ms);
        }
    }

    std::string endpoint_;
    Behavior behavior_;
    bool rejectedAsset_ = false;
    std::atomic<bool> stopping_{false};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::vector<Frame> frames_;
    std::vector<std::chrono::steady_clock::time_point> handshakeTimes_;
#ifdef _WIN32
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
#else
    int listener_ = -1;
    int peer_ = -1;
#endif
};

ClientOptions Options(const std::string& endpoint)
{
    ClientOptions options;
    options.applicationId = "123456789012345678";
    options.endpoints = {endpoint};
    options.reconnect = 250ms;
    options.regularUpdate = 150ms;
    options.meaningfulUpdate = 50ms;
    options.handshakeTimeout = 120ms;
    options.ioTimeout = 10ms;
    return options;
}

void Protocol()
{
    const std::string payload = R"({"test":"line\n\"quoted\""})";
    const auto bytes = EncodeFrame(Opcode::Frame, payload);
    Check(bytes[0] == 1 && bytes[1] == 0 && bytes[4] == payload.size() && bytes[5] == 0, "little endian frame header");
    FrameDecoder decoder;
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        Check(decoder.Append(std::span(bytes.data() + i, 1)), "fragment append");
        if (i + 1 < bytes.size()) Check(!decoder.Pop(), "incomplete frame not emitted");
    }
    const auto frame = decoder.Pop();
    Check(frame && frame->opcode == Opcode::Frame && frame->payload == payload, "fragmented frame roundtrip");
    auto combined = bytes;
    combined.insert(combined.end(), bytes.begin(), bytes.end());
    Check(decoder.Append(combined) && decoder.Pop() && decoder.Pop() && !decoder.Pop(), "coalesced frames");
    const std::array<uint8_t, 8> oversized{1, 0, 0, 0, 1, 0, 1, 0};
    Check(!decoder.Append(oversized), "oversized payload rejected before allocation");
    decoder.Reset();
    const std::array<uint8_t, 8> invalidOpcode{255, 0, 0, 0, 0, 0, 0, 0};
    Check(!decoder.Append(invalidOpcode), "unknown opcode rejected");
    Check(EncodeFrame(Opcode::Frame, std::string(kMaxPayload + 1, 'x')).empty(), "oversized outgoing rejected");
    Check(ParseResponse("{").kind == Reply::Invalid && ParseResponse("[]").kind == Reply::Invalid, "malformed JSON rejected");
    Check(ParseResponse(R"({"evt":{},"nonce":3,"cmd":[]})").kind == Reply::Other, "wrong field types do not throw");
    const auto handshake = Json::parse(BuildHandshake("123456789012345678"));
    Check(handshake["v"] == 1 && handshake["client_id"] == "123456789012345678", "handshake payload");
    const auto clear = Json::parse(BuildClearActivity(42, 1));
    Check(clear["args"]["activity"].is_null() && clear["args"]["pid"] == 42, "clear payload");
}

void Mapping()
{
    const std::array modes{Mode::Exploring, Mode::Battle, Mode::Cutscene, Mode::Movie, Mode::Menu, Mode::Title};
    const std::array labels{"Exploring", "In battle", "Cutscene", "Watching a movie", "In the menu", "At the title screen"};
    for (size_t i = 0; i < modes.size(); ++i)
    {
        const auto value = Json::parse(BuildActivity({2, "Uhra - \"Great Gate\"", modes[i]}, 12345));
        Check(value["type"] == 0 && value["state"] == std::string("Disc 2 - ") + labels[i], "mode mapping");
        Check(value["timestamps"]["start"] == 12345 && value["assets"]["large_image"] == "logo", "elapsed and asset");
        Check(value["details"] == (modes[i] == Mode::Title ? "At the title screen" : "Uhra - \"Great Gate\""), "location and title privacy");
    }
    auto value = Json::parse(BuildActivity({0, "", Mode::Exploring}, 1, false));
    Check(value["details"] == "Lost Odyssey Recomp" && value["state"] == "Exploring" && !value.contains("assets"), "missing location/disc/image fallback");
    std::string unicode;
    for (int i = 0; i < 80; ++i) unicode += "乌";
    value = Json::parse(BuildActivity({1, unicode, Mode::Exploring}, 1));
    Check(value["details"].get<std::string>().size() == 126, "UTF-8 truncation keeps complete codepoints");
    value = Json::parse(BuildActivity({1, "Gate\n\t", Mode::Exploring}, 1));
    Check(value["details"] == "Gate", "display control characters removed");
}

void Policy()
{
    UpdatePolicy policy;
    const auto start = UpdatePolicy::Clock::time_point{};
    Snapshot snapshot{1, "Uhra", Mode::Exploring};
    Check(policy.Due(snapshot, start), "first update immediate");
    policy.Sent(snapshot, start);
    Check(!policy.Due(snapshot, start + 60s), "identical state never refreshes");
    snapshot.location = "Great Gate";
    Check(!policy.Due(snapshot, start + 14999ms) && policy.Due(snapshot, start + 15s), "location update rate limit");
    snapshot.mode = Mode::Battle;
    Check(!policy.Due(snapshot, start + 4999ms) && policy.Due(snapshot, start + 5s), "meaningful change rate limit");
    policy.Sent(snapshot, start + 5s);
    snapshot.mode = Mode::Exploring;
    Check(!policy.Due(snapshot, start + 9999ms), "state flapping bounded");
    policy.Reset();
    Check(policy.Due(snapshot, start), "new connection refreshes latest");
    Check(ClientOptions{}.reconnect >= 30s, "production reconnect never faster than 30 seconds");
#ifdef _WIN32
    const auto endpoints = IpcEndpoints();
    Check(endpoints.size() == 10 && endpoints.front() == "\\\\.\\pipe\\discord-ipc-0" && endpoints.back() == "\\\\.\\pipe\\discord-ipc-9", "named pipe paths");
#else
    const auto endpoints = IpcEndpoints();
    for (const auto* suffix : {"/discord-ipc-0", "/app/com.discordapp.Discord/discord-ipc-9", "/snap.discord/discord-ipc-0"})
        Check(std::any_of(endpoints.begin(), endpoints.end(), [&](const auto& path) { return path.ends_with(suffix); }), "Unix/native/Flatpak/Snap paths");
#endif
}

void Ipc()
{
    FakeServer server;
    Client client;
    for (int i = 0; i < 100; ++i) client.Publish({2, "Superseded " + std::to_string(i), Mode::Exploring});
    client.Publish({2, "Great Gate", Mode::Exploring});
    client.Start(Options(server.Endpoint()));
    Check(Wait([&] { return !server.Activities().empty(); }), "fake IPC receives activity after READY");
    Check(Wait([&] {
        const auto frames = server.Frames();
        return std::any_of(frames.begin(), frames.end(), [](const auto& frame) {
            return frame.opcode == Opcode::Pong && frame.payload == R"({"ping":123})";
        });
    }), "PING echoed as PONG");
    const auto activity = server.Activities().front();
    Check(activity["args"]["pid"] == ProcessId() && activity["args"]["activity"]["state"] == "Disc 2 - Exploring", "actual process and activity JSON");
    Check(activity["args"]["activity"]["details"] == "Great Gate", "preconnection snapshots coalesce to the latest location");
    // Unchanged snapshots never refresh, regardless of observer scheduling.
    // Policy() checks update deadlines with a controlled clock.
    std::this_thread::sleep_for(300ms);
    Check(server.Activities().size() == 1, "unchanged coalesced snapshot is not resent");
    for (int i = 0; i < 100; ++i) client.Publish({2, "Area " + std::to_string(i), Mode::Exploring});
    Check(Wait([&] {
        // Publish may drop a contended write; the game's next snapshot retries it.
        client.Publish({2, "Area 99", Mode::Exploring});
        const auto values = server.Activities();
        return values.size() >= 2 && values.back()["args"]["activity"]["details"] == "Area 99";
    }), "coalesced update sends latest location");
    client.Stop();
    Check(Wait([&] { return server.Activities().back()["args"]["activity"].is_null(); }), "clear on shutdown");

    FakeServer assetServer(PrivateEndpoint(), Behavior::AssetError);
    Client assetClient;
    assetClient.Publish({1, "Uhra", Mode::Exploring});
    assetClient.Start(Options(assetServer.Endpoint()));
    Check(Wait([&] { return assetServer.Activities().size() >= 2; }), "missing logo retries text presence");
    Check(!assetServer.Activities()[1]["args"]["activity"].contains("assets"), "logo fallback omits assets");
    assetClient.Stop();
}

void Lifecycle()
{
    FakeServer server;
    Client client;
    client.SetEnabled(false);
    client.Publish({1, "Uhra", Mode::Exploring});
    client.Start(Options(server.Endpoint()));
    std::this_thread::sleep_for(40ms);
    Check(server.Handshakes() == 0, "disabled startup does not connect");
    client.SetEnabled(true);
    Check(Wait([&] { return !server.Activities().empty(); }), "enable connects live");
    const auto timestamp = server.Activities().front()["args"]["activity"]["timestamps"]["start"];
    client.SetEnabled(false);
    Check(Wait([&] { return server.Activities().back()["args"]["activity"].is_null(); }), "disable clears live");
    const auto count = server.Activities().size();
    client.Publish({2, "Ipsilon", Mode::Battle});
    std::this_thread::sleep_for(40ms);
    Check(server.Activities().size() == count, "disabled publication does not send");
    client.SetEnabled(true);
    Check(Wait([&] {
        const auto values = server.Activities();
        return values.size() > count && !values.back()["args"]["activity"].is_null();
    }), "reenable reconnects and publishes latest");
    Check(server.Activities().back()["args"]["activity"]["timestamps"]["start"] == timestamp, "toggles preserve session start");
    std::thread a([&] { client.Stop(); });
    std::thread b([&] { client.Stop(); });
    a.join();
    b.join();
    client.Stop();
}

void Unresponsive()
{
    FakeServer server(PrivateEndpoint(), Behavior::Silent);
    Client client;
    const auto start = std::chrono::steady_clock::now();
    client.Start(Options(server.Endpoint()));
    for (int i = 0; i < 10000; ++i) client.Publish({1, "Uhra", Mode::Exploring});
    const auto publishTime = std::chrono::steady_clock::now() - start;
    Check(publishTime < 250ms, "unresponsive IPC never blocks publishing");
    Check(Wait([&] { return server.Handshakes() >= 1; }), "silent server received handshake");
    Check(server.Activities().empty(), "no activity before READY");
    const auto beforeStop = std::chrono::steady_clock::now();
    client.Stop();
    Check(std::chrono::steady_clock::now() - beforeStop < 500ms, "unresponsive shutdown is bounded");
    std::printf("10000 foreground snapshots: %.3f ms\n", std::chrono::duration<double, std::milli>(publishTime).count());

    FakeServer malformed(PrivateEndpoint(), Behavior::Malformed);
    Client malformedClient;
    malformedClient.Start(Options(malformed.Endpoint()));
    Check(Wait([&] { return malformed.Handshakes() >= 2; }), "malformed JSON closes and retries");
    malformedClient.Stop();

    FakeServer closing(PrivateEndpoint(), Behavior::Close);
    Client closingClient;
    closingClient.Publish({1, "Uhra", Mode::Exploring});
    closingClient.Start(Options(closing.Endpoint()));
    Check(Wait([&] { return closing.Handshakes() >= 2; }), "CLOSE closes and retries");
    closingClient.Stop();
}

void Retry()
{
    FakeServer closing(PrivateEndpoint(), Behavior::Close);
    Client retrying;
    const auto retryOptions = Options(closing.Endpoint());
    retrying.Publish({1, "Uhra", Mode::Exploring});
    retrying.Start(retryOptions);
    std::jthread publisher([&](std::stop_token stop) {
        while (!stop.stop_requested())
        {
            retrying.Publish({1, "Uhra", Mode::Exploring});
            std::this_thread::sleep_for(1ms);
        }
    });
    Check(Wait([&] { return closing.Handshakes() >= 1; }), "retry fixture observes the first connection");
    // Delay the observer beyond the reconnect period. A zero-handshake assertion
    // after sleeps would fail under this same scheduling delay; actual event
    // intervals remain meaningful even if several connections have occurred.
    std::this_thread::sleep_for(300ms);
    Check(Wait([&] { return closing.Handshakes() >= 2; }), "CLOSE reconnects while snapshots keep arriving");
    publisher.request_stop();
    publisher.join();
    retrying.Stop();
    const auto times = closing.HandshakeTimes();
    for (size_t i = 1; i < times.size(); ++i)
        Check(times[i] - times[i - 1] >= retryOptions.reconnect, "snapshots do not shorten the reconnect interval");

    const auto endpoint = PrivateEndpoint();
    Client client;
    client.Publish({1, "Uhra", Mode::Exploring});
    client.Start(Options(endpoint));
    std::this_thread::sleep_for(300ms);
    FakeServer server(endpoint);
    for (int i = 0; i < 1000; ++i) client.Publish({1, "Uhra", Mode::Exploring});
    Check(Wait([&] { return !server.Activities().empty(); }), "slow reconnect finds newly started Discord");
    client.Stop();

    Client absent;
    auto options = Options(PrivateEndpoint());
    options.reconnect = 30s;
    absent.Start(std::move(options));
    std::this_thread::sleep_for(20ms);
    const auto start = std::chrono::steady_clock::now();
    absent.Stop();
    Check(std::chrono::steady_clock::now() - start < 500ms, "shutdown interrupts slow absent-client timer");
}

std::mutex logMutex;
std::vector<Json> logs;
std::vector<std::string> diagnostics;
void CaptureLog(const char* value)
{
    std::lock_guard lock(logMutex);
    if (*value == '{') logs.push_back(Json::parse(value));
    else diagnostics.emplace_back(value);
}

size_t DiagnosticCount(std::string_view text)
{
    std::lock_guard lock(logMutex);
    return size_t(std::count_if(diagnostics.begin(), diagnostics.end(), [&](const auto& line) {
        return line.find(text) != std::string::npos;
    }));
}

void Diagnostics()
{
    const auto endpoint = PrivateEndpoint();
    auto options = Options(endpoint);
    options.reconnect = 20ms;
    Client client;
    client.Start(options, CaptureLog);
    Check(Wait([] { return DiagnosticCount("connect failed endpoint=") == 1; }), "absent endpoint error is logged");
    std::this_thread::sleep_for(100ms);
    Check(DiagnosticCount("connect failed endpoint=") == 1, "repeated connect error is rate limited");
    FakeServer server(endpoint, Behavior::AssetError);
    Check(Wait([] { return DiagnosticCount("READY but waiting for a game snapshot") == 1; }), "missing feed is diagnosable");
    Check(Wait([&] { client.Publish({1, "Uhra", Mode::Exploring}); return DiagnosticCount("activity accepted") == 1; }),
        "late endpoint connects and activity is acknowledged");
    client.Stop();
    Check(DiagnosticCount("connected endpoint=") == 1, "successful endpoint is logged");
    Check(DiagnosticCount("handshake sent") == 1 && DiagnosticCount("READY received") == 1, "handshake lifecycle is logged");
    Check(DiagnosticCount("first activity sent") == 1 && DiagnosticCount("activity clear sent") == 1, "send and clear are logged");
    Check(DiagnosticCount("Discord ERROR") == 1 && DiagnosticCount("Invalid large_image asset") == 1, "error code/message is logged");
    Check(DiagnosticCount("disconnected reason=shutdown") == 1, "disconnect reason is logged");
    Check(DiagnosticCount("not retained") == 0, "READY user data is never logged");
    const auto safe = DescribeError(R"({"evt":"ERROR","data":{"code":4000,"message":"bad\nasset","user":{"username":"private"}}})");
    Check(safe.find("4000") != std::string::npos && safe.find("badasset") != std::string::npos &&
        safe.find("private") == std::string::npos, "diagnostics retain bounded error fields without user data or controls");
}

void DryRun()
{
    Client client;
    auto options = Options(PrivateEndpoint());
    options.dryRun = true;
    client.Publish({3, "Numara", Mode::Cutscene});
    client.Start(std::move(options), CaptureLog);
    Check(Wait([] { std::lock_guard lock(logMutex); return !logs.empty(); }), "dry run emits activity without Discord");
    {
        std::lock_guard lock(logMutex);
        Check(logs.front()["args"]["activity"]["state"] == "Disc 3 - Cutscene", "dry run state JSON");
    }
    client.Stop();
    std::lock_guard lock(logMutex);
    Check(logs.back()["args"]["activity"].is_null(), "dry run clear is logged");
}
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "supply one scenario");
        const std::string scenario = argv[1];
        if (scenario == "protocol") Protocol();
        else if (scenario == "mapping") Mapping();
        else if (scenario == "policy") Policy();
        else if (scenario == "ipc") Ipc();
        else if (scenario == "lifecycle") Lifecycle();
        else if (scenario == "unresponsive") Unresponsive();
        else if (scenario == "retry") Retry();
        else if (scenario == "dry-run") DryRun();
        else if (scenario == "diagnostics") Diagnostics();
        else throw std::runtime_error("unknown scenario");
        std::printf("PASS %s\n", argv[1]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
