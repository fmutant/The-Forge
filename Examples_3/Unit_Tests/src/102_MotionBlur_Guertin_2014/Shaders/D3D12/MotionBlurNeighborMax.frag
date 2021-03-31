#define UNORM 1

Texture2D<float2> TileMaxTexture : register(t11);

cbuffer cbMotionBlurConsts : register(b3)
{
	float4 mConsts;
}

SamplerState nearestSamplerClamp : register(s6);

struct VSOutput {
	float4 position : SV_POSITION;	
	float2 texcoord : TEXCOORD0;
};

float2 vmax_axis(float2 s, float2 v)
{
#if UNORM
	s = s * 2.0f - 1.0f;
#endif
	return dot(s, s) < dot(v, v) ? v : s;
}

float2 vmax_diag(float2 s, float2 v, float offset)
{
#if UNORM
	s = s * 2.0f - 1.0f;
#endif
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
	float2 neighbor_max = TileMaxTexture.Sample(nearestSamplerClamp, uv_tile);
#if UNORM
	neighbor_max = neighbor_max * 2.0f - 1.0f;
#endif
	s = TileMaxTexture.Sample(nearestSamplerClamp, uv_tile + uv_tile_diff * float2(-1.0f,  0.0f));
	neighbor_max = vmax_axis(s, neighbor_max);
	s = TileMaxTexture.Sample(nearestSamplerClamp, uv_tile + uv_tile_diff * float2( 1.0f,  0.0f));
	neighbor_max = vmax_axis(s, neighbor_max);
	s = TileMaxTexture.Sample(nearestSamplerClamp, uv_tile + uv_tile_diff * float2( 0.0f, -1.0f));
	neighbor_max = vmax_axis(s, neighbor_max);
	s = TileMaxTexture.Sample(nearestSamplerClamp, uv_tile + uv_tile_diff * float2( 0.0f,  1.0f));
	neighbor_max = vmax_axis(s, neighbor_max);
	s = TileMaxTexture.Sample(nearestSamplerClamp, uv_tile + uv_tile_diff * float2(-1.0f, -1.0f));
	neighbor_max = vmax_diag(s, neighbor_max, float2(-1.0f, -1.0f));
	s = TileMaxTexture.Sample(nearestSamplerClamp, uv_tile + uv_tile_diff * float2( 1.0f, -1.0f));
	neighbor_max = vmax_diag(s, neighbor_max, float2( 1.0f, -1.0f));
	s = TileMaxTexture.Sample(nearestSamplerClamp, uv_tile + uv_tile_diff * float2(-1.0f,  1.0f));
	neighbor_max = vmax_diag(s, neighbor_max, float2(-1.0f,  1.0f));
	s = TileMaxTexture.Sample(nearestSamplerClamp, uv_tile + uv_tile_diff * float2( 1.0f,  1.0f));
	neighbor_max = vmax_diag(s, neighbor_max, float2( 1.0f,  1.0f));
#if UNORM
	neighbor_max = neighbor_max * 0.5f + 0.5f;
#endif
	return neighbor_max;
}