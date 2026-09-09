#include "taa_collection.h"
#include "taa_collection_format.h"
#include "temporal_collection.h"
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
std::atomic<int> consent{-1};
std::once_flag initialized;
std::mutex mutex;
std::atomic<uint64_t> generation{0};
std::vector<SparseFrame> sparseFrames;
std::chrono::steady_clock::time_point nextSparse{};
using Key=std::tuple<uint64_t,uint64_t,uint32_t,uint32_t,int,uint32_t,uint32_t,uint32_t,position_evidence::Summary,uint32_t>;
struct Entry {uint32_t count=0, sent=0;uint64_t order=0;};
using Family=std::tuple<uint64_t,uint64_t,int,uint32_t,uint32_t,uint32_t,position_evidence::Summary,uint32_t>;
std::map<Family,unsigned> dimensionVariants;
uint64_t observationOrder=0;
int Priority(const Key& key) {
    const auto& [vs,ps,w,h,slot,candidates,flags,rejection,position,guards]=key;
    if((flags&3)==3 && !(flags&4)) {
        if(slot<0 && position.kind==1 && !position.issues && guards==31)return -1;
        if(slot<0&&candidates)return 0;
        if(slot>=0&&rejection)return 1;
        if(flags&1)return 2;
    }
    return (flags&1)?3:4;
}
std::map<Key,Entry> entries;
std::string backend="d3d12", gpu="Unknown", driver="0";
constexpr size_t capacity=2048;
std::jthread uploader;
#ifdef _WIN32
struct Http {
    HINTERNET value{};
    ~Http(){if(value)WinHttpCloseHandle(value);}
    operator HINTERNET()const{return value;}
};
bool Upload(std::string& body, bool sparse=false, const std::wstring& metadata=L"") {
    Http session{WinHttpOpen(L"LostOdysseyRecomp-TAA/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0)};
    if(!session.value)return false;
    WinHttpSetTimeouts(session,3000,3000,3000,3000);
    Http connection{WinHttpConnect(session,L"lo.dotslash.pro",INTERNET_DEFAULT_HTTPS_PORT,0)};
    if(!connection.value)return false;
    Http request{WinHttpOpenRequest(connection,L"POST",sparse?L"/v1/temporal":L"/v1/taa",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
    if(!request.value)return false;
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
    const std::wstring headers=sparse?L"Content-Type: application/octet-stream\r\n"+metadata:L"Content-Type: application/json\r\n";
    if(!Enabled() || !WinHttpSendRequest(request,headers.c_str(),DWORD(-1),body.data(),DWORD(body.size()),DWORD(body.size()),0) ||
       !WinHttpReceiveResponse(request,nullptr))return false;
    DWORD status=0,size=sizeof(status);
    return WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr) && status==200;
}
#else
bool Upload(std::string&,bool=false,const std::wstring& = L""){return false;}
#endif
void Run(std::stop_token stop) {
    std::mutex waitMutex;std::condition_variable_any wake;std::unique_lock waitLock(waitMutex);
    auto nextNormal=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    for(;;) {
        wake.wait_for(waitLock,stop,std::chrono::seconds(10),[]{return false;});
        if(stop.stop_requested())return;
        if(!Enabled())continue;
        std::vector<std::pair<Key,uint32_t>> batch;std::string body;uint64_t epoch;
        const bool normalDue=std::chrono::steady_clock::now()>=nextNormal;
        {
            std::lock_guard lock(mutex);if(!Enabled())continue;epoch=generation;
            std::vector<std::pair<Key,Entry>> pending;
            for(const auto& item:entries)if(!item.second.sent&&(normalDue||Priority(item.first)<=1))pending.push_back(item);
            std::sort(pending.begin(),pending.end(),[](const auto& a,const auto& b){
                return std::pair(Priority(a.first),a.second.order)<std::pair(Priority(b.first),b.second.order);
            });
            body=RequestStart(backend,gpu,driver);
            for(const auto& [key,entry]:pending) {
                const auto& [vs,ps,w,h,slot,candidates,flags,rejection,position,guards]=key;
                if(!batch.empty())body+=',';
                body+=RecordJson(vs,ps,w,h,slot,candidates,flags,rejection,entry.count,position,guards);
                batch.emplace_back(key,entry.count);if(batch.size()==32)break;
            }
            body+="]}";
        }
        if(!batch.empty()) {
            bool ok=false;try{if(epoch==generation&&Enabled())ok=Upload(body);}catch(...){}
            LOG_INFO("TAA collection: {} summaries {} (anomaly-first)",batch.size(),ok?"accepted":"pending retry");
            if(!ok)continue; // Shader delivery wins over archival MV traffic.
            std::lock_guard lock(mutex);
            if(epoch==generation&&Enabled())for(const auto& [key,count]:batch){auto it=entries.find(key);if(it!=entries.end())it->second.sent=count;}
        }
        if(!normalDue)continue;
        nextNormal=std::chrono::steady_clock::now()+std::chrono::seconds(60);
        std::vector<SparseFrame> sparseBatch;std::wstring meta;
        {
            std::lock_guard lock(mutex);
            const bool urgent=std::any_of(entries.begin(),entries.end(),[](const auto& item){return !item.second.sent&&Priority(item.first)<=1;});
            if(Enabled()&&generation==epoch&&!urgent&&sparseFrames.size()==32) {
                sparseBatch=sparseFrames;
                auto text=fmt::format("X-LO-Build: 0.5.0-temporal-1\r\nX-LO-Backend: {}\r\nX-LO-GPU: {}\r\nX-LO-Driver: {}\r\n",backend,gpu,driver);
                meta.assign(text.begin(),text.end());
            }
        }
        if(!sparseBatch.empty()) {
            auto raw=SparseRaw(sparseBatch);auto packed=CompressSparse(raw);bool accepted=false;
            try{if(generation==epoch&&Enabled())accepted=Upload(packed,true,meta);}catch(...){}
            LOG_INFO("Temporal collection: 32 frames raw={} packed={} {}",raw.size(),packed.size(),accepted?"accepted":"pending retry");
            if(accepted){std::lock_guard lock(mutex);if(generation==epoch)sparseFrames.clear();}
        }
    }
}
}
void Initialize(){std::call_once(initialized,[]{int value=-1;std::ifstream file("taa-collection.ini");file>>value;
    consent=(value==0||value==1)?value:-1;uploader=std::jthread(Run);});}
int Consent(){return consent.load(std::memory_order_relaxed);}
bool Enabled(){return Consent()==1;}
bool SetConsent(bool enabled){
    std::lock_guard lock(mutex);
    // Revocation takes effect even if the preference cannot be persisted.
    if(!enabled){consent=0;++generation;entries.clear();dimensionVariants.clear();sparseFrames.clear();nextSparse={};}
    std::ofstream file("taa-collection.ini.tmp",std::ios::trunc);file<<(enabled?1:0)<<'\n';file.close();if(!file)return false;
#ifdef _WIN32
    if(!MoveFileExW(L"taa-collection.ini.tmp",L"taa-collection.ini",MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
#else
    if(std::rename("taa-collection.ini.tmp","taa-collection.ini"))return false;
#endif
    consent=enabled?1:0;return true;
}
const wchar_t* Label(uint32_t language){static constexpr const wchar_t* text[]={L"TAA shader collection",L"TAA 著色器收集",L"TAA シェーダー収集",L"TAA 셰이더 수집",L"TAA 着色器收集"};return text[std::min(language,4u)];}
const wchar_t* Message(uint32_t language){static constexpr const wchar_t* text[]={
    L"Help improve TAA? Automatically send shader IDs, GPU/driver, resolution, jitter/camera matrices and sparse depth/camera-motion sequences to lo.dotslash.pro. No paths, saves, color images or shader source. Records expire after 30 days without updates. You can disable this in Settings. Enable collection?",
    L"協助改善 TAA？自動向 lo.dotslash.pro 傳送著色器 ID、GPU/驅動、解析度、抖動/相機矩陣和稀疏深度/相機運動序列。不含路徑、存檔、彩色畫面或著色器原始碼。30 天未更新的記錄會刪除，可隨時在設定關閉。啟用收集？",
    L"TAA の改善に協力しますか？シェーダー ID、GPU/ドライバー、解像度、ジッター・カメラ行列、疎な深度・カメラ動きの時系列を lo.dotslash.pro に自動送信します。パス、セーブ、カラー画像、ソースは含みません。30 日間更新のない記録は削除。設定で無効にできます。有効にしますか？",
    L"TAA 개선에 참여하시겠습니까? 셰이더 ID, GPU/드라이버, 해상도, 지터·카메라 행렬 및 희소 깊이·카메라 모션 시퀀스를 lo.dotslash.pro로 자동 전송합니다. 경로, 저장 파일, 컬러 이미지, 소스는 제외합니다. 30일간 갱신 없는 기록은 삭제하며 설정에서 끌 수 있습니다. 활성화할까요?",
    L"帮助改善 TAA？自动向 lo.dotslash.pro 发送着色器 ID、GPU/驱动、分辨率、抖动/相机矩阵和稀疏深度/相机运动序列。不含路径、存档、彩色画面或着色器源码。30 天未更新的记录会删除，可随时在设置关闭。启用收集？"};return text[std::min(language,4u)];}
void PromptFirstRun(uint32_t language){
#ifdef _WIN32
    if(Consent()<0 && !getenv("LO_BACKGROUND")) {
        const int choice=MessageBoxW(nullptr,Message(language),Label(language),MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
        if(!SetConsent(choice==IDYES))LOG_WARNING("TAA collection: consent preference could not be saved");
    }
#endif
}
void SetDevice(bool vk,const std::string& name,uint64_t version){std::lock_guard lock(mutex);backend=vk?"vulkan":"d3d12";gpu.clear();
    for(unsigned char c:name)if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='('||c==')'||c=='_'||c=='.'||c=='+'||c=='-') {if(gpu.size()<100)gpu+=char(c);}
    if(gpu.empty())gpu="Unknown";driver=std::to_string(version);}
void Observe(uint64_t vs,uint64_t ps,uint32_t width,uint32_t height,int slot,uint32_t candidates,uint32_t flags,uint32_t rejection,const position_evidence::Summary& position,uint32_t guards){
    if(!Enabled())return;
    std::unique_lock lock(mutex,std::try_to_lock);if(!lock||!Enabled())return;
    Key key{vs,ps,width,height,slot,candidates,flags,rejection,position,guards};auto it=entries.find(key);
    if(it==entries.end()){
        const int priority=Priority(key);const Family family{vs,ps,slot,candidates,flags,rejection,position,guards};
        if(priority>=2) {
            auto variants=dimensionVariants.find(family);
            if(entries.size()>=capacity/2||(variants!=dimensionVariants.end()&&variants->second>=4))return;
        }
        if(entries.size()>=capacity) {
            auto victim=std::find_if(entries.begin(),entries.end(),[&](const auto& item){return Priority(item.first)>priority;});
            if(victim==entries.end())return;entries.erase(victim);
        }
        if(priority>=2)++dimensionVariants[family];
        it=entries.emplace(key,Entry{0,0,++observationOrder}).first;
    }
    if(it->second.count<1000000000)++it->second.count;
}
bool WantSparse(){
    if(!Enabled())return false;
    std::unique_lock lock(mutex,std::try_to_lock);
    return lock&&Enabled()&&sparseFrames.size()<32&&std::chrono::steady_clock::now()>=nextSparse;
}
uint64_t ConsentEpoch(){return generation.load();}
void SubmitSparse(SparseFrame frame){
    if(!Enabled())return;
    std::unique_lock lock(mutex,std::try_to_lock);
    if(!lock||!Enabled()||frame.consentEpoch!=generation||sparseFrames.size()>=32||std::chrono::steady_clock::now()<nextSparse)return;
    if(!sparseFrames.empty()) {
        const auto& prev=sparseFrames.back();
        if(frame.frame==prev.frame)return;
        if(frame.frame!=prev.frame+1||frame.epoch!=prev.epoch||frame.width!=prev.width||frame.height!=prev.height)sparseFrames.clear();
    }
    sparseFrames.push_back(std::move(frame));
    if(sparseFrames.size()==32){nextSparse=std::chrono::steady_clock::now()+std::chrono::minutes(5);LOG_INFO("Temporal collection: 32-frame sparse sequence queued");}
}

}
