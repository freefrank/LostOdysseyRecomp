// Execute translated synthetic Xenos instructions on D3D12 and check the GPRs.
// No game assets or screenshots are used by this regression test.
#include <gpu/shader/xenos_translator.h>
#include <gpu/shader/xenos_shader_code.h>
#include <gpu/shader/dxc_compiler.h>
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
using Microsoft::WRL::ComPtr;

static void Check(HRESULT hr)
{
    if (FAILED(hr)) throw std::runtime_error("D3D12 failure: " + std::to_string(uint32_t(hr)));
}

static std::string TestShader()
{
    using namespace xenos;
    std::array<uint32_t, 9> code{};
    ControlFlowExecInstruction cf{};
    cf.address = 1;
    cf.count = 2;
    cf.opcode = ControlFlowOpcode::ExecEnd;
    std::memcpy(code.data(), &cf, 6);
    // Start with r0.x = invocation + 1. Both ALUs must read this old value.
    // Vector: r0.x = r0.x + r0.x; scalar: r1.x = r0.x + r0.x.
    AluInstruction first{};
    first.vectorDest = 0;
    first.vectorWriteMask = 1;
    first.scalarDest = 1;
    first.scalarWriteMask = 1;
    first.vectorOpcode = AluVectorOpcode::Add;
    first.scalarOpcode = AluScalarOpcode::Adds;
    first.src1Select = first.src2Select = first.src3Select = 1;
    first.src1Swizzle = first.src2Swizzle = first.src3Swizzle = 0x6C;
    std::memcpy(code.data() + 3, &first, 12);
    // A later instruction must see the committed scalar result.
    AluInstruction second = first;
    second.vectorDest = 2;
    second.src1Register = second.src2Register = 1;
    second.scalarWriteMask = 0;
    second.scalarOpcode = AluScalarOpcode::RetainPrev;
    std::memcpy(code.data() + 6, &second, 12);

    auto translated = TranslateShader(code.data(), uint32_t(code.size()), false);
    if (!translated.errors.empty()) throw std::runtime_error(translated.errors);
    auto source = translated.hlsl;
    // Wrap the real translated instruction body as compute, so its registers
    // can be observed without rasterization/viewport/depth affecting the test.
    auto begin = source.find("void main(");
    auto body = source.find('{', begin);
    std::string entry = "RWByteAddressBuffer results : register(u0);\n"
        "[numthreads(1,1,1)] void main(uint3 tid : SV_DispatchThreadID)\n{\n"
        "uint xeVertexId = tid.x + 1; float4 oPos;\n";
    for (int i = 0; i < 16; ++i) entry += "float4 o" + std::to_string(i) + ";\n";
    source.replace(begin, body - begin + 1, entry);
    auto epilogue = source.find("\tif ((xeFlags & 8u) == 0u)");
    if (epilogue == std::string::npos) throw std::runtime_error("Missing vertex epilogue");
    source.resize(epilogue);
    source += "results.Store4(tid.x * 16, asuint(float4(r0.x, r1.x, r2.x, ps)));\n"
        "float g = tid.x == 0u ? 0.0 : (tid.x == 1u ? 64.0/255.0 : (tid.x == 2u ? 96.0/255.0 : 192.0/255.0));\n"
        "float4 decoded = XeDecodeTexture(float4(g, 0.25, 0.75, 1.0), 0x60a03u);\n"
        "results.Store4(64u + tid.x * 16u, asuint(decoded));\n"
        "results.Store4(128u + tid.x * 16u, asuint(XeDecodeTexture(float4(1.0, 0.25, 0.5, 1.0), 0x160a00u)));\n}\n";
    return source;
}

int main()
{
    try
    {
        auto shader = xenos::CompileHlsl(TestShader(), "main", "cs_6_0");
        if (!shader.ok) throw std::runtime_error(shader.errors);
        ComPtr<ID3D12Device> device;
        Check(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ComPtr<ID3D12CommandQueue> queue;
        Check(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)));
        D3D12_ROOT_PARAMETER parameter{};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_SIGNATURE_DESC rd{};
        rd.NumParameters = 1;
        rd.pParameters = &parameter;
        ComPtr<ID3DBlob> signature, errors;
        Check(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors));
        ComPtr<ID3D12RootSignature> root;
        Check(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root)));
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};
        pd.pRootSignature = root.Get();
        pd.CS = {shader.dxil.data(), shader.dxil.size()};
        ComPtr<ID3D12PipelineState> pipeline;
        Check(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pipeline)));
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = 192;
        buffer.Height = 1;
        buffer.DepthOrArraySize = buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        buffer.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        ComPtr<ID3D12Resource> output, readback;
        Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&output)));
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        buffer.Flags = D3D12_RESOURCE_FLAG_NONE;
        Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)));
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> commands;
        Check(device->CreateCommandAllocator(qd.Type, IID_PPV_ARGS(&allocator)));
        Check(device->CreateCommandList(0, qd.Type, allocator.Get(), pipeline.Get(), IID_PPV_ARGS(&commands)));
        commands->SetComputeRootSignature(root.Get());
        commands->SetComputeRootUnorderedAccessView(0, output->GetGPUVirtualAddress());
        commands->Dispatch(4, 1, 1);
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = {output.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE};
        commands->ResourceBarrier(1, &barrier);
        commands->CopyResource(readback.Get(), output.Get());
        Check(commands->Close());
        ID3D12CommandList* lists[] = {commands.Get()};
        queue->ExecuteCommandLists(1, lists);
        ComPtr<ID3D12Fence> fence;
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        HANDLE ready = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!ready) throw std::runtime_error("CreateEvent failed");
        Check(fence->SetEventOnCompletion(1, ready));
        Check(queue->Signal(fence.Get(), 1));
        DWORD waited = WaitForSingleObject(ready, 10000);
        CloseHandle(ready);
        if (waited != WAIT_OBJECT_0) throw std::runtime_error("GPU readback timed out");
        float* values = nullptr;
        D3D12_RANGE range{0, 192};
        Check(readback->Map(0, &range, reinterpret_cast<void**>(&values)));
        bool pass = true;
        for (int i = 0; i < 4; ++i)
        {
            float n = float(i + 1);
            std::printf("input %.0f: r0=%.0f r1=%.0f r2=%.0f ps=%.0f\n", n,
                values[4*i], values[4*i+1], values[4*i+2], values[4*i+3]);
            pass &= values[4*i] == 2*n && values[4*i+1] == 2*n &&
                values[4*i+2] == 4*n && values[4*i+3] == 2*n;
            // The gamma flag belongs to source R, then the fetch swaps R/B.
            constexpr float gammaExpected[] = {0.0f, 64.0f/1023.0f, 128.0f/1023.0f, 516.0f/1023.0f};
            const float* decoded = values + 16 + i * 4;
            pass &= decoded[0] == 0.75f && decoded[1] == 0.25f &&
                std::abs(decoded[2] - gammaExpected[i]) < 1e-6f && decoded[3] == 1.0f;
            std::printf("gamma piece %d after BGR swizzle: %.8f\n", i, decoded[2]);
            const float* roundTrip = values + 32 + i * 4;
            pass &= roundTrip[0] == 1.0f && roundTrip[1] == 0.25f &&
                roundTrip[2] == 0.5f && roundTrip[3] == 1.0f;
        }
        D3D12_RANGE written{0, 0};
        readback->Unmap(0, &written);
        std::puts(pass ? "PASS: parallel ALU and texture gamma/swizzle" : "FAIL: shader semantic regression");
        return pass ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
