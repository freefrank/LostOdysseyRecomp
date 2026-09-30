"""Compile the production prebuild/GetShader bodies against explicit CPU fakes.

Tests orchestration, cache and memory behavior without game assets. Translation,
DXC and GPU creation are deliberately fake; this is NOT a real-driver test.
"""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]

def body(source: str, start: str, end: str) -> str:
    if source.count(start) != 1 or source.count(end) != 1:
        raise RuntimeError("Production boundaries changed")
    first, last = source.index(start), source.index(end)
    if last <= first:
        raise RuntimeError("Reversed production boundaries")
    return source[first:last]

PRELUDE = r'''
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include <gpu/shader/startup_cache.h>
#include <gpu/shader/resource_scan.h>
#include <gpu/shader/resource_xex.h>
#include <gpu/shader/resource_variants.h>
#include <gpu/shader/source_store.h>
#include <gpu/shader/preparation_queue.h>
#include <gpu/shader/binary_cache.h>
#include <gpu/shader/dxc_compiler.h>
#include <gpu/shader/portable_shader_contract.h>
#include <gpu/shader/retry_state.h>
namespace fs=std::filesystem;
static void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
#define LOG_INFO(...) ((void)0)
#define LOG_WARNING(...) ((void)0)
#define SHADER_LOG_WARNING(...) ((void)0)
#define SHADER_LOG_ERROR(...) ((void)0)
enum class LogType { Info,Error };
namespace os::shaderlog {
enum class HashNamespace { RendererByteFnv };
template<class... T> void Log(T&&...) {}
}
namespace fmt {
template<class... T> std::string format(std::string_view text,T&&... args) {
    return std::vformat(text,std::make_format_args(args...));
}
}
static uint32_t ByteSwap(uint32_t v) {return __builtin_bswap32(v);}
static std::atomic<size_t> translations{0},compilations{0};
static bool throwAllocation=false, rejectCompilation=false, transientCompilation=false;
namespace xenos {
static std::string compiler="fixture-compiler-A";
const std::string& DxcIdentity() {return compiler;}
const char* GetShaderCommonHlsl() {return "cbuffer XeConstants : register(b0, space0)\n";}
TranslatedShader TranslateShader(const uint32_t*,uint32_t,bool pixel) {
    if(throwAllocation) throw std::bad_alloc();
    ++translations;TranslatedShader info;info.isPixelShader=pixel;info.colorTargetsWritten=pixel?1:0;
    info.hlsl=startup_cache::Prelude(GetShaderCommonHlsl(),pixel)+std::string(32768,' ')+"fixture";return info;
}
CompiledShader CompileHlsl(const std::string&,const char*,const char*,ShaderBinaryFormat,bool) {
    ++compilations;
    if(transientCompilation) return {{},"transient fixture rejection",false,false};
    if(compiler.empty()) return {{},"unavailable fixture compiler",false,false};
    if(rejectCompilation) return {{},"deterministic fixture rejection",false,true};
    CompiledShader r;r.ok=true;r.bytecode.resize(44);std::memcpy(r.bytecode.data(),"DXBC",4);
    auto put=[&](size_t offset,uint32_t v) {std::memcpy(r.bytecode.data()+offset,&v,4);};
    put(24,44);put(28,1);put(32,36);put(36,0x4c495844);put(40,0);return r;
}
}
namespace FileSystem {
static fs::path gameRoot;
const fs::path& GetGameRoot() {return gameRoot;}
}
struct Memory {
    std::vector<uint8_t> image=std::vector<uint8_t>(0x185C60);
    void* Translate(uint32_t address) {Check(address==0x82000000,"unexpected guest memory read");return image.data();}
} g_memory;
namespace settings { struct Config { bool skipShaderPrebuild=false; }; inline Config GetConfig() { return {}; } }
namespace video {
enum class PreparationStage { CachedShaders,CacheValidation,IndexedExtraction,FallbackScan };
enum class PreparationUnit { Shaders,Entries,MiB,Files };
static size_t pumps=0,skipAfter=0;
static bool skipped=false;
void PumpEvents() {++pumps;if(skipAfter && pumps>=skipAfter) skipped=true;}
void ResetShaderPreparationSkip() {skipped=false;}
bool ShaderPreparationSkipped() { return skipped; }
void SetShaderPreparationProgress(uint32_t,uint32_t,PreparationStage=PreparationStage::CachedShaders,
    PreparationUnit=PreparationUnit::Shaders) {}
}
namespace taa_collection { template<class... T> void ObserveProgram(T&&...) {} }
struct Capture {template<class... T> void Observe(T&&...) {}};
struct Device {
    bool fail=false,nullModule=false;size_t calls=0;
    std::unique_ptr<int> createShader(const void* bytes,size_t size,const char*,int) {
        ++calls;if(fail) throw std::runtime_error("injected device failure");
        Check(xenos::cache::CompleteContainer({static_cast<const uint8_t*>(bytes),size}),"invalid device bytecode");
        return nullModule ? nullptr : std::make_unique<int>(1);
    }
};
struct Host {
    struct Shader {std::unique_ptr<int> shader;xenos::TranslatedShader info;bool valid=false;xenos::retry::State retry;};
    std::unordered_map<uint64_t,Shader> shaders[2];
    Device ownedDevice;Device* device=&ownedDevice;
    bool vulkan=false,initializationModuleFailure=false,cpuTimingEnabled=false;
    std::string shaderCacheDir,debugCaptureDir;
    std::shared_ptr<Capture> debugShaderSources;
    uint64_t frame=0,tShader=0,nShader=0;
    xenos::cache::Identity cacheIdentity;
    xenos::ShaderBinaryFormat binaryFormat=xenos::ShaderBinaryFormat::Dxil;
    int renderFormat=0;
    explicit Host(const fs::path& cache):shaderCacheDir(cache.string()),
        cacheIdentity(xenos::cache::MakeIdentity(xenos::cache::Backend::D3D12,xenos::DxcIdentity())) {
        fs::create_directories(cache);
    }
    struct ScopedTimer {ScopedTimer(uint64_t&,bool){}};
    void ResetTimers() {}
    static void CheckPreparationCancel() { video::PumpEvents(); if(video::ShaderPreparationSkipped()) throw xenos::preparation::Cancelled{}; }
    void PreparePositionEvidence(Shader&,const uint32_t*,uint32_t,uint64_t) {}
    #include <gpu/shader/portable_shader_pack_renderer.inl>
'''
TAIL = r'''
};
static void Write(const fs::path& path,std::span<const uint8_t> bytes) {
    std::ofstream out(path,std::ios::binary);xenos::startup_cache::WriteRaw(out,bytes);
}
static std::vector<uint8_t> Resource(bool pixel) {
    std::vector<uint8_t> bytes(120);
    auto put=[&](size_t at,uint32_t value){for(unsigned i=0;i<4;++i)bytes[at+i]=uint8_t(value>>(24-i*8));};
    put(0,pixel?0x102a1100:0x102a1101);put(4,96);put(8,24);put(16,36);put(24,64);
    put(48,pixel?0xffff0300:0xfffe0300);put(68,24);
    for(unsigned i=96;i<120;++i) bytes[i]=uint8_t(i+pixel);return bytes;
}
int main(int argc,char** argv) try {
    Check(argc==2,"need fresh fixture directory");const auto root=fs::absolute(argv[1]);
    Check(!fs::exists(root),"fixture output exists");fs::create_directories(root/"game");
    FileSystem::gameRoot=root/"game";
    const auto vs=Resource(false),ps=Resource(true);
    Write(root/"game/vertex.fpd",vs);Write(root/"game/pixel.fpd",ps);
    const auto cache=root/"cache",bundle=cache/"startup_dxil_v1.bundle";
    Host cold(cache);cold.PrepareKnownShaders();
    Check(compilations==2 && translations==2,"cold compile coverage");
    Check(!cold.initializationModuleFailure && cold.shaders[0].size()==1 && cold.shaders[1].size()==1,"cold module install");
    Check(fs::is_regular_file(bundle) && !fs::exists(cache/"source") && !fs::exists(cache/"resources.manifest"),"cold prebuild wrote intermediate source files");
    for(const auto& stage:cold.shaders)for(const auto& [hash,shader]:stage)
        Check(shader.valid && shader.info.hlsl.empty(),"cold path retained HLSL or invalid modules");
    // Warm startup must not open resource/source files at all.
    fs::rename(root/"game",root/"hidden-game");
    compilations=translations=video::pumps=0;
    Host warm(cache);warm.PrepareKnownShaders();
    Check(compilations==0 && translations==0 && video::pumps>=2,"warm cache reread sources or stopped pumping");
    Check(warm.shaders[0].size()==1 && warm.shaders[1].size()==1,"warm bundle failed without game files");
    const auto pixelBytes=std::span(ps).subspan(96,24);std::array<uint32_t,6> words{};
    std::memcpy(words.data(),pixelBytes.data(),pixelBytes.size());const auto hash=xenos::resources::Hash(pixelBytes);
    warm.debugCaptureDir=(root/"capture").string();
    auto* captured=warm.GetShader(true,words.data(),words.size(),hash);
    Check(captured && !captured->info.hlsl.empty() && translations==1 && compilations==0,"late capture did not reconstruct HLSL without DXC");
    fs::rename(root/"hidden-game",root/"game");
    // A replaced compiler must reject a well-formed old bundle, not trust its header.
    xenos::compiler="fixture-compiler-B";compilations=translations=0;
    Host updated(cache);updated.PrepareKnownShaders();
    Check(compilations==2 && translations==2,"compiler change accepted stale bundle");
    // Losing a final bundle still reuses successful compiler checkpoints.
    fs::remove(bundle);compilations=translations=0;
    Host resumed(cache);resumed.PrepareKnownShaders();
    Check(compilations==0 && translations==2 && fs::exists(bundle),"interrupted prebuild lost checkpoints");
    // Explicit force-scan rebuilds must publish the replacement, not resurrect
    // an old bundle on the next ordinary launch.
    fs::remove(bundle);setenv("LO_SHADER_FULL_SCAN","1",1);
    Host forced(cache);forced.PrepareKnownShaders();unsetenv("LO_SHADER_FULL_SCAN");
    Check(fs::exists(bundle),"forced scan did not replace startup bundle");
    // A successful explicit retry must replace persistent failure records too.
    xenos::compiler="fixture-compiler-retry";rejectCompilation=true;
    const auto retryCache=root/"retry-cache";
    Host rejected(retryCache);rejected.PrepareKnownShaders();rejectCompilation=false;
    Check(rejected.shaders[1].size()==1 && !rejected.shaders[1].begin()->second.valid,"negative compile fixture");
    setenv("LO_SHADER_RETRY_FAILURES","1",1);
    Host retried(retryCache);retried.PrepareKnownShaders();unsetenv("LO_SHADER_RETRY_FAILURES");
    Check(retried.shaders[1].begin()->second.valid,"explicit retry did not compile");
    compilations=translations=0;Host retryWarm(retryCache);retryWarm.PrepareKnownShaders();
    Check(!compilations && !translations && retryWarm.shaders[1].begin()->second.valid,"retry resurrected stale bundle rejection");
    xenos::compiler="fixture-compiler-B";
    // Exceptions from device module creation must fail startup and cancel workers.
    fs::remove(bundle);
    Host broken(cache);broken.ownedDevice.fail=true;broken.PrepareKnownShaders();
    Check(broken.initializationModuleFailure && broken.ownedDevice.calls==1 && !fs::exists(bundle),"device exception was swallowed or repeatedly retried");
    throwAllocation=true;
    Host exhausted(cache);exhausted.PrepareKnownShaders();throwAllocation=false;
    Check(exhausted.initializationModuleFailure && !fs::exists(bundle),"allocation failure did not stop preparation");
    xenos::compiler.clear();
    Host unavailable(cache);unavailable.PrepareKnownShaders();
    Check(!fs::exists(bundle),"uncertified compiler published a bundle");
    xenos::compiler="fixture-compiler-B";
    Host transient(root/"on-demand-transient");transientCompilation=true;
    const auto before=compilations.load();
    Check(!transient.GetShader(true,words.data(),words.size(),hash),"transient compile unexpectedly succeeded");
    transientCompilation=false;
    Check(!transient.GetShader(true,words.data(),words.size(),hash) && compilations==before+1,"retry spun DXC on every draw");
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    Check(transient.GetShader(true,words.data(),words.size(),hash),"transient compiler failure permanently poisoned cache");
    Host moduleRetry(root/"on-demand-module");moduleRetry.ownedDevice.fail=true;
    Check(!moduleRetry.GetShader(true,words.data(),words.size(),hash),"module exception escaped");
    moduleRetry.ownedDevice.fail=false;
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    Check(moduleRetry.GetShader(true,words.data(),words.size(),hash),"module exception permanently poisoned cache");
    Host nullRetry(root/"on-demand-null");nullRetry.ownedDevice.nullModule=true;
    Check(!nullRetry.GetShader(true,words.data(),words.size(),hash),"null module marked valid");
    nullRetry.ownedDevice.nullModule=false;
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    Check(nullRetry.GetShader(true,words.data(),words.size(),hash),"null module permanently poisoned cache");
    Host permanent(root/"on-demand-negative");rejectCompilation=true;
    Check(!permanent.GetShader(true,words.data(),words.size(),hash),"deterministic rejection succeeded");
    rejectCompilation=false;const auto rejectedCount=compilations.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    Check(!permanent.GetShader(true,words.data(),words.size(),hash) && compilations==rejectedCount,"deterministic rejection was recompiled");
    xenos::compiler="fixture-compiler-retry";
    const auto bundleSize=fs::file_size(retryCache/"startup_dxil_v1.bundle");
    video::skipAfter=video::pumps+2;
    Host cancelledBundle(retryCache);cancelledBundle.PrepareKnownShaders();
    Check(video::skipped && cancelledBundle.shaders[0].empty() && cancelledBundle.shaders[1].empty(),"cached startup ignored cancellation/rollback");
    Check(fs::file_size(retryCache/"startup_dxil_v1.bundle")==bundleSize,"cancel overwrote complete bundle");
    video::skipAfter=video::pumps+2;
    Host cancelledScan(root/"cancelled-scan");cancelledScan.PrepareKnownShaders();
    Check(video::skipped && !fs::exists(root/"cancelled-scan/startup_dxil_v1.bundle"),"scan cancellation published bundle");
    video::skipAfter=0;
    std::puts("PASS production GetShader transient/throw/null retry, bounded backoff, permanent negatives, cached/scan cancellation");
    std::puts("PASS production prebuild/GetShader: cold, warm/no game IO, compiler invalidation, checkpoint recovery, forced scan/retry, late capture, fatal module/allocation failures");
    std::puts("Translation, DXC and device calls are explicit fakes; no real GPU/game execution.");
    return 0;
} catch(const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx',default='clang++')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    source=(ROOT/'LostOdysseyRecomp/gpu/renderer.cpp').read_text(encoding='utf-8')
    prepare=body(source,'            void PrepareKnownShaders()','            std::unique_ptr<position_evidence::Collection>')
    get=body(source,'            Shader* GetShader(','            // ---- render targets')
    cpp=out/'prebuild.cpp';cpp.write_text(PRELUDE+prepare+get+TAIL,encoding='utf-8')
    flags=['-std=c++20','-pthread','-I'+str(ROOT/'LostOdysseyRecomp')]
    flags+=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if args.sanitize else ['-O2']
    command=[args.cxx,*flags,str(cpp),str(ROOT/'LostOdysseyRecomp/gpu/shader/portable_shader_pack.cpp'),'-lzstd','-o',str(out/'prebuild')]
    print('BUILD',' '.join(command),flush=True)
    build=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
    (out/'build.log').write_text(build.stdout,encoding='utf-8')
    if build.returncode: print(build.stdout);raise SystemExit(build.returncode)
    env={key:value for key,value in os.environ.items() if not key.startswith('LO_')}
    env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
    run=subprocess.run([str(out/'prebuild'),str(out/'data')],text=True,stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,timeout=60,env=env)
    (out/'run.log').write_text(run.stdout,encoding='utf-8');print(run.stdout,end='',flush=True)
    raise SystemExit(run.returncode)

if __name__=='__main__':main()
