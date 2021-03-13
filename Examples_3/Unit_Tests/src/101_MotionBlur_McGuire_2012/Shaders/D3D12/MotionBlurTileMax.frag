Texture2D<float2> VelocityTexture : register(t10);

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
	float2 uv_pixel = uv_tile / mKasfloat;
	float2 uv_pixel_diff = float2(1.0f / mWidth, 1.0f / mHeight);
	
	float2 tile_max = float2(0.0f, 0.0f);
	for (float i = 0.0f; i < mKasfloat; i += 1.0f)
	{
		for (float j = 0.0f; j < mKasfloat; j += 1.0f)
		{
			float2 uv_sample = float2(i, j) * uv_pixel_diff;
			float2 velocity = VelocityTexture.Sample(nearestSampler, uv_pixel + uv_sample);
			tile_max = vmax(velocity, tile_max);
		}
	}
	return tile_max;
}
