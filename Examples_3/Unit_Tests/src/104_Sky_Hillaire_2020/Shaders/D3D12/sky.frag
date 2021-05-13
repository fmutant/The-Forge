#include "atmosphereStructs.hlsl"

Texture2D<min16float4> TransmittanceLUT : register(t0);
SamplerState samplerLinearClamp : register(s0);

cbuffer cbCameraExtended : register(b1, UPDATE_FREQ_PER_FRAME)
{
	float4x4 mViewMat;
	float4x4 mProjMat;
	float4x4 mViewProjMat;
	float4x4 mInvViewProjMat;

	float4 mCameraWorldPos;
	float4 mViewPortSize;

	float4x4 mProjectViewInv;
	float4x4 mPrevProjectViewInv;
}

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
};

float raySphereIntersectNearest(float3 r0, float3 rd, float3 s0, float sR)
{
	float a = dot(rd, rd);
	float ainv = 1.0f / a;
	float3 s0_r0 = r0 - s0;
	float b = 2.0f * dot(rd, s0_r0);
	float c = dot(s0_r0, s0_r0) - (sR * sR);
	float delta = b * b - 4.0 * a * c;
	if (delta < 0.0f || a == 0.0f)
	{
		return -1.0f;
	}
	float dsqrt = sqrt(delta);
	float sol0 = 0.5f * (-b - dsqrt) * ainv;
	float sol1 = 0.5f * (-b + dsqrt) * ainv;
	if (sol0 < 0.0f && sol1 < 0.0f)
	{
		return -1.0f;
	}
	if (sol0 < 0.0f)
	{
		return max(0.0f, sol1);
	}
	else if (sol1 < 0.0f)
	{
		return max(0.0f, sol0);
	}
	return max(0.0f, min(sol0, sol1));
}

float3 GetSunLuminance(float3 WorldPos, float3 WorldDir, float PlanetRadius)
{
	float LdotW = dot(WorldDir, normalize(mSunDir)); //normalize of c++ part is not precise enough
	if (cos(0.5f * 0.505f * 3.14159f / 180.0f) < LdotW)
	{
		float t = raySphereIntersectNearest(WorldPos, WorldDir, float3(0.0f, 0.0f, 0.0f), PlanetRadius);
		if (t < 0.0f) // no intersection
		{
			const float3 SunLuminance = 65000.0f; // arbitrary. But fine, not use when comparing the models
			return SunLuminance;
		}
	}
	return float3(0.0f, 0.0f, 0.0f);
}

min16float4 main(VSOutput input) : SV_TARGET
{
	float2 pixPos = input.position.xy;
	float3 ClipSpace = float3((pixPos * mViewPortSize.zw) * float2(2.0f, -2.0f) - float2(1.0f, -1.0f), 1.0f);
	float4 HPos = mul(mProjectViewInv, float4(ClipSpace, 1.0));

	float3 camera = mCameraWorldPos.xyz;
	float3 WorldDir = normalize(HPos.xyz / HPos.w - camera);
	float3 WorldPos = camera + float3(0.0f, mRadiusBottom, 0.0f);
	
	float viewHeight = length(WorldPos);
	float3 L = GetSunLuminance(WorldPos, WorldDir, mRadiusBottom);
	return min16float4(L, dot(TransmittanceLUT.Sample(samplerLinearClamp, input.texcoord), float4(1.0f, 1.0f, 1.0f, 1.0f)));
}