// Synthetic miss-path microbenchmark, not game FPS or Android performance.
// g++ -std=c++20 -O3 -DNDEBUG -I LostOdysseyRecomp tools/tests/texture_descriptor_rewrite_bench.cpp -o /tmp/descriptor-bench
#include <gpu/texture_descriptor_cache.h>
#include <chrono>
#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>
using Key=std::array<int*,32>;
struct Input { Key key; uint32_t active; };
std::array<int,2048> textures;
volatile uint64_t checksum;
template<bool Sparse> __attribute__((noinline)) uint32_t Rewrite(uint32_t previous, const Input& input, Key& bound) {
 auto write=[&](uint32_t slot,int* texture){bound[slot]=texture;};
 if constexpr(Sparse) return gpu::texture_descriptors::RewriteBindings(previous,input.key,&textures[0],write,input.active);
 else return gpu::texture_descriptors::RewriteBindings(previous,input.key,&textures[0],write);
}
template<bool Sparse> double Run(const std::vector<Input>& inputs) {
 Key bound;bound.fill(&textures[0]);uint32_t previous=~0u;uint64_t sum=0;
 auto start=std::chrono::steady_clock::now();
 for(int repeat=0;repeat<1000;++repeat)for(const auto& input:inputs){previous=Rewrite<Sparse>(previous,input,bound);sum+=previous+uintptr_t(bound[13]);}
 checksum=sum;
 return std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count()/(inputs.size()*1000);
}
int main(){
 for(unsigned count: {1u,4u,8u,16u,32u}){
  std::vector<Input> inputs(2300);
  for(unsigned i=0;i<inputs.size();++i){auto& in=inputs[i];in.key.fill(&textures[0]);in.active=0;
   for(unsigned j=0;j<count;++j){unsigned slot=(i*13+j*7)%32;in.active|=1u<<slot;in.key[slot]=&textures[1+(i*31+j)%2047];}}
  std::array<double,5> full{},sparse{};
  for(unsigned round=0;round<5;++round){if(round%2){sparse[round]=Run<true>(inputs);full[round]=Run<false>(inputs);}else{full[round]=Run<false>(inputs);sparse[round]=Run<true>(inputs);}}
  std::sort(full.begin(),full.end());std::sort(sparse.begin(),sparse.end());
  printf("active=%u full_ns=%.2f sparse_ns=%.2f delta_percent=%.1f\n",count,full[2],sparse[2],100*(sparse[2]/full[2]-1));
 }
}
