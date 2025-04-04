#include "Macros.cginc"
#include "Standard.cginc"
#include "ToneMapping.cginc"

MIR_DECLARE_TEXCUBE(_MainTex, 0);

struct VertexInput
{
    float3 Pos : POSITION;
};

struct PixelInput
{
    float4 Pos : SV_POSITION;
	float3 Tex : TEXCOORD0;
};

#if DEPRECATE_SKYBOX
PixelInput VS(VertexInput input)
{
	PixelInput output;
	output.Pos = float4(input.Pos, 1.0);
	
	matrix WVPInv = mul(ViewInv, ProjectionInv);
	output.Tex = normalize(mul(WVPInv, output.Pos));
    return output;
}
#else
PixelInput VS(VertexInput input)
{
	PixelInput output;
#if !RIGHT_HANDNESS_RESOURCE
	output.Tex = input.Pos;
#else
    output.Tex = float3(input.Pos.x, input.Pos.y, -input.Pos.z);
#endif
	output.Pos = mul(View, float4(input.Pos, 0.0));//没有平移
#if REVERSE_Z
	output.Pos.xyw = mul(Projection, output.Pos).xyw;
	output.Pos.z = 0.0;//z永远为0
#else
	output.Pos = mul(Projection, output.Pos).xyww;//z永远为1
#endif
	return output;
}
#endif

float4 PS(PixelInput input) : SV_Target
{	
	float4 finalColor;
	finalColor.rgb = MIR_SAMPLE_TEXCUBE(_MainTex, input.Tex).rgb;
	finalColor.a = 1.0;
	
#if TONEMAP_MODE
	finalColor.rgb = toneMap(finalColor.rgb, CameraPositionExposure.w);
#endif
	return finalColor;
}