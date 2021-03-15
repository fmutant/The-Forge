Texture2D<float4> SceneTexture : register(t1);
Texture2D<float> DepthTexture : register(t9);
Texture2D<float2> VelocityTexture : register(t10);
//Texture2D<float2> TileMaxTexture : register(t11);
Texture2D<float2> NeighborMaxTexture : register(t12);

cbuffer cbMotionBlurConsts : register(b3)
{
	float4 consts;
	float4 params;
}

SamplerState nearestSamplerBorder : register(s6);

struct VSOutput {
	float4 position : SV_POSITION;	
	float2 texcoord : TEXCOORD0;
};

float random_half(float2 _st) {
    return frac(sin(dot(_st.xy,
                         float2(12.9898f, 78.233f))) *
        43758.5453123f)- 0.5f;
}

float cone(float2 uv0, float2 uv1, float2 V)
{
	return saturate(1.0f - length(uv0 - uv1) / length(V));
}
float cylinder(float2 uv0, float2 uv1, float2 V)
{
	float vlen = length(V);
	return 1.0f - smoothstep(0.95f * vlen, 1.05f * vlen, length(uv0 - uv1));
}
float softDepthCompare(float Za, float Zb)
{
	return saturate(1.0f - (Za - Zb) / params.z);
}

void Accumulate(float2 uv0, float t, float2 Vn, float2 Vx, float Dx, inout float4 sum, inout float weight)
{
	float2 uv1 = uv0 + (Vn * t + 0.5f) * consts.zw;
	
	float4 Cy = SceneTexture.Sample(nearestSamplerBorder, uv1);
	float Dy = DepthTexture.Sample(nearestSamplerBorder, uv1);
	float f = softDepthCompare(Dx, Dy);
	float b = softDepthCompare(Dy, Dx);
	
	float2 Vy = VelocityTexture.Sample(nearestSamplerBorder, uv1);
	float alpha = 	f * cone(uv1, uv0, Vy) +
					b * cone(uv0, uv1, Vx) +
					2.0f * cylinder(uv1, uv0, Vy) * cylinder(uv0, uv1, Vx);

	weight += alpha;
	sum += Cy * alpha;
}

float4 main(VSOutput input) : SV_TARGET
{
	float2 uv0 = input.texcoord;
	float4 Cx = SceneTexture.Sample(nearestSamplerBorder, uv0);
	float2 Vn = NeighborMaxTexture.Sample(nearestSamplerBorder, uv0);
	if (length(Vn) <= 1e-6f + 0.5f) return Cx;

	float2 Vx = VelocityTexture.Sample(nearestSamplerBorder, uv0);
	float Dx = DepthTexture.Sample(nearestSamplerBorder, uv0);
	float weight = 1.0f / length(Vx);
	float4 sum = Cx * weight;

	float j = random_half(uv0);
	static const float SamplesCount = 15.0f;
	for (float i0 = 0.0f, e0 = SamplesCount * 0.5f - 0.5f; i0 < e0; i0 += 1.0f)
	{
		float t = lerp(-1.0f, 1.0f, (i0 + j + 1.0f) / (SamplesCount + 1.0f));
		Accumulate(uv0, t, Vn, Vx, Dx, sum, weight);
	}

	for (float i1 = ceil(SamplesCount * 0.5f), e1 = SamplesCount; i1 < e1; i1 += 1.0f)
	{
		float t = lerp(-1.0f, 1.0f, (i1 + j + 1.0f) / (SamplesCount + 1.0f));
		Accumulate(uv0, t, Vn, Vx, Dx, sum, weight);
	}

	return sum / weight;
}
