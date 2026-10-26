#pragma once
// Qualcomm SGSR1, pinned to SnapdragonStudios/snapdragon-gsr
// d926f074bcb9d714e179f1ce0fcb9ee2eeb5074e (see thirdparty/licenses/SnapdragonGSR.txt).
// The algorithm below is unchanged. The adapter crops every gather tap to the
// valid image rather than the allocation; SM 6.0 uses 32-bit arithmetic and
// does not require shaderFloat16 on older/non-Adreno Vulkan devices.
namespace gpu::shader {
inline constexpr const char* kSgsr1Source = R"SGSR(
#define LO_PRESENT_SGSR1
#define SGSR_MOBILE
half4 SGSRRGBH(float2 uv) { return half4(sampleFrame(uv*imageSize),1); }
half4 SGSRH(float2 uv, uint channel) {
    uint w,h; frame.GetDimensions(w,h);
    float2 pixel=uv*imageSize;
    int2 first=int2(floor(pixel-0.5));
    if(all(first>=0) && all(first+1<int2(imageSize)))
        return frame.GatherGreen(linearClamp,pixel/float2(w,h));
    int2 last=int2(imageSize)-1;
    // HLSL gather order: lower left, lower right, upper right, upper left.
    return half4(frame.Load(int3(clamp(first+int2(0,1),0,last),0)).g,
                 frame.Load(int3(clamp(first+int2(1,1),0,last),0)).g,
                 frame.Load(int3(clamp(first+int2(1,0),0,last),0)).g,
                 frame.Load(int3(clamp(first,0,last),0)).g);
}
//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

////////////////////////
// USER CONFIGURATION //
////////////////////////

/*
* Operation modes:
* RGBA -> 1
* RGBY -> 3
* LERP -> 4
*/
#define OperationMode 1

/*
* If set, will use edge direction to improve visual quality
* Expect a minimal cost increase
*/
// #define UseEdgeDirection

#define EdgeThreshold 8.0/255.0

#define EdgeSharpness 2.0

////////////////////////
////////////////////////
////////////////////////

// ///////SGSR_GL_Mobile.frag/////////////////////////////////////////
#if defined(SGSR_MOBILE)
half fastLanczos2(half x)
{
	half wA = x- half(4.0);
	half wB = x*wA-wA;
	wA *= wA;
	return wB*wA;
}

#if defined(UseEdgeDirection)
half2 weightY(half dx, half dy, half c, half3 data)
#else
half2 weightY(half dx, half dy, half c, half data)
#endif
{
#if defined(UseEdgeDirection)
	half std = data.x;
	half2 dir = data.yz;

	half edgeDis = ((dx*dir.y)+(dy*dir.x));
	half x = (((dx*dx)+(dy*dy))+((edgeDis*edgeDis)*((clamp(((c*c)*std),0.0,1.0)*0.7)+-1.0)));
#else
	half std = data;
	half x = ((dx*dx)+(dy* dy))* half(0.5) + clamp(abs(c)*std, 0.0, 1.0);
#endif

	half w = fastLanczos2(x);
	return half2(w, w * c);
}

half2 edgeDirection(half4 left, half4 right)
{
	half2 dir;
	half RxLz = (right.x + (-left.z));
	half RwLy = (right.w + (-left.y));
	half2 delta;
	delta.x = (RxLz + RwLy);
	delta.y = (RxLz + (-RwLy));
	half lengthInv = rsqrt((delta.x * delta.x+ 3.075740e-05) + (delta.y * delta.y));
	dir.x = (delta.x * lengthInv);
	dir.y = (delta.y * lengthInv);
	return dir;
}

void SgsrYuvH(
	out half4 pix,
	float2 uv,
	float4 con1)
{
	int mode = OperationMode;
	half edgeThreshold = EdgeThreshold;
	half edgeSharpness = EdgeSharpness;
	if(mode == 1)
		pix.xyz = SGSRRGBH(uv).xyz;
    else
		pix.xyzw = SGSRRGBH(uv).xyzw;
	float xCenter;
	xCenter = abs(uv.x+-0.5);
	float yCenter;
	yCenter = abs(uv.y+-0.5);

	//todo: config the SR region based on needs
    //if ( mode!=4 && xCenter*xCenter+yCenter*yCenter<=0.4 * 0.4)
	if ( mode!=4)
	{
		float2 imgCoord = ((uv.xy*con1.zw)+ float2(-0.5,0.5));
		float2 imgCoordPixel = floor(imgCoord);
		float2 coord = (imgCoordPixel*con1.xy);
		half2 pl = (imgCoord+(-imgCoordPixel));
		half4  left = SGSRH(coord, mode);

		half edgeVote = abs(left.z - left.y) + abs(pix[mode] - left.y)  + abs(pix[mode] - left.z) ;
		if(edgeVote > edgeThreshold)
		{
			coord.x += con1.x;

			half4 right = SGSRH(coord + float2(con1.x,  0.0), mode);
			half4 upDown;
			upDown.xy = SGSRH(coord + float2(0.0, -con1.y), mode).wz;
			upDown.zw = SGSRH(coord + float2(0.0,  con1.y), mode).yx;

			half mean = (left.y+left.z+right.x+right.w)* half(0.25);
			left = left - half4(mean,mean,mean,mean);
			right = right - half4(mean, mean, mean, mean);
			upDown = upDown - half4(mean, mean, mean, mean);
			pix.w =pix[mode] - mean;

			half sum = (((((abs(left.x)+abs(left.y))+abs(left.z))+abs(left.w))+(((abs(right.x)+abs(right.y))+abs(right.z))+abs(right.w)))+(((abs(upDown.x)+abs(upDown.y))+abs(upDown.z))+abs(upDown.w)));
			half sumMean = 1.014185e+01/sum;
			half std = (sumMean*sumMean);

#if defined(UseEdgeDirection)
			half3 data = half3(std, edgeDirection(left, right));
#else
			half data = std;
#endif

			half2 aWY = weightY(pl.x, pl.y+1.0, upDown.x,data);
			aWY += weightY(pl.x-1.0, pl.y+1.0, upDown.y,data);
			aWY += weightY(pl.x-1.0, pl.y-2.0, upDown.z,data);
			aWY += weightY(pl.x, pl.y-2.0, upDown.w,data);
			aWY += weightY(pl.x+1.0, pl.y-1.0, left.x,data);
			aWY += weightY(pl.x, pl.y-1.0, left.y,data);
			aWY += weightY(pl.x, pl.y, left.z,data);
			aWY += weightY(pl.x+1.0, pl.y, left.w,data);
			aWY += weightY(pl.x-1.0, pl.y-1.0, right.x,data);
			aWY += weightY(pl.x-2.0, pl.y-1.0, right.y,data);
			aWY += weightY(pl.x-2.0, pl.y, right.z,data);
			aWY += weightY(pl.x-1.0, pl.y, right.w,data);

			half finalY = aWY.y/aWY.x;

			half max4 = max(max(left.y,left.z),max(right.x,right.w));
			half min4 = min(min(left.y,left.z),min(right.x,right.w));
			finalY = clamp(edgeSharpness*finalY, min4, max4);

			half deltaY = finalY -pix.w;

			pix.x = saturate((pix.x+deltaY));
			pix.y = saturate((pix.y+deltaY));
			pix.z = saturate((pix.z+deltaY));
		}
	}
	pix.w = 1.0;  //assume alpha channel is not used

}
#endif
////////////////////////////////////////////////////////////////////////
float3 sampleSgsr1(float2 pixel) {
    half4 color;
    SgsrYuvH(color,pixel/imageSize,float4(1.0/imageSize,imageSize));
    return color.rgb;
}
)SGSR";
} // namespace gpu::shader
