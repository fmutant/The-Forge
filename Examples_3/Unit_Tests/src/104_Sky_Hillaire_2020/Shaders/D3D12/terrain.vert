Texture2D<float4> terrainHeightTex : register(t0);
SamplerState samplerLinearClamp : register(s0);


cbuffer cbCamera : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4 mProjectView;
	float4x4 mPrevProjectView;
	float3 mCamPos;
};

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

struct VS_IN
{
	uint vid : SV_VertexID;
	uint iid : SV_InstanceID;
};

struct VS_OUT
{
	float4 positionPS : SV_POSITION;
	float4 positionWS : TEXCOORD0;
	float4 UV0_Nrm : TEXCOORD1;
	
	float4 positionThis : TEXCOORD2;
	float4 positionThat : TEXCOORD3;
};

float2 EncodeNormalOct(float3 n)
{
    n /= ( abs( n.x ) + abs( n.y ) + abs( n.z ) );
    n.xy = n.z >= 0.0 ? n.xy : (1.0f - abs(n.yx)) * (n.xy < 0.0f ? -1.0 : 1.0);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

float4 SampleTerrain(in float quadx, in float quady, in float3 qp)
{
	const float quadWidth = mTerrainWidth * mTerrainTilesCountInv;

	float2 Uvs = (float2(quadx, quady) + qp.xy) * mTerrainTilesCountInv;
#if 0
	const float height = terrainHeightTex.SampleLevel(samplerLinearClamp, Uvs, 0).r;
#else
	const float offset = 0.0008;
	float HeightAccum = terrainHeightTex.SampleLevel(samplerLinearClamp, Uvs + float2(0.0f, 0.0f), 0).r;
	HeightAccum+= terrainHeightTex.SampleLevel(samplerLinearClamp, Uvs + float2( offset, 0.0f), 0).r;
	HeightAccum+= terrainHeightTex.SampleLevel(samplerLinearClamp, Uvs + float2(-offset, 0.0f), 0).r;
	HeightAccum+= terrainHeightTex.SampleLevel(samplerLinearClamp, Uvs + float2( 0.0f, offset), 0).r;
	HeightAccum+= terrainHeightTex.SampleLevel(samplerLinearClamp, Uvs + float2( 0.0f,-offset), 0).r;
	const float height = HeightAccum * 0.2f; // 5;
#endif
	float4 WorldPos = float4((float3(quadx, quady, mTerrainHeight * height) + qp) * quadWidth - 0.5f * float3(mTerrainWidth, mTerrainWidth, 0.0), 1.0f);
	WorldPos.xyz += float3(-mTerrainWidth * 0.45, 0.4*mTerrainWidth, -0.0f);	// offset to position view
	return WorldPos;
}

VS_OUT main(in VS_IN In)
{
	VS_OUT Out = (VS_OUT)0;
	
	uint quadId = In.iid;
	uint vertId = In.vid;

	// For a range on screen in [-0.5,0.5]
	float3 qp = 0.0f;
	qp = vertId == 1 || vertId == 4 ? float3(1.0f, 0.0f, 0.0f) : qp;
	qp = vertId == 2 || vertId == 3 ? float3(0.0f, 1.0f, 0.0f) : qp;
	qp = vertId == 5 ? float3(1.0f, 1.0f, 0.0f) : qp;

	const float quadx = quadId * mTerrainTilesCountInv;
	const float quady = quadId % mTerrainTilesCount;

	float2 Uvs = (float2(quadx, quady) + qp.xy) * mTerrainTilesCountInv;
	float4 posWS = SampleTerrain(quadx, quady, qp);
	posWS.xyz = posWS.xzy - mCamPos;
	posWS.y -= mTerrainHeight * 0.5f;

	Out.positionWS = posWS;
	Out.positionPS = mul(mProjectView, posWS);

	//Out.color = 0.05 * (1.0 - gScreenshotCaptureActive);
	float3 normal;
	{
		const float offset = 5.0;
		float4 WorldPos0_ = SampleTerrain(quadx + qp.x - offset, quady + qp.y, 0.0f);
		float4 WorldPos1_ = SampleTerrain(quadx + qp.x + offset, quady + qp.y, 0.0f);
		float4 WorldPos_0 = SampleTerrain(quadx + qp.x,     quady + qp.y - offset, 0.0f);
		float4 WorldPos_1 = SampleTerrain(quadx + qp.x,     quady + qp.y + offset, 0.0f);
		normal = cross(normalize(WorldPos1_.xyz - WorldPos0_.xyz), normalize(WorldPos_1.xyz - WorldPos_0.xyz));
	}

	Out.UV0_Nrm.xy = Uvs;
	Out.UV0_Nrm.zw = EncodeNormalOct(normalize(normal));
	
	Out.positionThis = Out.positionPS;
	Out.positionThat = mul(mPrevProjectView, posWS);

	return Out;
}