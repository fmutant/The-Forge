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

float cone(float2 uv0, float2 uv1, float vlen)
{
	return saturate(1.0f - length(uv0 - uv1) / vlen);
}
float cylinder(float2 uv0, float2 uv1, float vlen)
{
	return 1.0f - smoothstep(0.95f * vlen, 1.05f * vlen, length(uv0 - uv1));
}
float softDepthCompare(float Za, float Zb)
{
	static const float cZExtent = 10.0f; // div by 0.1 in paper
	return saturate(1.0f - (Za - Zb) * cZExtent);
}

void Accumulate(float2 uv0, float t, float2 Vn, float vxlen, float Dx, inout float4 sum, inout float weight)
{
	float2 uv1 = uv0 + (Vn * t + 0.5f) * consts.zw;
	
	float4 Cy = SceneTexture.Sample(nearestSamplerBorder, uv1);
	float Dy = DepthTexture.Sample(nearestSamplerBorder, uv1);
	float f = softDepthCompare(Dx, Dy);
	float b = softDepthCompare(Dy, Dx);
	
	float2 Vy = VelocityTexture.Sample(nearestSamplerBorder, uv1);
	float vylen = length(Vy);
	float alpha = 	f * cone(uv1, uv0, vylen) +
					b * cone(uv0, uv1, vxlen) +
					2.0f * cylinder(uv1, uv0, vylen) * cylinder(uv0, uv1, vxlen);

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
	float Vxlen = length(Vx);
	float Dx = DepthTexture.Sample(nearestSamplerBorder, uv0);
	float weight = 1.0f / Vxlen;
	float4 sum = Cx * weight;

	float j = random_half(uv0);
	float params_count_inv = 1.0f / (params.x + 1.0f);
	for (float i0 = 0.0f, e0 = params.x * 0.5f - 0.5f; i0 < e0; i0 += 1.0f)
	{
		float t = lerp(-params.z, params.z, (i0 + j + 1.0f) * params_count_inv);
		Accumulate(uv0, t, Vn, Vxlen, Dx, sum, weight);
	}

	for (float i1 = ceil(params.x * 0.5f), e1 = params.x; i1 < e1; i1 += 1.0f)
	{
		float t = lerp(params.z, params.z, (i1 + j + 1.0f) * params_count_inv);
		Accumulate(uv0, t, Vn, Vxlen, Dx, sum, weight);
	}

	return sum / weight;
}
