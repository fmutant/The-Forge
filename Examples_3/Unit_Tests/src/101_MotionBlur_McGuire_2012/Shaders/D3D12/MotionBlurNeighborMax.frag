Texture2D<float2> TileMaxTexture : register(t11);

cbuffer cbMotionBlurConsts : register(b3)
{
	float4 mConsts;
	
	float mWidth;
	float mHeight;
	float mKasfloat;
	uint mKasuint;
}

SamplerState nearestSampler : register(s4);
SamplerState bilinearSampler : register(s5);

struct VSOutput {
	float4 position : SV_POSITION;	
	float2 texcoord : TEXCOORD0;
};

float2 vmax(float2 v0, float2 v1)
{
	return lerp(v0, v1, dot(v0, v0) < dot(v1, v1));
}

float2 main(VSOutput input) : SV_TARGET
{
	float2 uv_tile = input.texcoord;
	float2 uv_tile_diff = float2(mKasfloat / mWidth, mKasfloat / mHeight);
	float2 neighbor_max = float2(0.0f, 0.0f);
	for (float i = -1.0f; i <= 1.0f; i += 1.0f)
	{
		for (float j = -1.0f; j <= 1.0f; j += 1.0f)
		{
			float2 uv_sample = uv_tile + float2(i, j) * uv_tile_diff;
			float2 tilemax = TileMaxTexture.Sample(nearestSampler, uv_sample);
			neighbor_max = vmax(tilemax, neighbor_max);
		}
	}
	return neighbor_max;
}
