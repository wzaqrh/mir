#ifndef SHADOW_H
#define SHADOW_H
#include "Macros.cginc"
#include "Standard.cginc"
#include "PercentageCloserSoftShadow.cginc"
#include "VarianceShadow.cginc"

float CalcShadowFactor(float3 lightNdc, float3 lightViewPos)
{
#if DEBUG_SHADOW_MAP
	return lightNdc.y;
#endif

	lightNdc.xy = lightNdc.xy * float2(0.5, -0.5) + 0.5;
#if SHADOW_MODE == SHADOW_RAW
	return MIR_SAMPLE_SHADOW(_ShadowMap, lightNdc).r;
#elif SHADOW_MODE == SHADOW_PCF_FAST
	float3 dz_duv = float3(DepthGradient(lightNdc.xy, lightNdc.z), 0.0);
	return FastPCFShadow(lightNdc, dz_duv, ShadowMapSize.zw, MIR_PASS_SHADOWMAP(_ShadowMap));
#elif SHADOW_MODE == SHADOW_PCF
	PCFShadowInput pcfIn;
	pcfIn.lightRadiusUVNearFar = LightRadiusUVNearFar;
	pcfIn.lightDepthParam = LightDepthParam;
	float2 dz_duv = DepthGradient(lightNdc.xy, lightNdc.z);
	//float2 dz_duv = float2(0.0, 0.0);
	return PCFShadow(lightNdc.xy, lightNdc.z, dz_duv, lightViewPos.z, pcfIn, MIR_PASS_SHADOWMAP(_ShadowMap), MIR_PASS_TEX2D(_ShadowMapTex));
#elif SHADOW_MODE == SHADOW_PCSS
	PCFShadowInput pcfIn;
	pcfIn.lightRadiusUVNearFar = LightRadiusUVNearFar;
	pcfIn.lightDepthParam = LightDepthParam;
	float2 dz_duv = DepthGradient(lightNdc.xy, lightNdc.z);
	//float2 dz_duv = float2(0.0, 0.0);
	return PCSSShadow(lightNdc.xy, lightNdc.z, dz_duv, lightViewPos.z, pcfIn, MIR_PASS_SHADOWMAP(_ShadowMap), MIR_PASS_TEX2D(_ShadowMapTex));
#elif SHADOW_MODE == SHADOW_VSM
	return VSMShadow(lightNdc.xy, length(lightViewPos), MIR_PASS_TEX2D(_ShadowMapTex));
#else
	return 1.0;
#endif
}
#endif