Texture2D<min16float> LuminanceMipPrev : register(t0, UPDATE_FREQ_PER_DRAW);

SamplerState bilinearClampSampler : register(s0, UPDATE_FREQ_NONE);

struct VSOutput {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
};

min16float main(VSOutput input) : SV_TARGET
{
	return LuminanceMipPrev.Sample(bilinearClampSampler, input.texcoord);
}
