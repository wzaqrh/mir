#ifndef BRDF_COMMON_FUNCTION_H
#define BRDF_COMMON_FUNCTION_H
#include "CommonFunction.cginc"

inline float3 MakeDummyColor(float3 toEye)
{
	float3 fcolor = float3(1, 0, 1);
    fcolor.r = fcolor.b = float(max(2.0 * sin(0.1 * (toEye.x + toEye.y) * 1024), 0.0) + 0.3); 
	return fcolor;
}
inline float3 DisneyDiffuse(float nv, float nl, float lh, float perceptualRoughness, float3 baseColor)
{
    float fd90 = 0.5 + 2 * lh * lh * perceptualRoughness;
    float lightScatter = (1 + (fd90 - 1) * Pow5(1 - nl));
    float viewScatter  = (1 + (fd90 - 1) * Pow5(1 - nv));
    return baseColor * C_INV_PI * lightScatter * viewScatter;
}
inline float3 LambertDiffuse(float3 baseColor)
{
    return baseColor * C_INV_PI;
}

inline float GGXTRDistribution(float nh, float roughness) 
{
	float a2 = roughness * roughness;
    float denom = nh * nh * (a2 - 1) + 1.0f;
    return a2 * C_INV_PI / (denom * denom + MIR_EPS);
}
// Estevez and Kulla http://www.aconty.com/pdf/s2017_pbs_imageworks_sheen.pdf
inline float CharlieDistribution(float sheenRoughness, float nh)
{
    //sheenPerceptualRoughness = max(sheenPerceptualRoughness, 0.000001); //clamp (0,1]
    //float sheenRoughness = sheenPerceptualRoughness * sheenPerceptualRoughness;
    float invRoughness = 1.0 / sheenRoughness;
    float cos2h = nh * nh;
    float sin2h = 1.0 - cos2h;
    return (2.0 + invRoughness) * pow(sin2h, invRoughness * 0.5) / (2.0 * C_PI);
}

inline float SmithJointGGXVisibility(float nl, float nv, float roughness)
{
    float lambdaV = nl * (nv * (1 - roughness) + roughness);
    float lambdaL = nv * (nl * (1 - roughness) + roughness);
    return 0.5 / (lambdaV + lambdaL + 1e-5);
}
inline float SmithJointGGXFilamentVisibility(float nl, float nv, float roughness)
{
    float a2 = roughness * roughness;
    float lambdaV = nl * sqrt((1 - a2) * nv * nv + a2);
    float lambdaL = nv * sqrt((1 - a2) * nl * nl + a2);
	if (lambdaV + lambdaL <= 0.0) return 0.0;
    else return 0.5 / (lambdaV + lambdaL);
}

inline float LambdaSheenNumericHelper(float x, float roughness)
{
    float oneMinusRoughnessSq = (1.0 - roughness) * (1.0 - roughness);
    float a = lerp(21.5473, 25.3245, oneMinusRoughnessSq);
    float b = lerp(3.82987, 3.32435, oneMinusRoughnessSq);
    float c = lerp(0.19823, 0.16801, oneMinusRoughnessSq);
    float d = lerp(-1.97760, -1.27393, oneMinusRoughnessSq);
    float e = lerp(-4.32054, -4.85967, oneMinusRoughnessSq);
    return a / (1.0 + b * pow(x, c)) + d * x + e;
}
inline float LambdaSheen(float cosTheta, float roughness)
{
    if (abs(cosTheta) < 0.5) {
        return exp(LambdaSheenNumericHelper(cosTheta, roughness));
    }
    else {
        return exp(2.0 * LambdaSheenNumericHelper(0.5, roughness) - LambdaSheenNumericHelper(1.0 - cosTheta, roughness));
    }
}
inline float SheenVisibility(float nl, float nv, float sheenRoughness)
{
    //sheenPerceptualRoughness = max(sheenPerceptualRoughness, 0.000001); //clamp (0,1]
    //float sheenRoughness = sheenPerceptualRoughness * sheenPerceptualRoughness;
    float d = 1.0 + LambdaSheen(nv, sheenRoughness) + LambdaSheen(nl, sheenRoughness);
    return saturate(1.0 / (d * 4.0 * nv * nl));
}

//����lerp(f0, f90, (1-vh)^5)
inline float3 SchlickFresnel(float3 F0, float3 F90, float vh)
{
    return F0 + (F90 - F0) * Pow5(1 - vh);
}

#endif