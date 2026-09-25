#include <gpu/dlss_evaluate_capture.h>
#include <json.hpp>
#include <cassert>
#include <iostream>
#include <chrono>

int main(int argc, char** argv) {
    if (argc != 2 || std::string_view(argv[1]) != "--evaluate-capture-contract-only") return 2;
    using namespace gpu::dlss;
    using namespace gpu::dlss::capture;
    assert(TexelBytes(VK_FORMAT_R8G8B8A8_UNORM) == 4);
    assert(TexelBytes(VK_FORMAT_R16G16B16A16_SFLOAT) == 8);
    const uint8_t rgba8[] = {32, 64, 192, 255};
    const uint8_t rgba16[] = {0x00, 0x3c, 0x00, 0x38, 0x00, 0xc0, 0x00, 0x3c};
    assert(PreviewChannel(rgba8, VK_FORMAT_R8G8B8A8_UNORM, 0) == 32);
    assert(PreviewChannel(rgba16, VK_FORMAT_R16G16B16A16_SFLOAT, 0) == 255);
    assert(PreviewChannel(rgba16, VK_FORMAT_R16G16B16A16_SFLOAT, 1) == 128);
    assert(PreviewChannel(rgba16, VK_FORMAT_R16G16B16A16_SFLOAT, 2) == 0);
    const auto dir = std::filesystem::temp_directory_path() /
        ("lo-dlss-evaluate-contract-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);
    try {
        Image image{}; image.width = 1; image.height = 1; image.texelBytes = 8;
        image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        assert(WritePreview(dir / "half.bmp", rgba16, image));
        image.texelBytes = 4; image.format = VK_FORMAT_R8G8B8A8_UNORM;
        assert(WritePreview(dir / "byte.bmp", rgba8, image));
        std::ifstream bmp(dir / "half.bmp", std::ios::binary);
        std::vector<uint8_t> pixels((std::istreambuf_iterator<char>(bmp)), {});
        assert(pixels.size() == 58 && pixels[54] == 0 && pixels[55] == 128 && pixels[56] == 255);
        Page page; page.number = 2; page.frame = 91; page.gapResetBeforeInputs = true;
        page.resetAtFrameEnd = true; page.temporalEpochEnd = 12;
        assert(page.CanReserve(256));
        page.reserved = Page::kMaxSaved;
        assert(!page.CanReserve(256));
        page.reserved = 0; page.reservedBytes = Page::kMaxBytes;
        assert(!page.CanReserve(256));
        page.reservedBytes = 0;
        for (unsigned i = 0; i < 6; ++i) {
            auto e = std::make_shared<EvaluateCapture>();
            e->attemptIndex = ++page.attempts; e->renderFrame = page.frame; e->page = page.number;
            e->inputs.plan.cpuSerial = 50 + i;
            e->inputs.plan.requestSignature = 90 + i;
            e->inputs.plan.geometryEpoch = 30 + i;
            e->inputs.resetHistory = false;
            e->inputs.resetReasons = gpu::temporal::TemporalResetReason::EpochChanged;
            e->sdk.jitterX = .375f; e->sdk.colorX = 2; e->sdk.colorY = 3;
            e->sdk.renderWidth = 4; e->sdk.renderHeight = 4;
            e->sdk.featureCreated = i == 0; e->sdk.inputHistoryReset = false;
            e->sdk.reset = e->sdk.featureCreated;
            e->sdk.biasCurrentColorBound = i == 0;
            e->inputs.motionState = i == 0 ? gpu::temporal::MotionState::Hybrid : gpu::temporal::MotionState::Tracked;
            e->input.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            e->input.texelBytes = 8; e->input.width = e->input.height = 4;
            e->input.storageWidth = e->input.storageHeight = 8;
            e->input.x = 2; e->input.y = 3;
            if (i != 1) { e->evaluated = true; page.Called(e); }
            if (i >= 5) { e->omitted = true; ++page.omitted; }
            e->vendorResult = i == 2 ? -17 : 0;
            e->vendorSuccess = false; // Contract-only export must never map a buffer.
            page.entries.push_back(e);
        }
        assert(page.evaluates == 5 && page.entries[1]->evaluateIndex == std::nullopt);
        assert(page.Export(dir));
        std::ifstream jsonFile(dir / "dlss-evaluations.json");
        const auto json = nlohmann::json::parse(jsonFile);
        assert(json["page"].get<int>() == 2 && json["attempt_count"].get<int>() == 6);
        assert(json["evaluate_count"].get<int>() == 5 && json["truncated"].get<bool>());
        assert(json["evaluations"][0]["sdk"]["reset"].get<bool>());
        assert(json["evaluations"][0]["sdk"]["bias_current_color_bound_to_sdk"].get<bool>());
        assert(!json["evaluations"][1]["sdk"]["bias_current_color_bound_to_sdk"].get<bool>());
        assert(json["evaluations"][0]["sdk"]["motion_state"].get<int>() == 3);
        Page fsrPage; fsrPage.frame = 93;
        for (bool hybrid : {false,true}) {
            auto e = std::make_shared<EvaluateCapture>(); e->fsr = true;
            e->inputs.motionState = hybrid ? gpu::temporal::MotionState::Hybrid : gpu::temporal::MotionState::Tracked;
            e->fsrDispatch.compositionBound = hybrid; fsrPage.entries.push_back(e);
        }
        assert(fsrPage.Export(dir));
        std::ifstream fsrFile(dir / "fsr-evaluations.json");
        const auto fsrJson = nlohmann::json::parse(fsrFile);
        assert(!fsrJson["dispatches"][0]["sdk"]["transparency_composition_bound_to_sdk"].get<bool>());
        assert(fsrJson["dispatches"][1]["sdk"]["transparency_composition_bound_to_sdk"].get<bool>());
        assert(fsrJson["dispatches"][1]["sdk"]["invalidity_bound_to_sdk"].get<bool>());
        assert(fsrJson["dispatches"][1]["sdk"]["hybrid_confidence_bound_to_sdk"].get<bool>());
        assert(fsrJson["dispatches"][1]["sdk"]["motion_state"].get<int>() == 3);
        assert(!fsrJson["dispatches"][1]["sdk"]["reactive_bound_to_sdk"].get<bool>());
        assert(!json["evaluations"][1]["sdk"]["reset"].get<bool>());
        assert(json["evaluations"][1]["evaluate_index"].is_null());
        assert(json["evaluations"][0]["sdk"]["jitter_input_pixels"][0].get<double>() == .375);
        assert(json["evaluations"][0]["input"]["content_rect"] == nlohmann::json::array({2, 3, 4, 4}));
        assert(json["evaluations"][0]["input"]["storage"] == nlohmann::json::array({8, 8}));
        assert(json["evaluations"][0]["input"]["tight_row_stride"].get<int>() == 32);
        assert(json["evaluations"][3]["plan"]["cpu_serial"].get<int>() == 53);
        assert(json["evaluations"][0]["sdk"]["reset_reasons"][0].get<std::string>() == "epoch_changed");
        assert(json["evaluations"][2]["vendor_result"].get<int>() == -17);
        assert(json["evaluations"][2]["create_result"].is_null());
        Page zero;
        zero.frame = 92; zero.number = 3; zero.fallbackReason = "unknown_color_encoding";
        zero.framePlan.cpuSerial = 101;
        assert(std::string(FrameFallbackName(gpu::frame_plan::DlssEffectReason::UnknownColorEncoding)) ==
            "unknown_color_encoding");
        const auto empty = dir / "zero";
        std::filesystem::create_directory(empty);
        assert(zero.Export(empty));
        std::ifstream zeroFile(empty / "dlss-evaluations.json");
        const auto zeroJson = nlohmann::json::parse(zeroFile);
        assert(zeroJson["evaluate_count"].get<int>() == 0 &&
            zeroJson["frame_plan"]["cpu_serial"].get<int>() == 101 &&
            zeroJson["zero_evaluate_reason"].get<std::string>() == "unknown_color_encoding" &&
            zeroJson["evaluations"].empty());
    } catch (...) { std::filesystem::remove_all(dir); throw; }
    std::filesystem::remove_all(dir);
    std::cout << "PASS: evaluate capture typed metadata, bounds, preview and raw-format contract\n";
}
