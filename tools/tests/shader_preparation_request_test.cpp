#include "gpu/shader_preparation_request.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

static void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static std::string Read(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

int main()
{
    const auto root = fs::temp_directory_path() /
        ("lo-shader-preparation-request-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directories(root);
        const auto request = root / "config/shader-preparation-requested.txt";
        const auto pack = root / "shaders/portable_vk.lospv";
        const auto cache = root / "cache/shaders/pipeline_cache_vk.bin";
        fs::create_directories(pack.parent_path());
        fs::create_directories(cache.parent_path());
        { std::ofstream(pack) << "installed pack"; }
        { std::ofstream(cache) << "driver cache"; }

        Check(!gpu::shader_preparation::PendingNextStart(request), "missing request is pending");
        Check(gpu::shader_preparation::RequestNextStart(request), "request was not persisted");
        Check(gpu::shader_preparation::PendingNextStart(request), "persisted request was not found");
        Check(gpu::shader_preparation::PendingNextStart(request) && fs::exists(request),
              "inspecting request before pipeline preparation consumed it");
        Check(gpu::shader_preparation::ConsumeNextStart(request), "pending request was not consumed");
        Check(!gpu::shader_preparation::PendingNextStart(request) &&
              !gpu::shader_preparation::ConsumeNextStart(request), "request was consumed twice");
        Check(Read(pack) == "installed pack" && Read(cache) == "driver cache",
              "request changed shader pack or driver cache");

        const auto blocked = root / "blocked";
        { std::ofstream(blocked) << "unchanged"; }
        Check(!gpu::shader_preparation::RequestNextStart(blocked / "request.txt"),
              "request reported success in an unwritable location");
        Check(Read(blocked) == "unchanged", "failed request changed the blocking file");
        fs::remove_all(root);
        std::cout << "PASS shader preparation request persistence, one-shot consumption, cache preservation, write failure\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL shader preparation request: " << e.what() << '\n';
        std::error_code ignored;
        fs::remove_all(root, ignored);
        return 1;
    }
}
