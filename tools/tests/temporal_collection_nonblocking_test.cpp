#include "gpu/temporal_collection_gpu.h"
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
std::atomic<bool> tracking{false};
std::atomic<size_t> allocations{0};
size_t requested = 0, submitted = 0;
}
void* operator new(size_t size) {
    if(tracking)++allocations;
    if(auto* result=std::malloc(size?size:1))return result;
    throw std::bad_alloc();
}
void* operator new[](size_t size) {return ::operator new(size);}
void operator delete(void* value) noexcept {std::free(value);}
void operator delete[](void* value) noexcept {std::free(value);}
void operator delete(void* value,size_t) noexcept {std::free(value);}
void operator delete[](void* value,size_t) noexcept {std::free(value);}
namespace gpu::taa_collection {
bool WantSparse(){++requested;return true;}
uint64_t ConsentEpoch(){return 1;}
void SubmitSparse(SparseFrame){++submitted;}
}

int main() {
    gpu::taa_collection::SparseDepthGPU sampler;
    assert(!sampler.Ready());
    gpu::taa_collection::SparseFrame frame;
    frame.width=3840;frame.height=2160;
    tracking=true;
    for(uint64_t i=0;i<100;++i) {
        frame.frame=i;
        // Non-null sentinels prove that readiness rejects before dereferencing
        // GPU handles, consulting consent, compiling, mapping or submitting.
        sampler.Record(reinterpret_cast<plume::RenderCommandList*>(uintptr_t(1)),
            reinterpret_cast<plume::RenderTexture*>(uintptr_t(1)),frame);
        sampler.ReleaseCompleted();
    }
    tracking=false;
    assert(!allocations && !requested && !submitted && !sampler.Ready());
    std::cout<<"unprepared sparse GPU sampler: 100 safe skips, allocations=0, consent calls=0, submissions=0\n";
}
