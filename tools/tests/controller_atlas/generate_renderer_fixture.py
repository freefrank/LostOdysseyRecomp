"""Embed the exact production selection + invalidation methods into a CPU mock."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = (args.root / 'LostOdysseyRecomp/gpu/renderer.cpp').read_text(encoding='utf-8')
select = source[source.index('            HostTexture* SelectControllerAtlas('):
                source.index('            void TraceControllerAtlasRecorded(', source.index('            HostTexture* SelectControllerAtlas('))]
assert 'hid::prompts::atlas::PatchPlayStationAtlasRgba(' in select
assert select.index('hid::prompts::atlas::PatchPlayStationAtlasRgba(') < select.index('Upload(source->atlasPendingRgba.data(),')
assert 'if (video::GpuWorkStopped()) return nullptr;' in select
trace = source[source.index('            void TraceControllerAtlasRecorded('):
               source.index('            HostTexture* GetTexture(', source.index('            void TraceControllerAtlasRecorded('))]
assert 'bindings[candidate.bank][candidate.slot] != candidate.image' in trace
assert 'controllerAtlasTraceCount >= 64' in trace
assert 'debugDraw ? debugDraw - 1' in trace
assert 'atlas={}' in trace and 'controller_atlas::Name(candidate.identity)' in trace and 'state=recorded' in trace
plan = source[source.index('            bool PlanSuppressed('):
              source.index('            HostTexture* GetRenderTarget(', source.index('            bool PlanSuppressed('))]
assert 'if (video::GpuWorkStopped()) return true;' in plan
invalidate = source[source.index('            void InvalidateRange('):
                    source.index('            // ---- pipeline state', source.index('            void InvalidateRange('))]
get = source[source.index('            HostTexture* GetTexture('):
             source.index('            void InvalidateRange(', source.index('            HostTexture* GetTexture('))]
assert get.count('return SelectControllerAtlas(cached, bindingInfo, bindingEpoch);') == 2
assert 'return SelectControllerAtlas(result, bindingInfo, bindingEpoch);' in get
assert 'controller_atlas::Candidate(dimension, format, originalWidth, originalHeight,' in get
assert 'controller_atlas::Identify(rgba)' in get
assert get.index('SampleHash(src, tex->guestBytes)') < get.index('controller_atlas::Identify(rgba)')
assert get.count('controller_atlas::Identify(rgba)') == 1
assert get.index('auto it = textures.find(key)') < get.index('controller_atlas::Identify(rgba)')
assert 'Gpu().retiredTextures.push_back(std::move(it->second));' in get
recycle = source[source.index('            bool RecycleSlot('):source.index('            bool WaitForGpu(')]
assert recycle.index('if (!video::WaitForGpuFence(s.fence.get())) return false;') < recycle.index('s.retiredTextures.clear();')
bind = source[source.index('                auto bindTextures = '):source.index('                const auto samplerVersion = ')]
assert 'if (PlanSuppressed())' in bind and 'failedPlan = true;' in bind
assert bind.index('textureBindings[bank][slot] = temporalDisplay ?') < bind.index('controllerAtlasCandidates.push_back(')
assert 'traceControllerAtlas && tex->controllerAtlasRecognized' in bind
assert 'bank, slot,' in bind
drawIndexed = source.index('commandList->drawIndexedInstanced(indexCount, 1, 0, baseVertex, 0);')
drawInstanced = source.index('commandList->drawInstanced(indexCount, 1, uint32_t(baseVertex), 0);', drawIndexed)
recordedTrace = source.index('TraceControllerAtlasRecorded(controllerAtlasCandidates, textureBindings, key.vs, key.ps);', drawInstanced)
assert drawIndexed < drawInstanced < recordedTrace
template = Path(__file__).with_name('renderer_fixture.template.cpp').read_text(encoding='utf-8')
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(template.replace('/* PRODUCTION_SELECT */', select)
                      .replace('/* PRODUCTION_TRACE */', trace)
                      .replace('/* PRODUCTION_PLAN */', plan)
                      .replace('/* PRODUCTION_INVALIDATE */', invalidate), encoding='utf-8')
