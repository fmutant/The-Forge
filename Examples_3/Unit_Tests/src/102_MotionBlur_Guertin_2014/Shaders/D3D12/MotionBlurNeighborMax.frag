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

float2 vmax_axis(float2 sample, float2 vmax)
{
	return dot(sample, sample) < dot(vmax, vmax) ? vmax : sample;
}

float vmax_diag(float2 sample, float2 vmax, float offset)
{
	float2 sample_value = ceil(saturate(dot(sample, -offset))) * sample;
	return dot(sample_value, sample_value) < dot(vmax, vmax) ? vmax : sample;
}

float2 main(VSOutput input) : SV_TARGET
{
	float2 uv_tile = input.texcoord;
	float2 uv_tile_diff = mConsts.xx * mConsts.zw;
	float2 neighbor_max = TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile);

	neighbor_max = vmax_axis(TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2(-1.0f,  0.0f)), neighbor_max);
	neighbor_max = vmax_axis(TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2( 1.0f,  0.0f)), neighbor_max);
	neighbor_max = vmax_axis(TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2( 0.0f, -1.0f)), neighbor_max);
	neighbor_max = vmax_axis(TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2( 0.0f,  1.0f)), neighbor_max);
	neighbor_max = vmax_diag(TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2(-1.0f, -1.0f)), neighbor_max, float2(-1.0f, -1.0f));
	neighbor_max = vmax_diag(TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2( 1.0f, -1.0f)), neighbor_max, float2( 1.0f, -1.0f));
	neighbor_max = vmax_diag(TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2(-1.0f,  1.0f)), neighbor_max, float2(-1.0f,  1.0f));
	neighbor_max = vmax_diag(TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2( 1.0f,  1.0f)), neighbor_max, float2( 1.0f,  1.0f));

	return neighbor_max;
}
