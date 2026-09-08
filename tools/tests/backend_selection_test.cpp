#include "gpu/backend_selection.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <stdexcept>
using namespace gpu::backend;
int main() {
    assert(Parse("DX11") == Backend::D3D11 && Parse("d3d11") == Backend::D3D11);
    assert(Parse("VULKAN") == Backend::Vulkan && Parse("DX12") == Backend::D3D12);
    assert(!Parse("auto") && !Parse("bogus") && !Parse(""));
    assert(Requested(Backend::Vulkan, nullptr) == Backend::Vulkan);
    assert(Requested(Backend::Vulkan, "auto") == Backend::Vulkan);
    assert(Requested(Backend::Vulkan, "d3d12") == Backend::D3D12);
    assert(!Requested(Backend::Vulkan, "bad"));
    Capabilities c; c.device = c.geometryShader = c.bufferDeviceAddress = c.shaderInt64 = c.scalarBlockLayout = true;
    c.apiVersion = (1u << 22) | (2u << 12); c.shaderModel = 0x60; c.bindingTier = 2;
    c.boundSets = 5; c.samplers = 32; c.sampledImages = 96; c.storageBuffers = 1; c.pushConstants = 24;
    assert(Missing(Backend::D3D12, c).empty() && Missing(Backend::Vulkan, c).empty());
    assert(Missing(Backend::D3D11, c).starts_with("Unsupported"));
    for (auto member : {&Capabilities::device, &Capabilities::geometryShader, &Capabilities::bufferDeviceAddress,
                       &Capabilities::shaderInt64, &Capabilities::scalarBlockLayout}) {
        auto bad = c; bad.*member = false; assert(!Missing(Backend::Vulkan, bad).empty());
    }
    for (auto member : {&Capabilities::apiVersion, &Capabilities::boundSets, &Capabilities::samplers,
                       &Capabilities::sampledImages, &Capabilities::storageBuffers, &Capabilities::pushConstants}) {
        auto bad = c; --(bad.*member); assert(!Missing(Backend::Vulkan, bad).empty());
    }
    for (auto member : {&Capabilities::shaderModel, &Capabilities::bindingTier}) {
        auto bad = c; --(bad.*member); assert(!Missing(Backend::D3D12, bad).empty());
    }
    // Simulate failure at every owned startup layer, including exceptions before
    // renderer publication. The second backend must observe no prior children.
    for (auto first : {Backend::D3D12, Backend::Vulkan, Backend::D3D11}) {
        for (unsigned failAt = 1; failAt <= 7; ++failAt) {
            int attempts = 0, resets = 0; std::vector<std::unique_ptr<int>> children;
            auto result = Select(first, [&](Backend b) -> std::string {
                assert(b != Backend::D3D11 && children.empty()); ++attempts;
                for (unsigned stage = 1; stage <= 7; ++stage) {
                    children.push_back(std::make_unique<int>(stage));
                    if (attempts == 1 && stage == failAt) {
                        if (stage & 1) throw std::runtime_error("injected initialization failure");
                        return "injected missing resource";
                    }
                }
                return {};
            }, [&] { ++resets; children.clear(); });
            assert(result.selected && attempts == 2 && resets == 1 && children.size() == 7);
            assert(result.requested == first && result.attempts.size() == (first == Backend::D3D11 ? 3 : 2));
            assert(*result.selected == (first == Backend::Vulkan ? Backend::D3D12 : Backend::Vulkan));
        }
    }
    int attempts = 0, resets = 0;
    auto failed = Select(Backend::D3D11, [&](Backend) { ++attempts; return "unavailable"; }, [&] { ++resets; });
    assert(!failed.selected && attempts == 2 && resets == 2 && failed.attempts.size() == 3);
    assert(failed.Describe().find("Unsupported") != std::string::npos);
    attempts = resets = 0;
    auto ready = Select(Backend::D3D11, [&](Backend b) { ++attempts; assert(b == Backend::D3D12); return std::string{}; }, [&] { ++resets; });
    assert(ready.selected == Backend::D3D12 && attempts == 1 && resets == 0);
    auto unknown = Select(static_cast<Backend>(999), [&](Backend) { ++attempts; return std::string{}; }, [&] { ++resets; });
    assert(!unknown.selected && attempts == 1 && resets == 0);
    std::puts("PASS: parsing, precedence, minimum capabilities, 21 staged rollback/exception cases, explicit DX11, bounded dual failure, success retention");
}
