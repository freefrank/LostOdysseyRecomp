#include "gpu/position_evidence_collection.h"
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include <semaphore>

namespace {
thread_local bool trackAllocations=false;
thread_local size_t allocations=0;
int checks=0;
void Check(bool value){++checks;assert(value);}
struct Gate {
    std::binary_semaphore entered{0}, release{0}, finished{0};
    std::atomic<bool> rawMatched{false};
};
}
void* operator new(size_t size) {
    if(trackAllocations)++allocations;
    if(auto* result=std::malloc(size?size:1))return result;
    throw std::bad_alloc();
}
void* operator new[](size_t size){return ::operator new(size);}
void operator delete(void* value) noexcept{std::free(value);}
void operator delete[](void* value) noexcept{std::free(value);}
void operator delete(void* value,size_t) noexcept{std::free(value);}
void operator delete[](void* value,size_t) noexcept{std::free(value);}

int main(){
    using gpu::position_evidence::Collection;
    using gpu::position_evidence::Summary;
    using namespace std::chrono_literals;
    const uint32_t words[]{0x01020304,0x55667788};
    const Summary expected{1,1,7,0,5};
    auto gate=std::make_shared<Gate>();
    auto collection=std::make_unique<Collection>([gate,expected](std::span<const uint8_t> raw){
        const uint32_t expectedWords[]{0x01020304,0x55667788};
        gate->rawMatched=raw.size()==sizeof(expectedWords)&&std::memcmp(raw.data(),expectedWords,sizeof(expectedWords))==0;
        gate->entered.release();
        gate->release.acquire();
        gate->finished.release();
        return expected;
    });
    Summary output;
    trackAllocations=true;
    Check(!collection->TryGet(1,words,2,output));
    Check(allocations==0);
    trackAllocations=false;
    bool entered=false;
    const auto enterDeadline=std::chrono::steady_clock::now()+2s;
    while(!(entered=gate->entered.try_acquire())&&std::chrono::steady_clock::now()<enterDeadline){
        collection->TryGet(1,words,2,output);std::this_thread::yield();
    }
    Check(entered);
    const auto start=std::chrono::steady_clock::now();
    trackAllocations=true;
    bool allPending=true;
    for(size_t i=0;i<1000;++i)allPending&=!collection->TryGet(1,words,2,output);
    trackAllocations=false;
    Check(allPending);
    Check(output==Summary{});
    Check(allocations==0);
    Check(std::chrono::steady_clock::now()-start<250ms);
    Check(gate->rawMatched);
    gate->release.release();
    Check(gate->finished.try_acquire_for(2s));
    bool ready=false;
    const auto deadline=std::chrono::steady_clock::now()+2s;
    do{ready=collection->TryGet(1,words,2,output);if(!ready)std::this_thread::yield();}
    while(!ready&&std::chrono::steady_clock::now()<deadline);
    Check(ready&&output==expected);
    trackAllocations=true;
    Check(collection->TryGet(1,words,2,output));
    Check(allocations==0);
    trackAllocations=false;
    collection->Stop();
    Check(!collection->TryGet(1,words,2,output));
    Check(output==Summary{});
    collection.reset();

    auto blocked=std::make_shared<Gate>();
    auto stopping=std::make_unique<Collection>([blocked](std::span<const uint8_t>){
        blocked->entered.release();blocked->release.acquire();blocked->finished.release();return Summary{};
    });
    entered=false;
    const auto blockedDeadline=std::chrono::steady_clock::now()+2s;
    while(!(entered=blocked->entered.try_acquire())&&std::chrono::steady_clock::now()<blockedDeadline){
        stopping->TryGet(2,words,2,output);
        std::this_thread::yield();
    }
    Check(entered);
    const auto stopStart=std::chrono::steady_clock::now();
    stopping.reset();
    Check(std::chrono::steady_clock::now()-stopStart<250ms);
    blocked->release.release();
    Check(blocked->finished.try_acquire_for(2s));
    std::cout<<checks<<" checks passed: blocked analyzer does not delay producer/destructor; raw copy, zero producer allocations, unproven/ready result, stop gate and detached state lifetime\n";
}
