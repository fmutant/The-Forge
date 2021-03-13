Texture2D SceneTexture : register(t1);
Texture2D<float> DepthTexture : register(t9);
Texture2D<float2> VelocityTexture : register(t10);
Texture2D<float2> TileMaxTexture : register(t11);
Texture2D<float2> NeighborMaxTexture : register(t12);

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

float4 main(VSOutput input) : SV_TARGET
{
	float2 texcoord_pixel = input.texcoord;
	float2 texcoord_tile = texcoord_pixel / mKasfloat;
	
	float4 color = SceneTexture.Sample(nearestSampler, texcoord_pixel);
	float depth = DepthTexture.Sample(nearestSampler, texcoord_pixel);
	
	float2 tilemax = TileMaxTexture.Sample(nearestSampler, texcoord_tile);
	float2 neighbormax = NeighborMaxTexture.Sample(nearestSampler, texcoord_tile);
	
	float2 velocity = VelocityTexture.Sample(nearestSampler, texcoord_pixel);
	
	return color * depth * tilemax.xyxy * neighbormax.xyxy * velocity.xyxy;
}
