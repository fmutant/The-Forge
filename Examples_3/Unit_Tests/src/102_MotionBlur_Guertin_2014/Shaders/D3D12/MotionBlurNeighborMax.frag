Texture2D<float2> TileMaxTexture : register(t11);

cbuffer cbMotionBlurConsts : register(b3)
{
	float4 mConsts;
}

SamplerState nearestSamplerBorderZero : register(s6);

struct VSOutput {
	float4 position : SV_POSITION;	
	float2 texcoord : TEXCOORD0;
};

float2 vmax(float2 v0, float2 v1)
{
	return dot(v0, v0) < dot(v1, v1) ? v1 : v0;
}

float2 main(VSOutput input) : SV_TARGET
{
	float2 uv_tile = input.texcoord;
	float2 uv_tile_diff = mConsts.xx * mConsts.zw;
	float2 neighbor_max = float2(0.0f, 0.0f);
	for (float i = -1.0f; i <= 1.0f; i += 1.0f)
	{
		for (float j = -1.0f; j <= 1.0f; j += 1.0f)
		{
			float2 uv_sample = uv_tile + float2(i, j) * uv_tile_diff;
			float2 tilemax = TileMaxTexture.Sample(nearestSamplerBorderZero, uv_sample);
			neighbor_max = vmax(tilemax, neighbor_max);
		}
	}
	return neighbor_max;
}
