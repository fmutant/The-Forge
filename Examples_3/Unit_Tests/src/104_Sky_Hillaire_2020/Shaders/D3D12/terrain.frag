cbuffer cbTerrain : register(b1, UPDATE_FREQ_PER_FRAME)
{
	uint2 mViewportSize;
	uint mTerrainTilesCount;
	float mTerrainTilesCountInv;

	float mTerrainWidth;
	float mTerrainHeight;
	float mTerrainAlbedoMultiplier;
	float mPad0;
};

struct VS_OUT
{
	float4 positionPS : SV_POSITION;
	float4 positionWS : TEXCOORD0;
	float4 UV0_Nrm : TEXCOORD1;
	
	float4 positionThis : TEXCOORD2;
	float4 positionThat : TEXCOORD3;
};

typedef VS_OUT PS_IN;

struct PS_OUT
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
    float4 specular : SV_Target2;
    float2 motion : SV_Target3;
};

float3 DecodeNormalOct(float2 f)
{
    f = f * 2.0 - 1.0;
	
    float3 n = float3( f.x, f.y, 1.0 - abs( f.x ) - abs( f.y ) );
    float t = saturate( -n.z );
    n.xy += n.xy >= 0.0 ? -t : t;
    return normalize( n );
}

PS_OUT main(in PS_IN In)
{
	PS_OUT Out;
	float3 N = normalize(DecodeNormalOct(In.UV0_Nrm.zw));
	float NoL = max(0.0, dot(float3(0.0f, 1.0f, 0.0f), N));

	// Second evaluate transmittance due to participating media
	//AtmosphereParameters Atmosphere = GetAtmosphereParameters();
	//float3 P0 = In.positionWS.xyz / In.positionWS.w + float3(0, 0, Atmosphere.BottomRadius);
	//float viewHeight = length(P0);
	//const float3 UpVector = P0 / viewHeight;
	//float viewZenithCosAngle = dot(sun_direction, UpVector);
	//float2 uv;
	//LutTransmittanceParamsToUv(Atmosphere, viewHeight, viewZenithCosAngle, uv);
	//const float3 trans = TransmittanceLutTexture.SampleLevel(samplerLinearClamp, uv, 0).rgb;

	float3 color = float3(1.0f, 1.0f, 1.0f) * mTerrainAlbedoMultiplier;
	//return float4(color * NoL, 1.0f);
	
	Out.albedo = float4(color * NoL, 1.0f);
    Out.normal = float4(N, 0.0f);
    Out.specular = float4(0.0f, 0.0f, 0.0f, 0.0f);
    Out.motion = In.positionThis.xy / In.positionThis.w - In.positionThat.xy / In.positionThat.w;
	
	return Out;
}