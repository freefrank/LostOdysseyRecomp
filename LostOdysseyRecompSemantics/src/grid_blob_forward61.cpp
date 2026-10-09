#include "lo_semantics/grid_blob_forward61.h"
namespace lo::semantic::gpu::grid_blob_forward61 {
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state,VectorState& vectors){
 if(entry!=0x82b9cbc0u)return false;
 state.r[3]=state.r[4];state.r[4]=state.r[5];state.r[5]=state.r[6];state.r[6]=state.r[7];
 return grid_blob_routes61::Apply(0x82ba60f8u,memory,dependencies,state,vectors);
}
}
