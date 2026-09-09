#include <os/log_file.h>
#include <os/shader_log.h>
#include <os/capture_archive.h>
#include <gpu/shader/dxc_compiler.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;
static void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
static std::string Read(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

static void CheckGroups(const fs::path& root)
{
    const auto logs = root / "retention";
    fs::create_directories(logs);
    for (const auto stamp : {1, 2, 3, 4, 5, 6})
    {
        std::ofstream(logs / ("runtime-" + std::to_string(stamp) + ".log")) << "runtime";
        std::ofstream(logs / ("shader-" + std::to_string(stamp) + ".jsonl")) << "shader";
    }
    // A shader-only remnant is one session; it must not leak forever.
    std::ofstream(logs / "shader-0.jsonl") << "orphan";
    std::ofstream(logs / "shader-custom.jsonl") << "keep custom path";
    os::shaderlog::Sink active;
    Require(active.Open(logs / "shader-2.jsonl", "2"), "open active old shader group");
    os::logger::PruneDefaultLogs(logs / "runtime-1.log");
    Require(fs::exists(logs / "runtime-2.log") && fs::exists(logs / "shader-2.jsonl"), "active shader protects entire session");
    for (const auto stamp : {1, 5, 6})
        Require(fs::exists(logs / ("runtime-" + std::to_string(stamp) + ".log")) &&
            fs::exists(logs / ("shader-" + std::to_string(stamp) + ".jsonl")), "current and newest two complete groups retained");
    for (const auto stamp : {0, 3, 4})
        Require(!fs::exists(logs / ("shader-" + std::to_string(stamp) + ".jsonl")), "old shader companions and orphans pruned");
    Require(!fs::exists(logs / "runtime-3.log") && !fs::exists(logs / "runtime-4.log"), "old runtime companions pruned");
    Require(fs::exists(logs / "shader-custom.jsonl"), "nondefault custom path preserved");
    Require(!active.Close(), "close active group");
    os::logger::PruneDefaultLogs(logs / "runtime-1.log");
    Require(!fs::exists(logs / "runtime-2.log") && !fs::exists(logs / "shader-2.jsonl"), "inactive old group retried");
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc == 2 || argc == 3, "supply a new output directory and optional exit/disabled mode");
        const auto root = fs::absolute(fs::u8path(argv[1]));
        Require(!fs::exists(root), "fixture output must not already exist");
        fs::create_directories(root / "logs");
        const auto runtime = root / "logs" / "runtime-4242.log";
        Require(os::logger::OpenFile(runtime), "open fixture runtime log");
        if (argc == 3)
        {
            SHADER_LOG_INFO("exit-flush", RendererByteFnv, "last buffered record \"{}\"", "尾部");
            if (std::string_view(argv[2]) == "quick-exit")
            {
                os::shaderlog::CloseForExit();
                std::_Exit(0);
            }
            return 0; // Exercise the actual normal-exit destructor, not Flush().
        }
        CheckGroups(root);
        const auto shader = root / "logs" / "shader-4242.jsonl";
        Require(os::shaderlog::Current().Path() == shader, "runtime and shader session paths match");
        LOG_INFO("runtime marker");
        SHADER_LOG_INFO("identity", RendererByteFnv, "hash={:016x} escaped=\"\\\r\n\t{}", 42u, "中文");
        SHADER_LOG_INFO("identity", CommandWordFnv, "hash={:016x}", 42u);
        SHADER_LOG_INFO("malformed-utf8", None, "diagnostic {}", std::string("\xff\xc0\xaf\xed\xa0\x80", 6));
        SHADER_LOG_WARNING("compile-failed", RendererByteFnv, "vertex hash={:016x}\ncompiler full detail\nlast diagnostic", 42u);
        // Warning flushes the entire prefix before returning.
        const auto flushed = Read(shader);
        Require(flushed.find("last diagnostic") != std::string::npos, "complete compiler diagnostics flushed");
        Require(Read(runtime).find("compiler full detail") == std::string::npos, "runtime contains only failure summary");
        Require(Read(runtime).find("vertex hash=000000000000002a") != std::string::npos, "runtime summary retains shader identity");

        const auto snapshot = root / "snapshot.jsonl";
        Require(!os::shaderlog::Current().Snapshot(snapshot), "snapshot active shader sink");
        const auto before = Read(shader);
        Require(bool(os::shaderlog::Current().Snapshot(shader)), "reject self snapshot");
        const auto alias = root / "shader-alias.jsonl";
        fs::create_hard_link(shader, alias);
        Require(bool(os::shaderlog::Current().Snapshot(alias)), "reject hardlink snapshot");
        Require(bool(os::shaderlog::Current().Snapshot(root / "missing" / "snapshot.jsonl")), "report unavailable destination");
        Require(Read(shader) == before, "failed snapshots preserve source");

        std::vector<std::jthread> writers;
        for (unsigned thread = 0; thread < 4; ++thread)
            writers.emplace_back([thread] {
                for (unsigned i = 0; i < 256; ++i)
                    SHADER_LOG_INFO("thread-record", RendererByteFnv, "writer={} item={}", thread, i);
            });
        for (unsigned i = 0; i < 8; ++i)
        {
            const auto concurrent = root / ("concurrent-" + std::to_string(i) + ".jsonl");
            Require(!os::shaderlog::Current().Snapshot(concurrent), "snapshot while compiler threads log");
            const auto contents = Read(concurrent);
            Require(!contents.empty() && contents.back() == '\n', "concurrent snapshots end at complete records");
        }
        for (auto& writer : writers) writer.join();
        Require(xenos::DxcAvailable(), "fixture requires a configured real DXC");
        const auto failed = xenos::CompileHlsl("float4 main() : SV_Target { this_is_not_hlsl }", "main", "ps_6_0");
        Require(!failed.ok && !failed.errors.empty(), "real compiler rejected fixture source");
        const auto success = xenos::CompileHlsl("float4 main() : SV_Target { return 1; }", "main", "ps_6_0");
        Require(success.ok && !success.bytecode.empty(), "real compiler produced fixture shader");

        const auto capture = root / "captures" / "render-shader-log";
        fs::create_directories(capture);
        Require(!os::logger::SnapshotFile(capture / "runtime.log"), "capture runtime snapshot");
        os::shaderlog::CaptureSnapshot(capture, 1234);
        Require(Read(capture / "shader-log-status.txt").find("status=included") != std::string::npos, "shader snapshot status included");
        const auto expected = Read(capture / "shader.jsonl");
        Require(!os::shaderlog::Current().Snapshot(snapshot), "final external snapshot");
        Require(Read(snapshot) == expected, "F1 helper copies complete shader log");
        Require(os::StartCaptureArchive(capture).get().saved, "F1 archive publishes shader snapshot");
        Require(!os::shaderlog::Current().Close(), "explicit shutdown flush succeeds");

        const auto absent = root / "captures" / "render-no-shader-log";
        fs::create_directories(absent);
        os::shaderlog::CaptureSnapshot(absent, 1235);
        Require(Read(absent / "shader-log-status.txt").find("status=unavailable") != std::string::npos, "absent log recorded");
        Require(!fs::exists(absent / "shader.jsonl"), "absent log creates no false snapshot");
        Require(os::StartCaptureArchive(absent).get().saved, "missing shader log does not discard capture");
        SHADER_LOG_ERROR("sink-unavailable", RendererByteFnv, "fallback hash={:016x}\ncomplete fallback diagnostic", 43u);
        Require(Read(runtime).find("complete fallback diagnostic") != std::string::npos, "unavailable sink preserves full error in runtime");
        std::cout << "shader log fixture passed (1024 concurrent records; real DXC rejection/success; two capture archives)\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "shader log fixture failed: " << e.what() << '\n';
        return 1;
    }
}
