#ifndef COMMON_FUNCTION_H
#define COMMON_FUNCTION_H
#include "HLSLSupport.cginc"
#include "Macros.cginc"
#include "CommonConstants.cginc"

inline float3 SafeNormalize(float3 inVec)
{
	float dp3 = max(0.001f, dot(inVec, inVec));
	return inVec * rsqrt(dp3);
}
inline float Pow5(float x)
{
	return x * x * x * x * x;
}

inline float2 GetUV(float2 uv, float4 uvTransform) 
{
	uv = uvTransform.xy + uv * uvTransform.zw;
#if FLIP_UV0_Y
	uv.y = 1.0 - uv.y;
#endif
	return uv;
}

struct DpDuv 
{
	float3 dpx;
    float3 dpy;
    float2 dtx;
    float2 dty;
	float det;
};
inline DpDuv GetDpDuv(float3 worldPos, float2 uv) 
{
	DpDuv dd;
    dd.dpx = ddx(worldPos);
    dd.dpy = ddy(worldPos);
    dd.dtx = ddx(uv);
    dd.dty = ddy(uv);
	dd.det = dd.dtx.x * dd.dty.y - dd.dty.x * dd.dtx.y;
	return dd;
}

#if ENABLE_CORRCET_TANGENT_BASIS
inline void CorrectBitangent(inout float3 B, DpDuv dd) {
	float3 bitangent = (- dd.dty.x * dd.dpx + dd.dtx.x * dd.dpy) / dd.det;
	if (dot(bitangent, B) < 0) 
		B = -B;
}
#define CORRECT_BITANGENT(B, DD) CorrectBitangent(B, DD)
#else
#define CORRECT_BITANGENT(B, DD)
#endif

inline float3x3 GetTangentToWorldTBN(float3 T, float3 N, DpDuv dd)
{
	float3 B = cross(N, T);
	CORRECT_BITANGENT(B, dd);
	
    return float3x3(T, B, N);
}

/*
dPos = k·dTex
=>
|dp0|   |du0 dv0|  |T|
|dp1| = |du1 dv1|·|B|·k
=>
|T|       | dv1 -dv1|  |dpx|
|B| = k'·|-du0  du0|·|dpy|
	    ---------------
      du0·dv1 - du1·dv0
		
|dpx|   |dtx.x dtx.y|  |T|
|dpy| = |dty.x dty.y|·|B|
=>
|T|   | dty.y -dtx.y|  |dpx|
|B| = |-dty.x  dtx.x|·|dpy|
	  ---------------
 dtx.x·dty.y - dty.x·dtx.y
*/
inline float3x3 GetTangentToWorldTBN(float3 N, DpDuv dd)
{
    float3 T = (dd.dty.y * dd.dpx - dd.dtx.y * dd.dpy) / dd.det;
    T = normalize(T - dot(T,N) * N);
    
	float3 B = normalize(cross(N, T));
	CORRECT_BITANGENT(B, dd);
	
    return float3x3(T/*row0*/, B, N);
}

inline float3x3 GetTangentToWorldTBN(DpDuv dd)
{
	float3 N = normalize(cross(dd.dpy, dd.dpx));
    return GetTangentToWorldTBN(N, dd);
}

float3 UnpackScaleNormalRGorAG(float4 packednormal, float bumpScale)
{
#if defined(NO_DXT5nm)
    half3 normal = packednormal.xyz * 2 - 1;
    normal.xy *= bumpScale;
    return normal;
#else
    // This do the trick
	packednormal.x *= packednormal.w;

	float3 normal;
	normal.xy = (packednormal.xy * 2 - 1);
    normal.xy *= bumpScale;
	normal.z = sqrt(1.0 - saturate(dot(normal.xy, normal.xy)));
	return normal;
#endif
}

float2 DepthGradient(float2 uv, float z)
{
	float2 dz_duv = 0;

	float3 duvdist_dx = ddx(float3(uv, z));
	float3 duvdist_dy = ddy(float3(uv, z));

	dz_duv.x = duvdist_dy.y * duvdist_dx.z;
	dz_duv.x -= duvdist_dx.y * duvdist_dy.z;
    
	dz_duv.y = duvdist_dx.x * duvdist_dy.z;
	dz_duv.y -= duvdist_dy.x * duvdist_dx.z;

	float det = (duvdist_dx.x * duvdist_dy.y) - (duvdist_dx.y * duvdist_dy.x);
	dz_duv /= det;

	return dz_duv;
}

float BiasedZ(float z0, float2 dz_duv, float2 offset)
{
	return z0 + dot(dz_duv, offset);
}

float length2(float3 v)
{
	return dot(v, v);
}
//返回1.0/v.length
float invlength(float2 v)
{
	return rsqrt(dot(v, v));
}

//pl-p与p-pr最小微分
float3 min_diff(float3 P, float3 Pr, float3 Pl)
{
	float3 V1 = Pr - P;
	float3 V2 = P - Pl;
	return (length2(V1) < length2(V2)) ? V1 : V2;
}
//m = [cosθ, -sinθ]
//    [sinθ,  cosθ]
float2 rotate_direction(float2 Dir, float2 CosSin)
{
	return float2(Dir.x * CosSin.x - Dir.y * CosSin.y,
                  Dir.x * CosSin.y + Dir.y * CosSin.x);
}
//投影到<dPdu,dPdv,dPdw>构成的坐标系
//m = [dPdu.x dPdu.y dPdu.z]
//    [dPdv.x dPdv.y dPdv.z]
//    [dPdw.x dPdw.y dPdw.z]
float3 tangent_vector(float2 deltaUV, float3 dPdu, float3 dPdv)
{
	return deltaUV.x * dPdu + deltaUV.y * dPdv;
}
//T投影到xy平面夹角θ的正弦值
float tangent(float3 T)
{
	return -T.z * invlength(T.xy);
}
float tangent(float3 P, float3 S)
{
	return (P.z - S.z) / length(S.xy - P.xy);
}

float tan_to_sin(float x)
{
	return x * rsqrt(x * x + 1.0f);
}

inline float falloff(float r, float attenuation)
{
	return 1.0f - attenuation * r * r;
}

//uv范围[0,1] #使uv就近对齐整数像素
inline float2 snap_uv_offset(float2 uv, float4 resolution)
{
	return round(uv * resolution.xy) * resolution.zw;
}
//uv范围[0,1] #使uv就近对齐整数+0.5像素
inline float2 snap_uv_coord(float2 uv, float4 resolution)
{
	return uv - (frac(uv * resolution.xy) - 0.5f) * resolution.zw;
}

float3 uv_to_eye(float2 uv, float eye_z, float2 invFocalLen)
{
	uv = (uv * float2(2.0, -2.0) - float2(1.0, -1.0));
	return float3(uv * invFocalLen * eye_z, eye_z);
}

inline float LinearEyeDepth(float d, float2 depthParam)
{
#if 0
#define FARZ 1000.0
#define NEARZ 0.3
	return FARZ * NEARZ / (FARZ - d * (FARZ - NEARZ));
#else
	return 1.0 / (depthParam.x + depthParam.y * d);
#endif
}
#define ZClipToZEye LinearEyeDepth

inline float fetch_eye_z(float2 uv, float2 depthParam, MIR_ARGS_TEX2D(tDepth))
{
	float d = MIR_SAMPLE_TEX2D(tDepth, uv).r;
	return LinearEyeDepth(d, depthParam);
}
//获取uv对应的, '相机空间'上的, 离相机最近的表面位置
inline float3 fetch_eye_pos(float2 uv, float4 depthParam, float4 focalLen, MIR_ARGS_TEX2D(tDepth))
{
	float d = MIR_SAMPLE_TEX2D_LEVEL(tDepth, uv, 0).r;
	return uv_to_eye(uv, LinearEyeDepth(d, depthParam.xy), focalLen.zw);
}
//获取fetch_eye_pos, 并确保其与'法线'同向
inline float3 tangent_eye_pos(float2 uv, float4 tangentPlane, float4 depthParam, float4 focalLen, MIR_ARGS_TEX2D(tDepth))
{
    //view vector going through the surface point at uv
	float3 V = fetch_eye_pos(uv, depthParam, focalLen, MIR_PASS_TEX2D(tDepth));
	//intersect with tangent plane except for silhouette edges
	float NdotV = dot(tangentPlane.xyz, V);
	if (NdotV < 0.0)
		V *= (tangentPlane.w / NdotV);
	return V;
}

inline float3 DecodeLightmapRGBM (float4 data, float4 decodeInstructions)
{
    // If Linear mode is not supported we can skip exponent part
#if COLORSPACE == COLORSPACE_GAMMA
    #if FORCE_LINEAR_READ_FOR_RGBM
        return (decodeInstructions.x * data.a) * sqrt(data.rgb);
    #else
        return (decodeInstructions.x * data.a) * data.rgb;
    #endif
#else
    return (decodeInstructions.x * pow(data.a, decodeInstructions.y)) * data.rgb;
#endif
}

#endif