    // This is included in Harness to reuse its real production Renderer, shader
    // setup and checked Vulkan queue. It does not model another renderer.
    void RunFgHandoff(unsigned mode) {
        using namespace gpu::frame_generation;
        auto& r = R(); ++r.frame; r.temporalEpoch = 7;
        r.activePlan.output = {{8, 8}, 0, 0, 8, 8};
        r.activePlan.requestedUpscaler = gpu::upscaling::Upscaler::Dlss;
        r.fgSnapshotRequestedFrame = r.frame; r.fgSnapshotAttempted = false;
        const uint64_t realFrame = r.frame;
        const RenderTargetKey key{0, 3, 1280, 0, false};
        auto originalOwner = Texture(4, 4, RenderFormat::R16G16B16A16_FLOAT);
        auto* original = originalOwner.get(); r.renderTargets[key] = std::move(originalOwner);
        auto depth = Texture(4, 4, RenderFormat::R32_FLOAT);
        auto motion = Texture(4, 4, RenderFormat::R16G16_FLOAT);
        auto invalid = Texture(4, 4, RenderFormat::R8_UNORM);
        auto presentList = queue->createCommandList();
        auto presentFence = device->createCommandFence();
        auto output = Texture(8, 8, RenderFormat::R8G8B8A8_UNORM);
        gpu::Presentation presentation;
        Require(presentation.Init(device.get()), "production presentation init");
        LocalImageDrain imageDrain{device.get()};
        Clear(*original, RenderColor(.25f, .5f, .75f, .5f));
        Clear(*depth, RenderColor(.5f, 0, 0, 0));
        Clear(*motion, RenderColor(.25f, .5f, 0, 0));
        Clear(*invalid, RenderColor(0, 0, 0, 0));
        gpu::temporal::TemporalFrameInputs inputs{};
        inputs.plan = r.activePlan; inputs.renderFrameId = r.frame; inputs.temporalEpoch = r.temporalEpoch;
        inputs.currentInputsComplete = true; inputs.resetHistory = false;
        inputs.resetReasons = gpu::temporal::TemporalResetReason::None;
        inputs.colorEncoding = gpu::temporal::ColorEncoding::HdrLinear; // synthetic input, not game semantics
        inputs.depthConvention = gpu::temporal::DepthConvention::Forward;
        inputs.motionState = gpu::temporal::MotionState::Tracked;
        inputs.color = {original->texture.get(), {4,4}, 0,0,4,4};
        inputs.depth = {depth->texture.get(), {4,4}, 0,0,4,4};
        inputs.motion = {motion->texture.get(), {4,4}, 0,0,4,4};
        inputs.motionInvalidity = {invalid->texture.get(), {4,4}, 0,0,4,4};
        Require(r.PrepareSceneCopyDestination(key, *original, inputs), "production SR preparation");
        HostTexture* color = original; HostTexture* raster = color;
        RenderViewport guest(0,0,1280,720), viewport;
        RenderRect guestScissor(0,0,1280,720), scissor;
        Require(r.ActivateSceneCopyDestination(color, raster, viewport, scissor, guest, guestScissor), "promotion");
        Clear(*color, RenderColor(.25f, .5f, .75f, .5f));
        VendorFixture vendor;
        Require(r.RecordSceneCopyDlssUsing(vendor, color, raster), "actual SR continuation");
        auto source = r.fgInputSnapshot;
        Require(source && source->resolveSourceAllocation == color->allocationSerial &&
            source->resolveSourceGeneration == color->fgWriteGeneration, "lineage from actual promoted allocation/write");
        if (mode != 7) Require(r.Flush(), "source mid-frame submission");
        if (mode == 9) r.CancelFgSnapshots(HandoffCancel::DisplayChange);
        if (mode == 1) Clear(*color, RenderColor(0,1,0,1)); // invalidates source lineage, not ordinary present
        constexpr uint32_t address = 0x20000;
        if (mode == 3) Require(r.ResolveOnGpu(*color, address, 7, 1280, 720, 0,0,1280,720), "first format variant");
        Require(r.ResolveOnGpu(*color, address, 6, 1280, 720, 0,0,1280,720), "production final resolve");
        auto* resolved = r.FindResolved(address, 6);
        Require(resolved && resolved->tex, "actual resolved storage");
        auto packet = resolved->fgPacket.lock();
        if (mode == 1 || mode == 9) {
            Require(!packet, "source write-generation change rejects diagnostic");
            Require(r.Flush(), "ordinary resolve still submits"); r.AdvanceAfterPublicFlush();
            uint32_t w=0,h=0,f=0; ResolvedHandoff missing;
            Require(r.AcquireResolvedForPresent(address,w,h,f,nullptr,&missing) && !missing.packet,
                "ordinary selected texture survives missing source lineage");
            Require(r.WaitForGpu(), "fallback cleanup"); return;
        }
        Require(packet && packet->identity.resolve == resolved->writeOrdinal &&
            packet->identity.allocation == resolved->tex->allocationSerial &&
            packet->identity.generation == resolved->tex->fgWriteGeneration,
            "packet binds exact real resolve record and allocation generation");
        Require(packet->finalColor.region.texture != resolved->tex->texture.get(), "immutable final copy, not raw resolve lease");
        if (mode == 0) {
            for (uint32_t i = 1; i < ResolveHandoffPool::Capacity; ++i)
                Require(r.ResolveOnGpu(*color,address+0x1000*i,6,1280,720,0,0,1280,720), "bounded sibling resolve");
            Require(r.fgHandoffPool.LiveCount() == ResolveHandoffPool::Capacity, "bounded real in-flight allocations");
            Require(r.ResolveOnGpu(*color,address+0x10000,6,1280,720,0,0,1280,720), "full pool preserves ordinary resolve");
            Require(r.NewestResolved(address+0x10000)->fgPacket.expired(), "full pool produces no stale handoff");
        }
        if (mode == 3) {
            Require(r.ResolveOnGpu(*color,address,7,1280,720,0,0,1280,720), "later write to earlier format variant");
            Require(r.NewestResolved(address)->destFormat == 6 &&
                r.FindResolved(address,7)->writeOrdinal > resolved->writeOrdinal,
                "existing frame/variant selector preserved, not replaced by max ordinal");
        }
        if (mode == 5) {
            fixture::rejectSubmit = true;
            Require(!r.Flush() && gpu::video::GpuWorkStopped(), "real checked final submit failure stops GPU work");
            Require(packet->resolveDiscarded && !packet->resolveSerial && !packet->DiagnosticReady(), "failed packet not published");
            Require(source->producerSerial && !source->producerDiscarded, "earlier submitted source is retained");
            return;
        }
        Require(r.Flush(), "final resolve copy submission");
        Require(packet->resolveSerial && source->producerSerial &&
            ((mode == 7) ? packet->resolveSerial == source->producerSerial : packet->resolveSerial > source->producerSerial),
            "source/final submissions distinguish same-batch and mid-frame cases");
        uint32_t w=0,h=0,f=0; gpu::frame_plan::FramePlan selectedPlan; ResolvedHandoff handoff;
        Require(r.AcquireResolvedForPresent(address,w,h,f,&selectedPlan,&handoff) && !handoff.packet,
            "internal batch submit alone is not public frame completion");
        r.AdvanceAfterPublicFlush(); // same production frame-boundary method used by public Flush
        if (mode == 2) Clear(*resolved->tex, RenderColor(0,1,0,1));
        if (mode == 4) ++r.temporalEpoch;
        auto* actual = r.AcquireResolvedForPresent(address,w,h,f,&selectedPlan,&handoff);
        Require(actual == resolved->tex->texture.get(), "actual selected final texture returned");
        if (mode == 2 || mode == 4) {
            Require(!handoff.packet, "changed final generation/history epoch has no handoff");
            Require(r.Flush() && r.WaitForGpu(), "mismatch leaves ordinary rendering available"); return;
        }
        Require(handoff.Matches() && handoff.selected.frame == realFrame &&
            handoff.afterPublicFlushFrame == realFrame+1 && handoff.selected.plan == selectedPlan,
            "public-frame boundary and full plan carry through production acquisition");
        gpu::video::FgPresentBridge bridge;
        auto reject = [&](auto change) {
            auto wrong = handoff; change(wrong);
            Require(!bridge.Select(wrong,1,8,8,8,8), "video rejects nonmatching exact lineage");
        };
        reject([](auto& x){ ++x.selected.owner; });
        reject([](auto& x){ ++x.selected.sourceAllocation; });
        reject([](auto& x){ ++x.selected.sourceGeneration; });
        reject([](auto& x){ ++x.selected.resolve; });
        reject([](auto& x){ ++x.selected.allocation; });
        reject([](auto& x){ ++x.selected.generation; });
        reject([](auto& x){ ++x.selected.frame; });
        reject([](auto& x){ ++x.selected.historyEpoch; });
        reject([](auto& x){ ++x.currentHistoryEpoch; });
        reject([](auto& x){ ++x.selected.plan.cpuSerial; });
        reject([](auto& x){ ++x.selected.plan.geometryEpoch; });
        reject([](auto& x){ ++x.selected.plan.output.width; });
        Require(!bridge.Select(handoff,2,8,8,8,8), "device epoch mismatch");
        Require(!bridge.Select(handoff,1,8,8,16,16), "prepared output mismatch");
        Require(bridge.Select(handoff,1,8,8,8,8), "actual video boundary accepts matching selected resolve");
        Require(!packet->DiagnosticReady(), "selection is not GPU completion or provider readiness");
        if (mode == 6) {
            bridge.CancelAll(HandoffCancel::DisplayChange); r.CancelFgSnapshots(HandoffCancel::DisplayChange);
            Require(packet->canceled == HandoffCancel::DisplayChange && !packet->DiagnosticReady(), "display/mode cancel");
            fixture::rejectWaitOnce = true;
            Require(!r.WaitForGpu() && !packet->ProducersComplete(), "failed producer wait preserves pending packet");
            Require(r.WaitForGpu(), "later real wait completes pending producer work"); bridge.Collect();
            Require(!bridge.LiveCount(), "canceled packet retires after its producers complete"); return;
        }
        Require(gpu::video::BeginGpuCommands(presentList.get()), "real presentation commands");
        presentation.DrawComposited(presentList.get(),actual,output->texture.get(),8,8,8,8,gpu::ScalingFilter::Bilinear);
        Require(gpu::video::EndGpuCommands(presentList.get()), "present command end");
        const RenderCommandList* lists[]{presentList.get()}; uint64_t presentSerial=0; int32_t result=0;
        if (mode == 8) fixture::rejectSubmit = true;
        const bool submitted = gpu::video::SubmitRendererBatch(lists,1,presentFence.get(),presentSerial,result);
        if (mode == 8) {
            Require(!submitted, "injected consumer submission rejection"); bridge.CancelRecording();
            Require(!packet->presentSerial && !packet->DiagnosticReady(), "unsubmitted consumer has no completion token");
            Require(r.WaitForGpu(), "retire earlier producers after consumer failure"); bridge.Collect();
            Require(!bridge.LiveCount(), "consumer submission failure retires without fabricated completion"); return;
        }
        Require(submitted, "checked real video-queue presentation submit"); bridge.Submitted(1,presentSerial);
        // Overwrite the mutable resolve AFTER it was selected and submitted.
        // The normal queue is ordered; the diagnostic owns a separate copy.
        Clear(*resolved->tex, RenderColor(0,1,0,1));
        Require(r.Flush(), "later resolve overwrite submit");
        fixture::rejectWaitOnce = true;
        Require(!gpu::video::WaitForGpuFence(presentFence.get()), "failed consumer wait fixture");
        bridge.WaitFailed(); Require(!packet->DiagnosticReady() && packet->presentWaitFailed, "wait failure retains pending consumer");
        bridge.Completed(2,presentSerial);
        Require(!packet->DiagnosticReady(), "different-device completion cannot satisfy wait");
        Require(gpu::video::WaitForGpuFence(presentFence.get()), "actual present completion");
        Require(bridge.Completed(1,presentSerial) == 1 && packet->DiagnosticReady(), "exact real producer/present completion publishes diagnostic");
        Require(packet->producer->ui == UiSeparation::Unavailable, "never relabel SR snapshot HUDless");
        Require(!bridge.Select(handoff,1,8,8,8,8), "selected packet cannot be replayed");
        Require(r.WaitForGpu(), "retire later overwrite before readback");
        Require(gpu::video::BeginGpuCommands(presentList.get()), "independent final readback begin");
        presentList->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(packet->finalColor.region.texture,RenderTextureLayout::COPY_SOURCE));
        presentList->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),RenderFormat::R8G8B8A8_UNORM,8,8,1,64),
            RenderTextureCopyLocation::Subresource(packet->finalColor.region.texture));
        Require(gpu::video::EndGpuCommands(presentList.get()), "readback end");
        Require(gpu::video::SubmitRendererBatch(lists,1,presentFence.get(),presentSerial,result) &&
            gpu::video::WaitForGpuFence(presentFence.get()), "immutable readback completion");
        const auto* data = static_cast<const uint32_t*>(readback->map()); Require(data,"readback map");
        for (uint32_t y=0;y<8;++y) for (uint32_t x=0;x<8;++x)
            Require(data[y*64+x] == 0x804080ffu, "independent final pixels survive later raw resolve overwrite");
        readback->unmap();
        std::weak_ptr<void> finalLifetime = packet->finalColor.lifetime;
        handoff = {}; r.fgInputSnapshot.reset(); source.reset();
        r.fgHandoffPool.CancelAll(HandoffCancel::Shutdown); r.fgHandoffPool.Collect();
        packet.reset();
        Require(finalLifetime.expired(), "last consumer release reclaims final copy");
        std::printf("FG_HANDOFF mode=%u frame=%llu actual_resolve_pixels=64 provider_ready=0\n", mode,
            static_cast<unsigned long long>(realFrame));
    }
