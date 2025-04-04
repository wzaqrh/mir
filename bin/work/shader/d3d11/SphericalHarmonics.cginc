#ifndef SPHERICAL_HARMONICS_H
#define SPHERICAL_HARMONICS_H
#include "Macros.cginc"

inline float3 LinearToGammaSpace (float3 linRGB)
{
    linRGB = max(linRGB, float3(0,0,0));
    // An almost-perfect approximation from http://chilliant.blogspot.com.au/2012/08/srgb-approximations-for-hlsl.html?m=1
    return max(1.055f * pow(linRGB, 0.416666667f) - 0.055f, 0.0f);

    // Exact version, useful for debugging.
    //return half3(LinearToGammaSpaceExact(linRGB.r), LinearToGammaSpaceExact(linRGB.g), LinearToGammaSpaceExact(linRGB.b));
}

//c0c1 = |c0.x c1[-1].x c1[0].x c1[1].x|
//		 |c0.y c1[-1].y c1[0].y c1[1].y|
//		 |c0.z c1[-1].z c1[0].z c1[1].z|
//		 |0	   0	    0		0      |
float3 GetSHDgree01(float4 normal, matrix c0c1) {
    return mul(normal, c0c1).xyz;
}

//c2 = |c2[-2].x c2[-1].x c2[0].x c2[1].x|
//	   |c2[-2].y c2[-1].y c2[0].y c2[1].y|
//	   |c2[-2].z c2[-1].z c2[0].z c2[1].z|
//	   |0	     0	      0		  0      |
//c2_2 = c2[2]
float3 GetSHDgree2(float4 normal, matrix c2, float4 c2_2) {
	float3 color = mul(normal.xyzz * normal.yzzx, c2).xyz;
    color += (normal.x*normal.x - normal.y*normal.y) * c2_2.rgb;
#if COLORSPACE == COLORSPACE_GAMMA
    color = LinearToGammaSpace(color);
#endif
    return color;
}

float3 GetSphericalHarmonics01(float4 normal, matrix c0c1) {
    float3 color = GetSHDgree01(normal, c0c1);
#if COLORSPACE == COLORSPACE_GAMMA
    color = LinearToGammaSpace(color);
#endif
    return color;
}

float3 GetSphericalHarmonics012(float4 normal, matrix c0c1, matrix c2, float4 c2_2) {
    float3 color = GetSHDgree01(normal, c0c1);
    color += GetSHDgree2(normal, c2, c2_2);
#if COLORSPACE == COLORSPACE_GAMMA
    color = LinearToGammaSpace(color);
#endif
    return color;
}

#endif