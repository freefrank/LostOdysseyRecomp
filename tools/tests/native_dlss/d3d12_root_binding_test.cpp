#include <plume_d3d12.h>

#include <cstdio>
#include <stdexcept>
#include <wrl/client.h>

namespace plume { std::unique_ptr<RenderInterface> CreateD3D12Interface(); }

namespace {
using namespace plume;
using Microsoft::WRL::ComPtr;

void Check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

void Counts(const D3D12CommandList::RootBindingCount &count, uint64_t requests,
    uint64_t nativeCalls, const char *message) {
    if (count.requests != requests || count.nativeCalls != nativeCalls ||
        count.skipped != requests - nativeCalls) {
        std::fprintf(stderr, "%s: requests=%llu native=%llu skipped=%llu, expected %llu/%llu/%llu\n",
            message, static_cast<unsigned long long>(count.requests),
            static_cast<unsigned long long>(count.nativeCalls), static_cast<unsigned long long>(count.skipped),
            static_cast<unsigned long long>(requests), static_cast<unsigned long long>(nativeCalls),
            static_cast<unsigned long long>(requests - nativeCalls));
        throw std::runtime_error(message);
    }
}

void CheckDebugErrors(ID3D12InfoQueue &queue) {
    const uint64_t messageCount = queue.GetNumStoredMessages();
    for (uint64_t i = 0; i < messageCount; ++i) {
        SIZE_T bytes = 0;
        queue.GetMessage(i, nullptr, &bytes);
        std::vector<uint8_t> storage(bytes);
        auto *message = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
        Check(SUCCEEDED(queue.GetMessage(i, message, &bytes)), "read D3D12 debug message");
        if (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR ||
            message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION) {
            std::fprintf(stderr, "D3D12 debug error %u: %s\n", unsigned(message->ID), message->pDescription);
            throw std::runtime_error("D3D12 debug layer reported a binding error");
        }
    }
}
}

int main() {
    try {
        ComPtr<ID3D12Debug> debug;
        const bool haveDebugLayer = SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
        if (haveDebugLayer) debug->EnableDebugLayer();
        else std::puts("D3D12 debug layer unavailable; running native binding sequence");

        auto api = CreateD3D12Interface();
        auto device = api ? api->createDevice() : nullptr;
        if (!device) {
            std::puts("SKIP: D3D12 device unavailable");
            return 77;
        }
        auto &nativeDevice = *static_cast<D3D12Device *>(device.get());
        ComPtr<ID3D12InfoQueue> info;
        if (haveDebugLayer)
            Check(SUCCEEDED(nativeDevice.d3d->QueryInterface(IID_PPV_ARGS(&info))), "D3D12 info queue");

        auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
        auto listHolder = queue ? queue->createCommandList() : nullptr;
        auto continuationHolder = queue ? queue->createCommandList() : nullptr;
        Check(queue && listHolder && continuationHolder, "command lists");
        auto &list = *static_cast<D3D12CommandList *>(listHolder.get());
        auto &continuation = *static_cast<D3D12CommandList *>(continuationHolder.get());

        const RenderDescriptorRange ranges[] = {
            { RenderDescriptorRangeType::CONSTANT_BUFFER, 0, 1 },
            { RenderDescriptorRangeType::SAMPLER, 0, 1 }
        };
        const RenderDescriptorSetDesc setDescs[] = { { ranges, 2 }, { ranges, 2 } };
        RenderPipelineLayoutDesc layoutDesc{};
        layoutDesc.descriptorSetDescs = setDescs;
        layoutDesc.descriptorSetDescsCount = 2;
        const RenderPushConstantRange bConstant(0, 2, 0, 4, RenderShaderStageFlag::VERTEX);
        RenderPipelineLayoutDesc layoutBDesc = layoutDesc;
        layoutBDesc.pushConstantRanges = &bConstant;
        layoutBDesc.pushConstantRangesCount = 1;
        auto layoutA = device->createPipelineLayout(layoutDesc);
        auto layoutB = device->createPipelineLayout(layoutBDesc);
        auto layoutAlias = device->createPipelineLayout(layoutDesc);
        Check(layoutA && layoutB && layoutAlias, "root signatures");
        auto &a = *static_cast<D3D12PipelineLayout *>(layoutA.get());
        auto &b = *static_cast<D3D12PipelineLayout *>(layoutB.get());
        auto &alias = *static_cast<D3D12PipelineLayout *>(layoutAlias.get());
        // Distinct logical layouts can refer to the same native signature.
        alias.rootSignature->Release();
        alias.rootSignature = a.rootSignature;
        alias.rootSignature->AddRef();

        auto buffer = device->createBuffer(RenderBufferDesc::UploadBuffer(256));
        auto sampler = device->createSampler(RenderSamplerDesc{});
        auto setA = device->createDescriptorSet(setDescs[0]);
        auto setB = device->createDescriptorSet(setDescs[0]);
        Check(buffer && sampler && setA && setB, "descriptors");
        for (auto *set : {setA.get(), setB.get()}) {
            set->setBuffer(0, buffer.get());
            set->setSampler(1, sampler.get());
        }
        auto &nativeSetB = *static_cast<D3D12DescriptorSet *>(setB.get());

        ComPtr<ID3D12DescriptorHeap> externalViews;
        ComPtr<ID3D12DescriptorHeap> externalSamplers;
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        heapDesc.NumDescriptors = 8;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        Check(SUCCEEDED(nativeDevice.d3d->CreateDescriptorHeap(&heapDesc,
            IID_PPV_ARGS(&externalViews))), "external view heap");
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        Check(SUCCEEDED(nativeDevice.d3d->CreateDescriptorHeap(&heapDesc,
            IID_PPV_ARGS(&externalSamplers))), "external sampler heap");

        if (info) info->ClearStoredMessages();
        list.captureRootBindingStats = true;
        list.begin();
        list.setGraphicsPipelineLayout(layoutA.get());
        list.setGraphicsDescriptorSet(setA.get(), 0);
        list.setGraphicsDescriptorSet(setA.get(), 0);
        list.setGraphicsPipelineLayout(layoutAlias.get());
        Check(list.activeGraphicsPipelineLayout == &alias, "logical layout pointer updated");
        list.setGraphicsDescriptorSet(setA.get(), 0);
        list.setGraphicsDescriptorSet(setA.get(), 1); // Same handle, different native root index.
        list.setRootDescriptorTable(nativeDevice.viewHeapAllocator.get(), nativeSetB.viewAllocation,
            a.setViewRootIndices[0], false); // Change only one table.
        list.setGraphicsDescriptorSet(setA.get(), 0);
        list.setComputePipelineLayout(layoutA.get());
        list.setComputeDescriptorSet(setA.get(), 0);
        list.setComputeDescriptorSet(setA.get(), 0);
        list.setGraphicsDescriptorSet(setA.get(), 0); // Compute did not invalidate graphics.
        list.setGraphicsPipelineLayout(layoutB.get());
        list.setGraphicsDescriptorSet(setA.get(), 0);
        list.setGraphicsPipelineLayout(layoutA.get());
        list.setGraphicsDescriptorSet(setA.get(), 0);
        auto stats = list.getRootBindingStats();
        Counts(stats.graphicsSignatures, 4, 3, "A/alias/B/A root signatures");
        Counts(stats.graphicsViewTables, 9, 6, "graphics view table sequence");
        Counts(stats.graphicsSamplerTables, 8, 4, "graphics sampler table sequence");
        Counts(stats.computeSignatures, 1, 1, "compute root signature");
        Counts(stats.computeViewTables, 2, 1, "compute view table sequence");
        Counts(stats.computeSamplerTables, 2, 1, "compute sampler table sequence");

        ID3D12DescriptorHeap *externalHeaps[] = {externalViews.Get(), externalSamplers.Get()};
        list.d3d->SetDescriptorHeaps(2, externalHeaps);
        list.notifyDescriptorHeapWasChangedExternally();
        list.setGraphicsDescriptorSet(setA.get(), 0); // Same handle after heap restoration.
        list.setGraphicsDescriptorSet(setA.get(), 0);
        list.setComputeDescriptorSet(setA.get(), 0);
        stats = list.getRootBindingStats();
        Counts(stats.graphicsViewTables, 11, 7, "heap restore graphics view");
        Counts(stats.graphicsSamplerTables, 10, 5, "heap restore graphics sampler");
        Counts(stats.computeViewTables, 3, 2, "heap restore compute view");
        Counts(stats.computeSamplerTables, 3, 2, "heap restore compute sampler");

        list.d3d->SetGraphicsRootSignature(b.rootSignature);
        list.invalidateCachedNativeState();
        list.setGraphicsPipelineLayout(layoutA.get());
        list.setGraphicsDescriptorSet(setA.get(), 0);
        Counts(list.getRootBindingStats().graphicsSignatures, 5, 4, "external signature mutation");
        list.end();
        if (info) CheckDebugErrors(*info.Get());
        Counts(list.getRootBindingStats().graphicsViewTables, 12, 8, "Close preserves counters");

        // Reuse a descriptor address after freeing its owner; Reset must issue a new bind.
        const auto oldOffset = nativeSetB.viewAllocation.offset;
        setB.reset();
        auto setC = device->createDescriptorSet(setDescs[0]);
        Check(bool(setC), "reallocated descriptor set");
        auto &nativeSetC = *static_cast<D3D12DescriptorSet *>(setC.get());
        Check(nativeSetC.viewAllocation.offset == oldOffset, "descriptor address reuse");
        setC->setBuffer(0, buffer.get());
        setC->setSampler(1, sampler.get());
        list.begin();
        list.setGraphicsPipelineLayout(layoutA.get());
        list.setGraphicsDescriptorSet(setC.get(), 0);
        Counts(list.getRootBindingStats().graphicsViewTables, 1, 1, "Reset rebinds reused address");
        list.end();

        continuation.captureRootBindingStats = true;
        continuation.begin();
        continuation.setGraphicsPipelineLayout(layoutA.get());
        continuation.setGraphicsDescriptorSet(setA.get(), 0);
        Counts(continuation.getRootBindingStats().graphicsViewTables, 1, 1,
            "continuation list has independent cache");
        continuation.end();
        if (info) CheckDebugErrors(*info.Get());
        std::printf("D3D12 root binding sequence: PASS; debug layer: %s\n",
            info ? "PASS" : "unavailable");
        return 0;
    }
    catch (const std::exception &e) {
        std::fprintf(stderr, "D3D12 root binding test failed: %s\n", e.what());
        return 1;
    }
}
