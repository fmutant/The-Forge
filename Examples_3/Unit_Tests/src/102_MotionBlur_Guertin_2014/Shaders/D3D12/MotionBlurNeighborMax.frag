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

float2 vmax_axis(float2 s, float2 v)
{
	return dot(s, s) < dot(v, v) ? v : s;
}

float2 vmax_diag(float2 s, float2 v, float offset)
{
	float Slen = length(s) + 1e-6f;
	float weight = saturate(dot(s / Slen, -normalize(offset)));
	float2 sv = ceil(weight) * s;
	return dot(sv, sv) < dot(v, v) ? v : s;
}

float2 main(VSOutput input) : SV_TARGET
{
	float2 s;
	float2 uv_tile = input.texcoord;
	float2 uv_tile_diff = mConsts.xx * mConsts.zw;
	float2 neighbor_max = TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile);
	s = TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2(-1.0f,  0.0f));
	neighbor_max = vmax_axis(s, neighbor_max);
	s = TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2( 1.0f,  0.0f));
	neighbor_max = vmax_axis(s, neighbor_max);
	s = TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2( 0.0f, -1.0f));
	neighbor_max = vmax_axis(s, neighbor_max);
	s = TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2( 0.0f,  1.0f));
	neighbor_max = vmax_axis(s, neighbor_max);
	s = TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2(-1.0f, -1.0f));
	neighbor_max = vmax_diag(s, neighbor_max, float2(-1.0f, -1.0f));
	s = TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2( 1.0f, -1.0f));
	neighbor_max = vmax_diag(s, neighbor_max, float2( 1.0f, -1.0f));
	s = TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2(-1.0f,  1.0f));
	neighbor_max = vmax_diag(s, neighbor_max, float2(-1.0f,  1.0f));
	s = TileMaxTexture.Sample(nearestSamplerBorderZero, uv_tile + uv_tile_diff * float2( 1.0f,  1.0f));
	neighbor_max = vmax_diag(s, neighbor_max, float2( 1.0f,  1.0f));

	return neighbor_max;
}

