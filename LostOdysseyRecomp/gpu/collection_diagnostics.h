#pragma once
#include <algorithm>
#include "taa_binding_collection.h"
#include "summary_collection.h"
#include <atomic>
#include <chrono>
#include <limits>
#include <mutex>
#include <string_view>

namespace gpu::taa_collection::diagnostics {
inline constexpr size_t FrameCapacity=32, PairCapacity=24, BindingCapacity=8, MaxRequestBytes=32768;
inline constexpr const char* Build="0.5.2-collection-diagnostics-1";
inline constexpr uint32_t CounterLimit=1000000000;
inline void Increment(uint32_t& value,uint32_t amount=1) noexcept {value+=std::min(amount,CounterLimit-value);}
enum class Stream : size_t {Source,Summary,Binding,Sparse,Compact};
struct Counters {
    std::array<uint32_t,6> source{}; // queued, known, invalid, full, disabled, busy
    std::array<uint32_t,5> summary{}; // queued, counted, dropped, disabled, busy
    std::array<uint32_t,6> binding{}; // queued, counted, full, busy, disabled, stale
    std::array<uint32_t,8> sparse{}; // queued, duplicate, full, busy, disabled, stale, cooldown, discontinuous
    uint32_t pairDropped=0,bindingDropped=0,lockBusy=0,frameDiscontinuity=0;
};
// Final CPU recording state only. Never GPU completion or pixel acceptance.
struct Frame {
    uint32_t offset=0,sceneRejection=0,historyRejection=0;
    int32_t previousFrameDelta=-1;
    bool taa=false,ready=false,completed=false,reused=false,historyCaptured=false,
        cameraChecks=false,sameEpoch=false,resetAfterFrame=false,sparseReady=false;
};
struct Pair {summary_collection::Key key{};uint32_t first=0,last=0,count=0;};
struct Binding {uint32_t offset=0;binding::Record record{};};
struct Window {
    // Local tokens are deliberately omitted by every serializer.
    uint64_t token=0,consentEpoch=0;
    bool complete=false;
    uint32_t frameSpan=0;
    size_t frameCount=0,pairCount=0,bindingCount=0;
    Counters counters{};
    std::array<Frame,FrameCapacity> frames{};
    std::array<Pair,PairCapacity> pairs{};
    std::array<Binding,BindingCapacity> bindings{};
};
struct Delivery {uint32_t accepted=0,transportFailed=0,httpRejected=0;};
struct DeliverySnapshot {
    std::array<Delivery,5> streams{};
    uint32_t summaryPending=0,sourcePending=0,bindingPending=0,sparseFrames=0;
};
enum class SparseResult : size_t {Queued,Duplicate,Full,Busy,Disabled,Stale,Cooldown,Discontinuous};

// Own lock avoids holding the existing source/summary mutex on renderer hooks.
// Producers use try_lock only, fixed storage, bounded scans and copies. Worker
// snapshots/reset may lock; no producer ever waits for the worker or HTTP.
class Queue {
    mutable std::mutex mutex_;
    Window window_{};
    uint64_t nextToken_=0,startFrame_=0,currentFrame_=0;
    bool pending_=false;
    std::atomic<bool> active_{false};
    std::atomic<uint32_t> lockBusy_{0};
    std::chrono::steady_clock::time_point started_{},nextWindow_{};
    void Seal() noexcept {
        active_.store(false,std::memory_order_relaxed);
        window_.counters.lockBusy=std::min(lockBusy_.exchange(0),CounterLimit);
        window_.complete=window_.frameCount==FrameCapacity&&window_.frameSpan==FrameCapacity&&!window_.counters.frameDiscontinuity;
        pending_=window_.frameCount!=0;
        nextWindow_=std::chrono::steady_clock::now()+std::chrono::minutes(3);
    }
    void Miss() noexcept {if(active_.load(std::memory_order_relaxed))lockBusy_.fetch_add(1,std::memory_order_relaxed);}
    bool Current(uint64_t frame,uint64_t epoch) const noexcept {
        return active_.load(std::memory_order_relaxed)&&window_.consentEpoch==epoch&&frame==currentFrame_&&frame>=startFrame_&&frame-startFrame_<FrameCapacity;
    }
public:
    bool Active() const noexcept {return active_.load(std::memory_order_relaxed);}
    bool Begin(uint64_t frame,uint64_t epoch,std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now()) noexcept try {
        std::unique_lock lock(mutex_,std::try_to_lock);if(!lock){Miss();return false;}
        if(pending_)return false;
        if(active_&&window_.consentEpoch!=epoch){active_=false;window_={};}
        if(active_&&(frame<currentFrame_||frame-startFrame_>=FrameCapacity||now-started_>=std::chrono::seconds(10))) {
            if(frame!=currentFrame_+1)Increment(window_.counters.frameDiscontinuity);
            Seal();return false;
        }
        if(!active_) {
            if(now<nextWindow_)return false;
            window_={};window_.token=++nextToken_;window_.consentEpoch=epoch;
            startFrame_=currentFrame_=frame;started_=now;lockBusy_=0;active_=true;
        } else if(frame!=currentFrame_) {
            if(frame!=currentFrame_+1)Increment(window_.counters.frameDiscontinuity);
            currentFrame_=frame;
        }
        window_.frameSpan=uint32_t(frame-startFrame_)+1;
        return true;
    } catch(...){Miss();return false;}
    void Count(Stream stream,size_t result,uint64_t epoch) noexcept try {
        if(!Active())return;
        std::unique_lock lock(mutex_,std::try_to_lock);if(!lock){Miss();return;}
        if(!active_||window_.consentEpoch!=epoch)return;
        auto add=[&](auto& values){if(result<values.size())Increment(values[result]);};
        switch(stream){case Stream::Source:add(window_.counters.source);break;case Stream::Summary:add(window_.counters.summary);break;
            case Stream::Binding:add(window_.counters.binding);break;case Stream::Sparse:add(window_.counters.sparse);break;default:break;}
    } catch(...){Miss();}
    void ObservePair(uint64_t frame,uint64_t epoch,const summary_collection::Key& key) noexcept try {
        if(!Active())return;
        std::unique_lock lock(mutex_,std::try_to_lock);if(!lock){Miss();return;}
        if(!Current(frame,epoch))return;
        const auto width=std::get<2>(key),height=std::get<3>(key);
        if(!width||width>7680||!height||height>4320){Increment(window_.counters.pairDropped);return;}
        const uint32_t offset=uint32_t(frame-startFrame_);
        for(size_t i=0;i<window_.pairCount;++i)if(window_.pairs[i].key==key) {
            auto& pair=window_.pairs[i];pair.last=offset;Increment(pair.count);return;
        }
        if(window_.pairCount==PairCapacity){Increment(window_.counters.pairDropped);return;}
        window_.pairs[window_.pairCount++]={key,offset,offset,1};
    } catch(...){Miss();}
    void ObserveBinding(uint64_t frame,uint64_t epoch,const binding::Record& record) noexcept try {
        if(!Active())return;
        std::unique_lock lock(mutex_,std::try_to_lock);if(!lock){Miss();return;}
        if(!Current(frame,epoch))return;
        if(window_.bindingCount==BindingCapacity){Increment(window_.counters.bindingDropped);return;}
        window_.bindings[window_.bindingCount++]={uint32_t(frame-startFrame_),record};
    } catch(...){Miss();}
    void End(uint64_t frame,uint64_t epoch,Frame record) noexcept try {
        if(!Active())return;
        std::unique_lock lock(mutex_,std::try_to_lock);if(!lock){Miss();return;}
        if(!Current(frame,epoch))return;
        record.offset=uint32_t(frame-startFrame_);
        if(window_.frameCount&&window_.frames[window_.frameCount-1].offset>=record.offset)return;
        if(window_.frameCount>=FrameCapacity){Increment(window_.counters.frameDiscontinuity);Seal();return;}
        if(record.offset!=window_.frameCount)Increment(window_.counters.frameDiscontinuity);
        window_.frames[window_.frameCount++]=record;window_.frameSpan=record.offset+1;
        if(window_.frameSpan==FrameCapacity)Seal();
    } catch(...){Miss();}
    // Worker only. Timeout also seals when rendering stopped entirely.
    bool Snapshot(Window& output,std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now()) {
        std::lock_guard lock(mutex_);
        if(active_&&now-started_>=std::chrono::seconds(10))Seal();
        if(!pending_)return false;output=window_;return true;
    }
    void Acknowledge(uint64_t token,uint64_t epoch) {
        std::lock_guard lock(mutex_);if(pending_&&window_.token==token&&window_.consentEpoch==epoch)pending_=false;
    }
    void Reset() {
        std::lock_guard lock(mutex_);active_=false;pending_=false;window_={};lockBusy_=0;nextWindow_={};
    }
};

template<size_t N> inline void NamedCounters(std::ostream& out,const std::array<uint32_t,N>& values,const std::array<const char*,N>& names) {
    out<<'{';for(size_t i=0;i<N;++i){if(i)out<<',';out<<'"'<<names[i]<<"\":"<<values[i];}out<<'}';
}
inline std::string Request(std::string_view backend,std::string_view gpu,std::string_view driver,
    std::string_view runtimeVersion,std::string_view runtimeCommit,Window window,const DeliverySnapshot& delivery) {
    if(!window.frameCount||window.frameCount>FrameCapacity||!window.frameSpan||window.frameSpan>FrameCapacity||window.pairCount>PairCapacity||window.bindingCount>BindingCapacity)return {};
    // Only pre-allowlisted device/build strings enter this worker-only formatter.
    for(;;) {
        std::ostringstream out;out.imbue(std::locale::classic());out<<std::boolalpha;
        out<<"{\"schema\":4,\"build\":\""<<Build<<"\",\"backend\":\""<<backend<<"\",\"gpu\":\""<<gpu<<"\",\"driver\":\""<<driver<<"\",\"records\":[{";
        out<<"\"capabilities\":{\"version\":1,\"runtimeVersion\":\""<<runtimeVersion<<"\",\"runtimeCommit\":\""<<runtimeCommit
            <<"\",\"cpuWindowFrames\":32,\"cpuCooldownSeconds\":180,\"pendingWindowCapacity\":1,\"pairCapacity\":24,\"bindingCapacity\":8,"
            <<"\"bindingPairs\":[\"e810cfacc107fd3c:5b11f88a8bb293df\",\"e810cfacc107fd3c:78a5c96b2d7eaa91\"],\"bindingTextureSlot\":0,\"bindingPerPairPerFrame\":1,"
            <<"\"sourceMaxProgramBytes\":65536,\"sourceScope\":\"draw-program-observe\",\"summaryScope\":\"taa-draw-observe\",\"strictAck\":[\"compact\"],"
            <<"\"sparseSupported\":"<<(backend=="d3d12")<<",\"sparseFrames\":32,\"sparseCooldownSeconds\":300,\"sparseWindowLinked\":false,\"gpuCompletion\":false,\"colorImages\":false},"
            <<"\"complete\":"<<window.complete<<",\"frameSpan\":"<<window.frameSpan<<",\"counters\":{\"source\":";
        NamedCounters(out,window.counters.source,std::array{"queued","known","invalid","full","disabled","busy"});
        out<<",\"summary\":";NamedCounters(out,window.counters.summary,std::array{"queued","counted","dropped","disabled","busy"});
        out<<",\"binding\":";NamedCounters(out,window.counters.binding,std::array{"queued","counted","full","busy","disabled","stale"});
        out<<",\"sparse\":";NamedCounters(out,window.counters.sparse,std::array{"queued","duplicate","full","busy","disabled","stale","cooldown","discontinuous"});
        out<<",\"compact\":{\"pairDropped\":"<<window.counters.pairDropped<<",\"bindingDropped\":"<<window.counters.bindingDropped<<",\"lockBusy\":"<<window.counters.lockBusy<<",\"frameDiscontinuity\":"<<window.counters.frameDiscontinuity<<"}},\"delivery\":{\"scope\":\"since-consent-reset\"";
        constexpr std::array names{"source","summary","binding","sparse","compact"};
        for(size_t i=0;i<names.size();++i){const auto& d=delivery.streams[i];out<<",\""<<names[i]<<"\":{\"accepted\":"<<d.accepted<<",\"transportFailed\":"<<d.transportFailed<<",\"httpRejected\":"<<d.httpRejected<<'}';}
        out<<"},\"pending\":{\"summary\":"<<delivery.summaryPending<<",\"source\":"<<delivery.sourcePending<<",\"binding\":"<<delivery.bindingPending<<",\"sparseFrames\":"<<delivery.sparseFrames<<"},\"frames\":[";
        for(size_t i=0;i<window.frameCount;++i){const auto& f=window.frames[i];if(i)out<<',';
            out<<"{\"offset\":"<<f.offset<<",\"taa\":"<<f.taa<<",\"ready\":"<<f.ready<<",\"completed\":"<<f.completed<<",\"reused\":"<<f.reused
                <<",\"sceneRejection\":"<<f.sceneRejection<<",\"historyCaptured\":"<<f.historyCaptured<<",\"historyRejection\":"<<f.historyRejection
                <<",\"cameraChecks\":"<<f.cameraChecks<<",\"previousFrameDelta\":"<<f.previousFrameDelta<<",\"sameEpoch\":"<<f.sameEpoch
                <<",\"resetAfterFrame\":"<<f.resetAfterFrame<<",\"sparseReady\":"<<f.sparseReady<<'}';}
        out<<"],\"pairs\":[";
        for(size_t i=0;i<window.pairCount;++i){const auto& p=window.pairs[i];const auto& [vs,ps,w,h,slot,candidates,flags,rejection,position,guards]=p.key;if(i)out<<',';
            out<<"{\"first\":"<<p.first<<",\"last\":"<<p.last<<",\"record\":"<<taa_collection::RecordJson(vs,ps,w,h,slot,candidates,flags,rejection,p.count,position,guards)<<'}';}
        out<<"],\"bindings\":[";
        for(size_t i=0;i<window.bindingCount;++i){const auto& b=window.bindings[i];if(i)out<<',';out<<"{\"offset\":"<<b.offset<<",\"record\":"<<binding::RecordJson(b.record,1)<<'}';}
        out<<"]}]}";auto body=out.str();if(body.size()<=MaxRequestBytes)return body;
        // Preserve frame/gap diagnostics even when maximal decimal matrix values
        // expand JSON. Explicit drop counts describe every trimmed reference.
        if(window.pairCount){--window.pairCount;Increment(window.counters.pairDropped);}
        else if(window.bindingCount){--window.bindingCount;Increment(window.counters.bindingDropped);}
        else return {};
    }
}

// Strict bounded receipt parser: only the two required fields, no duplicates,
// no escapes or nested data. JSON whitespace/key order may vary.
inline bool AcceptedReceipt(std::string_view text) noexcept {
    if(text.size()>512)return false;size_t at=0;bool accepted=false,id=false;
    const auto space=[&]{while(at<text.size()&&(text[at]==' '||text[at]=='\r'||text[at]=='\n'||text[at]=='\t'))++at;};
    const auto take=[&](char c){space();if(at>=text.size()||text[at]!=c)return false;++at;return true;};
    const auto quoted=[&](std::string_view& value){if(!take('"'))return false;const auto begin=at;
        while(at<text.size()&&text[at]!='"'){if(text[at]=='\\'||static_cast<unsigned char>(text[at])<32)return false;++at;}
        if(at==text.size())return false;value=text.substr(begin,at-begin);++at;return true;};
    if(!take('{'))return false;
    for(unsigned field=0;field<2;++field){std::string_view key;if(!quoted(key)||!take(':'))return false;
        if(key=="accepted"){if(accepted||!take('1'))return false;accepted=true;}
        else if(key=="id"){std::string_view value;if(id||!quoted(value)||value.size()!=64)return false;
            for(char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;id=true;}
        else return false;
        if(field==0&&!take(','))return false;
    }
    if(!take('}'))return false;space();return accepted&&id&&at==text.size();
}
}
