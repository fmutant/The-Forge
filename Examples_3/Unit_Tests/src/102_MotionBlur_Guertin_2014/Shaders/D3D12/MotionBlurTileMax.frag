Texture2D<float2> VelocityTexture : register(t10);

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
	float2 tile_max = float2(0.0f, 0.0f);
	for (float i = 0.0f; i < mConsts.x; i += 1.0f)
	{
		for (float j = 0.0f; j < mConsts.x; j += 1.0f)
		{
			float2 uv_sample = uv_tile + float2(i, j) * mConsts.zw;
			float2 velocity = VelocityTexture.Sample(nearestSamplerBorderZero, uv_sample);
			tile_max = vmax(velocity, tile_max);
		}
	}
	return tile_max;
}
