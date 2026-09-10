#include "taa_collection.h"
#include "taa_collection_format.h"
#include "temporal_collection.h"
#include "shader_source_collection.h"
#include "collection_worker.h"
#include "summary_collection.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>
#include <tuple>
#include <vector>
#include <fmt/format.h>
#include <os/logger.h>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace gpu::taa_collection {
namespace {
using summary_collection::Key;
using summary_collection::Entry;
using summary_collection::Priority;
struct State {
    std::atomic<int> consent{-1};
    std::atomic<uint64_t> generation{0};
    UploadRequestGate uploadRequest;
    std::mutex mutex;
    std::vector<SparseFrame> sparseFrames;
    std::chrono::steady_clock::time_point nextSparse{};
    summary_collection::Queue summaries;
    shader_sources::Queue sourcePrograms;
    std::string backend="d3d12", gpu="Unknown", driver="0";
};
const std::shared_ptr<State> state=std::make_shared<State>();
std::once_flag initialized;
std::unique_ptr<CollectionWorker> uploader;
bool Enabled(const State& s){return s.consent.load(std::memory_order_relaxed)==1;}
enum class UploadKind { Summary, Sparse, Source };
#ifdef _WIN32
struct Http {
    HINTERNET value{};
    ~Http(){if(value)WinHttpCloseHandle(value);}
    operator HINTERNET()const{return value;}
};
bool Upload(State& s,const CollectionWorker::Control& control,std::string& body, uint64_t epoch, UploadKind kind=UploadKind::Summary, const std::wstring& metadata=L"") {
    const auto current=[&]{return !control.Stopped()&&Enabled(s)&&epoch==s.generation;};
    if(!current())return false;
    Http session{WinHttpOpen(L"LostOdysseyRecomp-TAA/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0)};
    if(!session.value||!current())return false;
    WinHttpSetTimeouts(session,3000,3000,3000,3000);
    Http connection{WinHttpConnect(session,L"lo.dotslash.pro",INTERNET_DEFAULT_HTTPS_PORT,0)};
    if(!connection.value||!current())return false;
    const wchar_t* path=kind==UploadKind::Sparse?L"/v1/temporal":kind==UploadKind::Source?L"/v1/shader-sources":L"/v1/taa";
    Http request{WinHttpOpenRequest(connection,L"POST",path,nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
    if(!request.value||!current())return false;
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
    const std::wstring headers=kind==UploadKind::Sparse?L"Content-Type: application/octet-stream\r\n"+metadata:L"Content-Type: application/json\r\n";
    if(!current() || !WinHttpSendRequest(request,headers.c_str(),DWORD(-1),body.data(),DWORD(body.size()),DWORD(body.size()),0) ||
       !current() || !WinHttpReceiveResponse(request,nullptr))return false;
    DWORD status=0,size=sizeof(status);
    return current() && WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr) && status==200;
}
#else
bool Upload(State&,const CollectionWorker::Control&,std::string&,uint64_t,UploadKind=UploadKind::Summary,const std::wstring& = L""){return false;}
#endif
void Run(const std::shared_ptr<State>& owned,CollectionWorker::Control& control) {
    auto& s=*owned;
    auto nextNormal=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    auto nextSource=std::chrono::steady_clock::now()+std::chrono::minutes(3);
    for(;;) {
        if(control.Wait(std::chrono::seconds(10)))return;
        const auto requestedEpoch=s.uploadRequest.Consume(s.consent,s.generation);
        if(!Enabled(s))continue;
        std::vector<std::pair<Key,uint32_t>> batch;std::string body;uint64_t epoch;
        const bool normalDue=std::chrono::steady_clock::now()>=nextNormal;
        bool forceUpload=false;
        {
            std::lock_guard lock(s.mutex);if(!Enabled(s))continue;epoch=s.generation;
            forceUpload=requestedEpoch&&*requestedEpoch==epoch;
            std::vector<std::pair<Key,Entry>> pending;
            for(const auto& item:s.summaries.Entries())if(!item.second.sent&&(normalDue||forceUpload||Priority(item.first)<=1))pending.push_back(item);
            std::sort(pending.begin(),pending.end(),[](const auto& a,const auto& b){
                return std::pair(Priority(a.first),a.second.order)<std::pair(Priority(b.first),b.second.order);
            });
            body=RequestStart(s.backend,s.gpu,s.driver);
            for(const auto& [key,entry]:pending) {
                const auto& [vs,ps,w,h,slot,candidates,flags,rejection,position,guards]=key;
                if(!batch.empty())body+=',';
                body+=RecordJson(vs,ps,w,h,slot,candidates,flags,rejection,entry.count,position,guards);
                batch.emplace_back(key,entry.count);if(batch.size()==32)break;
            }
            body+="]}";
        }
        bool summariesAccepted=true;
        if(!batch.empty()) {
            summariesAccepted=false;try{if(epoch==s.generation&&Enabled(s))summariesAccepted=Upload(s,control,body,epoch);}catch(...){}
            std::lock_guard lock(s.mutex);
            if(summariesAccepted&&epoch==s.generation&&Enabled(s))for(const auto& [key,count]:batch){auto it=s.summaries.Entries().find(key);if(it!=s.summaries.Entries().end())it->second.sent=count;}
        }
        // Source programs are an independent incremental stream. A failing
        // summary request must not starve these missing-program reports.
        if(control.Stopped())return;
        if(forceUpload||std::chrono::steady_clock::now()>=nextSource) {
            nextSource=std::chrono::steady_clock::now()+std::chrono::minutes(3);
            shader_sources::Batch sourceBatch;std::string sourceStart;
            {
                std::lock_guard lock(s.mutex);
                if(Enabled(s)&&s.generation==epoch) {
                    sourceStart=shader_sources::RequestStart(s.backend,s.gpu,s.driver);
                    sourceBatch=s.sourcePrograms.Pending(sourceStart.size());
                }
            }
            if(!sourceBatch.programs.empty()) {
                bool accepted=false;
                try {
                    auto sourceBody=shader_sources::Request(std::move(sourceStart),sourceBatch);
                    bool current=false;
                    {std::lock_guard lock(s.mutex);current=sourceBatch.epoch==s.sourcePrograms.Epoch();}
                    if(!sourceBody.empty()&&current&&s.generation==epoch&&Enabled(s))
                        accepted=Upload(s,control,sourceBody,epoch,UploadKind::Source);
                }catch(...){}
                if(accepted) {
                    std::lock_guard lock(s.mutex);
                    if(Enabled(s)&&s.generation==epoch)s.sourcePrograms.Acknowledge(sourceBatch);
                }
            }
        }
        if(control.Stopped())return;
        if(!summariesAccepted||!normalDue)continue; // Shader delivery wins over archival MV traffic.
        nextNormal=std::chrono::steady_clock::now()+std::chrono::seconds(60);
        std::vector<SparseFrame> sparseBatch;std::wstring meta;
        {
            std::lock_guard lock(s.mutex);
            const bool urgent=std::any_of(s.summaries.Entries().begin(),s.summaries.Entries().end(),[](const auto& item){return !item.second.sent&&Priority(item.first)<=1;});
            if(Enabled(s)&&s.generation==epoch&&!urgent&&s.sparseFrames.size()==32) {
                sparseBatch=s.sparseFrames;
                auto text=fmt::format("X-LO-Build: 0.5.0-temporal-1\r\nX-LO-Backend: {}\r\nX-LO-GPU: {}\r\nX-LO-Driver: {}\r\n",s.backend,s.gpu,s.driver);
                meta.assign(text.begin(),text.end());
            }
        }
        if(!sparseBatch.empty()) {
            auto raw=SparseRaw(sparseBatch);auto packed=CompressSparse(raw);bool accepted=false;
            try{if(s.generation==epoch&&Enabled(s))accepted=Upload(s,control,packed,epoch,UploadKind::Sparse,meta);}catch(...){}
            if(accepted){std::lock_guard lock(s.mutex);if(s.generation==epoch)s.sparseFrames.clear();}
        }
    }
}
}
void Initialize(){std::call_once(initialized,[]{auto& s=*state;int value=-1;std::ifstream file("taa-collection.ini");file>>value;
    try {
        s.sourcePrograms.Initialize();s.sparseFrames.reserve(32);
        uploader=std::make_unique<CollectionWorker>([owned=state](auto& control){Run(owned,control);});
        s.consent=(value==0||value==1)?value:-1;
    }catch(...){s.consent=0;}
});}
void Shutdown(){state->consent=0;++state->generation;if(uploader)uploader->Stop();}
void RequestUpload(){if(state->uploadRequest.Request(state->consent,state->generation)&&uploader)uploader->Notify();}
int Consent(){auto& s=*state;return s.consent.load(std::memory_order_relaxed);}
bool Enabled(){return Consent()==1;}
bool SetConsent(bool enabled){
    auto& s=*state;
    // Stop sampling and invalidate in-flight replies before waiting for the
    // uploader's brief snapshot lock or touching the preference file.
    if(!enabled){s.consent=0;++s.generation;}
    std::lock_guard lock(s.mutex);
    // Revocation takes effect even if the preference cannot be persisted.
    if(!enabled){s.summaries.Reset();s.sparseFrames.clear();s.sourcePrograms.Reset();s.nextSparse={};}
    std::ofstream file("taa-collection.ini.tmp",std::ios::trunc);file<<(enabled?1:0)<<'\n';file.close();if(!file)return false;
#ifdef _WIN32
    if(!MoveFileExW(L"taa-collection.ini.tmp",L"taa-collection.ini",MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
#else
    if(std::rename("taa-collection.ini.tmp","taa-collection.ini"))return false;
#endif
    s.consent=enabled?1:0;return true;
}
const wchar_t* Label(uint32_t language){static constexpr const wchar_t* text[]={L"TAA shader collection",L"TAA 著色器收集",L"TAA シェーダー収集",L"TAA 셰이더 수집",L"TAA 着色器收集"};return text[std::min(language,4u)];}
const wchar_t* Message(uint32_t language){static constexpr const wchar_t* text[]={
    L"Help improve TAA? Automatically send shader IDs, original VS/PS programs, GPU model/driver, resolution, jitter/camera matrices and sparse depth/camera-motion sequences to lo.dotslash.pro. No personal or device identifiers, serial numbers, paths, saves or color images. Records expire after 30 days without updates. You can disable this in Settings. Enable collection?",
    L"協助改善 TAA？自動向 lo.dotslash.pro 傳送著色器 ID、原始 VS/PS 程式、GPU 型號/驅動、解析度、抖動/相機矩陣和稀疏深度/相機運動序列。不含個人或裝置識別碼、序號、路徑、存檔或彩色畫面。30 天未更新的記錄會刪除，可隨時在設定關閉。啟用收集？",
    L"TAA の改善に協力しますか？シェーダー ID、元の VS/PS プログラム、GPU モデル/ドライバー、解像度、ジッター・カメラ行列、疎な深度・カメラ動きの時系列を lo.dotslash.pro に自動送信します。個人・端末識別子、シリアル番号、パス、セーブ、カラー画像は含みません。30 日間更新のない記録は削除。設定で無効にできます。有効にしますか？",
    L"TAA 개선에 참여하시겠습니까? 셰이더 ID, 원본 VS/PS 프로그램, GPU 모델/드라이버, 해상도, 지터·카메라 행렬 및 희소 깊이·카메라 모션 시퀀스를 lo.dotslash.pro로 자동 전송합니다. 개인·기기 식별자, 일련번호, 경로, 저장 파일, 컬러 이미지는 제외합니다. 30일간 갱신 없는 기록은 삭제하며 설정에서 끌 수 있습니다. 활성화할까요?",
    L"帮助改善 TAA？自动向 lo.dotslash.pro 发送着色器 ID、原始 VS/PS 程序、GPU 型号/驱动、分辨率、抖动/相机矩阵和稀疏深度/相机运动序列。不含个人或设备标识、序列号、路径、存档或彩色画面。30 天未更新的记录会删除，可随时在设置关闭。启用收集？"};return text[std::min(language,4u)];}
void PromptFirstRun(uint32_t language){
#ifdef _WIN32
    if(Consent()<0 && !getenv("LO_BACKGROUND")) {
        const int choice=MessageBoxW(nullptr,Message(language),Label(language),MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
        if(!SetConsent(choice==IDYES))LOG_WARNING("TAA collection: consent preference could not be saved");
    }
#endif
}
void SetDevice(bool vk,const std::string& name,uint64_t version){auto& s=*state;std::lock_guard lock(s.mutex);
    const std::string nextBackend=vk?"vulkan":"d3d12",nextDriver=std::to_string(version);std::string nextGpu;
    for(unsigned char c:name)if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='('||c==')'||c=='_'||c=='.'||c=='+'||c=='-') {if(nextGpu.size()<100)nextGpu+=char(c);}
    if(nextGpu.empty())nextGpu="Unknown";
    if(s.backend!=nextBackend||s.gpu!=nextGpu||s.driver!=nextDriver){
        ++s.generation;s.summaries.Reset();s.sparseFrames.clear();s.sourcePrograms.Reset();s.nextSparse={};
    }
    s.backend=nextBackend;s.gpu=std::move(nextGpu);s.driver=nextDriver;}
void ObserveProgram(bool vertex,uint64_t hash,const uint32_t* words,size_t count){
    auto& s=*state;
    // The preallocated queue never allocates, hashes microcode, logs or does I/O
    // here. A full queue or contended lock simply retries on a later draw.
    s.sourcePrograms.TryObserve(s.mutex,s.consent,vertex,hash,words,count);
}
void Observe(uint64_t vs,uint64_t ps,uint32_t width,uint32_t height,int slot,uint32_t candidates,uint32_t flags,uint32_t rejection,const position_evidence::Summary& position,uint32_t guards){
    auto& s=*state;
    s.summaries.TryObserve(s.mutex,s.consent,Key{vs,ps,width,height,slot,candidates,flags,rejection,position,guards});
}
bool WantSparse(){
    auto& s=*state;
    if(!Enabled())return false;
    std::unique_lock lock(s.mutex,std::try_to_lock);
    return lock&&Enabled()&&s.sparseFrames.size()<32&&std::chrono::steady_clock::now()>=s.nextSparse;
}
uint64_t ConsentEpoch(){auto& s=*state;return s.generation.load();}
void SubmitSparse(SparseFrame frame){
    auto& s=*state;
    if(!Enabled())return;
    std::unique_lock lock(s.mutex,std::try_to_lock);
    if(!lock||!Enabled()||frame.consentEpoch!=s.generation||s.sparseFrames.size()>=32||std::chrono::steady_clock::now()<s.nextSparse)return;
    if(!s.sparseFrames.empty()) {
        const auto& prev=s.sparseFrames.back();
        if(frame.frame==prev.frame)return;
        if(frame.frame!=prev.frame+1||frame.epoch!=prev.epoch||frame.width!=prev.width||frame.height!=prev.height)s.sparseFrames.clear();
    }
    s.sparseFrames.push_back(std::move(frame));
    if(s.sparseFrames.size()==32){s.nextSparse=std::chrono::steady_clock::now()+std::chrono::minutes(5);}
}

}
