#define UNORM 1

Texture2D<float2> VelocityTexture : register(t0);

cbuffer cbMotionBlurConsts : register(b3)
{
	float4 mConsts;
}

SamplerState nearestSamplerBorderZero : register(s6);

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
};

float2 vmax(float2 s, float2 v)
{
#if UNORM
	s = s * 2.0f - 1.0f;
#endif
	return dot(s, s) < dot(v, v) ? v : s;
}

float2 main(VSOutput input) : SV_TARGET
{
	float2 uv_tile = input.texcoord;
	float2 tile_max = float2(0.0f, 0.0f);
	for (float i = 0.0f; i < mConsts.x; i += 1.0f)
	{
		float2 uv_sample = uv_tile;
		uv_sample.x += i * mConsts.z;
		float2 velocity = VelocityTexture.Sample(nearestSamplerBorderZero, uv_sample).xy;

		tile_max = vmax(velocity, tile_max);
	}
#if UNORM
	tile_max = tile_max * 0.5f + 0.5f;
#endif
	return tile_max;
}
