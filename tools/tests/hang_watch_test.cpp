// os::hang_watch::Watch with short deadlines: a quick step stays silent, a late
// step reports its context, its stack and the foreign modules, then its total
// time once the next step starts. No GPU, window or game data.
#include <os/hang_watch.h>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
void Require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
std::string Read(FILE *file)
{
    std::fflush(file);
    const long size = std::ftell(file);
    std::string text(size_t(size > 0 ? size : 0), '\0');
    std::fseek(file, 0, SEEK_SET);
    if (size > 0) std::fread(text.data(), 1, size_t(size), file);
    std::fseek(file, 0, SEEK_END);
    return text;
}
__declspec(noinline) void LateStep()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
}
} // namespace

int main()
{
    try
    {
        const auto path = std::filesystem::temp_directory_path() / "lo-hang-watch-test.log";
        FILE *file = std::fopen(path.string().c_str(), "wb+");
        Require(file != nullptr, "open test log");
        os::logger::g_file = file;
        {
            using namespace std::chrono_literals;
            os::hang_watch::Watch watch("quick step", [] { return std::string("test context"); }, 150ms, 1000ms);
            std::this_thread::sleep_for(20ms);
            watch.Step("late step");
            LateStep();
            watch.Step("last quick step");
        }
        const auto text = Read(file);
        os::logger::g_file = nullptr;
        std::fclose(file);
        std::filesystem::remove(path);
        Require(text.find("quick step still running") == std::string::npos, "quick steps are silent");
        Require(text.find("hang watch: late step still running after 0 s; test context") != std::string::npos,
                "a late step is reported with its context");
        Require(text.find("hang watch: late step stack: #0 ") != std::string::npos, "a late step logs its stack");
        Require(text.find("LoHangWatchTest.exe+") != std::string::npos, "the stack reaches the watched thread's code");
        Require(text.find("hang watch: modules from outside Windows and the game folder:") != std::string::npos,
                "the first report lists foreign modules");
        Require(text.find("hang watch: late step finished after 0 s") != std::string::npos,
                "a reported step logs its total time");
        Require(text.find("last quick step") == std::string::npos, "the next quick step stays silent");
        std::puts("PASS hang watch: silent quick steps, late-step context, stack, modules and total time");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
