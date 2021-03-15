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



cbuffer cbCamera : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4 projView;
	float4x4 prevProjView;
	float3 camPos;
	
	float4 motionBlurParams;
	float farOverNear;
}

Texture2D<float> DepthTexture : register(t9);
SamplerState nearestSamplerBorder : register(s6);

struct PsIn
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct PSOut
{
    float linearDepth : SV_Target0;
};


PSOut main(PsIn input) : SV_TARGET
{
	PSOut Out;
	
	float depth = DepthTexture.Sample(nearestSamplerBorder, input.uv);
	float c1 = farOverNear;
	float c0 = 1.0f - c1;
	
	Out.linearDepth = 1.0f / (c0 * depth + c1);

	return Out;
}