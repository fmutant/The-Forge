/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
 * 
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

cbuffer cbExposure : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float aperture;
	float shutter;
	float iso;
	float luminance;
	
	float EV100;
	float EVmanualSBS;
	float EVmanualSOS;
	float pad0;
	
	float EVaverage;
	float EVmean;
	float pad1;
	uint EVmode;
}

#define FLT_MAX  3.402823466e+38F

Texture2D SceneTexture : register(t1);

SamplerState nearestSampler : register(s4);
SamplerState bilinearSampler : register(s5);


struct VSOutput
{
	float4 Position : SV_POSITION;	
	float2 uv:    TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{	
	float4 outColor = float4(0.0, 0.0, 0.0, 0.0);
	
	float EVtarget = 1.0f;
	switch (EVmode)
	{
		case 0: EVtarget = EVmanualSBS; break;
		case 1: EVtarget = EVmanualSOS; break;
		case 2: EVtarget = EVaverage; break;
		case 3: EVtarget = EVmean; break;
		default: EVtarget = 1.0f; break;
	};
	
	outColor = SceneTexture.Sample(bilinearSampler, input.uv);
	outColor = outColor * EVtarget;
	float gammaCorr = 1.0f / 2.2f;

	outColor.r = pow(outColor.r, gammaCorr);
	outColor.g = pow(outColor.g, gammaCorr);
	outColor.b = pow(outColor.b, gammaCorr);
	outColor.w = 1.0f;

	return outColor;
}