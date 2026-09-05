#include "gpu/depth_clear_layout.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
using namespace gpu::renderer;
bool contains(const std::vector<DepthClearRect>& rs,int x,int y) {for(auto r:rs) if(x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom)return true;return false;}
int main(){
 auto right=MapDepthClear(440,2,{240,0,400,216},880,896,0);
 assert(!right.empty());assert(contains(right,480,0));assert(contains(right,799,431));assert(!contains(right,200,100));assert(!contains(right,800,431));assert(!contains(right,480,432));
 // One source tile-row wraps into two destination rows when the pitch halves.
 auto wrap=MapDepthClear(160,0,{80,0,160,16},80,64,0);
 assert(contains(wrap,0,16)&&contains(wrap,79,31));assert(!contains(wrap,0,0));
 assert(MapDepthClear(80,0,{0,64,80,80},80,16,0).empty());
 // Compare coverage to independently enumerated sample addresses for all MSAA pairs.
 for(unsigned s=0;s<3;s++)for(unsigned t=0;t<3;t++) {
  unsigned sx=s==2?2:1,sy=s?2:1,dx=t==2?2:1,dy=t?2:1;
  auto r=MapDepthClear(160,s,{23,7,139,31},240,128,t);
  bool expected[128][240]={};
  for(unsigned y=7*sy;y<31*sy;y++)for(unsigned x=23*sx;x<139*sx;x++){
   unsigned address=((y/16)*((160*sx+79)/80)+x/80)*1280+(y%16)*80+x%80;
   unsigned tile=address/1280,inside=address%1280;
   unsigned tx=((tile%((240*dx+79)/80))*80+inside%80)/dx;
   unsigned ty=((tile/((240*dx+79)/80))*16+inside/80)/dy;
   if(tx<240&&ty<128)expected[ty][tx]=true;
  }
  for(int y=0;y<128;y++)for(int x=0;x<240;x++)assert(contains(r,x,y)==expected[y][x]);
 }
 puts("PASS: atlas isolation, pitch wrap, clipped empty mapping, 9 MSAA coverage pairs");
}
