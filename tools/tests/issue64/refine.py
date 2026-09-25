from pathlib import Path

def replace(path, old, new):
    p=Path(path);s=p.read_text()
    if s.count(old)!=1: raise RuntimeError(f'{path}: expected one context: {old[:70]!r}, found {s.count(old)}')
    p.write_text(s.replace(old,new,1))

root='LostOdysseyRecomp/gpu/'
replace(root+'sr_hybrid_mask.h',
 'const TemporalFrameInputs& inputs, const plume::VulkanDevice* device) {',
 'const TemporalFrameInputs& inputs, const plume::VulkanDevice* device,\n    plume::RenderTextureLayout expectedLayout = plume::RenderTextureLayout::SHADER_READ) {')
replace(root+'sr_hybrid_mask.h',
 'image->textureLayout == plume::RenderTextureLayout::SHADER_READ;',
 'image->textureLayout == expectedLayout;')
replace(root+'dlss_ngx.cpp',
 'temporal::ValidSrHybridMask(inputs,sessionDevice_)',
 'temporal::ValidSrHybridMask(inputs,sessionDevice_,plume::RenderTextureLayout::GENERAL)')
replace('tools/tests/sr_hybrid_motion_gpu_test.cpp',
 'auto stale=inputs;stale.motionInvalidity.width--;',
 '''cmd_->barriers(RenderBarrierStage::ALL,RenderTextureBarrier(inputs.motionInvalidity.texture,RenderTextureLayout::GENERAL));
                Check(temporal::ValidSrHybridMask(inputs,static_cast<VulkanDevice*>(device_.get()),RenderTextureLayout::GENERAL),"NGX confidence accepts required GENERAL layout");
                Check(!temporal::ValidSrHybridMask(inputs,static_cast<VulkanDevice*>(device_.get())),"FSR confidence rejects NGX-only layout");
                cmd_->barriers(RenderBarrierStage::ALL,RenderTextureBarrier(inputs.motionInvalidity.texture,RenderTextureLayout::SHADER_READ));
                Check(!temporal::ValidSrHybridMask(inputs,static_cast<VulkanDevice*>(device_.get()),RenderTextureLayout::GENERAL),"NGX confidence rejects untransitioned FSR layout");
                auto stale=inputs;stale.motionInvalidity.width--;''')
replace(root+'dlss_evaluate_capture.h',
 'bool reset = false, featureCreated = false, inputHistoryReset = false;',
 'bool reset = false, featureCreated = false, inputHistoryReset = false, biasCurrentColorBound = false;')
replace(root+'dlss_evaluate_capture.h',
 'bool maskBound = false, rcasEnabled = false;',
 'bool maskBound = false, rcasEnabled = false, compositionBound = false;')
replace(root+'dlss_ngx.cpp',
 'frozen.inputHistoryReset = inputs.resetHistory;',
 'frozen.inputHistoryReset = inputs.resetHistory;\n        frozen.biasCurrentColorBound = evaluate.pInBiasCurrentColorMask != nullptr;')
replace(root+'fsr_upscaler.cpp',
 'recorded.reset = dispatch.reset;',
 'recorded.reset = dispatch.reset; recorded.compositionBound = hybridConfidence != nullptr;')
replace(root+'dlss_evaluate_capture.h',
 r'''<< ",\"invalidity_bound_to_sdk\":false,\"reactive_bound_to_sdk\":" << (p.maskBound ? "true" : "false")''',
 r'''<< ",\"motion_state\":" << uint32_t(e->inputs.motionState)
                  << ",\"invalidity_bound_to_sdk\":" << (p.compositionBound ? "true" : "false")
                  << ",\"reactive_bound_to_sdk\":" << (p.maskBound ? "true" : "false")''')
replace(root+'dlss_evaluate_capture.h',
 r'''<< ",\"transparency_composition_bound_to_sdk\":false"''',
 r'''<< ",\"transparency_composition_bound_to_sdk\":" << (p.compositionBound ? "true" : "false")
                  << ",\"hybrid_confidence_bound_to_sdk\":" << (p.compositionBound ? "true" : "false")''')
replace(root+'dlss_evaluate_capture.h',
 r'''<< ",\"input_history_reset\":" << (s.inputHistoryReset ? "true" : "false")''',
 r'''<< ",\"input_history_reset\":" << (s.inputHistoryReset ? "true" : "false")
                 << ",\"motion_state\":" << uint32_t(e.inputs.motionState)
                 << ",\"bias_current_color_bound_to_sdk\":" << (s.biasCurrentColorBound ? "true" : "false")''')
replace('tools/tests/dlss_evaluate_capture_contract_test.cpp',
 'e->sdk.reset = e->sdk.featureCreated;',
 'e->sdk.reset = e->sdk.featureCreated;\n            e->sdk.biasCurrentColorBound = i == 0;\n            e->inputs.motionState = i == 0 ? gpu::temporal::MotionState::Hybrid : gpu::temporal::MotionState::Tracked;')
replace('tools/tests/dlss_evaluate_capture_contract_test.cpp',
 'assert(json["evaluations"][0]["sdk"]["reset"].get<bool>());',
 '''assert(json["evaluations"][0]["sdk"]["reset"].get<bool>());
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
        assert(!fsrJson["dispatches"][1]["sdk"]["reactive_bound_to_sdk"].get<bool>());''')
p=Path('tools/tests/motion_replay/CMakeLists.txt')
p.write_text(p.read_text()+'''
# Capture assertions must execute in Release too.
if(MSVC)
 target_compile_options(LoDlssEvaluateCaptureContractTest PRIVATE /UNDEBUG)
else()
 target_compile_options(LoDlssEvaluateCaptureContractTest PRIVATE -UNDEBUG)
endif()
add_test(NAME hybrid_capture_metadata COMMAND LoDlssEvaluateCaptureContractTest --evaluate-capture-contract-only)
set_tests_properties(sr_hybrid_motion_gpu hybrid_capture_metadata PROPERTIES TIMEOUT 180)
''')
print('Refined NGX layout contract and truthful SDK mask capture metadata')
