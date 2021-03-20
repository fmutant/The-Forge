Texture2D<float4> SceneTexture : register(t1);
Texture2D<float> DepthTexture : register(t9);
Texture2D<float2> VelocityTexture : register(t10);
//Texture2D<float2> TileMaxTexture : register(t11);
Texture2D<float2> NeighborMaxTexture : register(t12);

static const float gfPI = 3.14159265359f;
static const float gfEpsilon = 1e-6f;

cbuffer cbMotionBlurConsts : register(b3)
{
	float4 consts;
	float4 params;
}

SamplerState nearestSamplerBorderZero : register(s6);

struct VSOutput {
	float4 position : SV_POSITION;	
	float2 texcoord : TEXCOORD0;
};

float cone(float uvdiff, float vlen)
{
	return saturate(1.0f - uvdiff * vlen);
}
float cylinder(float uvdiff, float vlen)
{
	return 1.0f - smoothstep(0.95f * vlen, 1.05f * vlen, uvdiff);
}
float z_compare(float Za, float Zb)
{
	return saturate(1.0f - (Za - Zb) / min(Za, Zb));
}

float random_glsl(float2 sv_position)
{
	return frac(sin(dot(sv_position.xy, float2(12.9898f, 78.233f))) * 43758.5453123f);
}

float random_wang(float2 sv_position)
{
	uint seed = uint(sv_position.x + sv_position.y / consts.z);
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
	return float(seed) * (1.0f / 4294967296.0f);
}

float random_jimenez(float2 sv_position)
{
	return frac(52.9829189f * frac(0.06711056f * sv_position.x + 0.00583715f * sv_position.y));
}

float random_half(float2 sv_position)
{
    return random_glsl(sv_position) - 0.5f;
}

float halton(float2 sv_position, float x, float y)
{
	return random_glsl(sv_position) * (y - x) + x;
}

float random(float2 sv_position, float from, float to)
{
	//return random_glsl(sv_position) * (to - from) + from;
	return random_jimenez(sv_position) * (to - from) + from;
}

float2 sOffset(float2 sv_position, float j)
{
	float2 offsets[] = {
		float2(-1.0f,  0.0f),
		float2( 1.0f,  0.0f),
		float2( 0.0f, -1.0f),
		float2( 0.0f,  1.0f),
	};
	return offsets[j * 4.0f + 0.24f * 4.0f] * consts.zw;
}

float4 main(VSOutput input) : SV_TARGET
{
	const float fN = params.x;
	const float fr = consts.x;
	const float fTau = 1.0f;
	const float fKappa = 40.0f;
	const float fEta = 0.95f;
	const float fGamma = 1.5;
	const float fPhi = 27.0f;
	
	float2 sv_position = input.position.xy;
	float j = random(sv_position, -1.0f, 1.0f);
	
	float2 uv0 = input.texcoord;
	float4 Cx = SceneTexture.Sample(nearestSamplerBorderZero, uv0);
	float2 Vn = NeighborMaxTexture.Sample(nearestSamplerBorderZero, uv0 + sOffset(sv_position, j));
	float Vnlen = length(Vn);
	if (Vnlen <= 0.5f) return Cx;
	
	float Zp = DepthTexture.Sample(nearestSamplerBorderZero, uv0);
	float2 Vc = VelocityTexture.Sample(nearestSamplerBorderZero, uv0);
	float Vclen = length(Vc) + gfEpsilon;
	
	float2 Wn = normalize(Vn);
	float2 Wp = float2(-Wn.y, Wn.x);
	if (dot(Wp, Vc) < 0.0f) Wp = -Wp;
#if 0
	//float2 Wc = /*normalize*/(lerp(Vc / Vclen, Wp, saturate(Vclen - 0.5f) / fGamma));
	float2 Wc = (lerp(Vc / Vclen, Wp, min(Vclen - 0.5f, 0.0) / fGamma));
#else
	float2 Wc = (lerp(Wp, Vc / Vclen, min(Vclen - 0.5f, 0.0f) / fGamma));
#endif
	
	float total_weight = fN / (fKappa * Vclen);
	float4 result = Cx * total_weight;
	
	float j_prime = j * fEta * fPhi / fN;
	uint iu = 0u;
	[loop]
	for (float i = 0.0f; i < fN; i += 1.0f)
	{
		float t = lerp(-params.z, params.z, (i + j_prime + 1.0f) / (fN + 1.0f));
		float2 d = (iu & 0x1u) ? Vc : Vn;
		float T = t * Vnlen;
		float2 S = uv0 + t * d * consts.zw;
		float2 Vs = VelocityTexture.Sample(nearestSamplerBorderZero, S);
		float4 Cs = SceneTexture.Sample(nearestSamplerBorderZero, S);
		float Zs = DepthTexture.Sample(nearestSamplerBorderZero, S);
		float f = z_compare(Zp, Zs);
		float b = z_compare(Zs, Zp);
		
		float Vslen = length(Vs) + gfEpsilon;
		float weight = 0.0f;
#if 1
		float Wa = abs(dot(Wc, d));
#else
		float Wa = saturate(dot(Wc, d));
#endif
		if (Wa < 0.0f)	return float4(Vn, 0.0f, 1.0f);
#if 0
		float Vslenhalf = clamp(Vslen, gfEpsilon, 1.0f);
#else
		float Vslenhalf = clamp(Vslen, gfEpsilon, 0.5f);
#endif
#if 1
		float Wb = abs(dot(Vs / Vslenhalf, d));
#else
		float Wb = saturate(dot(Vs / Vslenhalf, d));
#endif
		if (Wb < 0.0f)	return 0;

		weight += f * cone(T, 1.0f / Vslen) * Wb;
		weight += b * cone(T, 1.0f / Vclen) * Wa;
		weight += 2.0f * cylinder(T, min(Vslen, Vclen)) * max(Wa, Wb);
		
		total_weight += weight;
		result += Cs * weight;
	}
	
	return result / total_weight;
}

