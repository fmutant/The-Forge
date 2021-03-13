Texture2D<float2> TileMaxTexture : register(t0);

cbuffer cbMotionBlurConsts : register(b0)
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
	float2 texcoord_tile = input.texcoord;
	float2 texcoord_neighbor = texcoord_tile;
	uint k = floor(mConsts.x);
	float2 neighbor_max = float2(0.0f, 0.0f);
	for (int i = -1; i <= 1; ++i)
	{
		for (int j = -1; j <= 1; ++j)
		{
			neighbor_max = vmax(TileMaxTexture.Sample(nearestSampler, texcoord_tile + float2(i, j)), neighbor_max);
		}
	}
	return neighbor_max;
}
