Texture2D<min16float> LuminanceMipPrev : register(t0);

SamplerState bilinearClampSampler : register(s0);

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
};

min16float main(VSOutput input) : SV_TARGET
{
	return log2(LuminanceMipPrev.Sample(bilinearClampSampler, input.texcoord));
}
