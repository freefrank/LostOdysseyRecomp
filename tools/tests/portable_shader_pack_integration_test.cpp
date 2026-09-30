// Compiles the exact production renderer .inl against explicit fake GPU/DXC
// services. This tests the new integration methods, not the whole renderer.
#include "gpu/shader/portable_shader_contract.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace fs=std::filesystem;
namespace pp=xenos::portable_pack;
namespace xenos {
std::string producer="local-compiler-A";
const std::string& DxcIdentity() {return producer;}
const char* GetShaderCommonHlsl(){return "test-only prelude";}

}
#define LOG_INFO(...) do {} while(false)
#define LOG_WARNING(...) do {} while(false)
struct Module {};
struct Device {
    int calls=0; bool fail=false;
    std::unique_ptr<Module> createShader(const uint8_t*,size_t,const char*,int) {
        ++calls;if(fail)throw std::runtime_error("injected driver failure");return std::make_unique<Module>();
    }
};
struct Shader {xenos::TranslatedShader info;std::unique_ptr<Module> shader;bool valid=false;};
struct RendererFixture {
    bool vulkan=true;int renderFormat=1;
    xenos::cache::Identity cacheIdentity=xenos::cache::MakeIdentity(xenos::cache::Backend::Vulkan,"local");
    Device driver;Device* device=&driver;
    std::array<std::unordered_map<uint64_t,Shader>,2> shaders;
    #include "gpu/shader/portable_shader_pack_renderer.inl"
};
int checks=0;
void Check(bool x,const char* m){++checks;if(!x)throw std::runtime_error(m);}
void Env(const char* name,const std::string& value) {
#ifdef _WIN32
    _putenv_s(name,value.c_str());
#else
    if(value.empty())unsetenv(name);else setenv(name,value.c_str(),1);
#endif
}
std::vector<uint8_t> Bytes(bool pixel) {
    std::vector<uint32_t> w{0x07230203,0x00010500,0,16,0,(3u<<16)|14,0,1,
        (5u<<16)|15,pixel?4u:0u,1,0x6e69616d,0};
    std::vector<uint8_t> b;for(auto v:w)for(unsigned i=0;i<4;++i)b.push_back(uint8_t(v>>(8*i)));return b;
}
int main() try {
    const char* controls[]={"LO_SHADER_EXPORT_PACK","LO_NO_PORTABLE_SHADER_PACK","LO_SHADER_FULL_SCAN",
        "LO_SHADER_HLSL_DIR","LO_SHADER_RETRY_FAILURES","LO_SHADER_PACK_PATH"};
    // Save/restore the invoking developer's settings.
    struct Restore {std::vector<std::pair<std::string,std::string>> old;~Restore(){for(auto& [k,v]:old)Env(k.c_str(),v);}} restore;
    for(auto name:controls){auto p=std::getenv(name);restore.old.emplace_back(name,p?p:"");Env(name,"");}
    auto root=fs::temp_directory_path()/("lo-pack-integration-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    struct Cleanup{fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}}cleanup{root};
    auto pack=root/"portable.lospv";std::vector<uint8_t>xex{1,2,3};RendererFixture reference;
    auto contract=reference.PortableShaderContract(xex);xenos::TranslatedShader info;info.hlsl="source";auto binary=Bytes(false);
    {pp::Writer w(pack,contract,"foreign-Windows-DXC");w.Add(42,info,binary);w.Finish();}
    Env("LO_SHADER_PACK_PATH",pack.string());
    RendererFixture runtime;runtime.cacheIdentity.compiler="Linux-compiler-different";xenos::producer.clear();
    Check(runtime.TryOpenPortableShaderPack(xex),"portable open incorrectly depends on local compiler");
    Check(runtime.driver.calls==0 && runtime.shaders[0].empty(),"portable open eagerly creates shaders");
    Check(runtime.TryLoadPortableShader(false,42) && runtime.driver.calls==1,"portable hit module");
    Check(runtime.shaders[0].at(42).info.hlsl.empty(),"retained HLSL");
    Check(runtime.TryLoadPortableShader(false,42) && runtime.driver.calls==1,"duplicate module creation");
    Check(!runtime.TryLoadPortableShader(false,99) && !runtime.shaders[0].contains(99),"miss poisoned retry");
    Check(!runtime.TryLoadPortableShader(true,42),"stage cross-hit");
    RendererFixture failing;Check(failing.TryOpenPortableShaderPack(xex),"setup failure reader");failing.driver.fail=true;
    Check(!failing.TryLoadPortableShader(false,42) && !failing.shaders[0].contains(42),"device failure poisoned cache");
    Check(!failing.portableShaderPack,"failed pack not disabled");
    RendererFixture mismatch;mismatch.cacheIdentity.translatorVersion++;
    Check(!mismatch.TryOpenPortableShaderPack(xex),"mismatched translator accepted");
    RendererFixture dx;dx.vulkan=false;Check(!dx.TryOpenPortableShaderPack(xex),"D3D loaded Vulkan pack");
    for(auto name:{"LO_NO_PORTABLE_SHADER_PACK","LO_SHADER_FULL_SCAN","LO_SHADER_HLSL_DIR","LO_SHADER_RETRY_FAILURES","LO_SHADER_EXPORT_PACK"}){
        Env(name,"1");RendererFixture explicitControl;Check(!explicitControl.TryOpenPortableShaderPack(xex),"diagnostic/export bypass not honored");Env(name,"");
    }
    auto exported=root/"export.lospv";Env("LO_SHADER_EXPORT_PACK",exported.string());
    RendererFixture noCompiler;noCompiler.BeginPortableShaderExport(xex);
    Check(!noCompiler.portableShaderExport,"uncertified export accepted");xenos::producer="verified-compiler";
    RendererFixture exporter;exporter.BeginPortableShaderExport(xex);
    Check(bool(exporter.portableShaderExport),"export did not start");
    exporter.ExportPortableShader(42,info,binary,"",false);
    exporter.ExportPortableShader(77,info,{},"deterministic rejection",true);
    exporter.FinishPortableShaderExport();Check(fs::exists(exported),"successful export not published");
    pp::Reader exportedReader(exported,contract);
    Check(exportedReader.Info().failuresOmitted==1 && !exportedReader.Get(false,77),"negative record shipped");
    Check(exportedReader.Get(false,42)->binary==binary,"export changed binary");
    auto before=fs::file_size(exported);
    RendererFixture transient;transient.BeginPortableShaderExport(xex);transient.ExportPortableShader(42,info,binary,"",false);
    transient.ExportPortableShader(78,info,{},"out of memory",false);transient.FinishPortableShaderExport();
    Check(!transient.portableShaderExport && fs::file_size(exported)==before,"transient failure published partial pack");
    pp::Reader unchanged(exported,contract);Check(unchanged.Get(false,42)->binary==binary,"previous export not retained");
    // Diagnostic capture recompiles HLSL in the existing GetShader hit path;
    // this fixture intentionally does not pretend to execute that path.
    std::cout<<"PASS "<<checks<<" production-inl checks with fake GPU/DXC services\n";return 0;
} catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
