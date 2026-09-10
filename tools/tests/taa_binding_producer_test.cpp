#include "gpu/taa_binding_producer.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include <type_traits>

namespace {
bool countAllocations = false;
unsigned allocations = 0, checks = 0;
void Check(bool condition) { ++checks; if (!condition) std::abort(); }
using namespace gpu::taa_collection::binding;
Transform Camera(bool jittered = true) {
    Transform t;
    t.slot = 7; t.phase = 5; t.applied = jittered;
    t.viewport = {0, 0, std::bit_cast<uint32_t>(1280.0f), std::bit_cast<uint32_t>(720.0f)};
    for (unsigned i = 0; i < 16; ++i) t.guestVP[i] = std::bit_cast<uint32_t>(i % 5 == 0 ? 1.0f : 0.0f);
    t.uploadedVP = t.guestVP;
    if (jittered) {
        t.jitterNdc = {std::bit_cast<uint32_t>(0.0001f), std::bit_cast<uint32_t>(-0.0002f)};
        t.uploadedVP[12] = t.jitterNdc[0]; t.uploadedVP[13] = t.jitterNdc[1];
    }
    return t;
}
}
void* operator new(size_t size) {
    if (countAllocations) ++allocations;
    if (auto* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }

int main() {
    static_assert(std::is_trivially_copyable_v<Producer>);
    static_assert(noexcept(Producer{}.Draw(0, 0, Transform{})));
    const auto camera = Camera();
    countAllocations = true;
    Check(ProvenTransform(camera));
    Producer source;
    source.Draw(3, 100, camera);
    Check(source.state == ProducerState::Unknown); // Uninitialized pixels remain unproven.
    source.Clear(3, 100, true);
    source.Draw(3, 100, camera);
    Check(source.state == ProducerState::UniformJittered);
    Check(source.draws == 1);
    auto otherSlot = camera; otherSlot.slot = 8;
    source.Draw(3, 100, otherSlot);
    Check(source.state == ProducerState::UniformJittered && source.draws == 2);
    Texture receipt;
    source.Describe(receipt, 3, 102);
    Check(receipt.producerFrameAge == 2 && receipt.producer == camera);
    Texture revoked;
    source.Describe(revoked, 4, 102);
    Check(revoked.producerFrameAge == -1 && revoked.producerState == ProducerState::Unknown);

    Producer resolved;
    resolved.Copy(source, 3, 104, true);
    Texture copied;
    resolved.Describe(copied, 3, 105);
    Check(copied.producerFrameAge == 5); // Copy time is not the content's producer time.
    Check(copied.producerState == ProducerState::UniformJittered);
    resolved.Copy(resolved, 3, 105, true);
    Check(resolved.frame == 100 && resolved.draws == 2); // Alias-safe metadata transfer.
    Producer cropped;
    cropped.Copy(resolved, 3, 106, true);
    Check(cropped.state == ProducerState::UniformJittered && cropped.frame == 100);
    cropped.Copy(source, 3, 106, false);
    Check(cropped.state == ProducerState::Mixed); // Untracked retained rectangle.
    Texture mixed;
    cropped.Describe(mixed, 3, 106);
    Check(mixed.producerState == ProducerState::Mixed && mixed.producer.slot == -1);

    Producer partialIntoClear;
    partialIntoClear.Clear(3, 106, true);
    partialIntoClear.Copy(source, 3, 106, false);
    Check(partialIntoClear.state == ProducerState::UniformJittered && partialIntoClear.frame == 100);
    partialIntoClear.Clear(3, 107, false);
    Check(partialIntoClear.state == ProducerState::Mixed);

    source.Draw(3, 101, camera);
    Check(source.state == ProducerState::Mixed); // Old-frame pixels were not cleared.
    source.Clear(3, 101, true);
    auto differentPhase = camera; differentPhase.phase = 6;
    source.Draw(3, 101, camera);
    source.Draw(3, 101, differentPhase);
    Check(source.state == ProducerState::Mixed);
    source.Clear(3, 102, true);
    source.Draw(3, 102, camera);
    auto differentVP = camera; differentVP.guestVP[0] = std::bit_cast<uint32_t>(2.0f);
    source.Draw(3, 102, differentVP);
    Check(source.state == ProducerState::Mixed);
    source.Clear(3, 103, true);
    auto invalid = camera; invalid.uploadedVP[0] = 0x7fc00000u;
    source.Draw(3, 103, invalid);
    Check(source.state == ProducerState::Unknown);
    source.Draw(3, 103, camera);
    Check(source.state == ProducerState::Mixed); // A good later draw does not erase bad evidence.

    source.Clear(3, 104, true);
    auto unjittered = Camera(false);
    source.Draw(3, 104, unjittered);
    unjittered.phase = 17;
    source.Draw(3, 104, unjittered);
    Check(source.state == ProducerState::UniformUnjittered && source.transform.phase == 0);
    source.Draw(4, 104, camera);
    Check(source.state == ProducerState::Unknown); // A fresh consent generation cannot inherit proof.
    resolved.Copy(source, 5, 110, true);
    Check(resolved.state == ProducerState::Unknown && !resolved.initialized);
    resolved.Clear(5, 110, true);
    resolved.Copy(source, 5, 110, false);
    Check(resolved.state == ProducerState::Mixed);

    Check(RelativeAge(10, 11) == -1);
    Check(RelativeAge(255, 0) == 255);
    Check(RelativeAge(256, 0) == -1);
    source.Clear(8, 120, true);
    for (unsigned i = 0; i < 65535; ++i) source.Draw(8, 120, camera);
    Check(source.draws == 65535 && source.state == ProducerState::UniformJittered);
    source.Draw(8, 120, camera);
    Check(source.draws == 0 && source.state == ProducerState::Unknown);
    source.Mixed(8, 120);
    source.Draw(8, 120, camera);
    Check(source.draws == 0 && source.state == ProducerState::Unknown);
    source.Clear(8, 121, true);
    source.Draw(8, 121, camera);
    Check(source.draws == 1 && source.state == ProducerState::UniformJittered);
    countAllocations = false;
    Check(allocations == 0);
    std::cout << checks << " checks passed; producer heap allocations=" << allocations << '\n';
}
