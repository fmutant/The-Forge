cbuffer cbAtmosphere : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float mRadiusBottom;
	float mRadiusTop;
	float mRadiusBottomSqr;
	float mRadiusTopSqr;

	float3 mScatteringRayleigh;
	float mDistanceBound;
	
	float3 mScatteringMie;
	float mPhaseFunctionGMie;
	
	float3 mExtinctionMie;
	float mSunAngularRadius;
	
	float3 mExtinctionAbsorption;
	float mTransmittanceHeightInv;

	float4 mDensityRayleigh[2];
	
	float2 mLayerWidthRayleigh;
	float2 mTransmittanceSizeInv;
	
	float4 mDensityMie[2];
	
	float2 mLayerWidthMie;
	float2 mIrradienceSizeInv;
	
	float4 mDensityAbsorption[2];
	
	float2 mLayerWidthAbsorption;
	float2 mPad2;
	
	float3 mSunDir;
	float mPad3;
	
	float3 mViewDir;
	float mPad4;
	
	float3 mSolarIrradience;
	float mPad5;
}