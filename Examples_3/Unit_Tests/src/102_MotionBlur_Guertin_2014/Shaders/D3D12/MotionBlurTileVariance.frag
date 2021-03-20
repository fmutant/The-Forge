Texture2D<float2> TileMaxTexture : register(t11);

cbuffer cbMotionBlurConsts : register(b3)
{
	float4 mConsts;
}

SamplerState pSamplerNearestEdge : register(s7);

struct VSOutput {
	float4 position : SV_POSITION;	
	float2 texcoord : TEXCOORD0;
};

float2 main(VSOutput input) : SV_TARGET
{
	float2 uv_tile = input.texcoord;
	float2 uv_tile_diff = mConsts.xx * mConsts.zw;
	float tile_variance = 0.0f;
	float2 this_tile = TileMaxTexture.Sample(pSamplerNearestEdge, uv_tile);
	for (float i = -1.0f; i <= 1.0f; i += 1.0f)
	{
		for (float j = -1.0f; j <= 1.0f; j += 1.0f)
		{
			float2 uv_sample = uv_tile + float2(i, j) * uv_tile_diff;
			float2 that_tile = TileMaxTexture.Sample(pSamplerNearestEdge, uv_sample);
			tile_variance += abs(dot(that_tile, this_tile));
		}
	}
	return 1.0f - tile_variance * 0.111111f;
}
