#ifdef _WIN32
#include "movie_decoder.h"
#include "h264_sample.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mftransform.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "ole32.lib")

namespace movie {
namespace {
using Microsoft::WRL::ComPtr;

// CLSID_CMSH264DecoderMFT (wmcodecdsp.h), spelled out to avoid another import library.
const GUID kH264DecoderMft = {0x62CE7E72, 0x4C71, 0x4d20, {0xB1, 0x5D, 0x45, 0x28, 0x31, 0xA8, 0x7D, 0x9D}};

std::mutex gMfMutex;
int gMfUsers = 0;

bool MfAcquire() {
    std::lock_guard lock(gMfMutex);
    // The game exe delay-loads mfplat.dll, and a failed delay load faults instead of
    // returning. Windows N editions lack it without the Media Feature Pack.
    if (gMfUsers == 0 && (!LoadLibraryExW(L"mfplat.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32) ||
                          FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))))
        return false;
    ++gMfUsers;
    return true;
}
void MfRelease() {
    std::lock_guard lock(gMfMutex);
    if (--gMfUsers == 0) MFShutdown();
}

// Media Foundation objects want COM in the calling thread; keep it until the thread ends.
struct ComThread {
    bool owned = false;
    ComThread() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        owned = SUCCEEDED(hr);
    }
    ~ComThread() { if (owned) CoUninitialize(); }
};
void EnsureCom() { thread_local ComThread com; }

std::string HrText(const char* what, HRESULT hr) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s failed (0x%08lX)", what, static_cast<unsigned long>(hr));
    return buf;
}

class MfDecoder final : public MovieDecoder {
public:
    explicit MfDecoder(bool preferHardware) : preferHardware_(preferHardware) {}
    ~MfDecoder() override {
        if (mft_) mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        mft_.Reset();
        staging_.Reset();
        context_.Reset();
        device_.Reset();
        manager_.Reset();
        if (started_) MfRelease();
    }

    bool Open(const Mp4Track& track, ByteSource& source, std::string& error) override {
        EnsureCom();
        track_ = &track;
        source_ = &source;
        if (track.samples.empty()) { error = "track has no samples"; return false; }
        if (track.bitDepth != 8 || track.chromaFormat != 1) { error = "only 8-bit 4:2:0 H.264 is supported"; return false; }
        if (!started_) {
            if (!MfAcquire()) { error = "Media Foundation unavailable"; return false; }
            started_ = true;
        }
        // The Microsoft decoder drops frames on the GPU path below 64 rows (seen at 64x32, 64x48, 96x48).
        const bool smallPicture = track.codedWidth < 64 || track.codedHeight < 64;
        if (preferHardware_ && !smallPicture && CreateDevice()) {
            if (CreateMft(true, error)) { Reset(); return true; }
            // Fall back to the software MFT.
            mft_.Reset();
            manager_.Reset();
            context_.Reset();
            device_.Reset();
            hardware_ = false;
        }
        error.clear();
        if (!CreateMft(false, error)) return false;
        Reset();
        return true;
    }

    bool Next(Nv12Frame& frame, std::string& error) override {
        EnsureCom();
        error.clear();
        if (!mft_) { error = "decoder is not open"; return false; }
        for (;;) {
            MFT_OUTPUT_DATA_BUFFER out{};
            ComPtr<IMFSample> own;
            if (!providesSamples_) {
                if (!AllocateOutput(own, error)) return false;
                out.pSample = own.Get();
            }
            DWORD status = 0;
            const HRESULT hr = mft_->ProcessOutput(0, 1, &out, &status);
            if (out.pEvents) out.pEvents->Release();
            ComPtr<IMFSample> sample;
            if (providesSamples_) sample.Attach(out.pSample); else sample = own;
            if (SUCCEEDED(hr)) {
                if (!sample) continue;
                return Convert(sample.Get(), frame, error);
            }
            if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
                if (!SetOutputType(error)) return false;
                continue;
            }
            if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
                if (draining_) return false; // everything was delivered
                if (!Feed(error)) return false;
                continue;
            }
            error = HrText("ProcessOutput", hr);
            return false;
        }
    }

    void Rewind() override {
        if (!mft_) return;
        mft_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        Reset();
    }

    const char* Name() const override { return hardware_ ? "MediaFoundation H.264 (D3D11)" : "MediaFoundation H.264 (software)"; }
    bool Hardware() const override { return hardware_; }

private:
    bool CreateDevice() {
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        D3D_FEATURE_LEVEL got{};
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                                     levels, 2, D3D11_SDK_VERSION, &device_, &got, &context_))) {
            device_.Reset();
            context_.Reset();
            return false;
        }
        ComPtr<ID3D10Multithread> mt;
        if (SUCCEEDED(device_.As(&mt))) mt->SetMultithreadProtected(TRUE);
        UINT token = 0;
        if (FAILED(MFCreateDXGIDeviceManager(&token, &manager_)) || FAILED(manager_->ResetDevice(device_.Get(), token))) {
            manager_.Reset();
            device_.Reset();
            context_.Reset();
            return false;
        }
        return true;
    }

    bool CreateMft(bool withManager, std::string& error) {
        mft_.Reset();
        HRESULT hr = CoCreateInstance(kH264DecoderMft, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&mft_));
        if (FAILED(hr)) { error = HrText("creating the H.264 decoder MFT", hr); return false; }
        if (withManager) {
            ComPtr<IMFAttributes> attrs;
            UINT32 aware = 0;
            if (FAILED(mft_->GetAttributes(&attrs)) || FAILED(attrs->GetUINT32(MF_SA_D3D11_AWARE, &aware)) || !aware) {
                error = "decoder MFT is not D3D11 aware";
                return false;
            }
            hr = mft_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(manager_.Get()));
            if (FAILED(hr)) { error = HrText("setting the D3D manager", hr); return false; }
            hardware_ = true;
        } else {
            hardware_ = false;
        }
        ComPtr<IMFMediaType> in;
        MFCreateMediaType(&in);
        in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        in->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        MFSetAttributeSize(in.Get(), MF_MT_FRAME_SIZE, track_->width, track_->height);
        if (track_->frameDuration) MFSetAttributeRatio(in.Get(), MF_MT_FRAME_RATE, track_->timescale, track_->frameDuration);
        MFSetAttributeRatio(in.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        in->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        hr = mft_->SetInputType(0, in.Get(), 0);
        if (FAILED(hr)) { error = HrText("SetInputType", hr); return false; }
        if (!SetOutputType(error)) return false;
        mft_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        hr = mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        if (SUCCEEDED(hr)) hr = mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
        if (FAILED(hr)) { error = HrText("starting the stream", hr); return false; }
        return true;
    }

    // Picks NV12 and reads what the decoder will deliver.
    bool SetOutputType(std::string& error) {
        for (DWORD i = 0;; ++i) {
            ComPtr<IMFMediaType> type;
            const HRESULT hr = mft_->GetOutputAvailableType(0, i, &type);
            if (hr == MF_E_NO_MORE_TYPES) break;
            if (FAILED(hr)) { error = HrText("GetOutputAvailableType", hr); return false; }
            GUID sub{};
            if (FAILED(type->GetGUID(MF_MT_SUBTYPE, &sub)) || sub != MFVideoFormat_NV12) continue;
            if (FAILED(mft_->SetOutputType(0, type.Get(), 0))) continue;
            UINT32 w = 0, h = 0;
            MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &w, &h);
            frameW_ = w;
            frameH_ = h;
            cropX_ = cropY_ = 0;
            cropW_ = track_->width;
            cropH_ = track_->height;
            MFVideoArea area{};
            if (SUCCEEDED(type->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, reinterpret_cast<UINT8*>(&area), sizeof(area), nullptr)) &&
                area.Area.cx > 0 && area.Area.cy > 0) {
                cropX_ = uint32_t(area.OffsetX.value);
                cropY_ = uint32_t(area.OffsetY.value);
                cropW_ = uint32_t(area.Area.cx);
                cropH_ = uint32_t(area.Area.cy);
            }
            MFT_OUTPUT_STREAM_INFO info{};
            if (FAILED(mft_->GetOutputStreamInfo(0, &info))) { error = "GetOutputStreamInfo failed"; return false; }
            providesSamples_ = (info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) != 0;
            outputBytes_ = info.cbSize;
            return true;
        }
        error = "decoder offers no NV12 output";
        return false;
    }

    bool AllocateOutput(ComPtr<IMFSample>& sample, std::string& error) {
        ComPtr<IMFMediaBuffer> buffer;
        HRESULT hr = MFCreateSample(&sample);
        if (SUCCEEDED(hr)) hr = MFCreateMemoryBuffer(std::max<DWORD>(outputBytes_, 1), &buffer);
        if (SUCCEEDED(hr)) hr = sample->AddBuffer(buffer.Get());
        if (FAILED(hr)) { error = HrText("allocating an output sample", hr); return false; }
        return true;
    }

    void Reset() {
        next_ = 0;
        draining_ = false;
        headerSent_ = false;
    }

    // Sends the next sample, or drains at the end.
    bool Feed(std::string& error) {
        if (next_ >= track_->samples.size()) {
            draining_ = true;
            const HRESULT hr = mft_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
            if (FAILED(hr)) { error = HrText("drain", hr); return false; }
            return true;
        }
        const Mp4Sample& s = track_->samples[next_];
        raw_.resize(s.size);
        if (!source_->ReadAt(s.offset, raw_.data(), raw_.size())) { error = "cannot read a video sample"; return false; }
        annexb_.clear();
        if (!headerSent_) {
            AppendH264ParameterSets(track_->sps, annexb_);
            AppendH264ParameterSets(track_->pps, annexb_);
            headerSent_ = true;
        }
        if (!AppendH264Sample(raw_, track_->nalLengthSize, annexb_, error)) return false;
        ComPtr<IMFMediaBuffer> buffer;
        ComPtr<IMFSample> sample;
        HRESULT hr = MFCreateMemoryBuffer(DWORD(annexb_.size()), &buffer);
        BYTE* dst = nullptr;
        if (SUCCEEDED(hr)) hr = buffer->Lock(&dst, nullptr, nullptr);
        if (SUCCEEDED(hr)) {
            std::memcpy(dst, annexb_.data(), annexb_.size());
            buffer->Unlock();
            hr = buffer->SetCurrentLength(DWORD(annexb_.size()));
        }
        if (SUCCEEDED(hr)) hr = MFCreateSample(&sample);
        if (SUCCEEDED(hr)) hr = sample->AddBuffer(buffer.Get());
        if (SUCCEEDED(hr)) hr = sample->SetSampleTime(ToHns(s.pts));
        if (SUCCEEDED(hr)) hr = sample->SetSampleDuration(ToHns(s.duration));
        if (SUCCEEDED(hr)) sample->SetUINT32(MFSampleExtension_CleanPoint, s.keyframe ? 1 : 0);
        if (FAILED(hr)) { error = HrText("building an input sample", hr); return false; }
        hr = mft_->ProcessInput(0, sample.Get(), 0);
        if (FAILED(hr)) { error = HrText("ProcessInput", hr); return false; }
        ++next_;
        return true;
    }

    int64_t ToHns(int64_t t) const {
        return int64_t(std::llround((long double)t * 10000000.0L / track_->timescale));
    }
    int64_t FromHns(int64_t t) const {
        return int64_t(std::llround((long double)t * track_->timescale / 10000000.0L));
    }

    bool Convert(IMFSample* sample, Nv12Frame& frame, std::string& error) {
        LONGLONG time = 0;
        sample->GetSampleTime(&time);
        frame.pts = FromHns(time);
        ComPtr<IMFMediaBuffer> buffer;
        HRESULT hr = sample->GetBufferByIndex(0, &buffer);
        if (FAILED(hr)) { error = HrText("GetBufferByIndex", hr); return false; }
        const uint32_t w = std::min(cropW_, track_->width ? track_->width : cropW_);
        const uint32_t h = std::min(cropH_, track_->height ? track_->height : cropH_);
        frame.width = w;
        frame.height = h;
        frame.lumaPitch = w;
        frame.chromaPitch = (w + 1) & ~1u;
        frame.luma.resize(size_t(w) * h);
        frame.chroma.resize(size_t(frame.chromaPitch) * ((h + 1) / 2));

        ComPtr<IMFDXGIBuffer> dxgi;
        if (SUCCEEDED(buffer.As(&dxgi))) {
            ComPtr<ID3D11Texture2D> tex;
            UINT index = 0;
            if (FAILED(dxgi->GetResource(IID_PPV_ARGS(&tex))) || FAILED(dxgi->GetSubresourceIndex(&index))) {
                error = "cannot get the decoder texture";
                return false;
            }
            D3D11_TEXTURE2D_DESC desc{};
            tex->GetDesc(&desc);
            ComPtr<ID3D11Device> texDevice;
            tex->GetDevice(&texDevice);
            ComPtr<ID3D11DeviceContext> ctx;
            texDevice->GetImmediateContext(&ctx);
            if (!staging_ || stagingDesc_.Width != desc.Width || stagingDesc_.Height != desc.Height || stagingDevice_ != texDevice.Get()) {
                stagingDesc_ = desc;
                stagingDesc_.Usage = D3D11_USAGE_STAGING;
                stagingDesc_.BindFlags = 0;
                stagingDesc_.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                stagingDesc_.MiscFlags = 0;
                stagingDesc_.ArraySize = 1;
                stagingDesc_.MipLevels = 1;
                staging_.Reset();
                hr = texDevice->CreateTexture2D(&stagingDesc_, nullptr, &staging_);
                if (FAILED(hr)) { error = HrText("creating the staging texture", hr); return false; }
                stagingDevice_ = texDevice.Get();
            }
            ctx->CopySubresourceRegion(staging_.Get(), 0, 0, 0, 0, tex.Get(), index, nullptr);
            D3D11_MAPPED_SUBRESOURCE map{};
            hr = ctx->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &map);
            if (FAILED(hr)) { error = HrText("mapping the staging texture", hr); return false; }
            hardware_ = true;
            CopyPlanes(static_cast<const uint8_t*>(map.pData), map.RowPitch, desc.Height, frame);
            ctx->Unmap(staging_.Get(), 0);
            return true;
        }

        hardware_ = false;
        ComPtr<IMF2DBuffer2> two;
        if (SUCCEEDED(buffer.As(&two))) {
            BYTE *scan0 = nullptr, *start = nullptr;
            LONG pitch = 0;
            DWORD length = 0;
            hr = two->Lock2DSize(MF2DBuffer_LockFlags_Read, &scan0, &pitch, &start, &length);
            if (FAILED(hr)) { error = HrText("Lock2DSize", hr); return false; }
            // The chroma plane follows the luma rows of the allocated frame height.
            CopyPlanes(scan0, uint32_t(pitch), frameH_, frame);
            two->Unlock2D();
            return true;
        }
        BYTE* data = nullptr;
        DWORD cur = 0;
        hr = buffer->Lock(&data, nullptr, &cur);
        if (FAILED(hr)) { error = HrText("Lock", hr); return false; }
        const uint32_t pitch = frameW_;
        CopyPlanes(data, pitch, frameH_, frame);
        buffer->Unlock();
        return true;
    }

    // base points at the luma plane; the chroma plane starts planeRows rows later.
    void CopyPlanes(const uint8_t* base, uint32_t pitch, uint32_t planeRows, Nv12Frame& frame) const {
        const uint8_t* luma = base + size_t(cropY_) * pitch + cropX_;
        for (uint32_t y = 0; y < frame.height; ++y)
            std::memcpy(frame.luma.data() + size_t(y) * frame.lumaPitch, luma + size_t(y) * pitch, frame.width);
        const uint8_t* chroma = base + size_t(planeRows) * pitch + size_t(cropY_ / 2) * pitch + (cropX_ & ~1u);
        const uint32_t rows = (frame.height + 1) / 2;
        for (uint32_t y = 0; y < rows; ++y)
            std::memcpy(frame.chroma.data() + size_t(y) * frame.chromaPitch, chroma + size_t(y) * pitch, frame.chromaPitch);
    }

    bool preferHardware_;
    bool started_ = false;
    bool hardware_ = false;
    const Mp4Track* track_ = nullptr;
    ByteSource* source_ = nullptr;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IMFDXGIDeviceManager> manager_;
    ComPtr<IMFTransform> mft_;
    ComPtr<ID3D11Texture2D> staging_;
    D3D11_TEXTURE2D_DESC stagingDesc_{};
    ID3D11Device* stagingDevice_ = nullptr;
    bool providesSamples_ = false;
    DWORD outputBytes_ = 0;
    uint32_t frameW_ = 0, frameH_ = 0, cropX_ = 0, cropY_ = 0, cropW_ = 0, cropH_ = 0;
    size_t next_ = 0;
    bool draining_ = false, headerSent_ = false;
    std::vector<uint8_t> raw_, annexb_;
};
}

std::unique_ptr<MovieDecoder> CreateMfMovieDecoder(bool preferHardware) {
    return std::make_unique<MfDecoder>(preferHardware);
}
}
#endif
